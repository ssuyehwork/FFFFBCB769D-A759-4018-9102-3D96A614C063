#pragma once
#include "common.h"

namespace OverlayEngine {
    void InstallHooks();
    void UninstallHooks();
    void StartTopmostThread(HWND hwnd);
    void StopTopmostThread();
    bool IsLocked();
}
