#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef uint32_t Result;
typedef uint64_t u64;
typedef uint32_t u32;
typedef struct {uint8_t major, minor, micro;} SetSysFirmwareVersion;
typedef int32_t s32;
typedef enum {AudioTarget_Invalid=-1,AudioTarget_Speaker=0} AudioTarget;
#define R_SUCCEEDED(r) ((r)==0)
#define R_FAILED(r) ((r)!=0)
#define Module_Libnx 345
#define LibnxError_BadInput 1
#define MAKERESULT(m,e) ((Result)(((m)<<9)|(e)))
Result audctlGetActiveOutputTarget(AudioTarget*);
Result audctlGetDefaultTarget(AudioTarget*);
Result audctlGetTargetVolume(s32*,AudioTarget);
Result audctlGetTargetVolumeMin(s32*);
Result audctlGetTargetVolumeMax(s32*);
Result audctlGetSystemOutputMasterVolume(float*);
Result audctlSetTargetVolume(AudioTarget,s32);
Result audctlSetSystemOutputMasterVolume(float);
Result audctlIsTargetMute(bool*,AudioTarget);
Result audctlSetTargetMute(AudioTarget,bool);
Result pmdmntInitialize(void);
void pmdmntExit(void);
Result pmdmntGetApplicationProcessId(u64*);
Result pmdmntGetProgramId(u64*,u64);
Result pminfoInitialize(void);
void pminfoExit(void);
Result pminfoGetProgramId(u64*,u64);

#define LibnxError_IncompatSysVer 2
#define MAKEHOSVERSION(a,b,c) (((a)<<16)|((b)<<8)|(c))
u32 hosversionGet(void);
void hosversionSet(u32 version);
bool hosversionIsAtmosphere(void);
Result setsysInitialize(void);
void setsysExit(void);
Result setsysGetFirmwareVersion(SetSysFirmwareVersion*);

#define LibnxError_NotInitialized 3
Result pmshellInitialize(void);
void pmshellExit(void);
Result pmshellTerminateProcess(u64 pid);
