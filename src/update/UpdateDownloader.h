#pragma once
#include <QObject>
#include <QString>

class UpdateDownloader : public QObject {
    Q_OBJECT
public:
    explicit UpdateDownloader(QObject* parent = nullptr);

    void download(const QString& sourceDir,
                  const QString& fileName,
                  const QString& expectedSha256);

    static QString localInstallerPath();

signals:
    void progressChanged(int percent);
    void downloadFinished(const QString& localPath);
    void downloadFailed(const QString& error);
};
