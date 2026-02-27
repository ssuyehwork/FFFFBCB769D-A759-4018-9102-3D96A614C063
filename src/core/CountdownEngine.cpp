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
    m_remainingSeconds = 0;
    setState(Locked);
    m_timer->stop();
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
            setState(Locked);
            emit lockActivated();
            m_timer->stop(); // 锁屏后由 UI 逻辑或外部信号恢复或结束
        }
    }
}

void CountdownEngine::setState(State s) {
    if (m_state != s) {
        m_state = s;
        emit stateChanged(m_state);
    }
}
