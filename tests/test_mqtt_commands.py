"""Run the production PUBLISH dispatcher against device-scoped command packets."""
import pathlib
import subprocess
import tempfile
import unittest
from test_mqtt_discovery import ROOT, function


class CommandTest(unittest.TestCase):
    def test_scope_retention_and_malformed_packets(self):
        main = (ROOT / "source/mqtt_client.c").read_text()
        code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define SHA_MAX_TOPIC 128
#define NOTIFY_TEXT_MAX 512
typedef struct {char device_name[64],mqtt_client_id[64],mqtt_topic_prefix[128];} AppConfig;
typedef struct {int unused;} AppState;
static int commands,controls,notifications,acks;
static bool mqtt_send_packet(int fd,const void*b,size_t n){(void)fd;(void)b;assert(n==4);acks++;return true;}
static bool extended_command(AppState*s,int fd,const AppConfig*c,const char*k,const char*p){(void)s;(void)fd;(void)c;assert(strcmp(k,"screen")==0);assert(strcmp(p,"ON")==0);controls++;return true;}
static void handle_command_payload(AppState*s,int fd,const AppConfig*c,const char*p){(void)s;(void)fd;(void)c;assert(strcmp(p,"reboot")==0);commands++;}
static void handle_notification_payload(AppState*s,int fd,const AppConfig*c,const char*t,const char*p){(void)s;(void)fd;(void)c;(void)t;(void)p;notifications++;}
'''
        code += "\n".join(function(main, n) for n in ["sanitize_id", "client_id_or_device", "state_topic", "notification_topic", "read_mqtt_remaining_length", "handle_mqtt_publish"])
        code += r'''
static void send_publish(AppState*s,AppConfig*c,unsigned char flags,const char*topic,const char*payload) {
 unsigned char b[1024];size_t tn=strlen(topic),pn=strlen(payload),n=2+tn+pn+((flags&6)?2:0);
 assert(n<128);b[0]=flags;b[1]=n;b[2]=tn>>8;b[3]=tn;memcpy(b+4,topic,tn);size_t pos=4+tn;
 if(flags&6){b[pos++]=0;b[pos++]=42;}
 memcpy(b+pos,payload,pn);handle_mqtt_publish(s,1,c,b,pos+pn);
}
int main(void){
 AppState state={0};AppConfig c={.mqtt_client_id="switch-a",.mqtt_topic_prefix="homeassistant"};
 send_publish(&state,&c,0x30,"switch_ha/switch_a/command","reboot");assert(commands==1);
 send_publish(&state,&c,0x30,"switch_ha/switch_b/command","reboot");assert(commands==1);
 send_publish(&state,&c,0x30,"homeassistant/command","reboot");assert(commands==1);
 send_publish(&state,&c,0x31,"switch_ha/switch_a/command","reboot");assert(commands==1);
 send_publish(&state,&c,0x32,"switch_ha/switch_a/command","reboot");assert(commands==2 && acks==1);
 send_publish(&state,&c,0x33,"switch_ha/switch_a/command","reboot");assert(commands==2 && acks==2);
 send_publish(&state,&c,0x30,"switch_ha/switch_a/control/screen","ON");assert(controls==1);
 send_publish(&state,&c,0x31,"switch_ha/switch_a/control/screen","ON");assert(controls==1);
 send_publish(&state,&c,0x30,"switch_ha/switch_b/control/screen","ON");assert(controls==1);
 unsigned char truncated[]={0x30,100,0,1,'a'};handle_mqtt_publish(&state,1,&c,truncated,sizeof(truncated));assert(commands==2);
 unsigned char bad_topic[]={0x30,2,0xff,0xff};handle_mqtt_publish(&state,1,&c,bad_topic,sizeof(bad_topic));
 puts("Command scope, retained-message rejection, QoS1 acknowledgments and malformed packets passed");
}
'''
        with tempfile.TemporaryDirectory() as temp:
            c = pathlib.Path(temp) / "commands.c"
            binary = pathlib.Path(temp) / "commands"
            c.write_text(code)
            subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", str(c), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
