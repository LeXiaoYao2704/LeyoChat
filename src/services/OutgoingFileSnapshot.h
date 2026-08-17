#pragma once

#include <optional>
#include <vector>

#include <QByteArray>
#include <QSet>
#include <QString>

struct OutgoingFileSnapshot {
    qint64 fileSize = 0;
    QString contentHash;
};

struct OutgoingFileChunk {
    int chunkIndex = -1;
    QByteArray encodedBody;
};

struct OutgoingFileChunkBatch {
    bool ok = false;
    bool reachedEnd = false;
    int nextChunkIndex = 0;
    QString errorCode;
    QString errorText;
    std::vector<OutgoingFileChunk> chunks;
};

std::optional<OutgoingFileSnapshot> prepareOutgoingFileSnapshot(const QString& filePath);

OutgoingFileChunkBatch prepareOutgoingFileChunkBatch(const QString& filePath,
                                                     int chunkSize,
                                                     int chunkCount,
                                                     int startChunkIndex,
                                                     const QSet<int>& completedChunks,
                                                     int maxChunks,
                                                     qint64 snapshotSize);
