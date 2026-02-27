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
        PreLockWarning,   // 最后20秒，PreLockNotification显示中
        Locking,          // 遮罩激活过渡帧
        Locked,           // 遮罩完全激活
        Unlocking,        // 解锁验证中
        LockedOut,        // 密码错误超限，临时封锁解锁入口
        Unlocked          // 解锁成功，回到Idle前的瞬态
    };

    explicit CountdownEngine(QObject *parent = nullptr);
    static CountdownEngine& instance();

    void start(int minutes);
    void forceLock();
    void stop();
    void pause();
    void resume();
    
    // 状态转换接口
    void enterUnlocking();
    void reportUnlockAttempt(bool success);
    void triggerLockedOut(int seconds);
    void endLockout();

    State state() const { return m_state; }
    int remainingSeconds() const { return m_remainingSeconds; }

signals:
    void stateChanged(State newState);
    void tickSecond(int remainingSeconds);     // 每秒发射，Counting阶段
    void warningPhaseStarted();                // 剩余20秒时发射，触发PreLockNotification
    void warningTick(int remainingSeconds);    // PreLockWarning阶段每秒发射（20→1）
    void lockActivated();                      // 触发遮罩
    void unlockSucceeded();
    void unlockFailed(int attemptsLeft);
    void lockedOut(int lockoutSeconds);
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
