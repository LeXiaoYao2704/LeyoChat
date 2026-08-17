#pragma once

#include <vector>

#include <QString>
#include <QtGlobal>

#include "domain/FileTransferTask.h"
#include "domain/ChatMessage.h"
#include "domain/MessageEnvelope.h"

QString filePreviewText(const QString& fileName);
QString sanitizeNotificationText(const QString& text, const QString& fallback);
QString humanReadableBytes(qint64 bytes);
QString fileTransferProgressText(qint64 bytesCompleted, qint64 fileSize);
QString fileTransferStatusText(const QString& fileName,
                               FileTransferState state,
                               FileTransferDirection direction,
                               qint64 bytesCompleted = 0,
                               qint64 fileSize = 0);
qint64 chunkBytesForTask(const FileTransferTask& task, int chunkIndex);
qint64 completedBytesForTask(const FileTransferTask& task, const std::vector<int>& completedChunks);
qint64 displayBytesForTransferState(const FileTransferTask& task, FileTransferState state);
QString transferPeerLabel(const FileTransferTask& task, const QString& peerDisplayName);
QString transferStatusChipText(const FileTransferTask& task);
QString transferDetailText(const FileTransferTask& task, const QString& peerDisplayName);
bool transferTaskRetryable(const FileTransferTask& task);
QString localFilePathForTransferTask(const FileTransferTask& task);
MessageDeliveryState effectiveFileTransferDeliveryState(
    FileTransferDirection direction,
    bool conversationIsOpen,
    MessageDeliveryState requestedState);
QString groupEnvelopePreviewText(const MessageEnvelope& envelope);
