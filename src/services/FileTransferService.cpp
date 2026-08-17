#include "services/FileTransferService.h"
#include "services/OutgoingFileSnapshot.h"

#include <algorithm>
#include <optional>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include "storage/FileTransferRepository.h"

namespace {
// File chunks still ride on the existing control channel, so keep each JSON
// payload small enough that chat/control messages are not blocked behind files.
constexpr qint64 kDefaultChunkSize = 64 * 1024;
constexpr qint64 kLargeFileThreshold = 100 * 1024 * 1024; // 100 MB

QString controlTypeToString(FileControlType type) {
    switch (type) {
    case FileControlType::Offer:
        return QStringLiteral("offer");
    case FileControlType::Accept:
        return QStringLiteral("accept");
    case FileControlType::Reject:
        return QStringLiteral("reject");
    case FileControlType::ResumeRequest:
        return QStringLiteral("resume_request");
    case FileControlType::ResumeResponse:
        return QStringLiteral("resume_response");
    case FileControlType::Progress:
        return QStringLiteral("progress");
    case FileControlType::Complete:
        return QStringLiteral("complete");
    case FileControlType::Fail:
        return QStringLiteral("fail");
    case FileControlType::Cancel:
        return QStringLiteral("cancel");
    }

    return QStringLiteral("offer");
}

std::optional<FileControlType> controlTypeFromString(const QString& value) {
    if (value == QStringLiteral("offer")) {
        return FileControlType::Offer;
    }
    if (value == QStringLiteral("accept")) {
        return FileControlType::Accept;
    }
    if (value == QStringLiteral("reject")) {
        return FileControlType::Reject;
    }
    if (value == QStringLiteral("resume_request")) {
        return FileControlType::ResumeRequest;
    }
    if (value == QStringLiteral("resume_response")) {
        return FileControlType::ResumeResponse;
    }
    if (value == QStringLiteral("progress")) {
        return FileControlType::Progress;
    }
    if (value == QStringLiteral("complete")) {
        return FileControlType::Complete;
    }
    if (value == QStringLiteral("fail")) {
        return FileControlType::Fail;
    }
    if (value == QStringLiteral("cancel")) {
        return FileControlType::Cancel;
    }

    return std::nullopt;
}

std::string toUtf8(const QString& value) {
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

QString toQString(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

qint64 chunkBytesForIndex(const FileTransferTask& task, int chunkIndex) {
    if (task.fileSize <= 0 || task.chunkSize <= 0 || chunkIndex < 0) {
        return 0;
    }

    const qint64 chunkOffset = static_cast<qint64>(chunkIndex) * task.chunkSize;
    if (chunkOffset >= task.fileSize) {
        return 0;
    }

    return std::min(task.chunkSize, task.fileSize - chunkOffset);
}

qint64 completedBytesForChunks(const FileTransferTask& task, const std::vector<int>& chunkIndexes) {
    qint64 bytesCompleted = 0;
    for (const int chunkIndex : chunkIndexes) {
        bytesCompleted += chunkBytesForIndex(task, chunkIndex);
    }
    return bytesCompleted;
}

int lastCompletedChunk(const std::vector<int>& chunkIndexes) {
    if (chunkIndexes.empty()) {
        return -1;
    }

    return *std::max_element(chunkIndexes.begin(), chunkIndexes.end());
}

QString uniqueFilePath(const QString& directoryPath, const QString& requestedName) {
    const QFileInfo requestedInfo(requestedName);
    const QString fileName =
        requestedInfo.fileName().trimmed().isEmpty() ? QStringLiteral("received.bin")
                                                     : requestedInfo.fileName();
    QDir directory(directoryPath);
    QString candidate = directory.filePath(fileName);
    if (!QFileInfo::exists(candidate)) {
        return candidate;
    }

    const QString baseName = QFileInfo(fileName).completeBaseName();
    const QString suffix = QFileInfo(fileName).suffix();
    int index = 1;
    while (true) {
        const QString numberedName =
            suffix.isEmpty() ? QStringLiteral("%1_%2").arg(baseName, QString::number(index))
                             : QStringLiteral("%1_%2.%3").arg(baseName, QString::number(index), suffix);
        candidate = directory.filePath(numberedName);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
        ++index;
    }
}

QString tempFilePathForTask(const QString& downloadRoot, const QString& taskId) {
    QDir root(downloadRoot);
    root.mkpath(QStringLiteral(".transfer"));
    return root.filePath(QStringLiteral(".transfer/%1.part").arg(taskId));
}

QString hashFileContents(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const qint64 fileSize = file.size();

    // For large files (>100MB), hash only first 1MB + last 1MB + file size
    // to avoid blocking for tens of seconds on multi-GB files.
    if (fileSize > kLargeFileThreshold) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        constexpr qint64 kSampleSize = 1024 * 1024; // 1 MB

        // Hash first 1MB
        const QByteArray head = file.read(kSampleSize);
        if (head.isEmpty()) return {};
        hash.addData(head);

        // Hash last 1MB
        if (file.seek(fileSize - kSampleSize)) {
            const QByteArray tail = file.read(kSampleSize);
            if (!tail.isEmpty()) hash.addData(tail);
        }

        // Mix in file size for uniqueness
        const QByteArray sizeBytes = QByteArray::number(fileSize);
        hash.addData(sizeBytes);

        return QString::fromLatin1(hash.result().toHex());
    }

    // Small files: full SHA256
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(64 * 1024);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            return {};
        }
        if (!chunk.isEmpty()) {
            hash.addData(chunk);
        }
    }

