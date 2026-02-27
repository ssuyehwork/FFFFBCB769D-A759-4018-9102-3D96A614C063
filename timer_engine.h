#pragma once
#include "common.h"

namespace TimerEngine {
    typedef void (*TimerCallback)(int remainingSeconds);

    void Start(int minutes, TimerCallback callback);
    void Stop();
    void Pause();
    void Resume();
    int GetRemainingSeconds();
    bool IsRunning();
}
