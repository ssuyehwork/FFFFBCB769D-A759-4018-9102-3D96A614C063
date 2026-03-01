#ifndef SESSIONLOGGER_H
#define SESSIONLOGGER_H

#include <QDateTime>
#include <QString>
#include <QList>

struct SessionRecord {
    QDateTime startTime;
    QDateTime endTime;
    int       touchCount;    // 被触碰次数
    bool      manualUnlock;  // true=手动解锁，false=倒计时结束
};

class SessionLogger {
public:
    static SessionLogger& instance();

    void logSession(const SessionRecord& record);
    QList<SessionRecord> getHistory() const;
    void clearHistory();

private:
    SessionLogger();
    QString m_logPath;
};

#endif // SESSIONLOGGER_H
