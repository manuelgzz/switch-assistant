#pragma once
#include <switch.h>

/* Custom __appInit overrides must reproduce libnx's firmware initialization.
 * Call after smInitialize(), before invoking firmware-dependent IPC wrappers. */
static inline Result runtime_version_init(void) {
    if (hosversionGet() != 0) return 0;
    Result rc = setsysInitialize();
    if (R_FAILED(rc)) return rc;
    SetSysFirmwareVersion version = {0};
    rc = setsysGetFirmwareVersion(&version);
    setsysExit();
    if (R_FAILED(rc)) return rc;
    if (version.major == 0) return MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer);
    u32 flags = hosversionIsAtmosphere() ? (1U << 31) : 0;
    hosversionSet(MAKEHOSVERSION(version.major, version.minor, version.micro) | flags);
    return 0;
}