    return QString::fromLatin1(hash.result().toHex());
}
}

QString FileTransferService::computeFileHash(const QString& filePath)
{
    return hashFileContents(filePath);
}

std::optional<FileTransferService::PreparedOutgoingFile>
FileTransferService::prepareOutgoingFile(const QString& filePath)
{
    const auto snapshot = prepareOutgoingFileSnapshot(filePath);
    if (!snapshot.has_value()) {
        return std::nullopt;
    }
    return PreparedOutgoingFile{snapshot->fileSize, snapshot->contentHash};
}

FileTransferService::PreparedOutgoingChunkBatch FileTransferService::prepareOutgoingChunkBatch(
    const QString& filePath,
    int chunkSize,
    int chunkCount,
    int startChunkIndex,
    const QSet<int>& completedChunks,
    int maxChunks,
    qint64 snapshotSize)
{
    const qint64 effectiveSnapshotSize = snapshotSize < 0
        ? QFileInfo(filePath).size()
        : snapshotSize;
    const auto prepared = prepareOutgoingFileChunkBatch(filePath,
                                                        chunkSize,
                                                        chunkCount,
                                                        startChunkIndex,
                                                        completedChunks,
                                                        maxChunks,
                                                        effectiveSnapshotSize);
    PreparedOutgoingChunkBatch batch;
    batch.ok = prepared.ok;
    batch.reachedEnd = prepared.reachedEnd;
    batch.nextChunkIndex = prepared.nextChunkIndex;
    batch.errorCode = prepared.errorCode;
    batch.errorText = prepared.errorText;
    batch.chunks.reserve(prepared.chunks.size());
    for (const auto& chunk : prepared.chunks) {
        batch.chunks.push_back(PreparedOutgoingChunk{chunk.chunkIndex, chunk.encodedBody});
    }
    return batch;
}

FileTransferService::FileTransferService(FileTransferRepository* repository)
    : m_repository(repository) {}

bool FileTransferService::shouldSendReadyEnvelope(bool dataChannelListening) {
    Q_UNUSED(dataChannelListening);
    return true;
}

bool FileTransferService::shouldPublishProgressUpdate(qint64 previouslyReportedBytes,
                                                      qint64 currentBytesCompleted,
                                                      int chunkIndex,
                                                      bool transferCompleted) {
    if (transferCompleted || chunkIndex <= 0) {
        return true;
    }

    return (currentBytesCompleted - previouslyReportedBytes) >= (1024 * 1024);
}

bool FileTransferService::saveTask(const FileTransferTask& task) const {
    return m_repository && m_repository->upsertTask(task);
}

bool FileTransferService::loadTask(const QString& taskId, FileTransferTask* outTask) const {
    return m_repository && m_repository->findTaskById(taskId, outTask);
}

