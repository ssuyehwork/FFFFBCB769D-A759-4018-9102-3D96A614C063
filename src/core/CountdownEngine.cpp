#include "CountdownEngine.h"

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
        emit tickSecond(m_remainingSeconds);

        if (m_remainingSeconds == 20 && m_state == Counting) {
            setState(LastWarning);
            emit warningPhaseStarted();
        } else if (m_remainingSeconds == 0) {
            if (m_state == Counting || m_state == LastWarning) {
                // 进入锁定阶段
                m_remainingSeconds = ConfigManager::instance().getConfig().lockDurationMinutes * 60;
                setState(Locked);
                emit lockActivated();
            } else if (m_state == Locked) {
                // 锁定时间自然结束
                setState(Idle);
                emit finished();
                m_timer->stop();
            }
        }
    } else if (m_remainingSeconds <= 0 && m_state != Idle) {
         if (m_state == Locked) {
            setState(Idle);
            emit finished();
         }
         m_timer->stop();
    }
}

void CountdownEngine::setState(State s) {
    if (m_state != s) {
        m_state = s;
        emit stateChanged(m_state);
    }
}
