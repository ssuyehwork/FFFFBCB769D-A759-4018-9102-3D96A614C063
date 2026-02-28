#include "SessionLogger.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>

SessionLogger::SessionLogger() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(path);
    m_logPath = path + "/sessions.jsonl";
}

SessionLogger& SessionLogger::instance() {
    static SessionLogger inst;
    return inst;
}

void SessionLogger::logSession(const SessionRecord& record) {
    QFile file(m_logPath);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QJsonObject obj;
        obj["startTime"] = record.startTime.toString(Qt::ISODate);
        obj["endTime"] = record.endTime.toString(Qt::ISODate);
        obj["touchCount"] = record.touchCount;
        obj["manualUnlock"] = record.manualUnlock;
        
        QTextStream out(&file);
        out << QJsonDocument(obj).toJson(QJsonDocument::Compact) << "\n";
    }
}

QList<SessionRecord> SessionLogger::getHistory() const {
    QList<SessionRecord> history;
    QFile file(m_logPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isNull()) {
                QJsonObject obj = doc.object();
                SessionRecord rec;
                rec.startTime = QDateTime::fromString(obj["startTime"].toString(), Qt::ISODate);
                rec.endTime = QDateTime::fromString(obj["endTime"].toString(), Qt::ISODate);
                rec.touchCount = obj["touchCount"].toInt();
                rec.manualUnlock = obj["manualUnlock"].toBool();
                history.append(rec);
            }
        }
    }
    return history;
}

void SessionLogger::clearHistory() {
    QFile file(m_logPath);
    if (file.exists()) {
        file.remove();
    }
}
