"""Exercise the actual power listener with deterministic Horizon/clock doubles."""
import pathlib
import subprocess
import tempfile
import unittest
from test_mqtt_discovery import ROOT, function


class PowerListenerTest(unittest.TestCase):
    def test_sleep_acknowledgement_is_bounded_and_errors_unregister(self):
        source = (ROOT / "sysmodule/source/main.c").read_text()
        code = r'''
#include "power_policy.h"
#include <assert.h>
#include <stdlib.h>
typedef int PscPmModuleId;
enum {PscPmModuleId_WlanSockets=25,PscPmModuleId_Nifm=38,PscPmModuleId_Fs=27};
typedef struct {int event;} PscPmModule;
typedef struct {int lock;bool exit_requested,svc_psc_ready,mqtt_sleep_publish_finished,mqtt_connected,power_sleeping,sensor_publish_requested,svc_lbl_ready;} AppState;
typedef int LblBacklightSwitchStatus;
enum {LblBacklightSwitchStatus_Disabled,LblBacklightSwitchStatus_Disabling};
static AppState *current;
static char mode;
static u64 clock_ms,ack_time;
static int waits,acks,finalized,closed;
static void mutexLock(int*x){(void)x;}
static void mutexUnlock(int*x){(void)x;}
static u64 monotonic_ms(void){return clock_ms;}
static void svcSleepThread(u64 ns){clock_ms+=ns/1000000;if(mode=='s'&&clock_ms>=20)current->mqtt_sleep_publish_finished=true;}
static Result smInitialize(void){return 0;}
static void smExit(void){}
static Result pscmInitialize(void){return 0;}
static void pscmExit(void){}
static Result pscmGetPmModule(PscPmModule*m,PscPmModuleId id,const u32*d,size_t n,bool autoclear){(void)m;assert(id==0xE3&&n==3&&d[0]==25&&d[1]==38&&d[2]==27&&autoclear);return 0;}
static Result eventWait(int*e,u64 timeout){(void)e;(void)timeout;if(waits++==0)return 0;current->exit_requested=true;return 1;}
static Result pscPmModuleGetRequest(PscPmModule*m,PscPmState*s,u32*f){(void)m;if(mode=='e'){current->exit_requested=true;return 66;}*s=PscPmState_ReadySleep;*f=0;return 0;}
static Result pscPmModuleAcknowledge(PscPmModule*m,PscPmState s){(void)m;assert(s==PscPmState_ReadySleep);acks++;ack_time=clock_ms;return 0;}
static Result pscPmModuleFinalize(PscPmModule*m){(void)m;finalized++;return 0;}
static void pscPmModuleClose(PscPmModule*m){(void)m;closed++;}
static void append_log_result(const char*s,Result r){(void)s;(void)r;}
static void screen_stream_set_paused(bool paused){(void)paused;}
static Result lblGetBacklightSwitchStatus(LblBacklightSwitchStatus*s){(void)s;return 1;}
'''
        code += function(source, "power_guard_thread_main")
        code += r'''
int main(int argc,char**argv){
 assert(argc==2);mode=argv[1][0];AppState state={0};current=&state;
 state.mqtt_connected=mode!='n';
 power_guard_thread_main(&state);
 assert(finalized==1&&closed==1);
 if(mode=='e'){assert(acks==0&&!state.svc_psc_ready&&!state.power_sleeping);return 0;}
 assert(acks==1&&ack_time<=500&&state.power_sleeping&&state.sensor_publish_requested);
 if(mode=='s')assert(ack_time==20);
 if(mode=='t')assert(ack_time==500);
 if(mode=='n')assert(ack_time==0);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            c = pathlib.Path(temp) / "listener.c"
            binary = pathlib.Path(temp) / "listener"
            c.write_text(code)
            subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-I" + str(ROOT / "tests/stubs"), "-I" + str(ROOT / "include"), str(c), "-o", str(binary)], check=True)
            for scenario in ["success", "timeout", "no_connection", "error"]:
                subprocess.run([str(binary), scenario], check=True, timeout=5)


if __name__ == "__main__":
    unittest.main()