std::vector<FileTransferTask> FileTransferService::loadAllTasks() const {
    return m_repository ? m_repository->loadAllTasks() : std::vector<FileTransferTask>{};
}

std::vector<FileTransferTask> FileTransferService::loadTasksForConversation(const QString& conversationId) const {
    return m_repository ? m_repository->loadTasksForConversation(conversationId)
                        : std::vector<FileTransferTask>{};
}

std::vector<FileTransferTask> FileTransferService::loadResumableTasks() const {
    return m_repository ? m_repository->loadResumableTasks() : std::vector<FileTransferTask>{};
}

std::vector<FileTransferTask> FileTransferService::loadRecentTasks(int limit) const {
    return m_repository ? m_repository->loadRecentTasks(limit) : std::vector<FileTransferTask>{};
}

bool FileTransferService::deleteTask(const QString& taskId) const {
    return m_repository && m_repository->deleteTask(taskId);
}

int FileTransferService::deleteTasksByStates(const std::vector<FileTransferState>& states) const {
    return m_repository ? m_repository->deleteTasksByStates(states) : 0;
}

bool FileTransferService::markTaskState(const QString& taskId,
                                        FileTransferState state,
                                        qint64 bytesCompleted,
                                        int lastChunkIndex,
                                        const QString& errorCode,
                                        const QString& errorText,
                                        qint64 updatedAtMs) const {
    return m_repository && m_repository->updateTaskState(taskId,
                                                         state,
                                                         bytesCompleted,
                                                         lastChunkIndex,
                                                         errorCode,
                                                         errorText,
                                                         updatedAtMs);
}

bool FileTransferService::createOutgoingTask(const QString& conversationId,
                                             const QString& peerClientId,
                                             const QString& groupId,
                                             const QString& filePath,
                                             qint64 nowMs,
                                             FileTransferTask* outTask,
                                             const QString& precomputedHash,
                                             qint64 precomputedSize) const {
    if (!m_repository || !outTask) {
        return false;
    }

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile() || fileInfo.size() <= 0) {
        return false;
    }
    if (precomputedSize == 0 || precomputedSize > fileInfo.size()) {
        return false;
    }

    const QString fileHash = precomputedHash.isEmpty()
        ? FileTransferService::computeFileHash(filePath)
        : precomputedHash;
    if (fileHash.isEmpty()) {
        return false;
    }

    FileTransferTask task;
    task.taskId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdWString();
    task.conversationId = conversationId.toStdWString();
    task.peerClientId = peerClientId.toStdWString();
    task.groupId = groupId.toStdWString();
    task.sourcePath = filePath.toStdWString();
    task.fileName = fileInfo.fileName().toStdWString();
    task.fileHash = fileHash.toStdWString();
    task.direction = FileTransferDirection::Outgoing;
    task.state = FileTransferState::WaitingAccept;
    task.fileSize = precomputedSize > 0 ? precomputedSize : fileInfo.size();
    task.chunkSize = kDefaultChunkSize;
    task.chunkCount = static_cast<int>((task.fileSize + task.chunkSize - 1) / task.chunkSize);
    task.createdAtMs = nowMs;
    task.updatedAtMs = nowMs;

    if (!m_repository->upsertTask(task)) {
        return false;
    }

    *outTask = task;
    return true;
}

