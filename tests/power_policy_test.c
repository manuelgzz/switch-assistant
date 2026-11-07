#include "power_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(!power_policy_sleeping(PscPmState_Awake,true));
    assert(power_policy_sleeping(PscPmState_ReadySleep,false));
    assert(power_policy_sleeping(PscPmState_ReadySleepCritical,true));
    assert(!power_policy_sleeping(PscPmState_ReadyAwaken,true));
    assert(!power_policy_sleeping(PscPmState_ReadyAwakenCritical,true));
    assert(!power_policy_sleeping(PscPmState_ReadyShutdown,false));
    assert(power_policy_sleeping((PscPmState)99,true));
    assert(!power_policy_sleeping((PscPmState)99,false));
    puts("Power policy: sleep/wake events and unrelated-state preservation passed");
}
