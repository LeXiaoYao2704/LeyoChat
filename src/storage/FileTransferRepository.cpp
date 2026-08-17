#include "storage/FileTransferRepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>

namespace {
QString toStorageValue(FileTransferDirection direction) {
    return direction == FileTransferDirection::Incoming ? QStringLiteral("incoming")
                                                        : QStringLiteral("outgoing");
}

FileTransferDirection directionFromStorage(const QString& value) {
    return value == QStringLiteral("incoming") ? FileTransferDirection::Incoming
                                               : FileTransferDirection::Outgoing;
}

QString toStorageValue(FileTransferState state) {
    switch (state) {
    case FileTransferState::PendingOffer:
        return QStringLiteral("pending_offer");
    case FileTransferState::WaitingAccept:
        return QStringLiteral("waiting_accept");
    case FileTransferState::ReadyToTransfer:
        return QStringLiteral("ready_to_transfer");
    case FileTransferState::Transferring:
        return QStringLiteral("transferring");
    case FileTransferState::Paused:
        return QStringLiteral("paused");
    case FileTransferState::Interrupted:
        return QStringLiteral("interrupted");
    case FileTransferState::Completing:
        return QStringLiteral("completing");
    case FileTransferState::Completed:
        return QStringLiteral("completed");
    case FileTransferState::Failed:
        return QStringLiteral("failed");
    case FileTransferState::Canceled:
        return QStringLiteral("canceled");
    }

    return QStringLiteral("pending_offer");
}

FileTransferState stateFromStorage(const QString& value) {
    if (value == QStringLiteral("waiting_accept")) {
        return FileTransferState::WaitingAccept;
    }
    if (value == QStringLiteral("ready_to_transfer")) {
        return FileTransferState::ReadyToTransfer;
    }
    if (value == QStringLiteral("transferring")) {
        return FileTransferState::Transferring;
    }
    if (value == QStringLiteral("paused")) {
        return FileTransferState::Paused;
    }
    if (value == QStringLiteral("interrupted")) {
        return FileTransferState::Interrupted;
    }
    if (value == QStringLiteral("completing")) {
        return FileTransferState::Completing;
    }
    if (value == QStringLiteral("completed")) {
        return FileTransferState::Completed;
    }
    if (value == QStringLiteral("failed")) {
        return FileTransferState::Failed;
    }
    if (value == QStringLiteral("canceled")) {
        return FileTransferState::Canceled;
    }
    return FileTransferState::PendingOffer;
}

FileTransferTask taskFromQuery(const QSqlQuery& query) {
    return FileTransferTask{
        query.value(0).toString().toStdWString(),
        query.value(1).toString().toStdWString(),
        query.value(2).toString().toStdWString(),
        query.value(3).toString().toStdWString(),
        query.value(8).toString().toStdWString(),
        query.value(9).toString().toStdWString(),
        query.value(10).toString().toStdWString(),
        query.value(6).toString().toStdWString(),
        query.value(7).toString().toStdWString(),
        query.value(11).toString().toStdWString(),
        query.value(12).toString().toStdWString(),
        directionFromStorage(query.value(4).toString()),
        stateFromStorage(query.value(5).toString()),
        query.value(13).toLongLong(),
        query.value(14).toLongLong(),
        query.value(16).toLongLong(),
        query.value(18).toLongLong(),
        query.value(19).toLongLong(),
        query.value(15).toInt(),
        query.value(17).toInt()
    };
}

bool isResumableState(FileTransferState state) {
    return state == FileTransferState::WaitingAccept || state == FileTransferState::ReadyToTransfer
           || state == FileTransferState::Transferring || state == FileTransferState::Paused
           || state == FileTransferState::Interrupted
           || state == FileTransferState::Completing;
}

qint64 bytesForChunk(qint64 fileSize, qint64 chunkSize, int chunkIndex) {
    if (fileSize <= 0 || chunkSize <= 0 || chunkIndex < 0) {
        return 0;
    }

    const qint64 chunkOffset = static_cast<qint64>(chunkIndex) * chunkSize;
    if (chunkOffset >= fileSize) {
        return 0;
    }

    return std::min(chunkSize, fileSize - chunkOffset);
}
}

FileTransferRepository::FileTransferRepository(QString connectionName)
    : m_connectionName(std::move(connectionName)) {}

