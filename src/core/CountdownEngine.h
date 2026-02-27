#ifndef COUNTDOWNENGINE_H
#define COUNTDOWNENGINE_H

#include <QObject>
#include <QTimer>

class CountdownEngine : public QObject {
    Q_OBJECT
public:
    enum State {
        Idle,
        Counting,
        LastWarning, // 最后20秒
        Locked        // 触发遮罩
    };

    explicit CountdownEngine(QObject *parent = nullptr);
    static CountdownEngine& instance();

    void start(int minutes);
    void stop();
    void pause();
    void resume();
    
    State state() const { return m_state; }
    int remainingSeconds() const { return m_remainingSeconds; }

signals:
    void stateChanged(State newState);
    void tickSecond(int remainingSeconds);
    void warningPhaseStarted();
    void lockActivated();
    void finished();

private slots:
    void onTick();

private:
    void setState(State s);
    
    State m_state = Idle;
    int m_remainingSeconds = 0;
    QTimer *m_timer;
};

#endif // COUNTDOWNENGINE_H
