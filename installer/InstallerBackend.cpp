#include "InstallerBackend.h"
#include "InstallerLaunchPolicy.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>

namespace
{

QString clientProcessName()
{
    return QStringLiteral("LeyoChat.exe");
}

QString launcherProcessName()
{
    return QStringLiteral("LeyoChatLauncher.exe");
}

QString processName(LeyoChatInstaller::ClientProcessKind kind)
{
    return kind == LeyoChatInstaller::ClientProcessKind::Launcher
        ? launcherProcessName()
        : clientProcessName();
}

QString processOutput(QProcess &process)
{
    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    const QString error = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    if (error.isEmpty())
        return output;
    if (output.isEmpty())
        return error;
    return output + QStringLiteral(" | ") + error;
}

bool isProcessRunning(const QString &name)
{
    QProcess tasklist;
    tasklist.start(QStringLiteral("tasklist"), {QStringLiteral("/FI"), QStringLiteral("IMAGENAME eq %1").arg(name), QStringLiteral("/NH")});
    if (!tasklist.waitForFinished(3000)) {
        qWarning().noquote() << "[installer] tasklist timed out for" << name;
        return true;
    }
    return tasklist.readAllStandardOutput().contains(name.toUtf8());
}

bool terminateProcessIfRunning(const QString &name, const QString &reason)
{
    const bool running = isProcessRunning(name);
    qInfo().noquote() << "[installer] process check reason=" << reason
                      << "name=" << name
                      << "running=" << running;
    if (!running)
        return true;

    QProcess taskkill;
    taskkill.start(QStringLiteral("taskkill.exe"),
                   {QStringLiteral("/IM"), name, QStringLiteral("/F")});
    if (!taskkill.waitForFinished(5000)) {
        taskkill.kill();
        taskkill.waitForFinished(1000);
        qWarning().noquote() << "[installer] taskkill timed out reason=" << reason
                             << "name=" << name;
        return false;
    }

    const QString output = processOutput(taskkill);
    const bool taskkillSucceeded =
        taskkill.exitStatus() == QProcess::NormalExit && taskkill.exitCode() == 0;
    const bool processStopped = !isProcessRunning(name);
    qInfo().noquote() << "[installer] taskkill finished reason=" << reason
                      << "name=" << name
                      << "exitCode=" << taskkill.exitCode()
                      << "taskkillSucceeded=" << taskkillSucceeded
                      << "processStopped=" << processStopped
                      << "output=" << output;
    return processStopped;
}

QString exitStatusName(QProcess::ExitStatus status)
{
    return status == QProcess::NormalExit
        ? QStringLiteral("NormalExit")
        : QStringLiteral("CrashExit");
}

} // namespace

InstallerBackend::InstallerBackend(QObject *parent)
    : QObject(parent)
{
    m_installPath = defaultInstallPath();

    // 模拟平滑进度条：每 300ms 递增，安装过程中让用户看到进度在动
    m_progressTimer.setInterval(300);
    connect(&m_progressTimer, &QTimer::timeout, this, &InstallerBackend::onProgressTick);
}

InstallerBackend::~InstallerBackend()
{
    m_progressTimer.stop();
    if (m_setupProcess) {
        m_setupProcess->kill();
        m_setupProcess->waitForFinished(3000);
    }
}

QString InstallerBackend::defaultInstallPath() const
{
    QString pf = qEnvironmentVariable("ProgramFiles", "C:\\Program Files");
    return QDir::toNativeSeparators(
        pf + (serverMode() ? "\\LeyoChat Server" : "\\LeyoChat"));
}

QString InstallerBackend::productName() const
{
    return serverMode()
        ? QStringLiteral("LeyoChat Server")
        : QStringLiteral("LeyoChat");
}

void InstallerBackend::setInstallerMode(const QString &mode)
{
    const QString normalized =
        mode.compare(QStringLiteral("server"), Qt::CaseInsensitive) == 0
            ? QStringLiteral("server")
            : QStringLiteral("client");
    if (m_installerMode == normalized)
        return;

    const QString oldDefaultPath = defaultInstallPath();
    m_installerMode = normalized;
    emit installerModeChanged();

    if (m_installPath.isEmpty() || m_installPath == oldDefaultPath)
        setInstallPath(defaultInstallPath());

    if (serverMode()) {
        setAutoLaunch(false);
        setAutoStartup(false);
    }
}

