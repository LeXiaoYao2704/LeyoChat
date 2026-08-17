#pragma once
#include <QString>
#include <QByteArray>
#include <optional>

class FileStorageManager {
public:
    explicit FileStorageManager(const QString& storageRoot);

    QString storageRoot() const { return m_storageRoot; }

    // Saves data to storage, returns relative path (e.g. "fileId/versionId.bin")
    std::optional<QString> saveFile(const QString& fileId,
                                    const QString& versionId,
                                    const QByteArray& data) const;

    // Reads file by relative storage path
    std::optional<QByteArray> readFile(const QString& relativePath) const;

    // Removes file at relative storage path; returns true if deleted, false if not found or error
    bool deleteFile(const QString& relativePath) const;

    QString absolutePath(const QString& relativePath) const;

    // Range 读取：返回文件片段（offset 起始字节，length -1 表示到末尾）
    struct FileRangeResult {
        QByteArray data;
        qint64 totalSize = 0;
        qint64 rangeStart = 0;
        qint64 rangeEnd = 0;
    };
    std::optional<FileRangeResult> readFileRange(const QString& relativePath,
                                                  qint64 offset = 0,
                                                  qint64 length = -1) const;

    // 获取文件大小（不存在返回 -1）
    qint64 fileSize(const QString& relativePath) const;

    // 从路径复制文件到存储（大文件友好，不加载到内存）
    std::optional<QString> saveFileFromPath(const QString& chatFileId,
                                             const QString& sourcePath) const;

private:
    QString m_storageRoot;
};
