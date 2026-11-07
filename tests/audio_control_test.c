#include "audio_control.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static s32 native_volume=7;
static float master_volume=0.4f;
static bool muted,active_failure=true,range_failure,set_failure,mute_failure,master_failure;
static int default_reads,master_writes;
Result audctlGetActiveOutputTarget(AudioTarget*out){if(active_failure)return 10;*out=AudioTarget_Speaker;return 0;}
Result audctlGetDefaultTarget(AudioTarget*out){default_reads++;*out=AudioTarget_Speaker;return 0;}
Result audctlGetTargetVolume(s32*out,AudioTarget t){assert(t==AudioTarget_Speaker);*out=native_volume;return 0;}
Result audctlGetTargetVolumeMin(s32*out){if(range_failure)return 11;*out=0;return 0;}
Result audctlGetTargetVolumeMax(s32*out){*out=15;return 0;}
Result audctlGetSystemOutputMasterVolume(float*out){if(master_failure)return 19;*out=master_volume;return 0;}
Result audctlSetTargetVolume(AudioTarget t,s32 v){assert(t==AudioTarget_Speaker);if(set_failure)return 12;assert(v>=0&&v<=15);native_volume=v;return 0;}
Result audctlSetSystemOutputMasterVolume(float v){assert(v>=0&&v<=1);master_writes++;master_volume=v;return 0;}
Result audctlIsTargetMute(bool*out,AudioTarget t){(void)t;if(mute_failure)return 13;*out=muted;return 0;}
Result audctlSetTargetMute(AudioTarget t,bool value){(void)t;if(mute_failure)return 13;muted=value;return 0;}
int main(void){
    AudioVolumeState state;
    assert(audio_control_read(&state)==0 && default_reads>0 && !state.master);
    assert(state.percent>46 && state.percent<47);
    assert(audio_control_step(1)==0 && native_volume==8);
    assert(audio_control_step(-1)==0 && native_volume==7);
    assert(audio_control_set_percent(100)==0 && native_volume==15);
    assert(audio_control_step(1)==0 && native_volume==15);
    assert(audio_control_set_percent(0)==0 && native_volume==0);
    assert(audio_control_step(-1)==0 && native_volume==0);
    assert(audio_control_set_percent(50)==0 && native_volume==8);
    assert(audio_control_set_percent(NAN)!=0 && audio_control_set_percent(INFINITY)!=0);
    assert(audio_control_set_percent(-1)!=0 && audio_control_set_percent(101)!=0 && native_volume==8);
    assert(audio_control_set_mute(true)==0 && muted && native_volume==8);
    assert(audio_control_read(&state)==0 && state.muted);
    assert(audio_control_set_mute(false)==0 && !muted && native_volume==8);
    mute_failure=true;
    assert(audio_control_set_mute(true)==0 && native_volume==0);
    assert(audio_control_set_mute(true)==0 && native_volume==0);
    assert(audio_control_read(&state)==0 && state.muted);
    assert(audio_control_set_mute(false)==0 && native_volume==8);
    range_failure=true;
    assert(audio_control_read(&state)==0 && state.master && state.percent==40);
    assert(audio_control_step(1)==0 && master_volume>0.449f && master_volume<0.451f);
    range_failure=false;set_failure=true;
    assert(audio_control_set_percent(65)==0 && master_writes>=2);
    assert(audio_control_read(&state)==0 && state.master && state.percent==65);
    master_failure=true;
    assert(audio_control_read(&state)==19);
    puts("Audio: target fallback, master fallback, native steps, mute/restore, bounds and errors passed");
}
