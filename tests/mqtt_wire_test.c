#include "mqtt_wire.h"
#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>

static bool receive_all(int fd,unsigned char*b,size_t n) {
    while(n){ssize_t got=recv(fd,b,n,0);if(got<=0)return false;b+=got;n-=got;}
    return true;
}
static void *fragment(void *arg) {
    int fd=*(int*)arg;
    unsigned char data[]={0x30,0x05,0x00,0x01,'a','O','N',0xd0,0x00};
    for(size_t i=0;i<sizeof(data);i++){assert(write(fd,data+i,1)==1);usleep(1000);}
    return NULL;
}
int main(void) {
    int pair[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
    pthread_t thread;assert(pthread_create(&thread,NULL,fragment,&pair[1])==0);
    unsigned char b[256];
    assert(mqtt_read_frame(pair[0],b,sizeof(b),receive_all)==7);
    assert(memcmp(b,"\x30\x05\x00\x01" "aON",7)==0);
    assert(mqtt_read_frame(pair[0],b,sizeof(b),receive_all)==2 && b[0]==0xd0);
    pthread_join(thread,NULL);
    unsigned char coalesced[]={0xc0,0,0xd0,0};assert(write(pair[1],coalesced,4)==4);
    assert(mqtt_read_frame(pair[0],b,sizeof(b),receive_all)==2 && b[0]==0xc0);
    assert(mqtt_read_frame(pair[0],b,sizeof(b),receive_all)==2 && b[0]==0xd0);
    unsigned char multi[131]={0x30,0x80,0x01};
    memset(multi+3,'x',128);assert(write(pair[1],multi,sizeof(multi))==sizeof(multi));
    assert(mqtt_read_frame(pair[0],b,sizeof(b),receive_all)==131);
    assert(b[3]=='x' && b[130]=='x');
    unsigned char large[]={0x30,0xff,0x7f};assert(write(pair[1],large,3)==3);
    assert(mqtt_read_frame(pair[0],b,sizeof(b),receive_all)==-1);
    unsigned char bad[]={0x30,0x80,0x80,0x80,0x80};assert(write(pair[1],bad,5)==5);
    assert(mqtt_read_frame(pair[0],b,sizeof(b),receive_all)==-1);
    close(pair[1]);assert(mqtt_read_frame(pair[0],b,sizeof(b),receive_all)==-1);close(pair[0]);
    assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
    unsigned char truncated[]={0x30,5,0,1};
    assert(write(pair[1],truncated,sizeof(truncated))==sizeof(truncated));close(pair[1]);
    assert(mqtt_read_frame(pair[0],b,sizeof(b),receive_all)==-1);close(pair[0]);
    assert(mqtt_read_frame(-1,b,1,receive_all)==-1);
    puts("MQTT framing: fragmented/coalesced/multibyte/oversized/malformed/truncated/EOF cases passed");
}
