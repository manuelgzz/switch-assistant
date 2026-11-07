"""Execute production sleep publishing against a local MQTT protocol peer."""
import pathlib
import subprocess
import tempfile
import unittest
from test_mqtt_discovery import ROOT, function


class SleepMqttTest(unittest.TestCase):
    def test_sleep_retains_mode_without_replacing_telemetry(self):
        main = (ROOT / "source/mqtt_client.c").read_text()
        ext = (ROOT / "source/mqtt_extensions.inc").read_text()
        code = r'''
#include "mqtt_wire.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#define MQTT_BUFFER 2048
typedef uint64_t u64;
typedef struct {char device_name[64],mqtt_client_id[64],mqtt_topic_prefix[128];} AppConfig;
typedef struct {int lock;bool mqtt_sleep_publish_finished,ha_validated;} AppState;
static void mutexLock(int*x){(void)x;}
static void mutexUnlock(int*x){(void)x;}
static u64 monotonic_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (u64)t.tv_sec*1000+t.tv_nsec/1000000;}
'''
        names = ["encode_remaining_length", "write_string", "mqtt_send_packet", "socket_recv_all", "mqtt_send_publish_bytes", "mqtt_send_publish", "sanitize_id", "client_id_or_device", "state_topic"]
        code += "\n".join(function(main, n) for n in names)
        code += "\n" + function(ext, "ext_escape")
        code += "\n" + function(main, "publish_state")
        code += "\n" + function(main, "mqtt_publish_sleep")
        code += r'''
static bool expect_sleep,drop_ack;
static unsigned publications;
static char retained_mode[32];
static void*broker(void*arg){
 int fd=*(int*)arg;unsigned char frame[2048];
 while(true){
  int n=mqtt_read_frame(fd,frame,sizeof(frame),socket_recv_all);assert(n>0);
  if(frame[0]==0x31 || frame[0]==0x33){
   size_t pos=1;while(frame[pos++]&128){}
   size_t len=(frame[pos]<<8)|frame[pos+1];pos+=2;
   char topic[192],payload[1000];assert(len<sizeof(topic));memcpy(topic,frame+pos,len);topic[len]=0;pos+=len;
   bool qos1=frame[0]==0x33;
   if(qos1){assert(frame[pos]==0x53 && frame[pos+1]==0x48);pos+=2;}
   assert((size_t)n-pos<sizeof(payload));memcpy(payload,frame+pos,n-pos);payload[n-pos]=0;
   publications++;
   if(strstr(topic,"/operation_mode")){assert(expect_sleep);assert(strcmp(payload,"Sleep")==0);strcpy(retained_mode,payload);}
   else if(strstr(topic,"/availability"))assert(strcmp(payload,"offline")==0);
   else if(strstr(topic,"/status"))assert(strstr(payload,expect_sleep?"\"status\":\"sleep\"":"\"status\":\"offline\""));
   else assert(!"Sleep must not overwrite battery/game/audio telemetry");
   if(qos1){assert(expect_sleep&&publications==3);const unsigned char ack[]={0x40,2,0x53,0x48};if(!drop_ack)assert(send(fd,ack,4,0)==4);}
  }else {assert(frame[0]==0xe0&&n==2);break;}
 }
 return NULL;
}
int main(void){
 AppConfig c={.device_name="Switch",.mqtt_client_id="switch_a"};
 for(int scenario=0;scenario<3;scenario++){
  expect_sleep=scenario!=1;drop_ack=scenario==2;publications=0;strcpy(retained_mode,"Handheld");
  int pair[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
  pthread_t peer;assert(pthread_create(&peer,NULL,broker,&pair[1])==0);
  AppState state={0};
  if(expect_sleep){u64 started=monotonic_ms();assert(mqtt_publish_sleep(&state,pair[0],&c)==!drop_ack);assert(monotonic_ms()-started<600);assert(state.mqtt_sleep_publish_finished);}
  else {publish_state(&state,pair[0],&c,"offline");const unsigned char end[]={0xe0,0};assert(mqtt_send_packet(pair[0],end,2));}
  pthread_join(peer,NULL);close(pair[0]);close(pair[1]);
  assert(publications==(expect_sleep?3:2));
  assert(strcmp(retained_mode,expect_sleep?"Sleep":"Handheld")==0);
 }
 puts("Sleep MQTT: retained mode, broker delivery barrier, graceful disconnect and ordinary-offline distinction passed");
}
'''
        with tempfile.TemporaryDirectory() as temp:
            c = pathlib.Path(temp) / "sleep.c"
            binary = pathlib.Path(temp) / "sleep"
            c.write_text(code)
            subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-pthread", "-I" + str(ROOT / "include"), str(c), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=10)


if __name__ == "__main__":
    unittest.main()
