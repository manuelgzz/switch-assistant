"""Verify passive MQTT last-will and wake status without power-manager sessions."""
import json
import pathlib
import subprocess
import tempfile
import unittest
from test_mqtt_discovery import ROOT, function


class PassivePowerStatusTest(unittest.TestCase):
    def test_retained_last_will_and_wake_payloads(self):
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
#include <unistd.h>
#define MQTT_BUFFER 2048
#define MQTT_KEEPALIVE 30
typedef struct {char device_name[64],mqtt_client_id[64],mqtt_topic_prefix[128],mqtt_username[256],mqtt_password[256];} AppConfig;
typedef struct {bool ha_validated;} AppState;
static char g_operation_mode[16]="Handheld";
'''
        names = ["encode_remaining_length", "write_string", "mqtt_send_packet", "socket_recv_all", "mqtt_send_publish_bytes", "mqtt_send_publish", "sanitize_id", "client_id_or_device", "state_topic", "mqtt_send_connect"]
        code += "\n".join(function(main, n) for n in names)
        code += "\n" + function(ext, "ext_escape")
        code += "\n" + function(main, "publish_state")
        code += r'''
static size_t body_start(const unsigned char*b){size_t p=1;while(b[p++]&128){}return p;}
static size_t read_string(const unsigned char*b,size_t p,char*out,size_t cap){size_t n=(b[p]<<8)|b[p+1];assert(n<cap);memcpy(out,b+p+2,n);out[n]=0;return p+2+n;}
int main(void){
 AppConfig config={.device_name="Switch",.mqtt_client_id="switch_a"};AppState state={0};
 int pair[2];unsigned char packet[2048];char protocol[10],client[80],topic[192],payload[1300];
 assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
 assert(mqtt_send_connect(&config,pair[0]));
 int count=mqtt_read_frame(pair[1],packet,sizeof(packet),socket_recv_all);assert(count>0&&packet[0]==0x10);
 size_t p=read_string(packet,body_start(packet),protocol,sizeof(protocol));assert(strcmp(protocol,"MQTT")==0&&packet[p++]==4);
 unsigned char flags=packet[p++];assert((flags&0x26)==0x26); /* Retained last will + clean session. */
 assert(packet[p]==0&&packet[p+1]==30);p+=2;
 p=read_string(packet,p,client,sizeof(client));assert(strcmp(client,"switch_a")==0);
 p=read_string(packet,p,topic,sizeof(topic));p=read_string(packet,p,payload,sizeof(payload));assert(p==(size_t)count);
 printf("will\t%s\t%s\n",topic,payload);
 close(pair[0]);assert(recv(pair[1],packet,1,0)==0);close(pair[1]);
 assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
 publish_state(&state,pair[0],&config,"online");
 for(int i=0;i<2;i++){
  count=mqtt_read_frame(pair[1],packet,sizeof(packet),socket_recv_all);assert(count>0&&packet[0]==0x31);
  p=read_string(packet,body_start(packet),topic,sizeof(topic));assert((size_t)count-p<sizeof(payload));memcpy(payload,packet+p,count-p);payload[count-p]=0;
  printf("wake\t%s\t%s\n",topic,payload);
 }
 close(pair[0]);close(pair[1]);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            c = pathlib.Path(temp) / "status.c"
            binary = pathlib.Path(temp) / "status"
            c.write_text(code)
            subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-I" + str(ROOT / "include"), str(c), "-o", str(binary)], check=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
        rows = [line.split("\t", 2) for line in result.stdout.splitlines()]
        self.assertEqual(rows[0][:2], ["will", "switch_ha/switch_a/status"])
        self.assertEqual(json.loads(rows[0][2]), {"status": "offline", "operation_mode": "Sleep", "mode_source": "mqtt_disconnect"})
        wake = json.loads(rows[1][2])
        self.assertEqual(wake["status"], "online")
        self.assertEqual(wake["operation_mode"], "Handheld")
        self.assertEqual(wake["mode_source"], "live")
        self.assertEqual(rows[2], ["wake", "switch_ha/switch_a/availability", "online"])

    def test_backlight_guard_has_no_power_manager_obligation(self):
        source = (ROOT / "sysmodule/source/main.c").read_text()
        self.assertNotIn("pscmGetPmModule(", source)
        code = r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
typedef unsigned Result;
typedef uint64_t u64;
#define R_SUCCEEDED(r) ((r)==0)
typedef struct {int lock;bool exit_requested,svc_lbl_ready;} AppState;
typedef int LblBacklightSwitchStatus;
enum {LblBacklightSwitchStatus_Disabled=0,LblBacklightSwitchStatus_Disabling=1};
static AppState*state;
static unsigned reads;
static void mutexLock(int*p){(void)p;}
static void mutexUnlock(int*p){(void)p;}
static Result lblGetBacklightSwitchStatus(LblBacklightSwitchStatus*out){reads++;*out=LblBacklightSwitchStatus_Disabled;return 0;}
static void screen_stream_set_paused(bool paused){assert(paused);}
static void svcSleepThread(u64 ns){assert(ns==100000000ULL);state->exit_requested=true;}
'''
        code += function(source, "backlight_guard_thread_main")
        code += "\nint main(void){AppState s={.svc_lbl_ready=true};state=&s;backlight_guard_thread_main(&s);assert(reads==1);}\n"
        with tempfile.TemporaryDirectory() as temp:
            c = pathlib.Path(temp) / "passive.c"
            binary = pathlib.Path(temp) / "passive"
            c.write_text(code)
            subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", str(c), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=5)


if __name__ == "__main__":
    unittest.main()
