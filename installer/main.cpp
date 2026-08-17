#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDateTime>
#include "InstallerBackend.h"

static QFile *g_logFile = nullptr;
static void msgHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (!g_logFile) return;
    const char *tag = "DBG";
    switch (type) {
    case QtWarningMsg: tag = "WRN"; break;
    case QtCriticalMsg: tag = "CRT"; break;
    case QtFatalMsg: tag = "FTL"; break;
    default: break;
    }
    QString line = QStringLiteral("[%1] %2: %3\n")
        .arg(QLatin1String(tag), QDateTime::currentDateTime().toString(Qt::ISODate), msg);
    g_logFile->write(line.toUtf8());
    g_logFile->flush();
}

int main(int argc, char *argv[])
{
    // 日志到 %TEMP%\LeyoChatSetup.log
    QString logPath = QDir::tempPath() + QStringLiteral("/LeyoChatSetup.log");
    static QFile logFile(logPath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        g_logFile = &logFile;
        qInstallMessageHandler(msgHandler);
    }
    qDebug("=== LeyoChatSetup starting ===");

    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("LeyoChat"));
    app.setApplicationName(QStringLiteral("LeyoChat"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("LeyoChat Installer"));
    parser.addHelpOption();

    // 内层 Inno Setup 安装器的路径（由 SFX 外壳传入）
    QCommandLineOption innerSetupOpt(
        QStringLiteral("inner-setup"),
        QStringLiteral("Path to the inner Inno Setup installer exe."),
        QStringLiteral("path"));
    parser.addOption(innerSetupOpt);

    QCommandLineOption appVersionOpt(
        QStringLiteral("app-version"),
        QStringLiteral("Application version string."),
        QStringLiteral("version"),
        QStringLiteral(APP_VERSION_STRING));
    parser.addOption(appVersionOpt);

    QCommandLineOption installerModeOpt(
        QStringLiteral("installer-mode"),
        QStringLiteral("Installer mode: client or server."),
        QStringLiteral("mode"),
        QStringLiteral("client"));
    parser.addOption(installerModeOpt);

    parser.process(app);

    QString innerSetupExe = parser.value(innerSetupOpt);
    if (innerSetupExe.isEmpty()) {
        // 默认：与自身同目录的 LeyoChat-inner-setup.exe
        innerSetupExe = QDir(QCoreApplication::applicationDirPath())
                            .absoluteFilePath(QStringLiteral("LeyoChat-inner-setup.exe"));
    }

    QString appVersion = parser.value(appVersionOpt);

    InstallerBackend backend;
    backend.setInnerSetupExe(innerSetupExe);
    backend.setAppVersion(appVersion);
    backend.setInstallerMode(parser.value(installerModeOpt));

    qDebug("innerSetupExe: %s", qPrintable(innerSetupExe));
    qDebug("appVersion: %s", qPrintable(appVersion));
    qDebug("installerMode: %s", qPrintable(parser.value(installerModeOpt)));
    qDebug("appDir: %s", qPrintable(QCoreApplication::applicationDirPath()));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("installer"), &backend);

    qDebug("loading QML module LeyoChatInstaller/Main ...");
    engine.loadFromModule("LeyoChatInstaller", "Main");

    qDebug("rootObjects count: %d", engine.rootObjects().size());
    if (engine.rootObjects().isEmpty()) {
        qCritical("QML load failed - no root objects");
        return -1;
    }

    return app.exec();
}
