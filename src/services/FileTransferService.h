#pragma once

#include <optional>
#include <vector>

#include <QByteArray>
#include <QSet>
#include <QString>

#include "domain/FileTransferProtocol.h"
#include "domain/MessageEnvelope.h"
#include "domain/FileTransferTask.h"

class FileTransferRepository;

class FileTransferService {
public:
    struct PreparedOutgoingFile {
        qint64 fileSize = 0;
        QString contentHash;
    };

    struct PreparedOutgoingChunk {
        int chunkIndex = -1;
        QByteArray encodedBody;
    };

    struct PreparedOutgoingChunkBatch {
        bool ok = false;
        bool reachedEnd = false;
        int nextChunkIndex = 0;
        QString errorCode;
        QString errorText;
        std::vector<PreparedOutgoingChunk> chunks;
    };

    explicit FileTransferService(FileTransferRepository* repository = nullptr);
    static bool shouldSendReadyEnvelope(bool dataChannelListening);
    static bool shouldPublishProgressUpdate(qint64 previouslyReportedBytes,
                                           qint64 currentBytesCompleted,
                                           int chunkIndex,
                                           bool transferCompleted);

    bool saveTask(const FileTransferTask& task) const;
    bool loadTask(const QString& taskId, FileTransferTask* outTask) const;
    std::vector<FileTransferTask> loadAllTasks() const;
    std::vector<FileTransferTask> loadTasksForConversation(const QString& conversationId) const;
    std::vector<FileTransferTask> loadResumableTasks() const;
    std::vector<FileTransferTask> loadRecentTasks(int limit = 20) const;
    bool deleteTask(const QString& taskId) const;
    int deleteTasksByStates(const std::vector<FileTransferState>& states) const;
    bool markTaskState(const QString& taskId,
                       FileTransferState state,
                       qint64 bytesCompleted,
                       int lastChunkIndex,
                       const QString& errorCode,
                       const QString& errorText,
                       qint64 updatedAtMs) const;
    bool createOutgoingTask(const QString& conversationId,
                            const QString& peerClientId,
                            const QString& groupId,
                            const QString& filePath,
                            qint64 nowMs,
                            FileTransferTask* outTask,
                            const QString& precomputedHash = {},
                            qint64 precomputedSize = -1) const;
    static QString computeFileHash(const QString& filePath);
    static std::optional<PreparedOutgoingFile> prepareOutgoingFile(const QString& filePath);
    static PreparedOutgoingChunkBatch prepareOutgoingChunkBatch(const QString& filePath,
                                                               int chunkSize,
                                                               int chunkCount,
                                                               int startChunkIndex,
                                                               const QSet<int>& completedChunks,
                                                               int maxChunks,
                                                               qint64 snapshotSize = -1);
    bool acceptIncomingOffer(const FileControlPayload& payload,
                             const QString& downloadRoot,
                             qint64 nowMs,
                             FileTransferTask* outTask) const;
    bool buildOfferEnvelope(const FileTransferTask& task,
                            const QString& senderId,
                            const QString& targetId,
                            const QString& dataHost,
                            quint16 dataPort,
                            MessageEnvelope* outEnvelope) const;
    bool buildReadyEnvelope(FileControlType type,
                            const QString& taskId,
                            const QString& senderId,
                            const QString& targetId,
                            const QString& dataHost,
                            quint16 dataPort,
                            MessageEnvelope* outEnvelope) const;
    bool buildResumeRequestEnvelope(const QString& taskId,
                                    const QString& senderId,
                                    const QString& targetId,
                                    MessageEnvelope* outEnvelope) const;
    bool applyRemoteReady(const FileControlPayload& payload, qint64 updatedAtMs) const;
    bool applyRemoteProgress(const FileControlPayload& payload, qint64 updatedAtMs) const;

    static bool payloadFromEnvelope(const MessageEnvelope& envelope, FileControlPayload* outPayload);
    static bool envelopeFromPayload(const FileControlPayload& payload, MessageEnvelope* outEnvelope);

private:
    FileTransferRepository* m_repository;
};
