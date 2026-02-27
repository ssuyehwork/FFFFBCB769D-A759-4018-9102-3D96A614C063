#ifndef POWERMANAGER_H
#define POWERMANAGER_H

class PowerManager {
public:
    static void preventSleepAndScreenOff();
    static void allowSleepAndScreenOff();
};

#endif // POWERMANAGER_H
