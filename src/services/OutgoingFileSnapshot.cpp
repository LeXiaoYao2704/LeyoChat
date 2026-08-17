#include "services/OutgoingFileSnapshot.h"

#include <algorithm>

#include <QCryptographicHash>
#include <QFile>

std::optional<OutgoingFileSnapshot> prepareOutgoingFileSnapshot(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }

    const qint64 snapshotSize = file.size();
    if (snapshotSize <= 0) {
        return std::nullopt;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 remaining = snapshotSize;
    while (remaining > 0) {
        const qint64 readSize = std::min<qint64>(64 * 1024, remaining);
        const QByteArray chunk = file.read(readSize);
        if (chunk.size() != readSize) {
            return std::nullopt;
        }
        hash.addData(chunk);
        remaining -= readSize;
    }

    return OutgoingFileSnapshot{
        snapshotSize,
        QString::fromLatin1(hash.result().toHex()),
    };
}

OutgoingFileChunkBatch prepareOutgoingFileChunkBatch(const QString& filePath,
                                                     int chunkSize,
                                                     int chunkCount,
                                                     int startChunkIndex,
                                                     const QSet<int>& completedChunks,
                                                     int maxChunks,
                                                     qint64 snapshotSize)
{
    OutgoingFileChunkBatch batch;
    batch.nextChunkIndex = std::max(0, startChunkIndex);
    if (filePath.trimmed().isEmpty() || chunkSize <= 0 || chunkCount <= 0 || maxChunks <= 0) {
        return batch;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return batch;
    }

    if (snapshotSize <= 0 || file.size() < snapshotSize) {
        batch.errorCode = QStringLiteral("source_changed");
        batch.errorText = QStringLiteral("源文件在传输准备后被截断");
        return batch;
    }

    for (int chunkIdx = batch.nextChunkIndex;
         chunkIdx < chunkCount && static_cast<int>(batch.chunks.size()) < maxChunks;
         ++chunkIdx) {
        batch.nextChunkIndex = chunkIdx + 1;
        if (completedChunks.contains(chunkIdx)) {
            continue;
        }

        const qint64 offset = static_cast<qint64>(chunkIdx) * chunkSize;
        const qint64 remaining = snapshotSize - offset;
        if (remaining <= 0) {
            batch.errorCode = QStringLiteral("source_changed");
            batch.errorText = QStringLiteral("源文件大小与已准备任务不一致");
            return batch;
        }
        if (!file.seek(offset)) {
            batch.errorCode = QStringLiteral("source_changed");
            batch.errorText = QStringLiteral("无法读取已准备的源文件位置");
            return batch;
        }

        const qint64 readSize = std::min<qint64>(chunkSize, remaining);
        const QByteArray chunkData = file.read(readSize);
        if (chunkData.size() != readSize) {
            batch.errorCode = QStringLiteral("source_changed");
            batch.errorText = QStringLiteral("源文件在传输过程中发生变化");
            return batch;
        }

        batch.chunks.push_back(OutgoingFileChunk{chunkIdx, chunkData.toBase64()});
    }

    batch.ok = true;
    batch.reachedEnd = batch.nextChunkIndex >= chunkCount;
    return batch;
}
