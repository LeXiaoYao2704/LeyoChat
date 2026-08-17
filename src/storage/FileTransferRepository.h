#pragma once

#include <vector>

#include <QString>

#include "domain/FileTransferTask.h"

class FileTransferRepository {
public:
    explicit FileTransferRepository(QString connectionName);

    bool upsertTask(const FileTransferTask& task) const;
    bool findTaskById(const QString& taskId, FileTransferTask* outTask) const;
    std::vector<FileTransferTask> loadAllTasks() const;
    std::vector<FileTransferTask> loadTasksForConversation(const QString& conversationId) const;
    std::vector<FileTransferTask> loadResumableTasks() const;
    std::vector<FileTransferTask> loadRecentTasks(int limit = 20) const;
    bool remapConversationId(const QString& oldConversationId, const QString& newConversationId) const;
    bool deleteTask(const QString& taskId) const;
    int deleteTasksByStates(const std::vector<FileTransferState>& states) const;
    bool recordCompletedChunk(const QString& taskId, int chunkIndex, qint64 size, qint64 updatedAtMs) const;
    bool replaceCompletedChunks(const QString& taskId,
                                const std::vector<int>& chunkIndexes,
                                qint64 chunkSize,
                                qint64 fileSize,
                                qint64 updatedAtMs) const;
    std::vector<int> loadCompletedChunkIndexes(const QString& taskId) const;
    bool updateTaskState(const QString& taskId,
                         FileTransferState state,
                         qint64 bytesCompleted,
                         int lastChunkIndex,
                         const QString& errorCode,
                         const QString& errorText,
                         qint64 updatedAtMs) const;

private:
    QString m_connectionName;
};