void InstallerBackend::setInstallPath(const QString &path)
{
    if (m_installPath != path) {
        m_installPath = path;
        emit installPathChanged();
    }
}

void InstallerBackend::setAutoLaunch(bool v)
{
    if (m_autoLaunch != v) {
        m_autoLaunch = v;
        emit autoLaunchChanged();
    }
}

void InstallerBackend::setAutoStartup(bool v)
{
    if (m_autoStartup != v) {
        m_autoStartup = v;
        emit autoStartupChanged();
    }
}

void InstallerBackend::browseDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(
        nullptr,
        QStringLiteral("\u9009\u62e9\u5b89\u88c5\u76ee\u5f55"),
        m_installPath);
    if (!dir.isEmpty()) {
        setInstallPath(QDir::toNativeSeparators(dir));
    }
}

void InstallerBackend::startInstall()
{
    if (m_innerSetupExe.isEmpty() || !QFile::exists(m_innerSetupExe)) {
        m_state = Error;
        m_statusText = QStringLiteral("\u627e\u4e0d\u5230\u5b89\u88c5\u7a0b\u5e8f\uff1a%1").arg(m_innerSetupExe);
        emit statusTextChanged();
        emit stateChanged();
        return;
    }

    bool launcherStopped = true;
    bool clientStopped = true;
    if (LeyoChatInstaller::shouldStopClientProcessesBeforeInstall(serverMode())) {
        for (const auto processKind : LeyoChatInstaller::clientProcessStopOrder()) {
            const bool stopped = terminateProcessIfRunning(
                processName(processKind), QStringLiteral("pre-install"));
            if (processKind == LeyoChatInstaller::ClientProcessKind::Launcher)
                launcherStopped = stopped;
            else
                clientStopped = stopped;
        }
    }
    if (!LeyoChatInstaller::canContinueAfterStoppingClientProcesses(
            serverMode(), launcherStopped, clientStopped)) {
        m_state = Error;
        m_statusText = QStringLiteral("无法关闭正在运行的 LeyoChat，请退出后重试。");
        emit statusTextChanged();
        emit stateChanged();
        return;
    }

    m_state = Installing;
    emit stateChanged();

    // 构建 Inno Setup 命令行参数
    // /VERYSILENT  — 完全静默，不显示任何 UI
    // /SUPPRESSMSGBOXES — 抑制消息框
    // /DIR="..."  — 安装目录
    // /FORCECLOSEAPPLICATIONS — 强制关闭旧进程，避免 Restart Manager 失败返回退出码 5
    QStringList args;
    args << QStringLiteral("/VERYSILENT")
         << QStringLiteral("/SUPPRESSMSGBOXES")
         << QStringLiteral("/NORESTART")
         << QStringLiteral("/NORESTARTAPPLICATIONS")
         << QStringLiteral("/FORCECLOSEAPPLICATIONS")
         << QStringLiteral("/DIR=%1").arg(m_installPath);

    if (!serverMode()) {
        QStringList tasks;
        if (m_autoStartup)
            tasks << QStringLiteral("autostart");
        args << QStringLiteral("/TASKS=%1").arg(tasks.join(QLatin1Char(',')));
    }

    m_setupProcess = new QProcess(this);
    connect(m_setupProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &InstallerBackend::onSetupFinished);

    m_statusText = QStringLiteral("\u6b63\u5728\u51c6\u5907\u5b89\u88c5...");
    m_progress = 0.0;
    emit progressChanged();
    emit statusTextChanged();

    m_installStartTime = QDateTime::currentMSecsSinceEpoch();
    m_progressTimer.start();

    qInfo().noquote() << "[installer] starting inner setup"
                      << "exe=" << m_innerSetupExe
                      << "args=" << args.join(QLatin1Char(' '));
    m_setupProcess->start(m_innerSetupExe, args);
}

