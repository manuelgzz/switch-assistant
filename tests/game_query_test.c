#include "game_query.h"
#include <assert.h>
#include <stdio.h>
static int opened,closed,info_opened,info_closed;
static u32 runtime_version=MAKEHOSVERSION(20,0,0);
u32 hosversionGet(void){return runtime_version;}
static bool primary_failure,no_game,init_failure;
static int shell_opened,shell_closed,terminated;
static Result shell_result,terminate_result;
Result pmshellInitialize(void){if(shell_result)return shell_result;shell_opened++;return 0;}
void pmshellExit(void){shell_closed++;}
Result pmshellTerminateProcess(u64 pid){assert(pid==42);terminated++;return terminate_result;}
Result pmdmntInitialize(void){if(init_failure)return 99;opened++;return 0;}
void pmdmntExit(void){closed++;}
Result pmdmntGetApplicationProcessId(u64*out){*out=no_game?0:42;return 0;}
Result pmdmntGetProgramId(u64*out,u64 pid){assert(pid==42);if(primary_failure)return 44;*out=0x010000000000A000ULL;return 0;}
Result pminfoInitialize(void){info_opened++;return 0;}
void pminfoExit(void){info_closed++;}
Result pminfoGetProgramId(u64*out,u64 pid){assert(pid==42);*out=0x010000000000B000ULL;return 0;}
int main(void){
 u64 pid,id;
 runtime_version=0;
 assert(game_query_current(&pid,&id)!=0 && pid==0 && id==0 && opened==0);
 runtime_version=MAKEHOSVERSION(20,0,0);
 assert(game_query_current(&pid,&id)==0 && pid==42 && id==0x010000000000A000ULL);
 assert(opened==closed && info_opened==0);
 primary_failure=true;
 assert(game_query_current(&pid,&id)==0 && id==0x010000000000B000ULL);
 assert(info_opened==1 && info_closed==1 && opened==closed);
 no_game=true;
 assert(game_query_current(&pid,&id)==0 && pid==0 && id==0);
 init_failure=true;
 assert(game_query_current(&pid,&id)==99 && pid==0 && id==0 && opened==closed);
 init_failure=false;no_game=false;primary_failure=false;
 assert(game_query_close_current()==0 && terminated==1 && shell_opened==shell_closed);
 terminate_result=88;
 assert(game_query_close_current()==88 && terminated==2 && shell_opened==shell_closed);
 no_game=true;
 assert(game_query_close_current()!=0 && terminated==2);
 no_game=false;shell_result=77;
 assert(game_query_close_current()==77 && terminated==2 && shell_opened==shell_closed);
 runtime_version=0;
 assert(game_query_close_current()!=0 && terminated==2);
 puts("Game query: direct process lookup, program-ID fallback, no game and initialization errors and application-PID termination passed");
}
