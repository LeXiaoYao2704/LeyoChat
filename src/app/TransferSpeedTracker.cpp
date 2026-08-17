#include "app/TransferSpeedTracker.h"

#include <QDateTime>

qint64 TransferSpeedTracker::updateSpeed(const QString& taskId, qint64 bytesCompleted)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    auto& sample = samples_[taskId];
    if (sample.lastMs > 0 && nowMs > sample.lastMs) {
        const qint64 dtMs = nowMs - sample.lastMs;
        if (dtMs >= 200) {
            const qint64 dBytes = bytesCompleted - sample.lastBytes;
            const qint64 instantSpeed = dBytes > 0 ? (dBytes * 1000 / dtMs) : 0;
            sample.speed = sample.speed > 0
                ? static_cast<qint64>(0.3 * instantSpeed + 0.7 * sample.speed)
                : instantSpeed;
            sample.lastMs = nowMs;
            sample.lastBytes = bytesCompleted;
        }
    } else {
        sample.lastMs = nowMs;
        sample.lastBytes = bytesCompleted;
    }
    return sample.speed;
}