void InstallerBackend::onProgressTick()
{
    // 模拟进度：前 80% 用 ~20 秒平滑递增，之后放慢
    // 真实完成由 onSetupFinished 触发 100%
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_installStartTime;
    double seconds = elapsed / 1000.0;

    // 使用渐近函数：progress = 1 - e^(-t/25)，上限 0.92
    double simulated = 1.0 - std::exp(-seconds / 25.0);
    if (simulated > 0.92)
        simulated = 0.92;

    if (simulated > m_progress) {
        m_progress = simulated;
        emit progressChanged();
    }

    // 更新状态文本
    static const QString statusTexts[] = {
        QStringLiteral("\u6b63\u5728\u5173\u95ed\u65e7\u7248\u672c\u7a0b\u5e8f..."),
        QStringLiteral("\u6b63\u5728\u590d\u5236\u6587\u4ef6..."),
        QStringLiteral("\u6b63\u5728\u914d\u7f6e\u7cfb\u7edf..."),
        QStringLiteral("\u6b63\u5728\u5b8c\u6210\u5b89\u88c5..."),
    };
    int phase = qMin(static_cast<int>(seconds / 6.0), 3);
    if (m_statusText != statusTexts[phase]) {
        m_statusText = statusTexts[phase];
        emit statusTextChanged();
    }
}

void InstallerBackend::onSetupFinished(int exitCode, QProcess::ExitStatus status)
{
    m_progressTimer.stop();

    const QString standardOutput = QString::fromLocal8Bit(m_setupProcess->readAllStandardOutput()).trimmed();
    const QString standardError = QString::fromLocal8Bit(m_setupProcess->readAllStandardError()).trimmed();
    qInfo().noquote() << "[installer] inner setup finished"
                      << "exitCode=" << exitCode
                      << "status=" << exitStatusName(status)
                      << "stdout=" << standardOutput
                      << "stderr=" << standardError;

    if (status == QProcess::NormalExit && exitCode == 0) {
        m_progress = 1.0;
        emit progressChanged();

        if (m_autoLaunch) {
            // 勾选了自动启动：进度满 → 提示"正在启动" → 停留 1.5 秒 → 启动并退出
            m_statusText = QStringLiteral("\u5b89\u88c5\u5b8c\u6210\uff0c\u6b63\u5728\u542f\u52a8 LeyoChat\u2026");
            emit statusTextChanged();
            // 不切换到 Complete 状态，保持在进度页面展示 100%
            QTimer::singleShot(2500, this, [this]() {
                launchApp();
            });
            return;
        }

        // 未勾选自动启动 → 显示完成页面
        m_statusText = QStringLiteral("\u5b89\u88c5\u5b8c\u6210");
        m_state = Complete;
        emit statusTextChanged();
        emit stateChanged();
        return;
    } else {
        m_statusText = QStringLiteral("\u5b89\u88c5\u5931\u8d25\uff08\u9000\u51fa\u7801\uff1a%1\uff09").arg(exitCode);
        m_state = Error;
    }
    emit progressChanged();
    emit statusTextChanged();
    emit stateChanged();
}

void InstallerBackend::launchApp()
{
    const bool launcherRunning = isProcessRunning(launcherProcessName());
    const bool clientRunning = isProcessRunning(clientProcessName());
    const auto plan = LeyoChatInstaller::clientLaunchPlan(
        serverMode(), launcherRunning, clientRunning);

    qInfo().noquote() << "[installer] launch plan"
                      << "serverMode=" << serverMode()
                      << "launcherRunning=" << launcherRunning
                      << "clientRunning=" << clientRunning
                      << "terminateExistingLauncher=" << plan.terminateExistingLauncher
                      << "terminateExistingClient=" << plan.terminateExistingClient
                      << "launchLauncher=" << plan.launchLauncher;

    const bool launcherStopped = !plan.terminateExistingLauncher
        || terminateProcessIfRunning(launcherProcessName(), QStringLiteral("pre-launch"));
    const bool clientStopped = !plan.terminateExistingClient
        || terminateProcessIfRunning(clientProcessName(), QStringLiteral("pre-launch"));

    if (!LeyoChatInstaller::canContinueAfterStoppingClientProcesses(
            serverMode(), launcherStopped, clientStopped)) {
        m_state = Error;
        m_statusText = QStringLiteral("无法关闭正在运行的 LeyoChat，请退出后重试。");
        emit statusTextChanged();
        emit stateChanged();
        return;
    }

    if (!plan.launchLauncher) {
        quit();
        return;
    }

    const QString exe = QDir(m_installPath).absoluteFilePath(launcherProcessName());
    const bool started = QProcess::startDetached(exe, {});
    qInfo().noquote() << "[installer] launch launcher"
                      << "exe=" << exe
                      << "started=" << started;
    quit();
}

void InstallerBackend::quit()
{
    QCoreApplication::quit();
}