bool FileTransferService::acceptIncomingOffer(const FileControlPayload& payload,
                                              const QString& downloadRoot,
                                              qint64 nowMs,
                                              FileTransferTask* outTask) const {
    if (!m_repository || !outTask || payload.taskId.empty() || payload.fileName.empty()
        || payload.fileSize <= 0 || payload.chunkSize <= 0 || payload.chunkCount <= 0
        || downloadRoot.trimmed().isEmpty()) {
        return false;
    }

    FileTransferTask existingTask;
    if (m_repository->findTaskById(toQString(payload.taskId), &existingTask)) {
        existingTask.conversationId = toQString(payload.conversationId).toStdWString();
        existingTask.peerClientId = toQString(payload.senderId).toStdWString();
        existingTask.groupId = toQString(payload.groupId).toStdWString();
        existingTask.fileName = toQString(payload.fileName).toStdWString();
        existingTask.fileHash = toQString(payload.fileHash).toStdWString();
        existingTask.direction = FileTransferDirection::Incoming;
        existingTask.fileSize = payload.fileSize;
        existingTask.chunkSize = payload.chunkSize;
        existingTask.chunkCount = payload.chunkCount;
        existingTask.errorCode.clear();
        existingTask.errorText.clear();
        existingTask.updatedAtMs = nowMs;
        if (existingTask.targetPath.empty()) {
            existingTask.targetPath =
                uniqueFilePath(downloadRoot, toQString(payload.fileName)).toStdWString();
        }
        if (existingTask.tempPath.empty()) {
            existingTask.tempPath =
                tempFilePathForTask(downloadRoot, toQString(payload.taskId)).toStdWString();
        }
        const auto completedChunks = m_repository->loadCompletedChunkIndexes(toQString(payload.taskId));
        const bool hasTransferProgress =
            existingTask.state == FileTransferState::Transferring
            || existingTask.state == FileTransferState::Completing
            || existingTask.state == FileTransferState::Completed
            || existingTask.bytesCompleted > 0
            || !completedChunks.empty();
        if (hasTransferProgress) {
            if (!m_repository->upsertTask(existingTask)) {
                return false;
            }
            *outTask = existingTask;
            return true;
        }

        existingTask.state = FileTransferState::ReadyToTransfer;
        existingTask.bytesCompleted = 0;
        existingTask.lastChunkIndex = -1;
        const QString existingTempPath = QString::fromStdWString(existingTask.tempPath);
        if (!existingTempPath.isEmpty() && QFileInfo::exists(existingTempPath)) {
            QFile::remove(existingTempPath);
        }
        if (!m_repository->replaceCompletedChunks(toQString(payload.taskId),
                                                  {},
                                                  existingTask.chunkSize,
                                                  existingTask.fileSize,
                                                  nowMs)) {
            return false;
        }
        if (!m_repository->upsertTask(existingTask)) {
            return false;
        }
        *outTask = existingTask;
        return true;
    }

    QDir().mkpath(downloadRoot);
    const QString targetPath = uniqueFilePath(downloadRoot, toQString(payload.fileName));
    const QString tempPath = tempFilePathForTask(downloadRoot, toQString(payload.taskId));

    FileTransferTask task;
    task.taskId = toQString(payload.taskId).toStdWString();
    task.conversationId = toQString(payload.conversationId).toStdWString();
    task.peerClientId = toQString(payload.senderId).toStdWString();
    task.groupId = toQString(payload.groupId).toStdWString();
    task.targetPath = targetPath.toStdWString();
    task.tempPath = tempPath.toStdWString();
    task.fileName = toQString(payload.fileName).toStdWString();
    task.fileHash = toQString(payload.fileHash).toStdWString();
    task.direction = FileTransferDirection::Incoming;
    task.state = FileTransferState::ReadyToTransfer;
    task.fileSize = payload.fileSize;
    task.chunkSize = payload.chunkSize;
    task.chunkCount = payload.chunkCount;
    task.createdAtMs = nowMs;
    task.updatedAtMs = nowMs;

    if (!m_repository->upsertTask(task)) {
        return false;
    }

    *outTask = task;
    return true;
}

bool FileTransferService::buildOfferEnvelope(const FileTransferTask& task,
                                             const QString& senderId,
                                             const QString& targetId,
                                             const QString& dataHost,
                                             quint16 dataPort,
                                             MessageEnvelope* outEnvelope) const {
    if (!outEnvelope) {
        return false;
    }

    FileControlPayload payload;
    payload.type = FileControlType::Offer;
    payload.taskId = toUtf8(QString::fromStdWString(task.taskId));
    payload.conversationId = toUtf8(QString::fromStdWString(task.conversationId));
    payload.groupId = toUtf8(QString::fromStdWString(task.groupId));
    payload.senderId = toUtf8(senderId);
    payload.targetId = toUtf8(targetId);
    payload.fileName = toUtf8(QString::fromStdWString(task.fileName));
    payload.fileHash = toUtf8(QString::fromStdWString(task.fileHash));
    payload.dataHost = toUtf8(dataHost);
    payload.fileSize = task.fileSize;
    payload.chunkSize = task.chunkSize;
    payload.chunkCount = task.chunkCount;
    payload.dataPort = dataPort;
    return envelopeFromPayload(payload, outEnvelope);
}

