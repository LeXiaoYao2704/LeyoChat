#pragma once
#include <QObject>
#include <QString>
#include <QTimer>

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    struct UpdateInfo {
        QString version;
        QString fileName;
        QString sha256;
        QString releaseNotes;
        QString minVersion;
    };

    explicit UpdateChecker(QObject* parent = nullptr);

    void setUpdateSourcePath(const QString& path);
    QString updateSourcePath() const;

    void start(int intervalMs = 3600000);
    void stop();
    void checkNow();

signals:
    void updateAvailable(const UpdateChecker::UpdateInfo& info);
    void noUpdateAvailable();
    void checkFailed(const QString& error);

private:
    void performCheck();

    QTimer* m_timer = nullptr;
    QString m_updateSourcePath;
};

Q_DECLARE_METATYPE(UpdateChecker::UpdateInfo)
