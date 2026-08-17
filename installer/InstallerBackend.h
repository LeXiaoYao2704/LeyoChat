#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

// Qt Quick UI 只是一个前端壳：
//   1. 收集用户选项（安装路径、是否开机启动、是否安装后启动）
//   2. 以 /VERYSILENT 启动内层 Inno Setup 安装器
//   3. 监控 Inno Setup 进程状态，显示动画进度
//   4. 完成后允许启动应用
class InstallerBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString installPath READ installPath WRITE setInstallPath NOTIFY installPathChanged)
    Q_PROPERTY(bool autoLaunch READ autoLaunch WRITE setAutoLaunch NOTIFY autoLaunchChanged)
    Q_PROPERTY(bool autoStartup READ autoStartup WRITE setAutoStartup NOTIFY autoStartupChanged)
    Q_PROPERTY(bool serverMode READ serverMode NOTIFY installerModeChanged)
    Q_PROPERTY(QString productName READ productName NOTIFY installerModeChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(int state READ state NOTIFY stateChanged)

public:
    enum State { Options = 0, Installing = 1, Complete = 2, Error = 3 };
    Q_ENUM(State)

    explicit InstallerBackend(QObject *parent = nullptr);
    ~InstallerBackend() override;

    QString installPath() const { return m_installPath; }
    void setInstallPath(const QString &path);

    bool autoLaunch() const { return m_autoLaunch; }
    void setAutoLaunch(bool v);

    bool autoStartup() const { return m_autoStartup; }
    void setAutoStartup(bool v);

    qreal progress() const { return m_progress; }
    QString statusText() const { return m_statusText; }
    QString appVersion() const { return m_appVersion; }
    int state() const { return m_state; }
    bool serverMode() const { return m_installerMode == QStringLiteral("server"); }
    QString productName() const;

    void setInnerSetupExe(const QString &exe) { m_innerSetupExe = exe; }
    void setAppVersion(const QString &ver) { m_appVersion = ver; }
    void setInstallerMode(const QString &mode);

    Q_INVOKABLE void browseDirectory();
    Q_INVOKABLE void startInstall();
    Q_INVOKABLE void launchApp();
    Q_INVOKABLE void quit();
    Q_INVOKABLE QString defaultInstallPath() const;

signals:
    void installPathChanged();
    void autoLaunchChanged();
    void autoStartupChanged();
    void installerModeChanged();
    void progressChanged();
    void statusTextChanged();
    void stateChanged();

private slots:
    void onSetupFinished(int exitCode, QProcess::ExitStatus status);
    void onProgressTick();

private:
    QString m_installPath;
    bool m_autoLaunch = true;
    bool m_autoStartup = true;
    qreal m_progress = 0.0;
    QString m_statusText;
    QString m_appVersion;
    QString m_installerMode = QStringLiteral("client");
    QString m_innerSetupExe;   // 内层 Inno Setup 安装器 exe 路径
    int m_state = Options;

    QProcess *m_setupProcess = nullptr;
    QTimer m_progressTimer;
    qint64 m_installStartTime = 0;
};
