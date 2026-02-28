#include "CountdownEngine.h"
#include "ConfigManager.h"

CountdownEngine::CountdownEngine(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &CountdownEngine::onTick);
}

CountdownEngine& CountdownEngine::instance() {
    static CountdownEngine inst;
    return inst;
}

void CountdownEngine::resumeFromTime(int seconds) {
    m_remainingSeconds = seconds;
    setState(Counting);
    m_timer->start(1000);
    emit tickSecond(m_remainingSeconds);
}

void CountdownEngine::start(int minutes) {
    m_remainingSeconds = minutes * 60;

    // 持久化：计算预计结束时间并保存
    AppConfig config = ConfigManager::instance().getConfig();
    config.targetEndTime = QDateTime::currentDateTime().addSecs(m_remainingSeconds);
    ConfigManager::instance().setConfig(config);
    ConfigManager::instance().save();

    // 自动开启注册表自启动
    ConfigManager::instance().setLaunchOnStartup(true);

    setState(Counting);
    m_timer->start(1000);
    emit tickSecond(m_remainingSeconds);
}

void CountdownEngine::forceLock() {
    m_timer->stop();
    m_remainingSeconds = 0;
    // 关键：先改变状态，确保 main.cpp 处理 lockActivated 信号时 state() 为 Locked
    setState(Locked);
    emit lockActivated();
}

void CountdownEngine::stop() {
    m_timer->stop();
    setState(Idle);
}

void CountdownEngine::pause() {
    m_timer->stop();
}

void CountdownEngine::resume() {
    if (m_state != Idle) {
        m_timer->start(1000);
    }
}

void CountdownEngine::onTick() {
    if (m_remainingSeconds > 0) {
        m_remainingSeconds--;
        
        if (m_state == Counting) {
            int warningThreshold = ConfigManager::instance().getConfig().warningDurationSeconds;
            if (m_remainingSeconds <= warningThreshold) {
                setState(PreLockWarning);
                emit warningPhaseStarted();
                emit warningTick(m_remainingSeconds);
                emit tickSecond(m_remainingSeconds);
            } else {
                emit tickSecond(m_remainingSeconds);
            }
        } else if (m_state == PreLockWarning) {
            emit warningTick(m_remainingSeconds);
            emit tickSecond(m_remainingSeconds);
            if (m_remainingSeconds == 0) {
                m_timer->stop();
                m_remainingSeconds = 0;
                // 关键：先 setState 确保 main.cpp 的 activateLock 能正确执行钩子阻塞
                setState(Locked);
                emit lockActivated();
            }
        } else if (m_state == LockedOut) {
            if (m_remainingSeconds == 0) {
                endLockout();
            }
        }
    } else if (m_remainingSeconds <= 0) {
        m_timer->stop();
    }
}

void CountdownEngine::enterUnlocking() {
    setState(Unlocking);
}

void CountdownEngine::reportUnlockAttempt(bool success) {
    if (success) {
        // 清理持久化状态
        AppConfig config = ConfigManager::instance().getConfig();
        config.targetEndTime = QDateTime();
        ConfigManager::instance().setConfig(config);
        ConfigManager::instance().save();

        setState(Unlocked);
        emit unlockSucceeded();
        setState(Idle);
        m_timer->stop();
    } else {
        emit unlockFailed(-1);
    }
}

void CountdownEngine::triggerLockedOut(int seconds) {
    m_remainingSeconds = seconds;
    setState(LockedOut);
    emit lockedOut(seconds);
    m_timer->start(1000);
}

void CountdownEngine::endLockout() {
    setState(Locked);
    m_timer->stop();
}

void CountdownEngine::setState(State s) {
    if (m_state != s) {
        m_state = s;
        emit stateChanged(m_state);
    }
}
