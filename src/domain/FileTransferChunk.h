#pragma once

#include <string>

#include <QMetaType>
#include <QtGlobal>

struct FileTransferChunkHeader {
    std::string taskId;
    qint64 payloadSize = 0;
    int chunkIndex = 0;
};

Q_DECLARE_METATYPE(FileTransferChunkHeader)
