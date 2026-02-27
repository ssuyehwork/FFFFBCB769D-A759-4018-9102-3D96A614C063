#include "timer_engine.h"
#include <mmsystem.h>
#include <atomic>

#pragma comment(lib, "winmm.lib")

namespace TimerEngine {

MMRESULT g_timerId = 0;
std::atomic<int> g_remainingSeconds(0);
TimerCallback g_callback = nullptr;
bool g_isPaused = false;

void CALLBACK InternalTimerProc(UINT uID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    if (g_isPaused) return;

    int current = --g_remainingSeconds;
    if (g_callback) {
        g_callback(current);
    }

    if (current <= 0) {
        Stop();
    }
}

void Start(int minutes, TimerCallback callback) {
    Stop();
    g_remainingSeconds = minutes * 60;
    g_callback = callback;
    g_isPaused = false;
    g_timerId = timeSetEvent(1000, 10, InternalTimerProc, 0, TIME_PERIODIC);
}

void Stop() {
    if (g_timerId) {
        timeKillEvent(g_timerId);
        g_timerId = 0;
    }
}

void Pause() { g_isPaused = true; }
void Resume() { g_isPaused = false; }
int GetRemainingSeconds() { return g_remainingSeconds; }
bool IsRunning() { return g_timerId != 0; }

}
