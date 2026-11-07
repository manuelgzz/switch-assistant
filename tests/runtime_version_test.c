#include "runtime_version.h"
#include <assert.h>
#include <stdio.h>

static u32 version;
static int init_count,exit_count;
static Result init_result,query_result;
static bool invalid_version;
u32 hosversionGet(void){return version & ~(1U<<31);}
void hosversionSet(u32 value){version=value;}
bool hosversionIsAtmosphere(void){return (version & (1U<<31))!=0;}
Result setsysInitialize(void){init_count++;return init_result;}
void setsysExit(void){exit_count++;}
Result setsysGetFirmwareVersion(SetSysFirmwareVersion*out){out->major=invalid_version?0:20;out->minor=1;out->micro=2;return query_result;}
int main(void){
 assert(runtime_version_init()==0 && hosversionGet()==MAKEHOSVERSION(20,1,2));
 assert(init_count==1 && exit_count==1);
 assert(runtime_version_init()==0 && init_count==1); /* Retain loader-provided version. */
 version=1U<<31;
 assert(runtime_version_init()==0 && hosversionIsAtmosphere());
 assert(hosversionGet()==MAKEHOSVERSION(20,1,2));
 version=0;init_result=55;
 assert(runtime_version_init()==55 && hosversionGet()==0 && exit_count==2);
 init_result=0;query_result=66;
 assert(runtime_version_init()==66 && hosversionGet()==0 && exit_count==3);
 query_result=0;invalid_version=true;
 assert(runtime_version_init()!=0 && hosversionGet()==0 && exit_count==4);
 puts("Runtime version: firmware initialization, existing version, Atmosphere bit and failure guards passed");
}
