#pragma once

#include <QHash>
#include <QString>
#include <QtGlobal>

class TransferSpeedTracker {
public:
    qint64 updateSpeed(const QString& taskId, qint64 bytesCompleted);

private:
    struct Sample {
        qint64 lastMs = 0;
        qint64 lastBytes = 0;
        qint64 speed = 0;
    };

    QHash<QString, Sample> samples_;
};
