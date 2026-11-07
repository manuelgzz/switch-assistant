#pragma once
#include <switch.h>
#include <stdbool.h>

typedef struct {
    AudioTarget target;
    s32 volume, minimum, maximum;
    float percent;
    bool master;
    bool muted;
} AudioVolumeState;

Result audio_control_read(AudioVolumeState *state);
Result audio_control_set_percent(float percent);
Result audio_control_step(int direction);
Result audio_control_set_mute(bool muted);
