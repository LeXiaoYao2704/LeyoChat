#include "services/FileReceiveWorker.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "services/FileTransferService.h"
#include "storage/FileTransferRepository.h"

namespace {
constexpr char kConnectionName[] = "leyochat-ft-worker";
}

FileReceiveWorker::FileReceiveWorker(const QString& dbPath, QObject* parent)
    : QObject(parent),
      m_dbPath(dbPath),
      m_connectionName(QLatin1String(kConnectionName)) {}

FileReceiveWorker::~FileReceiveWorker() {
    delete m_repository;
    m_repository = nullptr;
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

void FileReceiveWorker::ensureDbConnection() {
    if (m_dbReady) return;

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        qWarning() << "[FileReceiveWorker] Failed to open database:" << m_dbPath;
        return;
    }
    QSqlQuery query(db);
    query.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    query.exec(QStringLiteral("PRAGMA busy_timeout = 3000"));
    query.exec(QStringLiteral("PRAGMA wal_autocheckpoint = 0"));

    m_repository = new FileTransferRepository(m_connectionName);
    m_dbReady = true;
}

void FileReceiveWorker::processChunk(const FileTransferChunkHeader& header,
                                     const QByteArray& payload) {
    ensureDbConnection();
    if (!m_repository) {
        emit chunkFailed(
            QString::fromUtf8(header.taskId.data(), static_cast<int>(header.taskId.size())),
            header.chunkIndex,
            QStringLiteral("DB connection not available"));
        return;
    }

    const QString taskId =
        QString::fromUtf8(header.taskId.data(), static_cast<int>(header.taskId.size()));

    // 1. 加载任务元数据
    FileTransferTask task;
    if (!m_repository->findTaskById(taskId, &task)
        || task.direction != FileTransferDirection::Incoming) {
        return;  // 非法/非接收任务，静默忽略
    }

    // 2. 写入临时文件
    const QString tempPath = QString::fromStdWString(task.tempPath);
    if (tempPath.isEmpty()) {
        emit chunkFailed(taskId, header.chunkIndex, QStringLiteral("empty tempPath"));
        return;
    }

    QFileInfo tempInfo(tempPath);
    QDir().mkpath(tempInfo.absolutePath());

    QFile partFile(tempPath);
    if (!partFile.open(QIODevice::ReadWrite)) {
        emit chunkFailed(taskId, header.chunkIndex,
                         QStringLiteral("cannot open temp file: ") + partFile.errorString());
        return;
    }

    const qint64 chunkOffset = static_cast<qint64>(header.chunkIndex) * task.chunkSize;
    if (!partFile.seek(chunkOffset) || partFile.write(payload) != payload.size()) {
        partFile.close();
        emit chunkFailed(taskId, header.chunkIndex, QStringLiteral("write failed"));
        return;
    }
    partFile.close();

    // 3. 记录完成的 chunk
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    m_repository->recordCompletedChunk(taskId, header.chunkIndex, payload.size(), nowMs);

    // 4. 计算进度
    const qint64 chunkEndOffset = std::min<qint64>(
        task.fileSize,
        static_cast<qint64>(header.chunkIndex) * task.chunkSize + payload.size());
    qint64 bytesCompleted = std::max(task.bytesCompleted, chunkEndOffset);

    std::vector<int> completedChunks;
    const bool likelyFinalChunk = header.chunkIndex >= (task.chunkCount - 1);

    if (likelyFinalChunk) {
        completedChunks = m_repository->loadCompletedChunkIndexes(taskId);
        // 精确计算已完成字节数
        qint64 sumBytes = 0;
        for (int idx : completedChunks) {
            if (idx < task.chunkCount - 1) {
                sumBytes += task.chunkSize;
            } else {
                sumBytes += task.fileSize - static_cast<qint64>(idx) * task.chunkSize;
            }
        }
        bytesCompleted = sumBytes;
    }

    bool transferCompleted = likelyFinalChunk
        && completedChunks.size() >= static_cast<std::size_t>(task.chunkCount);

    const bool shouldSendProgress = FileTransferService::shouldPublishProgressUpdate(
        task.bytesCompleted,
        bytesCompleted,
        header.chunkIndex,
        transferCompleted);

    if (shouldSendProgress && !likelyFinalChunk) {
        completedChunks = m_repository->loadCompletedChunkIndexes(taskId);
        qint64 sumBytes = 0;
        for (int idx : completedChunks) {
            if (idx < task.chunkCount - 1) {
                sumBytes += task.chunkSize;
            } else {
                sumBytes += task.fileSize - static_cast<qint64>(idx) * task.chunkSize;
            }
        }
        bytesCompleted = sumBytes;
    }

    if (shouldSendProgress && !transferCompleted) {
        transferCompleted =
            completedChunks.size() >= static_cast<std::size_t>(task.chunkCount);
    }

    // 兜底完整性检查
    if (!transferCompleted && task.bytesCompleted >= task.fileSize) {
        if (completedChunks.empty()) {
            completedChunks = m_repository->loadCompletedChunkIndexes(taskId);
            qint64 sumBytes = 0;
            for (int idx : completedChunks) {
                if (idx < task.chunkCount - 1) {
                    sumBytes += task.chunkSize;
                } else {
                    sumBytes += task.fileSize - static_cast<qint64>(idx) * task.chunkSize;
                }
            }
            bytesCompleted = sumBytes;
        }
        transferCompleted =
            completedChunks.size() >= static_cast<std::size_t>(task.chunkCount);
    }

    // 5. 更新任务状态（写 DB）
    if (shouldSendProgress) {
        m_repository->updateTaskState(taskId,
                                      transferCompleted ? FileTransferState::Completing
                                                       : FileTransferState::Transferring,
                                      bytesCompleted,
                                      header.chunkIndex,
                                      QString(),
                                      QString(),
                                      nowMs);
    }

    // 6. 更新 task 的内存副本，发回主线程
    task.bytesCompleted = bytesCompleted;
    task.lastChunkIndex = header.chunkIndex;

    if (shouldSendProgress || transferCompleted) {
        emit chunkProcessed(taskId, task, bytesCompleted,
                            transferCompleted, shouldSendProgress, completedChunks);
    }
}
