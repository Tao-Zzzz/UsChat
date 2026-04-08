#ifndef DIARYTASK_H
#define DIARYTASK_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>

struct DiaryTask
{
    QString thing;
    QDateTime startTime;
    QDateTime endTime;
    bool finished = false;

    QJsonObject ToJson() const
    {
        QJsonObject obj;
        obj["thing"] = thing;
        obj["start_time"] = startTime.toString(Qt::ISODate);
        obj["end_time"] = endTime.toString(Qt::ISODate);
        obj["finished"] = finished;
        return obj;
    }

    static DiaryTask FromJson(const QJsonObject& obj)
    {
        DiaryTask task;
        task.thing = obj["thing"].toString();
        task.startTime = QDateTime::fromString(obj["start_time"].toString(), Qt::ISODate);
        task.endTime = QDateTime::fromString(obj["end_time"].toString(), Qt::ISODate);
        task.finished = obj["finished"].toBool(false);
        return task;
    }
};

#endif // DIARYTASK_H
