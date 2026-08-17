#include "app/LocalFileServiceAutostart.h"

#include "integrations/RemoteFileServiceSettings.h"
#include "storage/GroupRepository.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTcpSocket>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

namespace {

QString detectFirstLanIpv4()
{
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const auto entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            const QHostAddress addr = entry.ip();
            if (addr.protocol() == QAbstractSocket::IPv4Protocol
                && !addr.isLoopback()) {
                return addr.toString();
            }
        }
    }
    return QString();
}

} // namespace

void autoStartLocalFileServices(const GroupRepository& groupRepo,
                                const QString& localClientId)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString serviceExe = QDir(appDir).filePath(QStringLiteral("LeyoFileService.exe"));
    if (!QFileInfo::exists(serviceExe)) {
        return; // 没有和主程序一起部署文件服务
    }

    // 读取本地文件服务全局配置（端口、OnlyOffice 等）
    const auto localCfg = LocalFileServiceSettingsStore::load();

    const auto groups = groupRepo.loadGroupsForMember(localClientId.toStdWString());

    // 按端口聚合：同一端口的多个群共享同一个文件服务实例
    QStringList workspaceIds;
    for (const auto& group : groups) {
        const QString groupId = QString::fromStdWString(group.groupId);
        const auto cfg = GroupFileServiceSettingsStore::load(groupId);
        if (!cfg.enabled || cfg.baseUrl.isEmpty()) continue;

        const QUrl url(cfg.baseUrl);
        const QString host = url.host();
        if (host != QStringLiteral("localhost") && host != QStringLiteral("127.0.0.1"))
            continue;

        if (!cfg.workspaceId.isEmpty()
            && !workspaceIds.contains(cfg.workspaceId)) {
            workspaceIds.append(cfg.workspaceId);
        }
    }

    // 计算用户可写的数据目录
    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/LeyoFileService");

    const quint16 port = localCfg.port;
    const auto onlyOfficeUrl     = localCfg.onlyOfficeUrl;
    const auto onlyOfficeSecret  = localCfg.onlyOfficeJwtSecret;
    const auto externalUrl       = localCfg.externalUrl;
    const int  ttlDays           = localCfg.chatFileTtlDays;
    const int  quotaMb           = localCfg.chatFileQuotaMb;

    // 在后台线程做 TCP 探测，避免阻塞 UI
    (void)QtConcurrent::run([=]() {
        QTcpSocket socket;
        socket.connectToHost(QStringLiteral("127.0.0.1"), port);
        if (socket.waitForConnected(2000)) {
            socket.disconnectFromHost();
            qInfo() << "[file-service-autostart] port" << port
                    << "already alive, skip";
            return;
        }

        // 确保数据目录存在
        QDir().mkpath(dataDir);
        const QString dbPath = QDir(dataDir).filePath(QStringLiteral("leyo-files.db"));
        const QString storagePath = QDir(dataDir).filePath(QStringLiteral("storage"));
        QDir().mkpath(storagePath);

        QStringList args;
        args << QStringLiteral("--port") << QString::number(port);
        args << QStringLiteral("--db") << dbPath;
        args << QStringLiteral("--storage") << storagePath;

        if (!workspaceIds.isEmpty())
            args << QStringLiteral("--workspaces")
                 << workspaceIds.join(QLatin1Char(','));
        else
            args << QStringLiteral("--allow-wildcard-workspaces");

        args << QStringLiteral("--chat-file-ttl-days") << QString::number(ttlDays);
        args << QStringLiteral("--chat-file-quota-mb") << QString::number(quotaMb);

        // OnlyOffice 参数（如已配置）
        if (!onlyOfficeUrl.isEmpty()) {
            args << QStringLiteral("--onlyoffice-url") << onlyOfficeUrl;
            if (!onlyOfficeSecret.isEmpty())
                args << QStringLiteral("--onlyoffice-jwt-secret") << onlyOfficeSecret;

            // external-url: 优先使用显式配置，否则自动探测本机 LAN IP
            QString effectiveExternalUrl = externalUrl;
            if (effectiveExternalUrl.isEmpty()) {
                const QString lanIp = detectFirstLanIpv4();
                if (!lanIp.isEmpty()) {
                    effectiveExternalUrl = QStringLiteral("http://%1:%2")
                        .arg(lanIp).arg(port);
                }
            }
            if (!effectiveExternalUrl.isEmpty())
                args << QStringLiteral("--external-url") << effectiveExternalUrl;
        }

        if (QProcess::startDetached(serviceExe, args, dataDir)) {
            qInfo() << "[file-service-autostart] started LeyoFileService"
                    << "port=" << port
                    << "db=" << dbPath
                    << "workspaces=" << workspaceIds;
        } else {
            qWarning() << "[file-service-autostart] FAILED to start LeyoFileService"
                       << "port=" << port;
        }
    });
}