bool FileTransferRepository::upsertTask(const FileTransferTask& task) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT OR REPLACE INTO file_tasks (
            task_id, conversation_id, peer_client_id, group_id, direction, state,
            file_name, file_hash, source_path, target_path, temp_path, error_code, error_text,
            file_size, chunk_size, chunk_count, bytes_completed, last_chunk_index,
            created_at_ms, updated_at_ms
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )"));
    query.addBindValue(QString::fromStdWString(task.taskId));
    query.addBindValue(QString::fromStdWString(task.conversationId));
    query.addBindValue(QString::fromStdWString(task.peerClientId));
    query.addBindValue(QString::fromStdWString(task.groupId));
    query.addBindValue(toStorageValue(task.direction));
    query.addBindValue(toStorageValue(task.state));
    query.addBindValue(QString::fromStdWString(task.fileName));
    query.addBindValue(QString::fromStdWString(task.fileHash));
    query.addBindValue(QString::fromStdWString(task.sourcePath));
    query.addBindValue(QString::fromStdWString(task.targetPath));
    query.addBindValue(QString::fromStdWString(task.tempPath));
    query.addBindValue(QString::fromStdWString(task.errorCode));
    query.addBindValue(QString::fromStdWString(task.errorText));
    query.addBindValue(task.fileSize);
    query.addBindValue(task.chunkSize);
    query.addBindValue(task.chunkCount);
    query.addBindValue(task.bytesCompleted);
    query.addBindValue(task.lastChunkIndex);
    query.addBindValue(task.createdAtMs);
    query.addBindValue(task.updatedAtMs);
    return query.exec();
}

bool FileTransferRepository::findTaskById(const QString& taskId, FileTransferTask* outTask) const {
    if (!outTask) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT task_id, conversation_id, peer_client_id, group_id, direction, state,
               file_name, file_hash, source_path, target_path, temp_path, error_code, error_text,
               file_size, chunk_size, chunk_count, bytes_completed, last_chunk_index,
               created_at_ms, updated_at_ms
        FROM file_tasks
        WHERE task_id = ?
        LIMIT 1
    )"));
    query.addBindValue(taskId);
    if (!query.exec() || !query.next()) {
        return false;
    }

    *outTask = taskFromQuery(query);
    query.finish();
    return true;
}

std::vector<FileTransferTask> FileTransferRepository::loadAllTasks() const {
    std::vector<FileTransferTask> tasks;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(QStringLiteral(R"(
        SELECT task_id, conversation_id, peer_client_id, group_id, direction, state,
               file_name, file_hash, source_path, target_path, temp_path, error_code, error_text,
               file_size, chunk_size, chunk_count, bytes_completed, last_chunk_index,
               created_at_ms, updated_at_ms
        FROM file_tasks
        ORDER BY updated_at_ms DESC, rowid DESC
    )"))) {
        return tasks;
    }

    while (query.next()) {
        tasks.push_back(taskFromQuery(query));
    }

    return tasks;
}

std::vector<FileTransferTask> FileTransferRepository::loadTasksForConversation(const QString& conversationId) const {
    std::vector<FileTransferTask> tasks;
    const QString trimmedConversationId = conversationId.trimmed();
    if (trimmedConversationId.isEmpty()) {
        return tasks;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT task_id, conversation_id, peer_client_id, group_id, direction, state,
               file_name, file_hash, source_path, target_path, temp_path, error_code, error_text,
               file_size, chunk_size, chunk_count, bytes_completed, last_chunk_index,
               created_at_ms, updated_at_ms
        FROM file_tasks
        WHERE conversation_id = ?
        ORDER BY updated_at_ms DESC, created_at_ms DESC, rowid DESC
    )"));
    query.addBindValue(trimmedConversationId);
    if (!query.exec()) {
        return tasks;
    }

    while (query.next()) {
        tasks.push_back(taskFromQuery(query));
    }

    return tasks;
}

