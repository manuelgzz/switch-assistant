"""Game telemetry must not require the running game to be in titles.txt."""
import pathlib
import subprocess
import tempfile
import unittest
from test_mqtt_discovery import ROOT, function


class GameDetectionTest(unittest.TestCase):
    def test_unknown_titles_and_legacy_fallback(self):
        source = (ROOT / "source/mqtt_client.c").read_text()
        code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
typedef uint64_t u64;
typedef unsigned Result;
#define R_SUCCEEDED(r) ((r)==0)
typedef struct {bool svc_pminfo_ready;} AppState;
static bool direct_failure,legacy_called;
static Result game_query_current(u64*pid,u64*id){if(direct_failure)return 19;*pid=42;*id=0x010000000000A000ULL;return 0;}
static bool title_cache_lookup(u64 id,char*out,size_t size){(void)id;(void)out;(void)size;return false;}
static void set_game_status(AppState*s,const char*fmt,...){(void)s;(void)fmt;}
static bool scan_process_list_for_known_game(AppState*s,u64*pid,u64*id,char*name,size_t size){(void)s;legacy_called=true;*pid=43;*id=0x010000000000B000ULL;snprintf(name,size,"Legacy game");return true;}
'''
        code += function(source, "current_game")
        code += r'''
int main(void){
 AppState s={.svc_pminfo_ready=false};u64 id=0;char name[100];
 assert(current_game(&s,&id,name,sizeof(name)) && id==0x010000000000A000ULL);
 assert(strstr(name,"010000000000A000") && !legacy_called);
 direct_failure=true;s.svc_pminfo_ready=true;
 assert(current_game(&s,&id,name,sizeof(name)) && legacy_called && strcmp(name,"Legacy game")==0);
 s.svc_pminfo_ready=false;legacy_called=false;
 assert(!current_game(&s,&id,name,sizeof(name)) && !legacy_called);
 puts("Game telemetry: unknown catalog title, direct lookup without pminfo and legacy fallback passed");
}
'''
        with tempfile.TemporaryDirectory() as temp:
            c = pathlib.Path(temp) / "game.c"
            binary = pathlib.Path(temp) / "game"
            c.write_text(code)
            subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", str(c), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
