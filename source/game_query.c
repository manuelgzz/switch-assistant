#include "game_query.h"

Result game_query_current(u64 *pid, u64 *application_id) {
    *pid = 0;
    *application_id = 0;
    /* With version zero libnx chooses pre-5.0 IPC #5, which on modern firmware
     * is HookToCreateApplicationProcess, not GetApplicationProcessId (#4). */
    if (hosversionGet() == 0)
        return MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer);
    Result rc = pmdmntInitialize();
    if (R_FAILED(rc)) return rc;
    rc = pmdmntGetApplicationProcessId(pid);
    if (R_SUCCEEDED(rc) && *pid) {
        rc = pmdmntGetProgramId(application_id, *pid);
        if (R_FAILED(rc)) {
            rc = pminfoInitialize();
            if (R_SUCCEEDED(rc)) {
                rc = pminfoGetProgramId(application_id, *pid);
                pminfoExit();
            }
        }
    }
    pmdmntExit();
    return rc;
}

Result game_query_close_current(void) {
    u64 pid = 0, application_id = 0;
    Result rc = game_query_current(&pid, &application_id);
    if (R_FAILED(rc)) return rc;
    if (!pid || !application_id)
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    rc = pmshellInitialize();
    if (R_FAILED(rc)) return rc;
    /* No catalog scan, no process-list guessing, and no privileged syscall. */
    rc = pmshellTerminateProcess(pid);
    pmshellExit();
    return rc;
}