std::vector<FileTransferTask> FileTransferRepository::loadResumableTasks() const {
    std::vector<FileTransferTask> tasks;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(QStringLiteral(R"(
        SELECT task_id, conversation_id, peer_client_id, group_id, direction, state,
               file_name, file_hash, source_path, target_path, temp_path, error_code, error_text,
               file_size, chunk_size, chunk_count, bytes_completed, last_chunk_index,
               created_at_ms, updated_at_ms
        FROM file_tasks
        ORDER BY updated_at_ms ASC, rowid ASC
    )"))) {
        return tasks;
    }

    while (query.next()) {
        const FileTransferTask task = taskFromQuery(query);
        if (isResumableState(task.state)) {
            tasks.push_back(task);
        }
    }

    return tasks;
}

std::vector<FileTransferTask> FileTransferRepository::loadRecentTasks(int limit) const {
    std::vector<FileTransferTask> tasks;
    if (limit <= 0) {
        return tasks;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT task_id, conversation_id, peer_client_id, group_id, direction, state,
               file_name, file_hash, source_path, target_path, temp_path, error_code, error_text,
               file_size, chunk_size, chunk_count, bytes_completed, last_chunk_index,
               created_at_ms, updated_at_ms
        FROM file_tasks
        ORDER BY updated_at_ms DESC, created_at_ms DESC, rowid DESC
        LIMIT ?
    )"));
    query.addBindValue(limit);
    if (!query.exec()) {
        return tasks;
    }

    while (query.next()) {
        tasks.push_back(taskFromQuery(query));
    }

    return tasks;
}

bool FileTransferRepository::remapConversationId(const QString& oldConversationId,
                                                 const QString& newConversationId) const {
    const QString oldId = oldConversationId.trimmed();
    const QString newId = newConversationId.trimmed();
    if (oldId.isEmpty() || newId.isEmpty() || oldId == newId) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        UPDATE file_tasks
        SET conversation_id = ?
        WHERE conversation_id = ?
    )"));
    query.addBindValue(newId);
    query.addBindValue(oldId);
    return query.exec();
}

bool FileTransferRepository::deleteTask(const QString& taskId) const {
    if (taskId.trimmed().isEmpty()) {
        return false;
    }

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isValid() || !database.transaction()) {
        return false;
    }

    QSqlQuery deleteChunksQuery(database);
    deleteChunksQuery.prepare(QStringLiteral("DELETE FROM file_chunks WHERE task_id = ?"));
    deleteChunksQuery.addBindValue(taskId);
    if (!deleteChunksQuery.exec()) {
        database.rollback();
        return false;
    }

    QSqlQuery deleteTaskQuery(database);
    deleteTaskQuery.prepare(QStringLiteral("DELETE FROM file_tasks WHERE task_id = ?"));
    deleteTaskQuery.addBindValue(taskId);
    if (!deleteTaskQuery.exec()) {
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        database.rollback();
        return false;
    }

    return deleteTaskQuery.numRowsAffected() > 0;
}

int FileTransferRepository::deleteTasksByStates(const std::vector<FileTransferState>& states) const {
    if (states.empty()) {
        return 0;
    }

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isValid() || !database.transaction()) {
        return 0;
    }

    QStringList stateValues;
    stateValues.reserve(static_cast<qsizetype>(states.size()));
    for (const FileTransferState state : states) {
        stateValues.append(toStorageValue(state));
    }

    QStringList placeholders;
    placeholders.reserve(stateValues.size());
    for (int index = 0; index < stateValues.size(); ++index) {
        placeholders.append(QStringLiteral("?"));
    }

    QSqlQuery selectQuery(database);
    selectQuery.prepare(QStringLiteral("SELECT task_id FROM file_tasks WHERE state IN (%1)")
                            .arg(placeholders.join(QStringLiteral(", "))));
    for (const QString& stateValue : stateValues) {
        selectQuery.addBindValue(stateValue);
    }
    if (!selectQuery.exec()) {
        database.rollback();
        return 0;
    }

    QStringList taskIds;
    while (selectQuery.next()) {
        taskIds.append(selectQuery.value(0).toString());
    }

    if (taskIds.isEmpty()) {
        database.commit();
        return 0;
    }

    QSqlQuery deleteChunksQuery(database);
    deleteChunksQuery.prepare(QStringLiteral("DELETE FROM file_chunks WHERE task_id = ?"));

    QSqlQuery deleteTaskQuery(database);
    deleteTaskQuery.prepare(QStringLiteral("DELETE FROM file_tasks WHERE task_id = ?"));

    int deletedCount = 0;
    for (const QString& taskId : taskIds) {
        deleteChunksQuery.bindValue(0, taskId);
        if (!deleteChunksQuery.exec()) {
            database.rollback();
            return 0;
        }
        deleteTaskQuery.bindValue(0, taskId);
        if (!deleteTaskQuery.exec()) {
            database.rollback();
            return 0;
        }
        deletedCount += deleteTaskQuery.numRowsAffected();
    }

    if (!database.commit()) {
        database.rollback();
        return 0;
    }

    return deletedCount;
}

