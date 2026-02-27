#pragma once
#include "common.h"
#include "config.h"

namespace UIOverlay {
    void StartFocus(int minutes, const AppConfig& config);
    void StopFocus();
    void UpdateCountdown(int seconds);
}