bool FileTransferService::buildReadyEnvelope(FileControlType type,
                                             const QString& taskId,
                                             const QString& senderId,
                                             const QString& targetId,
                                             const QString& dataHost,
                                             quint16 dataPort,
                                             MessageEnvelope* outEnvelope) const {
    if (!m_repository || !outEnvelope
        || (type != FileControlType::Accept && type != FileControlType::ResumeResponse)) {
        return false;
    }

    FileTransferTask task;
    if (!m_repository->findTaskById(taskId, &task)) {
        return false;
    }

    FileControlPayload payload;
    payload.type = type;
    payload.taskId = toUtf8(taskId);
    payload.conversationId = toUtf8(QString::fromStdWString(task.conversationId));
    payload.groupId = toUtf8(QString::fromStdWString(task.groupId));
    payload.senderId = toUtf8(senderId);
    payload.targetId = toUtf8(targetId);
    payload.fileName = toUtf8(QString::fromStdWString(task.fileName));
    payload.fileHash = toUtf8(QString::fromStdWString(task.fileHash));
    payload.dataHost = toUtf8(dataHost);
    payload.fileSize = task.fileSize;
    payload.chunkSize = task.chunkSize;
    payload.chunkCount = task.chunkCount;
    payload.dataPort = dataPort;
    payload.completedChunks = m_repository->loadCompletedChunkIndexes(taskId);
    return envelopeFromPayload(payload, outEnvelope);
}

bool FileTransferService::buildResumeRequestEnvelope(const QString& taskId,
                                                     const QString& senderId,
                                                     const QString& targetId,
                                                     MessageEnvelope* outEnvelope) const {
    if (!m_repository || !outEnvelope) {
        return false;
    }

    FileTransferTask task;
    if (!m_repository->findTaskById(taskId, &task)) {
        return false;
    }

    FileControlPayload payload;
    payload.type = FileControlType::ResumeRequest;
    payload.taskId = toUtf8(taskId);
    payload.conversationId = toUtf8(QString::fromStdWString(task.conversationId));
    payload.groupId = toUtf8(QString::fromStdWString(task.groupId));
    payload.senderId = toUtf8(senderId);
    payload.targetId = toUtf8(targetId);
    payload.fileName = toUtf8(QString::fromStdWString(task.fileName));
    payload.fileHash = toUtf8(QString::fromStdWString(task.fileHash));
    payload.fileSize = task.fileSize;
    payload.chunkSize = task.chunkSize;
    payload.chunkCount = task.chunkCount;
    payload.completedChunks = m_repository->loadCompletedChunkIndexes(taskId);
    return envelopeFromPayload(payload, outEnvelope);
}

bool FileTransferService::applyRemoteReady(const FileControlPayload& payload, qint64 updatedAtMs) const {
    if (!m_repository
        || (payload.type != FileControlType::Accept && payload.type != FileControlType::ResumeResponse)
        || payload.taskId.empty()) {
        return false;
    }

    FileTransferTask task;
    const QString taskId = toQString(payload.taskId);
    if (!m_repository->findTaskById(taskId, &task)) {
        return false;
    }

    if (!m_repository->replaceCompletedChunks(taskId,
                                              payload.completedChunks,
                                              task.chunkSize,
                                              task.fileSize,
                                              updatedAtMs)) {
        return false;
    }

    const qint64 completedBytes = completedBytesForChunks(task, payload.completedChunks);
    const int completedChunkIndex = lastCompletedChunk(payload.completedChunks);
    const bool remoteAlreadyCompleted =
        task.chunkCount > 0
        && payload.completedChunks.size() >= static_cast<std::size_t>(task.chunkCount)
        && completedBytes >= task.fileSize;
    const bool keepExistingProgressState =
        task.state == FileTransferState::Transferring
        || task.state == FileTransferState::Completing
        || task.state == FileTransferState::Completed;
    const FileTransferState nextState =
        remoteAlreadyCompleted ? FileTransferState::Completed
                               : (keepExistingProgressState ? task.state
                                                            : FileTransferState::ReadyToTransfer);
    const qint64 nextBytesCompleted =
        remoteAlreadyCompleted ? task.fileSize
                               : (keepExistingProgressState ? std::max(task.bytesCompleted, completedBytes)
                                                            : completedBytes);
    const int nextLastChunkIndex =
        remoteAlreadyCompleted ? task.chunkCount - 1
                               : (keepExistingProgressState ? std::max(task.lastChunkIndex, completedChunkIndex)
                                                            : completedChunkIndex);

    return m_repository->updateTaskState(taskId,
                                         nextState,
                                         nextBytesCompleted,
                                         nextLastChunkIndex,
                                         QString(),
                                         QString(),
                                         updatedAtMs);
}