bool FileTransferRepository::recordCompletedChunk(const QString& taskId,
                                                  int chunkIndex,
                                                  qint64 size,
                                                  qint64 updatedAtMs) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT OR REPLACE INTO file_chunks (task_id, chunk_index, size, updated_at_ms)
        VALUES (?, ?, ?, ?)
    )"));
    query.addBindValue(taskId);
    query.addBindValue(chunkIndex);
    query.addBindValue(size);
    query.addBindValue(updatedAtMs);
    return query.exec();
}

bool FileTransferRepository::replaceCompletedChunks(const QString& taskId,
                                                    const std::vector<int>& chunkIndexes,
                                                    qint64 chunkSize,
                                                    qint64 fileSize,
                                                    qint64 updatedAtMs) const {
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isValid()) {
        return false;
    }

    if (!database.transaction()) {
        return false;
    }

    QSqlQuery deleteQuery(database);
    deleteQuery.prepare(QStringLiteral("DELETE FROM file_chunks WHERE task_id = ?"));
    deleteQuery.addBindValue(taskId);
    if (!deleteQuery.exec()) {
        database.rollback();
        return false;
    }

    for (const int chunkIndex : chunkIndexes) {
        QSqlQuery insertQuery(database);
        insertQuery.prepare(QStringLiteral(R"(
            INSERT OR REPLACE INTO file_chunks (task_id, chunk_index, size, updated_at_ms)
            VALUES (?, ?, ?, ?)
        )"));
        insertQuery.addBindValue(taskId);
        insertQuery.addBindValue(chunkIndex);
        insertQuery.addBindValue(bytesForChunk(fileSize, chunkSize, chunkIndex));
        insertQuery.addBindValue(updatedAtMs);
        if (!insertQuery.exec()) {
            database.rollback();
            return false;
        }
    }

    return database.commit();
}

std::vector<int> FileTransferRepository::loadCompletedChunkIndexes(const QString& taskId) const {
    std::vector<int> chunkIndexes;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT chunk_index
        FROM file_chunks
        WHERE task_id = ?
        ORDER BY chunk_index ASC
    )"));
    query.addBindValue(taskId);
    if (!query.exec()) {
        return chunkIndexes;
    }

    while (query.next()) {
        chunkIndexes.push_back(query.value(0).toInt());
    }

    return chunkIndexes;
}

bool FileTransferRepository::updateTaskState(const QString& taskId,
                                             FileTransferState state,
                                             qint64 bytesCompleted,
                                             int lastChunkIndex,
                                             const QString& errorCode,
                                             const QString& errorText,
                                             qint64 updatedAtMs) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        UPDATE file_tasks
        SET state = ?,
            bytes_completed = ?,
            last_chunk_index = ?,
            error_code = ?,
            error_text = ?,
            updated_at_ms = ?
        WHERE task_id = ?
    )"));
    query.addBindValue(toStorageValue(state));
    query.addBindValue(bytesCompleted);
    query.addBindValue(lastChunkIndex);
    query.addBindValue(errorCode.isNull() ? QStringLiteral("") : errorCode);
    query.addBindValue(errorText.isNull() ? QStringLiteral("") : errorText);
    query.addBindValue(updatedAtMs);
    query.addBindValue(taskId);
    if (!query.exec()) {
        return false;
    }
    if (query.numRowsAffected() > 0) {
        return true;
    }

    // SQLite reports 0 rows for idempotent updates; treat that as success
    // when the row still exists.
    QSqlQuery existsQuery(QSqlDatabase::database(m_connectionName, false));
    existsQuery.prepare(QStringLiteral("SELECT 1 FROM file_tasks WHERE task_id = ? LIMIT 1"));
    existsQuery.addBindValue(taskId);
    return existsQuery.exec() && existsQuery.next();
}
