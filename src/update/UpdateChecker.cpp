#include "update/UpdateChecker.h"
#include "app/ApplicationInfo.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <QtConcurrent>

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    qRegisterMetaType<UpdateChecker::UpdateInfo>();
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &UpdateChecker::performCheck);
}

void UpdateChecker::setUpdateSourcePath(const QString& path)
{
    m_updateSourcePath = path;
}

QString UpdateChecker::updateSourcePath() const
{
    return m_updateSourcePath;
}

void UpdateChecker::start(int intervalMs)
{
    m_timer->start(intervalMs);
    performCheck();
}

void UpdateChecker::stop()
{
    m_timer->stop();
}

void UpdateChecker::checkNow()
{
    performCheck();
}

void UpdateChecker::performCheck()
{
    const QString sourcePath = m_updateSourcePath;
    const QString currentVer = ApplicationInfo::currentVersion();

    (void)QtConcurrent::run([this, sourcePath, currentVer]() {
        const QString jsonPath = QDir(sourcePath).filePath(QStringLiteral("latest.json"));
        QFile file(jsonPath);
        if (!file.open(QIODevice::ReadOnly)) {
            QMetaObject::invokeMethod(this, [this, jsonPath]() {
                emit checkFailed(QStringLiteral("无法读取更新信息：%1").arg(jsonPath));
            });
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        file.close();

        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            QMetaObject::invokeMethod(this, [this, parseError]() {
                emit checkFailed(QStringLiteral("更新信息格式错误：%1").arg(parseError.errorString()));
            });
            return;
        }

        const QJsonObject obj = doc.object();
        UpdateInfo info;
        info.version = obj.value(QStringLiteral("version")).toString().trimmed();
        info.fileName = obj.value(QStringLiteral("file")).toString().trimmed();
        info.sha256 = obj.value(QStringLiteral("sha256")).toString().trimmed();
        info.releaseNotes = obj.value(QStringLiteral("releaseNotes")).toString();
        info.minVersion = obj.value(QStringLiteral("minVersion")).toString().trimmed();

        if (info.version.isEmpty() || info.fileName.isEmpty()) {
            QMetaObject::invokeMethod(this, [this]() {
                emit checkFailed(QStringLiteral("更新信息缺少必要字段"));
            });
            return;
        }

        const QVersionNumber remote = QVersionNumber::fromString(info.version);
        const QVersionNumber local = QVersionNumber::fromString(currentVer);

        if (remote > local) {
            QMetaObject::invokeMethod(this, [this, info]() {
                emit updateAvailable(info);
            });
        } else {
            QMetaObject::invokeMethod(this, [this]() {
                emit noUpdateAvailable();
            });
        }
    });
}
