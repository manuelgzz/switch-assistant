#pragma once
#include <switch.h>
#include <stdbool.h>

/* Only power-management events establish sleep; backlight state never does. */
static inline bool power_policy_sleeping(PscPmState request, bool previous) {
    switch (request) {
        case PscPmState_ReadySleep:
        case PscPmState_ReadySleepCritical:
            return true;
        case PscPmState_Awake:
        case PscPmState_ReadyAwaken:
        case PscPmState_ReadyAwakenCritical:
            return false;
        default:
            return previous;
    }
}