bool FileTransferService::applyRemoteProgress(const FileControlPayload& payload, qint64 updatedAtMs) const {
    if (!m_repository || payload.type != FileControlType::Progress || payload.taskId.empty()) {
        return false;
    }

    FileTransferTask task;
    const QString taskId = toQString(payload.taskId);
    if (!m_repository->findTaskById(taskId, &task)) {
        return false;
    }

    if (!m_repository->replaceCompletedChunks(taskId,
                                              payload.completedChunks,
                                              task.chunkSize,
                                              task.fileSize,
                                              updatedAtMs)) {
        return false;
    }

    const qint64 nextBytesCompleted =
        std::max(task.bytesCompleted, completedBytesForChunks(task, payload.completedChunks));
    const int nextLastChunkIndex =
        std::max(task.lastChunkIndex, lastCompletedChunk(payload.completedChunks));
    FileTransferState nextState = FileTransferState::Transferring;
    if (task.state == FileTransferState::Completed ||
        task.state == FileTransferState::Completing) {
        nextState = task.state;
    }

    return m_repository->updateTaskState(taskId,
                                         nextState,
                                         nextBytesCompleted,
                                         nextLastChunkIndex,
                                         QString(),
                                         QString(),
                                         updatedAtMs);
}

bool FileTransferService::payloadFromEnvelope(const MessageEnvelope& envelope,
                                              FileControlPayload* outPayload) {
    if (!outPayload || envelope.type != MessageType::FileControl) {
        return false;
    }

    const auto controlType = controlTypeFromString(toQString(envelope.controlType));
    if (!controlType.has_value()) {
        return false;
    }

    FileControlPayload payload;
    payload.type = *controlType;
    payload.taskId = envelope.fileTaskId;
    payload.conversationId = envelope.conversationId;
    payload.groupId = envelope.body;
    payload.senderId = envelope.senderId;
    payload.targetId = envelope.targetId;
    payload.fileName = envelope.attachmentName;
    payload.fileHash = envelope.fileHash;
    payload.dataHost = envelope.dataHost;
    payload.reason = envelope.reason;
    payload.fileSize = envelope.fileSize;
    payload.chunkSize = envelope.chunkSize;
    payload.chunkCount = envelope.chunkCount;
    payload.dataPort = envelope.dataPort;
    payload.completedChunks = envelope.completedChunks;
    *outPayload = payload;
    return true;
}

bool FileTransferService::envelopeFromPayload(const FileControlPayload& payload,
                                              MessageEnvelope* outEnvelope) {
    if (!outEnvelope) {
        return false;
    }

    MessageEnvelope envelope;
    envelope.type = MessageType::FileControl;
    envelope.senderId = payload.senderId;
    envelope.targetId = payload.targetId;
    envelope.conversationId = payload.conversationId;
    envelope.body = payload.groupId;
    envelope.attachmentName = payload.fileName;
    envelope.controlType = toUtf8(controlTypeToString(payload.type));
    envelope.fileTaskId = payload.taskId;
    envelope.fileHash = payload.fileHash;
    envelope.dataHost = payload.dataHost;
    envelope.reason = payload.reason;
    envelope.fileSize = payload.fileSize;
    envelope.chunkSize = payload.chunkSize;
    envelope.chunkCount = payload.chunkCount;
    envelope.dataPort = payload.dataPort;
    envelope.completedChunks = payload.completedChunks;
    *outEnvelope = envelope;
    return true;
}
