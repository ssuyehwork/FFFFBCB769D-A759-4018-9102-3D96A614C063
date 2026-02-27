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

void CountdownEngine::start(int minutes) {
    m_remainingSeconds = minutes * 60;
    setState(Counting);
    m_timer->start(1000);
    emit tickSecond(m_remainingSeconds);
}

void CountdownEngine::forceLock() {
    m_remainingSeconds = ConfigManager::instance().getConfig().lockDurationMinutes * 60;
    setState(Locked);
    m_timer->start(1000); // 锁定状态也需要计时以实现“自然结束”
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
            if (m_remainingSeconds <= 20) {
                setState(PreLockWarning);
                emit warningPhaseStarted();
                emit warningTick(m_remainingSeconds);
                emit tickSecond(m_remainingSeconds); // 同时发射以更新托盘
            } else {
                emit tickSecond(m_remainingSeconds);
            }
        } else if (m_state == PreLockWarning) {
            emit warningTick(m_remainingSeconds);
            emit tickSecond(m_remainingSeconds); // 预警期也需要更新托盘
            if (m_remainingSeconds == 0) {
                setState(Locking);
                m_remainingSeconds = ConfigManager::instance().getConfig().lockDurationMinutes * 60;
                emit lockActivated();
                setState(Locked);
            }
        } else if (m_state == Locked) {
            emit tickSecond(m_remainingSeconds);
            if (m_remainingSeconds == 0) {
                setState(Idle);
                emit finished();
                m_timer->stop();
            }
        } else if (m_state == LockedOut) {
            if (m_remainingSeconds == 0) {
                endLockout();
            }
        }
    } else if (m_remainingSeconds <= 0) {
        if (m_state == Locked) {
            setState(Idle);
            emit finished();
        }
        m_timer->stop();
    }
}

void CountdownEngine::enterUnlocking() {
    setState(Unlocking);
}

void CountdownEngine::reportUnlockAttempt(bool success) {
    if (success) {
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
}

void CountdownEngine::endLockout() {
    setState(Locked);
}

void CountdownEngine::setState(State s) {
    if (m_state != s) {
        m_state = s;
        emit stateChanged(m_state);
    }
}
