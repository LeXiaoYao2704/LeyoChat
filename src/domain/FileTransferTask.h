#pragma once

#include <string>

#include <QMetaType>
#include <QtGlobal>

enum class FileTransferDirection {
    Outgoing,
    Incoming
};

enum class FileTransferState {
    PendingOffer,
    WaitingAccept,
    ReadyToTransfer,
    Transferring,
    Paused,
    Interrupted,
    Completing,
    Completed,
    Failed,
    Canceled
};

struct FileTransferTask {
    std::wstring taskId;
    std::wstring conversationId;
    std::wstring peerClientId;
    std::wstring groupId;
    std::wstring sourcePath;
    std::wstring targetPath;
    std::wstring tempPath;
    std::wstring fileName;
    std::wstring fileHash;
    std::wstring errorCode;
    std::wstring errorText;
    FileTransferDirection direction = FileTransferDirection::Outgoing;
    FileTransferState state = FileTransferState::PendingOffer;
    qint64 fileSize = 0;
    qint64 chunkSize = 0;
    qint64 bytesCompleted = 0;
    qint64 createdAtMs = 0;
    qint64 updatedAtMs = 0;
    int chunkCount = 0;
    int lastChunkIndex = -1;
};

Q_DECLARE_METATYPE(FileTransferTask)
