#include "PowerManager.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

void PowerManager::preventSleepAndScreenOff() {
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#endif
}

void PowerManager::allowSleepAndScreenOff() {
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}
