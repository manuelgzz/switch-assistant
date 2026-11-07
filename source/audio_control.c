#include "audio_control.h"
#include <math.h>
#include <string.h>

static bool prefer_master;
static bool software_muted;
static float restore_percent = 50.0f;

Result audio_control_read(AudioVolumeState *state) {
    memset(state, 0, sizeof(*state));
    state->target = AudioTarget_Invalid;
    Result target_rc = audctlGetActiveOutputTarget(&state->target);
    if (R_FAILED(target_rc)) target_rc = audctlGetDefaultTarget(&state->target);
    Result rc = target_rc;
    if (!prefer_master && R_SUCCEEDED(target_rc)) {
        rc = audctlGetTargetVolume(&state->volume, state->target);
        if (R_SUCCEEDED(rc)) rc = audctlGetTargetVolumeMin(&state->minimum);
        if (R_SUCCEEDED(rc)) rc = audctlGetTargetVolumeMax(&state->maximum);
        if (R_SUCCEEDED(rc) && state->maximum > state->minimum) {
            state->percent = 100.0f * (state->volume - state->minimum) / (state->maximum - state->minimum);
            bool hardware_muted = false;
            audctlIsTargetMute(&hardware_muted, state->target);
            state->muted = software_muted || hardware_muted;
            return 0;
        }
    }
    float master;
    rc = audctlGetSystemOutputMasterVolume(&master);
    if (R_FAILED(rc)) return rc;
    state->master = true;
    state->percent = master * 100.0f;
    state->muted = software_muted;
    if (R_SUCCEEDED(target_rc)) {
        bool hardware_muted = false;
        audctlIsTargetMute(&hardware_muted, state->target);
        state->muted = software_muted || hardware_muted;
    }
    return 0;
}

Result audio_control_set_percent(float percent) {
    if (!isfinite(percent) || percent < 0 || percent > 100)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    AudioVolumeState state;
    Result rc = audio_control_read(&state);
    if (R_FAILED(rc)) return rc;
    if (!state.master) {
        /* Round to the nearest hardware step, instead of truncating 5% to zero. */
        s32 volume = state.minimum + (s32)((state.maximum - state.minimum) * percent / 100.0f + 0.5f);
        rc = audctlSetTargetVolume(state.target, volume);
        if (R_SUCCEEDED(rc)) return rc;
    }
    rc = audctlSetSystemOutputMasterVolume(percent / 100.0f);
    if (R_SUCCEEDED(rc)) prefer_master = true;
    return rc;
}

Result audio_control_step(int direction) {
    AudioVolumeState state;
    Result rc = audio_control_read(&state);
    if (R_FAILED(rc)) return rc;
    float percent;
    if (state.master) percent = state.percent + (direction > 0 ? 5.0f : -5.0f);
    else {
        s32 step = (state.maximum - state.minimum + 10) / 20;
        if (step < 1) step = 1;
        s32 volume = state.volume + (direction > 0 ? step : -step);
        percent = 100.0f * (volume - state.minimum) / (state.maximum - state.minimum);
    }
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return audio_control_set_percent(percent);
}

Result audio_control_set_mute(bool muted) {
    AudioVolumeState state;
    Result rc = audio_control_read(&state);
    if (R_FAILED(rc)) return rc;
    if (!muted && software_muted) {
        rc = audio_control_set_percent(restore_percent);
        if (R_FAILED(rc)) return rc;
        software_muted = false;
        if (state.target != AudioTarget_Invalid) audctlSetTargetMute(state.target, false);
        return 0;
    }
    if (state.target != AudioTarget_Invalid) {
        rc = audctlSetTargetMute(state.target, muted);
        if (R_SUCCEEDED(rc)) return rc;
    }
    if (muted) {
        if (!software_muted && state.percent > 0) restore_percent = state.percent;
        rc = audio_control_set_percent(0);
        if (R_SUCCEEDED(rc)) software_muted = true;
        return rc;
    }
    return rc;
}
