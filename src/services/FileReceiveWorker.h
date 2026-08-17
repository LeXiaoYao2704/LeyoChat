#pragma once

#include <QObject>
#include <QString>
#include <QThread>

#include "domain/FileTransferChunk.h"
#include "domain/FileTransferTask.h"

class FileTransferRepository;

/// 文件接收工作线程：将 chunk 写盘 + DB 记录移出主线程，避免批量传输阻塞 UI。
/// 本对象运行在专属 QThread 上，通过 queued signal/slot 与主线程通信。
class FileReceiveWorker : public QObject {
    Q_OBJECT
public:
    /// @param dbPath  SQLite 数据库文件路径（同主线程使用的 DB，WAL 模式安全）
    explicit FileReceiveWorker(const QString& dbPath, QObject* parent = nullptr);
    ~FileReceiveWorker() override;

public slots:
    /// 由主线程通过 queued connection 调用，将 chunk 写入磁盘并记录到 DB。
    void processChunk(const FileTransferChunkHeader& header, const QByteArray& payload);

signals:
    /// chunk 处理完成后发回主线程，携带进度/完成状态。
    /// @param taskId           任务 ID
    /// @param task             任务完整数据（用于主线程 UI 更新和网络发送）
    /// @param bytesCompleted   已完成字节数
    /// @param transferCompleted 是否全部 chunk 已到齐
    /// @param shouldSendProgress 是否需要向对端发送 progress 反馈
    /// @param completedChunks  已完成 chunk 索引列表（仅 shouldSendProgress 时有值）
    void chunkProcessed(QString taskId,
                        FileTransferTask task,
                        qint64 bytesCompleted,
                        bool transferCompleted,
                        bool shouldSendProgress,
                        std::vector<int> completedChunks);

    /// chunk 写盘失败（无需主线程做额外处理，仅用于日志/诊断）
    void chunkFailed(QString taskId, int chunkIndex, QString reason);

private:
    FileTransferRepository* m_repository = nullptr;
    QString m_dbPath;
    QString m_connectionName;
    bool m_dbReady = false;

    void ensureDbConnection();
};
