#include "app/LeyoApplication.h"

#include "app/AppSettings.h"
#include "app/AboutDialogHelpers.h"
#include "app/ApplicationInfo.h"
#include "app/AppPathHelpers.h"
#include "app/AppResourceDiagnostics.h"
#include "app/AttachmentPayloadHelpers.h"
#include "app/AvatarStorageHelpers.h"
#include "app/CallControlWireCodec.h"
#include "app/ConnectionRegistryUtils.h"
#include "app/ConversationSummaryHelpers.h"
#include "app/DeliveryReceiptHelpers.h"
#include "app/DocumentDialogHelpers.h"
#include "app/FileOpenHelpers.h"
#include "app/GroupFanOutDelivery.h"
#include "app/GroupFanOutPayloadBuilder.h"
#include "app/GroupFileServiceConfigResolver.h"
#include "store/ChatDataStore.h"
#include "store/DatabaseWorker.h"
#include "app/GroupFileSendRouting.h"
#include "app/ImageFileTypeHelpers.h"
#include "app/IntegrationNotificationPresentation.h"
#include "app/IntegrationPollBackoff.h"
#include "app/LocalFileServiceAutostart.h"
#include "app/MessagePresentationHelpers.h"
#include "app/PeerProfileInfo.h"
#include "app/PeerPresentationHelpers.h"
#include "app/PresenceHeuristics.h"
#include "app/ReminderActionRouting.h"
#include "app/ReminderItemFactory.h"
#include "app/ReminderNotificationPresentation.h"
#include "app/RemotePresenceUiAdapter.h"
#include "app/SystemActivityHelpers.h"
#include "app/TestModeContext.h"
#include "app/TransferSpeedTracker.h"
#include "app/UiRestoreHelpers.h"
#include "architecture/ResourceReferenceMessage.h"
#include "call/CallSession.h"
#include "call/CallSoundPlayer.h"
#include "call/AudioChannel.h"
#include "call/ScreenCaptureChannel.h"
#include "call/ScreenViewerWidget.h"
#include "call/RemoteInputChannel.h"
#include "domain/CallProtocol.h"
#include "domain/PeerPresenceEvaluator.h"
#include "network/FileTransferConnection.h"
#include "network/LanDiscoveryService.h"
#include "network/MessageCodec.h"
#include "network/FileTransferServer.h"
#include "network/PeerConnection.h"
#include "network/PeerHandshake.h"
#include "network/PeerServer.h"
#include "network/PeerSessionManager.h"
#include "network/TlsHelper.h"
#include "services/ChatService.h"
#include "services/DirectConversationAddressing.h"
#include "services/FileReceiveWorker.h"
#include "services/FileTransferService.h"
#include "services/GroupService.h"
#include "services/GroupFileTransferService.h"
#include "services/IdentityService.h"
#include "services/MessageMutationService.h"
#include "services/MessageRoutingCapabilities.h"
#include "services/MessageSyncService.h"
#include "services/P2PConnectionPolicy.h"
#include "services/PeerDirectoryService.h"
#include "services/RecipientCapabilityResolver.h"
#include "services/ReminderService.h"
#include "services/ReliableDirectEnvelopeSender.h"
#include "services/ReliableDirectMessageSender.h"
#include "services/ReliableGroupEnvelopeSender.h"
#include "services/ReliableGroupFileMessageSender.h"
#include "services/ReliableGroupMessageSender.h"
#include "services/RemoteMessageDeviceIdentity.h"
#include "services/RemoteMessageEventConsumer.h"
#include "services/RemoteMessageSyncCoordinator.h"
#include "services/SharedFileResourceSync.h"
#include "services/ResourceRefRouter.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"
#include "storage/FileTransferRepository.h"
#include "storage/GroupRepository.h"
#include "storage/ProfileRepository.h"
#include "storage/ReminderRepository.h"
#include "storage/ServiceBindingRepository.h"
#include "storage/ServiceRegistryRepository.h"
#include "storage/ServiceResourceRepository.h"
#include "architecture/HybridRoutingPolicy.h"
#include "architecture/PersistedRuntimeArchitectureLoader.h"
#include "architecture/RuntimeArchitecturePresentation.h"
#include "diagnostics/Diagnostics.h"
#include "recovery/ClientRecoveryState.h"
#include "integrations/AzureDevOpsSettings.h"
#include "integrations/KnowledgeServiceSettings.h"
#include "integrations/LocalAzureDevOpsAdapter.h"
#include "integrations/LocalOutlookAdapter.h"
#include "integrations/OutlookSettings.h"
#include "integrations/RemoteFileServiceAdapter.h"
#include "integrations/RemoteChatServiceSettings.h"
#include "integrations/RemoteFileServiceSettings.h"
#include "integrations/ServerMessageClient.h"
#include "ui/FileVersionHistoryDialog.h"
#include "ui/GroupFileManagerDialog.h"
#ifdef LEYOCHAT_HAS_WEBENGINE
#include "ui/OnlineEditorWidget.h"
#endif
#include "services/AzureDevOpsBuildNotificationPoller.h"
#include "services/AzureDevOpsNotificationDispatcher.h"
#include "services/OutlookNotificationDispatcher.h"
#include "services/OutlookNotificationPoller.h"
#include "services/OutlookStreamingConnection.h"
#include "ui/AzureDevOpsInsertDialog.h"
#include "ui/AzureDevOpsNotificationDialog.h"
#include "ui/OutlookNotificationDialog.h"
#include "ui/ContactListModel.h"
#include "ui/ConnectIpDialog.h"
#include "ui/ConversationListModel.h"
#include "ui/CreateGroupDialog.h"
#include "ui/GroupInfoPanel.h"
#include "ui/LoginDialog.h"
#include "ui/MainWindow.h"
#include "ui/GlobalSearchHistory.h"
#include "ui/CallWindow.h"
#include "ui/ClientPreferences.h"
#include "ui/ProfileCardPopup.h"
#include "ui/ReminderDialog.h"
#include "ui/StickerManager.h"
#include "ui/ChatComposerWidget.h"
#include "ui/ChatHeaderWidget.h"
#include "ui/MessageListModel.h"
#include "ui/PinyinHelper.h"
#include "ui/ContactSelectionDialog.h"
#include "ui/RuntimeArchitectureDialog.h"
#include "ui/SettingsPage.h"
#include "ui/StorageCleanupDialog.h"
#include "ui/TransferListModel.h"
#include "ui/UpdateBar.h"
#include "update/UpdateDownloader.h"
#include "ui/AppStyle.h"
#include "ui/ImageViewerWidget.h"
#include "ui/KnowledgeServiceSettingsWidget.h"
#include "ui/MarkdownRenderer.h"

#include <ElaApplication.h>
#include <ElaCheckBox.h>
#include <ElaContentDialog.h>
#include <ElaDialog.h>
#include <ElaFrame.h>
#include <ElaListView.h>
#include <ElaLineEdit.h>
#include <ElaPushButton.h>
#include <ElaScrollArea.h>
#include <ElaSpinBox.h>
#include <ElaStackedWidget.h>
#include <ElaText.h>
#include <ElaTheme.h>

#include <QApplication>
#include <QByteArray>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHash>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <ElaListWidget.h>
#include <ElaTextEdit.h>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QScopeGuard>
#include "ui/LeyoDialog.h"
#include <QQueue>
#include <QMenu>
#include <QSet>
#include <QDirIterator>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTcpSocket>
#include <QThread>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include "ui/GlobalHotkeyManager.h"
#include <QDesktopServices>

#include <QFormLayout>
#include <QGridLayout>
#include <QFrame>
#include <QPainter>
#include <ElaFrame.h>
#include <QLabel>
#include <QImage>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QSettings>
#include <QSplashScreen>
#include <QSpinBox>
#include <QStyleHints>
#include <QStringListModel>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

#include <QtConcurrent/QtConcurrent>

#include <QAccessible>
#include <QAccessibleInterface>
#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
QString applicationDisplayName() {
    return QStringLiteral("LeyoChat");
}

GroupFileServiceConfig effectiveGroupFileServiceConfigForGroup(const QString& groupId)
{
    return GroupFileServiceConfigResolver::makeDefaultConfig(
        groupId,
        RemoteChatServiceSettingsStore::load());
}

RemoteFileServiceConnectionSettings fileServiceConnectionSettingsForGroup(const QString& groupId)
{
    return GroupFileServiceConfigResolver::makeConnectionSettings(
        effectiveGroupFileServiceConfigForGroup(groupId));
}

struct RecipientRoutePartition {
    QVector<QString> serverRecipientIds;
    QVector<QString> p2pRecipientIds;
};

bool remoteChatServiceRecentlyReachable(const RemoteChatServiceSettings& settings)
{
    return settings.shouldAttemptMessageService(
        QDateTime::currentMSecsSinceEpoch());
}

bool remoteChatServiceProbeAllowed(const RemoteChatServiceSettings& settings)
{
    return settings.shouldProbeMessageService(
        QDateTime::currentMSecsSinceEpoch());
}

QStringList localMessageRoutingCapabilities()
{
    return QStringList{
        MessageRoutingCapabilities::remoteMessageV1(),
        MessageRoutingCapabilities::serverReceiveV1(),
        MessageRoutingCapabilities::p2pDeliveryReceiptV1()
    };
}

}

LeyoApplication::LeyoApplication(QApplication& app,
                                         ClientStartupOptions startupOptions)
    : m_app(app),
      m_startupOptions(std::move(startupOptions))
{
    // 初始化 ElaWidgetTools
    eApp->init();

    const AppStyle::ThemeMode mode = AppStyle::storedThemeMode();
    m_app.setProperty("leyochat.themeMode", AppStyle::themeModeToString(mode));
    {
        const ElaThemeType::ThemeMode elaMode = AppStyle::toElaThemeMode(mode);
        if (eTheme->getThemeMode() != elaMode) {
            eTheme->setThemeMode(elaMode);
        }
    }
    m_app.setPalette(AppStyle::applicationPalette(mode));
    if (m_app.styleHints()) {
        QObject::connect(m_app.styleHints(), &QStyleHints::colorSchemeChanged, &m_app,
                         [this](Qt::ColorScheme) {
            if (AppStyle::followsSystemTheme()) {
                const AppStyle::ThemeMode mode = AppStyle::storedThemeMode();
                m_app.setProperty("leyochat.themeMode", AppStyle::themeModeToString(mode));
                const ElaThemeType::ThemeMode elaMode = AppStyle::toElaThemeMode(mode);
                if (eTheme->getThemeMode() != elaMode) {
                    eTheme->setThemeMode(elaMode);
                }
                m_app.setPalette(AppStyle::applicationPalette(mode));
                if (m_mainWindow) {
                    m_mainWindow->reloadAppearanceSettings();
                }
            }
        });
    }
}

void LeyoApplication::setSplashScreen(QSplashScreen* splash) {
    m_splashScreen = splash;
}

LeyoApplication::~LeyoApplication() = default;

int LeyoApplication::run() {
    const QString appDisplayName = applicationDisplayName();
    DatabaseManager databaseManager(databasePath(), QStringLiteral("leyochat-main"));
    if (!databaseManager.open()) {
        return 1;
    }

    PersistedRuntimeArchitectureLoader runtimeArchitectureLoader(QStringLiteral("leyochat-main"));
    ServiceRegistryRepository stage2RegistryRepository(QStringLiteral("leyochat-main"));
    ServiceBindingRepository stage2BindingRepository(QStringLiteral("leyochat-main"));
    ServiceResourceRepository stage2ResourceRepository(QStringLiteral("leyochat-main"));

    auto refreshRuntimeArchitectureState = [&]() {
        m_runtimeArchitectureSnapshot = runtimeArchitectureLoader.loadSnapshot();
        Diagnostics::writeRuntimeArchitectureSnapshot(Diagnostics::defaultSourcePaths(),
                                                      *m_runtimeArchitectureSnapshot);
        qInfo().noquote()
            << QStringLiteral("[stage2] persisted runtime snapshot loaded: services=%1 registry=%2 workspaces=%3 groups=%4 resources=%5 bound=%6")
                   .arg(m_runtimeArchitectureSnapshot->discoveryResult.services.size())
                   .arg(m_runtimeArchitectureSnapshot->serviceRegistry.size())
                   .arg(m_runtimeArchitectureSnapshot->workspaceBindings.size())
                   .arg(m_runtimeArchitectureSnapshot->groupBindings.size())
                   .arg(m_runtimeArchitectureSnapshot->visibleResources.size())
                   .arg(m_runtimeArchitectureSnapshot->selection.bound ? QStringLiteral("true")
                                                                      : QStringLiteral("false"));
        if (m_mainWindow) {
            m_mainWindow->setRuntimeArchitectureSummary(
                m_runtimeArchitectureSnapshot->discoveryResult.services.size(),
                m_runtimeArchitectureSnapshot->workspaceBindings.size(),
                m_runtimeArchitectureSnapshot->groupBindings.size(),
                m_runtimeArchitectureSnapshot->visibleResources.size(),
                m_runtimeArchitectureSnapshot->selection.bound,
                m_runtimeArchitectureSnapshot->selection.serviceName);
            m_mainWindow->setRuntimeArchitectureSnapshot(*m_runtimeArchitectureSnapshot);
        }
    };
    QElapsedTimer startupTimer; startupTimer.start();
    qInfo() << "[startup-perf] run() entered";
    refreshRuntimeArchitectureState();
    qInfo() << "[startup-perf] refreshRuntimeArchitectureState:" << startupTimer.elapsed() << "ms";

    const TestModeContext testModeContext = TestModeContext::current();
    const QString recoveryRuntimeDir = testModeContext.enabled
        ? testModeContext.runtimeDirectoryPath()
        : Diagnostics::defaultSourcePaths().runtimeDir;
    ClientRecoveryStateStore recoveryStateStore(
        QDir(recoveryRuntimeDir).filePath(QStringLiteral("client-recovery-state.json")));
    std::optional<ClientRecoveryState> pendingRecoveryState;
    if (m_startupOptions.validRecoveryRequest()) {
        QString recoveryError;
        pendingRecoveryState = recoveryStateStore.loadForSession(
            m_startupOptions.recoverySessionId,
            QDateTime::currentMSecsSinceEpoch(),
            &recoveryError);
        if (pendingRecoveryState.has_value()) {
            qInfo() << "[startup-recovery] state loaded for session"
                    << m_startupOptions.recoverySessionId;
        } else {
            qWarning() << "[startup-recovery] state unavailable:" << recoveryError;
        }
    }

    ProfileRepository profileRepository(QStringLiteral("leyochat-main"));
    IdentityService identityService(&profileRepository);

    auto profile = identityService.loadProfile();
    if (!profile) {
        if (testModeContext.enabled) {
            Profile devProfile;
            devProfile.clientId = testModeContext.clientId.toStdWString();
            devProfile.displayName = testModeContext.displayName.toStdWString();
            devProfile.employeeCode = testModeContext.profile.toStdWString();
            devProfile.listenPort = testModeContext.listenPort;
            profile = std::make_unique<Profile>(devProfile);
        } else {
            LoginDialog dialog;
            if (dialog.exec() != QDialog::Accepted) {
                return 0;
            }

            const auto validation = identityService.validateInput(dialog.displayName(), dialog.employeeCode());
            if (!validation.isValid) {
                QMessageBox::warning(nullptr, appDisplayName, validation.errorMessage);
                return 1;
            }

            profile = std::make_unique<Profile>(
                identityService.createProfile(dialog.displayName(), dialog.employeeCode(), 45454));
        }
        if (!identityService.saveProfile(*profile)) {
            return 1;
        }
    } else if (testModeContext.enabled) {
        bool changed = false;
        if (toQString(profile->clientId) != testModeContext.clientId) {
            profile->clientId = testModeContext.clientId.toStdWString();
            changed = true;
        }
        if (toQString(profile->displayName) != testModeContext.displayName) {
            profile->displayName = testModeContext.displayName.toStdWString();
            changed = true;
        }
        if (toQString(profile->employeeCode) != testModeContext.profile) {
            profile->employeeCode = testModeContext.profile.toStdWString();
            changed = true;
        }
        if (profile->listenPort != testModeContext.listenPort) {
            profile->listenPort = testModeContext.listenPort;
            changed = true;
        }
        if (changed && !identityService.saveProfile(*profile)) {
            return 1;
        }
    }

    qInfo() << "[startup-perf] profile+repos ready:" << startupTimer.elapsed() << "ms";
    ConversationRepository conversationRepository(QStringLiteral("leyochat-main"));
    FileTransferRepository fileTransferRepository(QStringLiteral("leyochat-main"));
    ReminderRepository reminderRepository(QStringLiteral("leyochat-main"));
    FileTransferService fileTransferService(&fileTransferRepository);
    TransferSpeedTracker transferSpeedTracker;
    GroupFileTransferService groupFileTransferService(m_mainWindow.get());
    GroupRepository groupRepository(QStringLiteral("leyochat-main"));
    GroupService groupService(&groupRepository, &conversationRepository);
    const QString localClientId = toQString(profile->clientId);
    QString localDisplayName = toQString(profile->displayName);
    const QString localAppVersion = ApplicationInfo::currentVersion();
    const QStringList localRoutingCapabilities = localMessageRoutingCapabilities();
    PeerDirectoryService peerDirectoryService;
    ConversationListModel conversationModel;
    ContactListModel contactModel;
    MessageListModel messageModel;
    TransferListModel transferModel;

    // ──── 响应式数据层 ────
    ChatDataStore chatDataStore;
    chatDataStore.setLocalClientId(localClientId);
    QThread dbWorkerThread;
    dbWorkerThread.setObjectName(QStringLiteral("DatabaseWorkerThread"));
    DatabaseWorker* dbWorker = new DatabaseWorker(databasePath());
    dbWorker->setLocalClientId(localClientId);
    dbWorker->moveToThread(&dbWorkerThread);
    QObject::connect(&dbWorkerThread, &QThread::finished,
                     dbWorker, &QObject::deleteLater);

    // 文件接收工作线程：chunk 写盘 + DB 记录移出主线程
    qRegisterMetaType<FileTransferChunkHeader>("FileTransferChunkHeader");
    qRegisterMetaType<FileTransferTask>("FileTransferTask");
    qRegisterMetaType<std::vector<int>>("std::vector<int>");
    QThread fileReceiveThread;
    fileReceiveThread.setObjectName(QStringLiteral("FileReceiveWorkerThread"));
    FileReceiveWorker* fileReceiveWorker = new FileReceiveWorker(databasePath());
    fileReceiveWorker->moveToThread(&fileReceiveThread);
    QObject::connect(&fileReceiveThread, &QThread::finished,
                     fileReceiveWorker, &QObject::deleteLater);
    fileReceiveThread.start();

    CallSession callSession;
    CallSoundPlayer callSoundPlayer;
    std::unique_ptr<AudioChannel> audioChannel;
    std::unique_ptr<ScreenCaptureChannel> screenCapture;
    std::unique_ptr<ScreenViewerWidget> screenViewer;
    RemoteInputChannel remoteInput;
    QHostAddress pendingRemoteAudioHost;
    quint16 pendingRemoteAudioPort = 0;
    PeerSessionManager sessionManager(localClientId);
    PeerServer peerServer(localClientId);
    FileTransferServer fileTransferServer;

    quint16 activeListenPort = profile->listenPort;
    bool listening = peerServer.listen(QHostAddress::AnyIPv4, activeListenPort);
    if (!listening) {
        listening = peerServer.listen(QHostAddress::AnyIPv4, 0);
        if (listening) {
            activeListenPort = peerServer.serverPort();
        }
    }
    if (!listening) {
        QMessageBox::warning(nullptr,
                             appDisplayName,
                             QStringLiteral("\u65E0\u6CD5\u542F\u52A8\u672C\u5730 TCP \u76D1\u542C\u3002"));
    } else if (activeListenPort != profile->listenPort) {
        profile->listenPort = activeListenPort;
        identityService.saveProfile(*profile);
    }

    quint16 activeFileTransferPort = activeListenPort < 65535 ? static_cast<quint16>(activeListenPort + 1) : 0;
    bool fileTransferListening = fileTransferServer.listen(QHostAddress::AnyIPv4, activeFileTransferPort);
    if (!fileTransferListening) {
        fileTransferListening = fileTransferServer.listen(QHostAddress::AnyIPv4, 0);
        if (fileTransferListening) {
            activeFileTransferPort = fileTransferServer.serverPort();
        }
    }

    qInfo() << "[startup-perf] pre-MainWindow:" << startupTimer.elapsed() << "ms";
    logUserObjects("pre-MainWindow");
    m_mainWindow = std::make_unique<MainWindow>();
    logUserObjects("post-MainWindow");
    qInfo() << "[startup-perf] MainWindow constructed:" << startupTimer.elapsed() << "ms";
    m_mainWindow->setConversationModel(&conversationModel);
    m_mainWindow->setLocalDisplayName(localDisplayName);
    m_mainWindow->setContactModel(&contactModel);
    m_mainWindow->setMessageModel(&messageModel);
    m_mainWindow->setTransferModel(&transferModel);
    m_mainWindow->setListenPort(activeListenPort);
    m_mainWindow->setRuntimeArchitectureSummary(
        m_runtimeArchitectureSnapshot ? m_runtimeArchitectureSnapshot->discoveryResult.services.size() : 0,
        m_runtimeArchitectureSnapshot ? m_runtimeArchitectureSnapshot->workspaceBindings.size() : 0,
        m_runtimeArchitectureSnapshot ? m_runtimeArchitectureSnapshot->groupBindings.size() : 0,
        m_runtimeArchitectureSnapshot ? m_runtimeArchitectureSnapshot->visibleResources.size() : 0,
        m_runtimeArchitectureSnapshot ? m_runtimeArchitectureSnapshot->selection.bound : false,
        m_runtimeArchitectureSnapshot ? m_runtimeArchitectureSnapshot->selection.serviceName : QString());
    if (m_runtimeArchitectureSnapshot) {
        m_mainWindow->setRuntimeArchitectureSnapshot(*m_runtimeArchitectureSnapshot);
    }
    qRegisterMetaType<ReminderItem>("ReminderItem");
    ReminderService reminderService(&reminderRepository, m_mainWindow.get());

    // 灞€鍩熺綉鑷姩鍙戠幇
    LanDiscoveryService lanDiscovery;
    RecipientCapabilityResolver recipientCapabilityResolver;
    if (listening) {
        lanDiscovery.start(localClientId,
                           localDisplayName,
                           activeListenPort,
                           0,
                           localAppVersion,
                           localRoutingCapabilities);
        qInfo() << "[startup-perf] lanDiscovery started:" << startupTimer.elapsed() << "ms";
    }

    QHash<QString, QPointer<PeerConnection>> connectionsByTargetId;
    ConnectionRegistryUtils::ConnectionIdentityRegistry<PeerConnection> peerIdsByConnection;
    QHash<QString, QString> resolvedTargetIdsByAlias;
    QHash<QString, QString> peerSignatures;
    QHash<QString, bool> peerDeliveryReceiptCapabilities;
    QHash<QString, PeerProfileInfo> peerProfiles;
    auto appShuttingDown = std::make_shared<bool>(false);

    // ──── ChatDataStore 解析器 + ViewModel 绑定 ────
    chatDataStore.setDisplayNameResolver([&](const QString& clientId) -> QString {
        if (clientId == localClientId) return localDisplayName;
        const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(clientId));
        return peer.has_value() ? displayNameForPeer(*peer) : clientId;
    });
    chatDataStore.setAvatarPathResolver([&](const QString& clientId) -> QString {
        return cachedAvatarPathForClient(clientId);
    });
    chatDataStore.setOnlineChecker([&](const QString& clientId) -> bool {
        if (clientId == localClientId) return true;
        const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(clientId));
        return peer.has_value()
            && PeerPresenceEvaluator::isOnlineOrAway(
                *peer,
                QDateTime::currentMSecsSinceEpoch());
    });
    conversationModel.bindToStore(&chatDataStore);
    messageModel.bindToStore(&chatDataStore);
    contactModel.bindToStore(&chatDataStore);

    QObject::connect(m_mainWindow.get(), &MainWindow::voiceCallRequested, m_mainWindow.get(),
                     [&](const QString& peerId) {
                         QString normalizedPeerId = peerId.trimmed();
                         if (normalizedPeerId.isEmpty()) {
                             return;
                         }
                         // 会话列表传入的可能是 "clientA|clientB" 格式的 conversationId，
                         // 需要提取对端的 clientId 才能在 connectionsByTargetId 中查找
                         if (normalizedPeerId.contains('|')) {
                             normalizedPeerId = DirectConversationAddressing::otherParticipant(
                                 localClientId, normalizedPeerId);
                             if (normalizedPeerId.isEmpty()) {
                                 return;
                             }
                         }
                         if (!connectionsByTargetId.contains(normalizedPeerId)
                             || !connectionsByTargetId.value(normalizedPeerId)
                             || !connectionsByTargetId.value(normalizedPeerId)->isConnected()) {
                             m_mainWindow->setStatusMessage(QStringLiteral("对方未在线，无法发起通话"), 2500);
                             return;
                         }
                         const QString callId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                         if (!callSession.startCall(callId,
                                                    normalizedPeerId,
                                                    static_cast<int>(CallMediaFlag::Audio))) {
                             m_mainWindow->setStatusMessage(QStringLiteral("当前已有进行中的通话"), 2500);
                             return;
                         }
                         const auto calleePeer = peerDirectoryService.findPeerByClientId(toUtf8(normalizedPeerId));
                         const QString calleeName = calleePeer.has_value() ? displayNameForPeer(*calleePeer) : normalizedPeerId;
                         m_mainWindow->setStatusMessage(QStringLiteral("正在呼叫 %1...").arg(calleeName), 2500);
                     });

    QObject::connect(m_mainWindow.get(), &MainWindow::callAnswered, m_mainWindow.get(),
                     [&](const QString& /*callId*/) {
                         callSession.answerCall();
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::callRejected, m_mainWindow.get(),
                     [&](const QString& /*callId*/) {
                         callSession.rejectCall(QStringLiteral("user_reject"));
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::callCancelRequested, m_mainWindow.get(),
                     [&]() {
                         callSession.cancelCall();
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::callHungUp, m_mainWindow.get(),
                     [&]() {
                         callSession.hangup();
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::callMuteToggled, m_mainWindow.get(),
                     [&](bool muted) {
                         callSession.setAudioMuted(muted);
                     });

    QObject::connect(&callSession, &CallSession::outgoingSignal, m_mainWindow.get(),
                     [&](const CallControlPayload& payload) {
                         QString targetId = QString::fromStdString(payload.targetId).trimmed();
                         if (targetId.isEmpty()) {
                             targetId = callSession.peerId().trimmed();
                         }
                         if (targetId.isEmpty()) {
                             return;
                         }

                         PeerConnection* connection = connectionsByTargetId.value(targetId, nullptr);
                         if (!connection || !connection->isConnected()) {
                             return;
                         }

                         MessageEnvelope envelope;
                         envelope.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
                         envelope.type = MessageType::CallControl;
                         envelope.senderId = localClientId.toStdString();
                         envelope.targetId = targetId.toStdString();
                         envelope.controlType = callControlTypeToWire(payload.type).toStdString();
                         envelope.payloadJson = buildCallPayloadJson(payload).toStdString();
                         envelope.reason = payload.reason;
                         envelope.fileTaskId = payload.callId;
                         envelope.createdAtMs = QDateTime::currentMSecsSinceEpoch();

                         connection->sendPayload(QByteArray::fromStdString(MessageCodec::encode(envelope)));
                     });

    QObject::connect(&callSession, &CallSession::stateChanged, m_mainWindow.get(),
                     [&](CallSession::State state) {
                         switch (state) {
                         case CallSession::State::OutgoingRing: {
                             const QString peerId = callSession.peerId();
                             const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(peerId));
                             const QString calleeName = peer.has_value() ? displayNameForPeer(*peer) : peerId;
                             m_mainWindow->showCallWindow(true, calleeName);
                             callSoundPlayer.playRingback();
                             break;
                         }
                         case CallSession::State::IncomingRing: {
                             const QString peerId = callSession.peerId();
                             const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(peerId));
                             const QString callerName = peer.has_value() ? displayNameForPeer(*peer) : peerId;
                             m_mainWindow->showCallWindow(false, callerName);
                             callSoundPlayer.playRingtone();
                             break;
                         }
                         case CallSession::State::Connecting: {
                             callSoundPlayer.stopAll();
                             m_mainWindow->updateCallWindowState(state);
                             m_mainWindow->setStatusMessage(QStringLiteral("正在建立通话连接..."), 1500);
                             // 创建 AudioChannel 并绑定 UDP 端口
                             audioChannel = std::make_unique<AudioChannel>();
                             quint16 udpPort = audioChannel->bind();
                             if (udpPort > 0) {
                                 CallControlPayload ready;
                                 ready.type = CallControlType::MediaReady;
                                 ready.callId = callSession.callId().toStdString();
                                 ready.targetId = callSession.peerId().toStdString();
                                 ready.audioUdpPort = udpPort;
                                 emit callSession.outgoingSignal(ready);
                             }
                             break;
                         }
                         case CallSession::State::Active: {
                             callSoundPlayer.stopAll();
                             m_mainWindow->updateCallWindowState(state);
                             m_mainWindow->setStatusMessage(QStringLiteral("通话已建立"), 1200);
                             if (audioChannel) {
                                 // 应用竞态期间缓存的远端地址
                                 if (pendingRemoteAudioPort > 0) {
                                     audioChannel->setRemoteEndpoint(pendingRemoteAudioHost, pendingRemoteAudioPort);
                                     pendingRemoteAudioHost.clear();
                                     pendingRemoteAudioPort = 0;
                                 }
                                 audioChannel->start();
                             }
                             break;
                         }
                         case CallSession::State::Ended:
                         case CallSession::State::Idle:
                             callSoundPlayer.stopAll();
                             if (audioChannel) {
                                 audioChannel->stop();
                                 audioChannel.reset();
                             }
                             if (screenCapture) {
                                 screenCapture->stop();
                                 screenCapture.reset();
                             }
                             if (screenViewer) {
                                 screenViewer->close();
                                 screenViewer.reset();
                             }
                             remoteInput.setEnabled(false);
                             m_mainWindow->updateCallWindowState(state);
                             break;
                         default:
                             break;
                         }
                     });

    QObject::connect(&callSession, &CallSession::callError, m_mainWindow.get(),
                     [&](const QString& reason) {
                         callSoundPlayer.stopAll();
                         callSoundPlayer.playHangup();
                         if (reason == QStringLiteral("peer_rejected")) {
                             m_mainWindow->setStatusMessage(QStringLiteral("对方已拒绝通话"), 3000);
                         } else if (reason == QStringLiteral("peer_busy")) {
                             m_mainWindow->setStatusMessage(QStringLiteral("对方正忙，请稍后再试"), 3000);
                         } else if (reason == QStringLiteral("no_answer")) {
                             m_mainWindow->setStatusMessage(QStringLiteral("对方无人接听"), 3000);
                         } else if (reason == QStringLiteral("cancelled")) {
                             m_mainWindow->setStatusMessage(QStringLiteral("已取消呼叫"), 2000);
                         } else {
                             m_mainWindow->setStatusMessage(QStringLiteral("通话失败：%1").arg(reason), 3000);
                         }
                     });

    QObject::connect(&callSession, &CallSession::audioMutedChanged, m_mainWindow.get(),
                     [&](bool muted) {
                         m_mainWindow->setCallWindowMuted(muted);
                         if (audioChannel) {
                             audioChannel->setMuted(muted);
                         }
                     });
    QObject::connect(&callSession, &CallSession::mediaChannelReady, m_mainWindow.get(),
                     [&](quint16 audioUdpPort, quint16 /*screenTcpPort*/) {
                         const QString peerId = callSession.peerId();
                         PeerConnection* conn = connectionsByTargetId.value(peerId, nullptr);
                         if (!conn) return;
                         const QHostAddress remoteHost(normalizeHost(conn->peerHost()));
                         if (audioChannel) {
                             audioChannel->setRemoteEndpoint(remoteHost, audioUdpPort);
                         } else {
                             // audioChannel 尚未创建（竞态），缓存远端信息
                             pendingRemoteAudioHost = remoteHost;
                             pendingRemoteAudioPort = audioUdpPort;
                         }
                     });
    QObject::connect(&callSession, &CallSession::screenShareChanged, m_mainWindow.get(),
                     [&](bool sharing) {
                         // 更新 CallWindow 共享按钮状态
                         if (m_mainWindow && m_mainWindow->callWindow()) {
                             m_mainWindow->callWindow()->setScreenSharing(sharing || callSession.isScreenSharing());
                         }
                         if (sharing) {
                             // 对端开始共享 → 打开观看窗口
                             // （自己发起共享在 screenShareToggled 信号中处理）
                         } else {
                             if (screenViewer) {
                                 screenViewer->close();
                                 screenViewer.reset();
                             }
                             if (screenCapture) {
                                 screenCapture->stop();
                                 screenCapture.reset();
                             }
                             remoteInput.setEnabled(false);
                         }
                     });
    QObject::connect(&callSession, &CallSession::remoteControlRequested, m_mainWindow.get(),
                     [&](const QString& peerId) {
                         const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(peerId));
                         const QString peerName = peer.has_value() ? displayNameForPeer(*peer) : peerId;
                         qInfo() << "[RemoteControl] request received from" << peerId << "name=" << peerName;

                         const QString bodyText = QStringLiteral("<b>%1</b> 请求控制你的桌面\n\n对方将能够操作你的鼠标和键盘，是否允许？")
                                                      .arg(peerName.toHtmlEscaped());
                         ElaContentDialog rcDlg(m_mainWindow.get());
                         rcDlg.setWindowTitle(QStringLiteral("远程控制请求"));
                         rcDlg.setLeftButtonText(QStringLiteral("拒绝"));
                         rcDlg.setMiddleButtonText(QString());
                         rcDlg.setRightButtonText(QStringLiteral("允许"));
                         auto* rcContent = new QWidget(&rcDlg);
                         auto* rcLayout = new QVBoxLayout(rcContent);
                         rcLayout->setContentsMargins(24, 16, 24, 16);
                         auto* rcLabel = new QLabel(bodyText, rcContent);
                         rcLabel->setWordWrap(true);
                         rcLabel->setTextFormat(Qt::RichText);
                         rcLayout->addWidget(rcLabel);
                         rcDlg.setCentralWidget(rcContent);
                         bool rcAllowed = false;
                         QObject::connect(&rcDlg, &ElaContentDialog::rightButtonClicked, &rcDlg, [&]() {
                             rcAllowed = true;
                         });
                         rcDlg.exec();
                         if (rcAllowed) {
                             qInfo() << "[RemoteControl] granted to" << peerId;
                             callSession.grantRemoteControl();
                             remoteInput.setEnabled(true);
                         } else {
                             qInfo() << "[RemoteControl] denied to" << peerId;
                         }
                     });
    QObject::connect(&callSession, &CallSession::remoteControlChanged, m_mainWindow.get(),
                     [&](bool active) {
                         qInfo() << "[RemoteControl] changed: active=" << active
                                 << "isScreenSharing=" << callSession.isScreenSharing()
                                 << "hasViewer=" << (screenViewer != nullptr);
                         // 只在共享端（自己在共享桌面）启用输入注入
                         if (callSession.isScreenSharing()) {
                             remoteInput.setEnabled(active);
                         }
                         if (screenViewer) {
                             screenViewer->setRemoteControlEnabled(active);
                         }
                         // 更新 CallWindow 控制按钮文案
                         if (m_mainWindow && m_mainWindow->callWindow()) {
                             m_mainWindow->callWindow()->setRemoteControlActive(active);
                         }
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::screenShareToggled, m_mainWindow.get(),
                     [&]() {
                         qInfo() << "[ScreenShare] screenShareToggled fired, callState="
                                 << static_cast<int>(callSession.state())
                                 << "isSharing=" << callSession.isScreenSharing();
                         if (callSession.state() != CallSession::State::Active) return;
                         if (callSession.isScreenSharing()) {
                             callSession.stopScreenShare();
                         } else {
                             // 多屏场景：弹窗让用户选择要共享的屏幕
                             int screenIndex = 0;
                             const auto screens = QGuiApplication::screens();
                             if (screens.size() > 1) {
                                 QStringList items;
                                 for (int i = 0; i < screens.size(); ++i) {
                                     const QRect geo = screens[i]->geometry();
                                     items << QStringLiteral("屏幕 %1  (%2x%3)")
                                                  .arg(i + 1)
                                                  .arg(geo.width())
                                                  .arg(geo.height());
                                 }
                                 bool ok = false;
                                 QWidget* dialogParent = m_mainWindow->callWindow()
                                     ? static_cast<QWidget*>(m_mainWindow->callWindow())
                                     : static_cast<QWidget*>(m_mainWindow.get());
                                 const QString chosen = LeyoDialog::getItem(
                                     dialogParent,
                                     QStringLiteral("选择共享屏幕"),
                                     QStringLiteral("请选择要共享的屏幕："),
                                     items, 0, false, &ok);
                                 if (!ok) return;
                                 screenIndex = items.indexOf(chosen);
                                 if (screenIndex < 0) screenIndex = 0;
                             }

                             screenCapture = std::make_unique<ScreenCaptureChannel>();
                             screenCapture->setTargetScreen(screenIndex);
                             quint16 tcpPort = screenCapture->startServer();
                             qInfo() << "[ScreenShare] startServer tcpPort=" << tcpPort;
                             if (tcpPort > 0) {
                                 callSession.startScreenShare();
                                 // 发送 ScreenShareStart 信令携带 TCP 端口
                                 CallControlPayload ssPayload;
                                 ssPayload.type = CallControlType::ScreenShareStart;
                                 ssPayload.callId = callSession.callId().toStdString();
                                 ssPayload.targetId = callSession.peerId().toStdString();
                                 ssPayload.screenTcpPort = tcpPort;
                                 emit callSession.outgoingSignal(ssPayload);
                                 qInfo() << "[ScreenShare] outgoingSignal emitted, port=" << tcpPort
                                         << "target=" << callSession.peerId();
                             }
                         }
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::remoteControlToggled, m_mainWindow.get(),
                     [&]() {
                         if (callSession.state() != CallSession::State::Active) return;
                         if (callSession.isRemoteControlActive()) {
                             callSession.revokeRemoteControl();
                             qInfo() << "[RemoteControl] revoked by local user";
                             m_mainWindow->setStatusMessage(QStringLiteral("已撤销远程控制"), 1500);
                         } else {
                             // 只有对方在共享屏幕时才能请求控制
                             if (!screenViewer) {
                                 m_mainWindow->setStatusMessage(
                                     QStringLiteral("对方未共享桌面，无法请求控制"), 2500);
                                 return;
                             }
                             callSession.requestRemoteControl();
                             qInfo() << "[RemoteControl] request sent to" << callSession.peerId();
                             m_mainWindow->setStatusMessage(QStringLiteral("已向对方发送远程控制请求，等待确认..."), 3000);
                         }
                     });
    QHash<QString, FileTransferConnection*> activeFileConnectionsByTaskId;
    QString currentConversationId;
    QString currentTargetId;
    PeerPresenceStatus localPresence = PeerPresenceStatus::Online;

    QTimer recoverySnapshotTimer(m_mainWindow.get());
    recoverySnapshotTimer.setSingleShot(true);
    recoverySnapshotTimer.setInterval(350);
    ClientRecoveryState lastRecoveryPayload;
    bool hasLastRecoveryPayload = false;
    qint64 lastRecoveryWriteAtMs = 0;
    const auto captureRecoveryState = [&]() {
        const QString sessionId = m_startupOptions.stateSessionId();
        if (sessionId.isEmpty() || !m_mainWindow) {
            return;
        }

        ClientRecoveryState state;
        state.sessionId = sessionId;
        if (!m_mainWindow->isVisible()) {
            state.windowMode = RecoveryWindowMode::TrayHidden;
        } else if (m_mainWindow->isMinimized()) {
            state.windowMode = RecoveryWindowMode::Minimized;
        }
        state.windowGeometry = m_mainWindow->saveGeometry();
        state.windowMaximized = m_mainWindow->isMaximized();
        state.navigationPageId = m_mainWindow->recoveryPageId();
        state.conversationId = m_mainWindow->activeComposerContextId();
        if (!state.conversationId.isEmpty()) {
            const ComposerRecoveryContext composer =
                m_mainWindow->activeComposerRecoveryContext();
            state.composerHtml = composer.composerHtml;
            state.replyMessageId = composer.replyMessageId;
            state.replySenderId = composer.replySenderId;
            state.replySenderName = composer.replySenderName;
            state.replyBody = composer.replyBody;
            state.editingMessageId = composer.editingMessageId;
            state.editingBody = composer.editingBody;
        }

        const bool payloadChanged = !hasLastRecoveryPayload || state != lastRecoveryPayload;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (!ClientRecoveryStateStore::shouldPersistSnapshot(
                payloadChanged, lastRecoveryWriteAtMs, nowMs)) {
            return;
        }
        state.savedAtMs = nowMs;
        QString saveError;
        if (!recoveryStateStore.save(state, &saveError)) {
            qWarning() << "[startup-recovery] state save failed:" << saveError;
            return;
        }
        state.savedAtMs = 0;
        lastRecoveryPayload = std::move(state);
        hasLastRecoveryPayload = true;
        lastRecoveryWriteAtMs = nowMs;
    };
    const auto scheduleRecoverySnapshot = [&]() {
        recoverySnapshotTimer.start();
    };
    QObject::connect(&recoverySnapshotTimer, &QTimer::timeout,
                     m_mainWindow.get(), captureRecoveryState);
    QObject::connect(m_mainWindow.get(), &MainWindow::composerRecoveryContextChanged,
                     m_mainWindow.get(), scheduleRecoverySnapshot);
    QObject::connect(m_mainWindow.get(), &MainWindow::composerRecoveryContextCommitted,
                     m_mainWindow.get(), captureRecoveryState);
    QTimer recoveryHeartbeatTimer(m_mainWindow.get());
    recoveryHeartbeatTimer.setInterval(
        ClientRecoveryStateStore::snapshotRefreshIntervalMs());
    QObject::connect(&recoveryHeartbeatTimer, &QTimer::timeout,
                     m_mainWindow.get(), captureRecoveryState);
    recoveryHeartbeatTimer.start();
    QObject::connect(m_mainWindow.get(), &ElaWindow::navigationNodeClicked,
                     m_mainWindow.get(),
                     [scheduleRecoverySnapshot](ElaNavigationType::NavigationNodeType,
                                                const QString&) {
                         scheduleRecoverySnapshot();
                     });

    const QIcon trayNormalIcon(QStringLiteral(":/app/leyochat-icon.png"));
    QIcon trayBlankIcon;
    {
        QPixmap blankPix(32, 32);
        blankPix.fill(Qt::transparent);
        trayBlankIcon.addPixmap(blankPix);
    }
    auto trayIcon = std::make_unique<QSystemTrayIcon>(trayNormalIcon);
    bool trayBlinkState = false;
    auto* trayBlinkTimer = new QTimer(&m_app);
    trayBlinkTimer->setInterval(500);
    QObject::connect(trayBlinkTimer, &QTimer::timeout, m_mainWindow.get(), [&]() {
        trayBlinkState = !trayBlinkState;
        trayIcon->setIcon(trayBlinkState ? trayBlankIcon : trayNormalIcon);
    });
    trayIcon->setToolTip(
        IntegrationNotificationPresentation::trayUnreadToolTip(appDisplayName, 0));
    auto* trayMenu = new QMenu();
    // ── 托盘菜单样式（修复深色模式下菜单全黑不可见） ──
    const auto applyTrayMenuStyle = [trayMenu]() {
        const auto mode = AppStyle::currentThemeMode();
        trayMenu->setStyleSheet(QStringLiteral(
            "QMenu { background-color: %1; border: 1px solid %2; padding: 4px 0px; }"
            "QMenu::item { color: %3; padding: 6px 24px; }"
            "QMenu::item:selected { background-color: %4; }"
            "QMenu::item:disabled { color: %5; }"
            "QMenu::separator { height: 1px; background: %2; margin: 4px 8px; }")
            .arg(AppStyle::surface(mode),
                 AppStyle::border(mode),
                 AppStyle::textPrimary(mode),
                 AppStyle::isDarkTheme(mode) ? QStringLiteral("rgba(255,255,255,15)")
                                             : QStringLiteral("rgba(0,0,0,10)"),
                 AppStyle::textSecondary(mode)));
    };
    applyTrayMenuStyle();
    QObject::connect(eTheme, &ElaTheme::themeModeChanged, trayMenu, applyTrayMenuStyle);
    auto* unreadStatusAction = trayMenu->addAction(QStringLiteral("当前没有未处理提醒"));
    unreadStatusAction->setEnabled(false);
    trayMenu->addSeparator();
    auto* openAction = trayMenu->addAction(QStringLiteral("\u6253\u5f00 %1").arg(appDisplayName));
    auto* quitAction = trayMenu->addAction(QStringLiteral("\u9000\u51fa %1").arg(appDisplayName));
    trayIcon->setContextMenu(trayMenu);
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        trayIcon->show();
    }

    // ── 全局截图热键 ──────────────────────────────────────────
    auto* globalHotkeyManager = new GlobalHotkeyManager(&m_app);
    {
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        const QString hotkeyStr = cfg.value(QStringLiteral("screenshotHotkey"),
                                            QStringLiteral("Ctrl+Alt+A")).toString();
        const QKeySequence seq(hotkeyStr);
        if (!seq.isEmpty()) {
            if (!globalHotkeyManager->registerHotkey(seq)) {
                qWarning() << "[app] screenshot hotkey registration failed:" << hotkeyStr;
                if (QSystemTrayIcon::isSystemTrayAvailable()) {
                    trayIcon->showMessage(
                        appDisplayName,
                        QStringLiteral("截图快捷键 %1 注册失败，可能已被其他程序占用。\n"
                                       "请在 个人设置 → 高级设置 中修改快捷键。").arg(hotkeyStr),
                        QSystemTrayIcon::Warning,
                        5000);
                }
            }
        }
    }
    QObject::connect(globalHotkeyManager, &GlobalHotkeyManager::hotkeyTriggered,
                     m_mainWindow.get(), [&]() {
        if (QWidget* modal = QApplication::activeModalWidget()) {
            if (modal->objectName() == QStringLiteral("closeToTrayDialog")) {
                qInfo() << "[app] screenshot hotkey closes close-to-tray dialog before capture";
                QMetaObject::invokeMethod(modal, "reject", Qt::QueuedConnection);
                QTimer::singleShot(120, m_mainWindow.get(), [this]() {
                    if (m_mainWindow) {
                        m_mainWindow->triggerScreenshot(false);
                    }
                });
                return;
            }
            // 其他模态对话框（如存储管理）也允许截图
            qInfo() << "[app] screenshot hotkey triggered while modal dialog is active";
        }
        if (m_mainWindow) {
            m_mainWindow->triggerScreenshot(false);
        }
    });

    int unreadReminderCount = 0;
    int systemUnreadReminderCount = 0;
    QString lastUnreadTitle;
    std::function<void()> flushDeferredTransferRefreshIfVisible;
    int uiRestoreSequence = 0;
    QElapsedTimer uiRestoreElapsed;
    const auto syncUiRestoreTraceProperties = [&]() {
        const bool active = uiRestoreElapsed.isValid()
            && uiRestoreElapsed.elapsed() <= kUiRestoreTraceWindowMs;
        m_app.setProperty("leyochat.uiRestoreTraceEnabled", active);
        m_app.setProperty("leyochat.uiRestoreTraceSequence", uiRestoreSequence);
        return active;
    };
    const auto logCurrentUiRestoreTrace = [&](const QString& phase, const QString& detail = QString()) {
        if (!syncUiRestoreTraceProperties()) {
            return;
        }
        const QString elapsedDetail = uiRestoreElapsed.isValid()
            ? QStringLiteral("elapsedMs=%1").arg(QString::number(uiRestoreElapsed.elapsed()))
            : QStringLiteral("elapsedMs=na");
        const QString mergedDetail = detail.trimmed().isEmpty()
            ? elapsedDetail
            : QStringLiteral("%1 %2").arg(elapsedDetail, detail.trimmed());
        logUiRestoreTrace(uiRestoreSequence, phase, mergedDetail);
    };
    const auto updateTrayUnreadPresentation = [&]() {
        const int totalUnreadReminderCount = unreadReminderCount + systemUnreadReminderCount;
        if (totalUnreadReminderCount <= 0) {
            trayIcon->setToolTip(
                IntegrationNotificationPresentation::trayUnreadToolTip(appDisplayName, 0));
            unreadStatusAction->setText(
                IntegrationNotificationPresentation::trayUnreadStatusActionText(QString(), 0));
            // 未读数归零时立即停止闪烁，避免托盘图标空转
            if (trayBlinkTimer->isActive()) {
                trayBlinkTimer->stop();
                trayBlinkState = false;
                trayIcon->setIcon(trayNormalIcon);
            }
            return;
        }
        trayIcon->setToolTip(IntegrationNotificationPresentation::trayUnreadToolTip(
            appDisplayName,
            totalUnreadReminderCount));
        unreadStatusAction->setText(
            IntegrationNotificationPresentation::trayUnreadStatusActionText(
                lastUnreadTitle,
                totalUnreadReminderCount));
    };
    const auto clearTrayUnreadPresentation = [&]() {
        unreadReminderCount = 0;
        systemUnreadReminderCount = 0;
        lastUnreadTitle.clear();
        trayBlinkTimer->stop();
        trayBlinkState = false;
        trayIcon->setIcon(trayNormalIcon);
        updateTrayUnreadPresentation();
    };
    syncUiRestoreTraceProperties();

    const auto showCurrentReleaseNotes = [&](const QString& title,
                                             const QString& subtitle) {
        showCurrentReleaseNotesDialog(m_mainWindow.get(), title, subtitle);
    };

    const auto showAboutDialog = [&]() {
        showAboutDialogWindow(m_mainWindow.get(), appDisplayName);
    };

    const auto restoreMainWindow = [&]() {
        if (!m_mainWindow) {
            return;
        }
        ++uiRestoreSequence;
        uiRestoreElapsed.restart();
        syncUiRestoreTraceProperties();
        QTimer::singleShot(static_cast<int>(kUiRestoreTraceWindowMs + 250), m_mainWindow.get(), [&]() {
            syncUiRestoreTraceProperties();
        });
        logCurrentUiRestoreTrace(QStringLiteral("begin"), windowStateSummary(m_mainWindow.get()));
        // 仅去除 Minimized 标志，保留 Maximized 等其余状态；
        // 比原来 6 次冗余调用精简为 3 次，减少 re-layout / DWM 刷新
        m_mainWindow->setWindowState(m_mainWindow->windowState() & ~Qt::WindowMinimized);
        m_mainWindow->show();
        m_mainWindow->raise();
        m_mainWindow->activateWindow();
#ifdef LEYOCHAT_HAS_WEBENGINE
        // 默认不再强制预热，避免应用运行中随机主线程卡顿。
        // 如需预热可通过 LEYOCHAT_WEBENGINE_WARMUP_MS 设置延迟毫秒数（>=0 生效）。
        bool warmupEnvOk = false;
        const int warmupDelayMs = qEnvironmentVariableIntValue("LEYOCHAT_WEBENGINE_WARMUP_MS", &warmupEnvOk);
        if (warmupEnvOk && warmupDelayMs >= 0) {
            QTimer::singleShot(warmupDelayMs, &m_app, [&]() {
                qInfo() << "[startup-perf] WebEngine warmUp starting:" << startupTimer.elapsed() << "ms";
                OnlineEditorWidget::warmUp();
                qInfo() << "[startup-perf] WebEngine warmUp done:" << startupTimer.elapsed() << "ms";
            });
        }
#endif
        logCurrentUiRestoreTrace(QStringLiteral("qt-restore-complete"),
                                 windowStateSummary(m_mainWindow.get()));
#ifdef Q_OS_WIN
        const HWND hwnd = reinterpret_cast<HWND>(m_mainWindow->winId());
        if (hwnd) {
            // showNormal() 已完成 SW_RESTORE，此处只做前台激活和停止闪烁
            SetForegroundWindow(hwnd);
            BringWindowToTop(hwnd);
            FLASHWINFO flashInfo{};
            flashInfo.cbSize = sizeof(FLASHWINFO);
            flashInfo.hwnd = hwnd;
            flashInfo.dwFlags = FLASHW_STOP;
            FlashWindowEx(&flashInfo);
        }
        logCurrentUiRestoreTrace(QStringLiteral("win32-restore-complete"),
                                 QStringLiteral("hwnd=%1")
                                     .arg(hwnd ? QStringLiteral("valid") : QStringLiteral("null")));
#endif
        if (flushDeferredTransferRefreshIfVisible) {
            logCurrentUiRestoreTrace(QStringLiteral("deferred-refresh-armed"),
                                     QStringLiteral("delayMs=180"));
            QTimer::singleShot(180, m_mainWindow.get(), [&]() {
                if (flushDeferredTransferRefreshIfVisible) {
                    logCurrentUiRestoreTrace(QStringLiteral("deferred-refresh-fired"));
                    flushDeferredTransferRefreshIfVisible();
                }
            });
        }
        clearTrayUnreadPresentation();
        logCurrentUiRestoreTrace(QStringLiteral("end"));
    };
    const auto shakeMainWindow = [&]() {
        if (!m_mainWindow) {
            return;
        }
        restoreMainWindow();
#ifdef Q_OS_WIN
        const HWND hwnd = reinterpret_cast<HWND>(m_mainWindow->winId());
        if (hwnd) {
            FLASHWINFO flashInfo{};
            flashInfo.cbSize = sizeof(FLASHWINFO);
            flashInfo.hwnd = hwnd;
            flashInfo.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
            flashInfo.uCount = 3;
            flashInfo.dwTimeout = 0;
            FlashWindowEx(&flashInfo);
        }
#endif
        if (m_mainWindow->isMaximized()) {
            QApplication::alert(m_mainWindow.get(), 0);
            return;
        }
        const QPoint originalPos = m_mainWindow->pos();
        const QList<QPoint> offsets{
            QPoint(-12, 0),
            QPoint(12, 0),
            QPoint(-9, 0),
            QPoint(9, 0),
            QPoint(-5, 0),
            QPoint(5, 0),
            QPoint(0, 0)
        };
        auto* tick = new int(0);
        QPointer<MainWindow> safeWindow = m_mainWindow.get();
        const auto step = std::make_shared<std::function<void()>>();
        *step = [safeWindow, originalPos, offsets, tick, step]() {
            if (!safeWindow) {
                delete tick;
                return;
            }
            if (*tick >= offsets.size()) {
                safeWindow->move(originalPos);
                delete tick;
                return;
            }
            safeWindow->move(originalPos + offsets.at(*tick));
            ++(*tick);
            QTimer::singleShot(36, safeWindow, [step]() { (*step)(); });
        };
        (*step)();
    };
    QObject::connect(openAction, &QAction::triggered, m_mainWindow.get(), restoreMainWindow);
    QObject::connect(quitAction, &QAction::triggered, m_mainWindow.get(), [&]() {
        qInfo() << "[app-exit] quitAction triggered, setting force_quit and calling quit()";
        // 先停止所有可能访问栈变量的定时器，防止退出过程中 use-after-free
        trayBlinkTimer->stop();
        m_app.setProperty("leyochat_force_quit", true);
        m_app.quit();
    });
    QObject::connect(trayIcon.get(), &QSystemTrayIcon::activated, m_mainWindow.get(),
                     [&](QSystemTrayIcon::ActivationReason reason) {
                         if (reason == QSystemTrayIcon::DoubleClick
                             || reason == QSystemTrayIcon::Trigger) {
                             restoreMainWindow();
                         }
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::windowMinimizedToTrayRequested, m_mainWindow.get(),
                     [&]() {
                         if (!QSystemTrayIcon::isSystemTrayAvailable()) {
                             return;
                         }
                         m_mainWindow->hide();
                         scheduleRecoverySnapshot();
                         trayIcon->showMessage(appDisplayName,
                                               QStringLiteral("已最小化到系统托盘，收到新消息会在这里提醒你。"),
                                               QSystemTrayIcon::Information,
                                               2500);
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::windowInteractiveStateChanged, m_mainWindow.get(),
                     [&]() {
                         scheduleRecoverySnapshot();
                         logCurrentUiRestoreTrace(QStringLiteral("window-interactive-state-changed"),
                                                  windowStateSummary(m_mainWindow.get()));
                         QTimer::singleShot(150, m_mainWindow.get(), [&]() {
                             if (flushDeferredTransferRefreshIfVisible) {
                                 logCurrentUiRestoreTrace(QStringLiteral("interactive-refresh-fired"));
                                 flushDeferredTransferRefreshIfVisible();
                             }
                         });
                     });

    const auto notifyUnreadActivity = [&](const QString& title,
                                          const QString& body,
                                          bool countAsUnread = true) {
        if (m_mainWindow && m_mainWindow->isVisible()
            && !m_mainWindow->windowState().testFlag(Qt::WindowMinimized)
            && m_app.applicationState() == Qt::ApplicationActive) {
            return;
        }

        const QString safeTitle = sanitizeNotificationText(title, appDisplayName);
        const QString safeBody = sanitizeNotificationText(body, QStringLiteral("你有一条新提醒"));
        if (countAsUnread) {
            ++unreadReminderCount;
            lastUnreadTitle = safeTitle;
            updateTrayUnreadPresentation();
        }
        if (!trayBlinkTimer->isActive()) {
            trayBlinkTimer->start();
        }
        QApplication::alert(m_mainWindow.get(), 0);
        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            trayIcon->show();
            QSettings trayNotifCfg(AppSettings::organizationName(), AppSettings::applicationName());
            if (trayNotifCfg.value(QStringLiteral("notification/trayPopupEnabled"), false).toBool()) {
                trayIcon->showMessage(safeTitle, safeBody, QSystemTrayIcon::Information, 4000);
            }
        }
    };
    const auto windowCanConsumeIncomingConversation = [&]() {
        return m_mainWindow && m_mainWindow->isVisible()
               && !m_mainWindow->windowState().testFlag(Qt::WindowMinimized)
               && m_app.applicationState() == Qt::ApplicationActive;
    };

    const auto normalizeLegacyDirectConversations = [&]() {
        const auto summaries = conversationRepository.loadConversationSummaries();
        QHash<QString, ConversationSummary> summariesById;
        for (const auto& summary : summaries) {
            summariesById.insert(QString::fromStdWString(summary.conversationId), summary);
        }

        for (const auto& summary : summaries) {
            const QString legacyConversationId = QString::fromStdWString(summary.conversationId).trimmed();
            if (legacyConversationId.isEmpty() || legacyConversationId.contains('|')
                || legacyConversationId == localClientId
                || groupService.isGroupConversation(legacyConversationId)) {
                continue;
            }

            const QString canonicalConversationId =
                DirectConversationAddressing::conversationIdForPeers(localClientId, legacyConversationId);
            if (canonicalConversationId.isEmpty() || canonicalConversationId == legacyConversationId) {
                continue;
            }

            ConversationSummary mergedSummary = summary;
            mergedSummary.conversationId = canonicalConversationId.toStdWString();

            const auto existingIt = summariesById.constFind(canonicalConversationId);
            bool isPinned = summary.isPinned;
            bool isStarred = summary.isStarred;
            bool isMuted = summary.isMuted;
            bool isDone = summary.isDone;
            bool isManuallyUnread = summary.isManuallyUnread;
            if (existingIt != summariesById.constEnd()) {
                const ConversationSummary& existingSummary = existingIt.value();
                if (existingSummary.lastMessageAtMs >= mergedSummary.lastMessageAtMs) {
                    mergedSummary.lastMessagePreview = existingSummary.lastMessagePreview;
                    mergedSummary.lastMessageAtMs = existingSummary.lastMessageAtMs;
                }
                if (QString::fromStdWString(mergedSummary.title).trimmed().isEmpty()) {
                    mergedSummary.title = existingSummary.title;
                }
                isPinned = existingSummary.isPinned || isPinned;
                isStarred = existingSummary.isStarred || isStarred;
                isMuted = existingSummary.isMuted || isMuted;
                isDone = existingSummary.isDone && isDone;
                isManuallyUnread = existingSummary.isManuallyUnread || isManuallyUnread;
            }

            if (QString::fromStdWString(mergedSummary.title).trimmed().isEmpty()) {
                mergedSummary.title = legacyConversationId.toStdWString();
            }

            if (!conversationRepository.remapConversationId(legacyConversationId, canonicalConversationId)) {
                continue;
            }

            fileTransferRepository.remapConversationId(legacyConversationId, canonicalConversationId);
            conversationRepository.upsertConversation(mergedSummary);
            conversationRepository.setConversationFlag(canonicalConversationId, ConversationFlag::Pinned, isPinned);
            conversationRepository.setConversationFlag(canonicalConversationId, ConversationFlag::Starred, isStarred);
            conversationRepository.setConversationFlag(canonicalConversationId, ConversationFlag::Muted, isMuted);
            conversationRepository.setConversationFlag(canonicalConversationId, ConversationFlag::Done, isDone);
            conversationRepository.setConversationFlag(
                canonicalConversationId, ConversationFlag::ManuallyUnread, isManuallyUnread);
            conversationRepository.deleteConversation(legacyConversationId);
            summariesById.insert(canonicalConversationId, mergedSummary);
        }
    };

    // normalizeLegacyDirectConversations 延迟至 show() 后执行

    // 鍚姩鏃跺姞杞藉巻鍙插凡鐭?peer，写入内存目录（重连在主窗口显示后延迟触发）
    const auto knownPeersAtStartup = conversationRepository.loadKnownPeers();
    for (const auto& peer : knownPeersAtStartup) {
        PeerEndpoint restoredPeer = peer;
        restoredPeer.isConnected = false;
        restoredPeer.presence = PeerPresenceStatus::Offline;
        restoredPeer.lastPresenceAtMs = 0;
        peerDirectoryService.upsertDiscoveredPeer(restoredPeer);
    }
    qInfo() << "[startup-perf] knownPeers loaded:" << startupTimer.elapsed() << "ms" << "count=" << knownPeersAtStartup.size();

    const auto remoteOnlineClientIds = [&]() -> QSet<QString> {
        const RemoteChatServiceSettings settings =
            RemoteChatServiceSettingsStore::load();
        if (!remoteChatServiceProbeAllowed(settings)
            || settings.workspaceId.trimmed().isEmpty()) {
            return {};
        }
        return conversationRepository.loadOnlineRemoteSessionClientIds(
            settings.workspaceId);
    };

    const auto refreshConversationList = [&]() {
        QElapsedTimer timer;
        timer.start();
        const auto summaries = ChatService::loadConversationSummaries(&conversationRepository);
        const QSet<QString> unreadIds =
            conversationRepository.loadConversationsWithUnreadMessages(localClientId);
        // 合并为一次 model reset，避免连续两次 beginResetModel 导致全量重绘翻倍
        conversationModel.setItemsAndUnread(
            decorateConversationVector(summaries, localClientId, peerDirectoryService),
            unreadIds);

        // 为每个会话构建头像路径
        QHash<QString, QString> avatarPaths;
        for (const auto& s : summaries) {
            const QString convId = QString::fromStdWString(s.conversationId);
            const QString peerId =
                DirectConversationAddressing::otherParticipant(localClientId, convId);
            if (!peerId.isEmpty()) {
                const QString path = cachedAvatarPathForClient(peerId);
                if (!path.isEmpty()) {
                    avatarPaths.insert(convId, path);
                }
            }
        }
        conversationModel.setAvatarPaths(avatarPaths);

        // 构建在线 peer ID 集合，用于头像灰度显示
        QSet<QString> onlinePeerIds;
        onlinePeerIds =
            RemotePresenceUiAdapter::directConversationIdsForOnlineClients(
                localClientId,
                summaries,
                remoteOnlineClientIds());
        onlinePeerIds.unite(
            RemotePresenceUiAdapter::directConversationIdsForOnlinePeers(
                localClientId,
                summaries,
                toPeerVector(peerDirectoryService.visiblePeers(toUtf8(localClientId))),
                QDateTime::currentMSecsSinceEpoch()));
        conversationModel.setOnlinePeerIds(onlinePeerIds);

        logCurrentUiRestoreTrace(QStringLiteral("refresh-conversation-list"),
                                 QStringLiteral("count=%1 unread=%2 costMs=%3")
                                     .arg(QString::number(summaries.size()),
                                          QString::number(unreadIds.size()),
                                          QString::number(timer.elapsed())));
    };
    const auto refreshTransferList = [&]() {
        QElapsedTimer timer;
        timer.start();
        qInfo() << "[startup-perf] refreshTransferList: entered, startupTimer=" << startupTimer.elapsed() << "ms";
        QVector<TransferListItem> items;
        const auto allTasks = fileTransferService.loadAllTasks();
        items.reserve(static_cast<qsizetype>(allTasks.size()));
        // Build a hash map from known peers for O(1) fallback lookup instead
        // of O(n) linear search per task.
        const auto cachedKnownPeers = conversationRepository.loadKnownPeers();
        QHash<QString, QString> knownPeerNameMap;
        knownPeerNameMap.reserve(static_cast<qsizetype>(cachedKnownPeers.size()));
        for (const PeerEndpoint& kp : cachedKnownPeers) {
            knownPeerNameMap.insert(QString::fromStdString(kp.clientId),
                                   QString::fromStdString(kp.displayName).trimmed());
        }
        for (const auto& task : allTasks) {
            QString peerDisplayName;
            const auto peer =
                peerDirectoryService.findPeerByClientId(toUtf8(QString::fromStdWString(task.peerClientId)));
            if (peer.has_value()) {
                peerDisplayName = displayNameForPeer(*peer);
            } else {
                const QString peerClientId = QString::fromStdWString(task.peerClientId);
                auto it = knownPeerNameMap.constFind(peerClientId);
                if (it != knownPeerNameMap.constEnd())
                    peerDisplayName = it.value();
            }
            const QString localFilePath = localFilePathForTransferTask(task);
            // Only check disk for completed/interrupted tasks to avoid
            // blocking the main thread with QFileInfo::exists() calls during
            // active transfers (70 tasks × disk I/O = visible UI lag).
            const bool fileExists = (task.state == FileTransferState::Completed
                                     || task.state == FileTransferState::Interrupted)
                && !localFilePath.trimmed().isEmpty()
                && QFileInfo::exists(localFilePath);
            items.push_back(TransferListItem{
                QString::fromStdWString(task.taskId),
                QString::fromStdWString(task.fileName),
                transferStatusChipText(task),
                transferDetailText(task, peerDisplayName),
                transferPeerLabel(task, peerDisplayName),
                localFilePath,
                task.direction,
                task.state,
                fileExists,
                fileExists,
                transferTaskRetryable(task)
            });
        }
        transferModel.setItems(std::move(items));
        logCurrentUiRestoreTrace(QStringLiteral("refresh-transfer-list"),
                                 QStringLiteral("count=%1 costMs=%2")
                                     .arg(QString::number(allTasks.size()),
                                          QString::number(timer.elapsed())));
    };
    const auto refreshContactList = [&]() {
        QElapsedTimer timer;
        timer.start();
        auto peers = toPeerVector(peerDirectoryService.visiblePeers(toUtf8(localClientId)));
        peers = RemotePresenceUiAdapter::applyOnlineClientsToPeers(
            std::move(peers),
            remoteOnlineClientIds(),
            QDateTime::currentMSecsSinceEpoch());
        contactModel.setItems(peers);
        logCurrentUiRestoreTrace(QStringLiteral("refresh-contact-list"),
                                 QStringLiteral("count=%1 costMs=%2")
                                     .arg(QString::number(peers.size()),
                                          QString::number(timer.elapsed())));
    };
    const auto refreshMessageList = [&]() {
        QElapsedTimer timer;
        timer.start();
        if (currentConversationId.isEmpty()) {
            messageModel.setTransferStates({});
            messageModel.setItems({});
            logCurrentUiRestoreTrace(QStringLiteral("refresh-message-list"),
                                     QStringLiteral("conversation=none count=0 costMs=%1")
                                         .arg(QString::number(timer.elapsed())));
            return;
        }
        const auto page =
            conversationRepository.loadRecentMessagesPage(currentConversationId.toStdWString());
        const auto messages = toMessageVector(page.messages);
        QHash<QString, MessageListModel::TransferVisualState> transferStates;
        const auto conversationTasks = fileTransferService.loadTasksForConversation(currentConversationId);
        for (const auto& task : conversationTasks) {
            MessageListModel::TransferVisualState visualState;
            visualState.taskId = QString::fromStdWString(task.taskId);
            visualState.state = task.state;
            visualState.bytesCompleted = task.bytesCompleted;
            visualState.fileSize = task.fileSize;
            visualState.cancelable = task.direction == FileTransferDirection::Outgoing
                && (task.state == FileTransferState::PendingOffer
                    || task.state == FileTransferState::WaitingAccept
                    || task.state == FileTransferState::ReadyToTransfer
                    || task.state == FileTransferState::Transferring
                    || task.state == FileTransferState::Paused
                    || task.state == FileTransferState::Interrupted);
            transferStates.insert(visualState.taskId, visualState);
        }
        messageModel.setTransferStates(transferStates);
        messageModel.setItems(messages);
        logCurrentUiRestoreTrace(QStringLiteral("refresh-message-list"),
                                 QStringLiteral("conversation=%1 count=%2 costMs=%3")
                                     .arg(currentConversationId,
                                          QString::number(messages.size()),
                                          QString::number(timer.elapsed())));
    };
    const auto updateMessageDisplayContext = [&]() {
        const QString resolvedTargetId = resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
        const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(resolvedTargetId));
        const QString peerDisplayName = peer.has_value() ? displayNameForPeer(*peer) : resolvedTargetId;
        messageModel.setDisplayContext(localClientId, peerDisplayName, resolvedTargetId);
        messageModel.setAvatarContext(cachedAvatarPathForClient(localClientId),
                                      cachedAvatarPathForClient(resolvedTargetId));
    };
    QTimer transferUiRefreshTimer(m_mainWindow.get());
    transferUiRefreshTimer.setSingleShot(true);
    bool transferConversationRefreshPending = false;
    bool transferMessageRefreshPending = false;
    bool transferListRefreshPending = false;
    bool            flushInFlight = false;
    bool            devOpsFlushInFlight  = false;
    bool            outlookFlushInFlight = false;
    QSet<QString> olderMessageLoadsInFlight;

    struct AsyncFlushResults {
        bool hadTransfers     = false;
        bool hadConversations = false;
        bool hadMessages      = false;

        std::vector<FileTransferTask> transferTasks;
        std::vector<PeerEndpoint>     knownPeers;

        std::vector<ConversationSummary> conversationSummaries;
        QSet<QString>                    unreadConversationIds;

        std::vector<ChatMessage> messages;
        QString                  queriedConversationId;
        bool                     messagesHasMoreBefore = false;
    };

    struct DevOpsPollResult {
        bool hadConfig = false;
        AzureDevOpsConnectionSettings updatedSettings;
        QVector<AzureDevOpsNotificationEvent> events;
        QString errorMessage;
    };

    struct OutlookPollResult {
        bool hadConfig = false;
        OutlookNotificationPollResult pollResult;
        QString errorMessage;
    };

    const auto flushDeferredTransferRefresh = [&]() {
        logCurrentUiRestoreTrace(QStringLiteral("flush-deferred-begin"),
                                 QStringLiteral("pendingConversations=%1 pendingMessages=%2 pendingTransfers=%3")
                                     .arg(transferConversationRefreshPending ? QStringLiteral("true")
                                                                             : QStringLiteral("false"),
                                          transferMessageRefreshPending     ? QStringLiteral("true")
                                                                            : QStringLiteral("false"),
                                          transferListRefreshPending        ? QStringLiteral("true")
                                                                           : QStringLiteral("false")));
        if (flushInFlight) {
            return;
        }

        const bool doTransfers     = transferListRefreshPending;
        const bool doConversations = transferConversationRefreshPending;
        const bool doMessages      = transferMessageRefreshPending;
        if (!doTransfers && !doConversations && !doMessages) {
            return;
        }
        transferListRefreshPending         = false;
        transferConversationRefreshPending = false;
        transferMessageRefreshPending      = false;

        const QString dbPathStr             = databasePath();
        const QString localClientIdCopy     = localClientId;
        const QString snappedConversationId = currentConversationId;

        flushInFlight = true;

        auto* flushWatcher = new QFutureWatcher<AsyncFlushResults>(m_mainWindow.get());
        QObject::connect(flushWatcher, &QFutureWatcher<AsyncFlushResults>::finished,
                         m_mainWindow.get(), [&, flushWatcher, doTransfers, doConversations, doMessages,
                                              snappedConversationId]() {
            const auto results = flushWatcher->result();
            flushWatcher->deleteLater();
            qInfo() << "[syncFlush] watcher finished, mainWindow=" << (m_mainWindow ? "valid" : "NULL")
                    << "doTransfers=" << doTransfers << "doConv=" << doConversations << "doMsg=" << doMessages;
            flushInFlight = false;

            if (doTransfers && results.hadTransfers) {
                QVector<TransferListItem> items;
                items.reserve(
                    static_cast<qsizetype>(results.transferTasks.size()));
                for (const auto& task : results.transferTasks) {
                    QString peerDisplayName;
                    const auto peer = peerDirectoryService.findPeerByClientId(
                        toUtf8(QString::fromStdWString(task.peerClientId)));
                    if (peer.has_value()) {
                        peerDisplayName = displayNameForPeer(*peer);
                    } else {
                        for (const PeerEndpoint& kp : results.knownPeers) {
                            if (QString::fromStdString(kp.clientId)
                                == QString::fromStdWString(task.peerClientId)) {
                                peerDisplayName =
                                    QString::fromStdString(kp.displayName).trimmed();
                                break;
                            }
                        }
                    }
                    const QString localFilePath = localFilePathForTransferTask(task);
                    const bool fileExists =
                        !localFilePath.trimmed().isEmpty()
                        && QFileInfo::exists(localFilePath);
                    items.push_back(TransferListItem{
                        QString::fromStdWString(task.taskId),
                        QString::fromStdWString(task.fileName),
                        transferStatusChipText(task),
                        transferDetailText(task, peerDisplayName),
                        transferPeerLabel(task, peerDisplayName),
                        localFilePath,
                        task.direction,
                        task.state,
                        fileExists,
                        fileExists,
                        transferTaskRetryable(task)
                    });
                }
                transferModel.setItems(std::move(items));
                logCurrentUiRestoreTrace(QStringLiteral("flush-deferred-transfers"),
                                         QStringLiteral("count=%1")
                                             .arg(results.transferTasks.size()));
            }

            // 用最新的传输任务数据刷新消息模型的 transfer states，
            // 确保聊天气泡中的紧凑传输卡片（进度条）能正确显示。
            if (doTransfers && results.hadTransfers
                && !currentConversationId.isEmpty()) {
                QHash<QString, MessageListModel::TransferVisualState> transferStates;
                for (const auto& task : results.transferTasks) {
                    if (QString::fromStdWString(task.conversationId) != currentConversationId) {
                        continue;
                    }
                    MessageListModel::TransferVisualState vs;
                    vs.taskId = QString::fromStdWString(task.taskId);
                    vs.state = task.state;
                    vs.bytesCompleted = task.bytesCompleted;
                    vs.fileSize = task.fileSize;
                    if (task.state == FileTransferState::Transferring) {
                        vs.speedBytesPerSec = transferSpeedTracker.updateSpeed(vs.taskId, task.bytesCompleted);
                    }
                    vs.cancelable = task.direction == FileTransferDirection::Outgoing
                        && (task.state == FileTransferState::PendingOffer
                            || task.state == FileTransferState::WaitingAccept
                            || task.state == FileTransferState::ReadyToTransfer
                            || task.state == FileTransferState::Transferring
                            || task.state == FileTransferState::Paused
                            || task.state == FileTransferState::Interrupted);
                    transferStates.insert(vs.taskId, vs);
                }
                messageModel.setTransferStates(transferStates);
            }

            if (doConversations && results.hadConversations) {
                conversationModel.setItemsAndUnread(
                    decorateConversationVector(results.conversationSummaries,
                                              localClientId,
                                              peerDirectoryService),
                    results.unreadConversationIds);
                logCurrentUiRestoreTrace(
                    QStringLiteral("flush-deferred-conversations"),
                    QStringLiteral("count=%1 unread=%2")
                        .arg(results.conversationSummaries.size())
                        .arg(results.unreadConversationIds.size()));
            }

            if (doMessages && results.hadMessages
                && currentConversationId == snappedConversationId) {
                chatDataStore.setMessages(snappedConversationId,
                                          results.messages,
                                          results.messagesHasMoreBefore);
                logCurrentUiRestoreTrace(
                    QStringLiteral("flush-deferred-messages"),
                    QStringLiteral("conversation=%1 count=%2")
                        .arg(snappedConversationId)
                        .arg(results.messages.size()));
            }

            logCurrentUiRestoreTrace(QStringLiteral("flush-deferred-end"));

            if (transferListRefreshPending
                || transferConversationRefreshPending
                || transferMessageRefreshPending) {
                if (flushDeferredTransferRefreshIfVisible) {
                    flushDeferredTransferRefreshIfVisible();
                }
            }
        });
        flushWatcher->setFuture(QtConcurrent::run(
            [dbPathStr, localClientIdCopy, doTransfers, doConversations,
             doMessages, snappedConversationId]() -> AsyncFlushResults
            {
                AsyncFlushResults results;
                const QString connName = QStringLiteral("leyochat-bg-%1")
                    .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
                {
                    QSqlDatabase db = QSqlDatabase::addDatabase(
                        QStringLiteral("QSQLITE"), connName);
                    db.setDatabaseName(dbPathStr);
                    if (!db.open()) {
                        db = QSqlDatabase();
                    } else {
                        db.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
                        if (doTransfers) {
                            FileTransferRepository ftRepo(connName);
                            ConversationRepository convRepo(connName);
                            results.transferTasks = ftRepo.loadAllTasks();
                            results.knownPeers    = convRepo.loadKnownPeers();
                            results.hadTransfers  = true;
                        }
                        if (doConversations) {
                            ConversationRepository convRepo(connName);
                            results.conversationSummaries =
                                convRepo.loadConversationSummaries();
                            results.unreadConversationIds =
                                convRepo.loadConversationsWithUnreadMessages(
                                    localClientIdCopy);
                            results.hadConversations = true;
                        }
                        if (doMessages && !snappedConversationId.isEmpty()) {
                            ConversationRepository convRepo(connName);
                            auto page = convRepo.loadRecentMessagesPage(snappedConversationId.toStdWString());
                            results.messages = std::move(page.messages);
                            results.messagesHasMoreBefore = page.hasMoreBefore;
                            results.queriedConversationId = snappedConversationId;
                            results.hadMessages = true;
                        }

                        db.close();
                        db = QSqlDatabase();
                    }
                }
                QSqlDatabase::removeDatabase(connName);
                return results;
            }
        ));
    };
    flushDeferredTransferRefreshIfVisible = [&]() {
        logCurrentUiRestoreTrace(QStringLiteral("flush-visible-check"),
                                 QStringLiteral("canConsume=%1 pendingConversations=%2 pendingMessages=%3 pendingTransfers=%4")
                                     .arg(windowCanConsumeIncomingConversation() ? QStringLiteral("true")
                                                                                 : QStringLiteral("false"),
                                          transferConversationRefreshPending ? QStringLiteral("true")
                                                                             : QStringLiteral("false"),
                                          transferMessageRefreshPending ? QStringLiteral("true")
                                                                        : QStringLiteral("false"),
                                          transferListRefreshPending ? QStringLiteral("true")
                                                                     : QStringLiteral("false")));
        if (!windowCanConsumeIncomingConversation()) {
            return;
        }
        if (!transferConversationRefreshPending
            && !transferMessageRefreshPending
            && !transferListRefreshPending) {
            return;
        }
        if (transferUiRefreshTimer.isActive()) {
            transferUiRefreshTimer.stop();
        }
        flushDeferredTransferRefresh();
    };
    QObject::connect(&transferUiRefreshTimer, &QTimer::timeout, m_mainWindow.get(),
                     [&]() {
                         logCurrentUiRestoreTrace(QStringLiteral("transfer-ui-timer-fired"),
                                                  QStringLiteral("intervalMs=%1")
                                                      .arg(QString::number(transferUiRefreshTimer.interval())));
                         if (flushDeferredTransferRefreshIfVisible) {
                             flushDeferredTransferRefreshIfVisible();
                         }
                     });
    const auto scheduleDeferredTransferRefresh = [&](bool refreshMessages = true,
                                                     bool refreshConversations = true,
                                                     bool refreshTransfers = true) {
        transferConversationRefreshPending = transferConversationRefreshPending || refreshConversations;
        transferMessageRefreshPending = transferMessageRefreshPending || refreshMessages;
        transferListRefreshPending = transferListRefreshPending || refreshTransfers;
        logCurrentUiRestoreTrace(QStringLiteral("schedule-deferred"),
                                 QStringLiteral("requestConversations=%1 requestMessages=%2 requestTransfers=%3 canConsume=%4 timerActive=%5")
                                     .arg(refreshConversations ? QStringLiteral("true")
                                                               : QStringLiteral("false"),
                                          refreshMessages ? QStringLiteral("true")
                                                          : QStringLiteral("false"),
                                          refreshTransfers ? QStringLiteral("true")
                                                           : QStringLiteral("false"),
                                          windowCanConsumeIncomingConversation() ? QStringLiteral("true")
                                                                                 : QStringLiteral("false"),
                                          transferUiRefreshTimer.isActive() ? QStringLiteral("true")
                                                                            : QStringLiteral("false")));
        if (windowCanConsumeIncomingConversation() && !transferUiRefreshTimer.isActive()) {
            transferUiRefreshTimer.start(180);
            logCurrentUiRestoreTrace(QStringLiteral("schedule-deferred-timer-started"),
                                     QStringLiteral("delayMs=180"));
        }
    };
    bool deferGroupPanelRefresh = false;
    const auto updateChatHeader = [&]() {
        if (!m_mainWindow) {
            return;
        }

        // 群聊会话优先按群信息刷新头部和右侧面板，避免受残留的直聊 target 影响
        if (!currentConversationId.isEmpty()
            && groupService.isGroupConversation(currentConversationId)) {
            const auto groupOpt = groupRepository.findGroupById(currentConversationId.toStdWString());
            const QString groupName = groupOpt
                ? QString::fromStdWString(groupOpt->groupName)
                : currentConversationId;
            const auto members = groupRepository.loadMembers(currentConversationId.toStdWString());
            int activeMemberCount = 0;
            for (const auto& member : members) {
                if (member.isActive) {
                    ++activeMemberCount;
                }
            }
            // 构建 senderId 鈫?displayName 鏄犲皠锛屼緵娑堟伅姘旀场鏄剧ず姣忎綅鎴愬憳鐨勫悕瀛?
            QHash<QString, QString> memberNames;
            GroupMemberListEntries memberEntries;
            const QString ownerClientId = groupOpt
                ? QString::fromStdWString(groupOpt->ownerClientId)
                : QString();
            const QSet<QString> onlineClients = remoteOnlineClientIds();
            memberEntries.reserve(static_cast<qsizetype>(members.size()));
            for (const auto& m : members) {
                if (!m.isActive) {
                    continue;
                }
                const QString memberId = QString::fromStdWString(m.memberClientId);
                QString displayName;
                bool resolvedKnownIdentity = false;
                if (memberId == localClientId && !localDisplayName.trimmed().isEmpty()) {
                    displayName = localDisplayName.trimmed();
                    resolvedKnownIdentity = true;
                }
                // 1. 鍏堝皾璇曞湪绾?peer 鐩綍
                const auto peerOpt = peerDirectoryService.findPeerByClientId(toUtf8(memberId));
                if (displayName.isEmpty() && peerOpt.has_value()) {
                    displayName = displayNameForPeer(*peerOpt);
                    resolvedKnownIdentity = !displayName.trimmed().isEmpty();
                }
                // 2. 回落到 GroupMember 快照
                if (displayName.isEmpty() && !m.memberDisplayNameSnapshot.empty()) {
                    displayName = QString::fromStdWString(m.memberDisplayNameSnapshot);
                    resolvedKnownIdentity = !displayName.trimmed().isEmpty();
                }
                // 3. 最后回落到安全占位名，避免把未知 UUID 直接暴露到 UI
                if (displayName.isEmpty()) {
                    displayName = (memberId == localClientId)
                        ? QStringLiteral("我")
                        : QStringLiteral("未知成员");
                }
                const QString avatarPath = (resolvedKnownIdentity || memberId == localClientId)
                    ? cachedAvatarPathForClient(memberId)
                    : QString();
                memberNames.insert(memberId, displayName);
                const bool isAdmin = (m.role == L"admin");
                const bool peerOnline =
                    peerOpt.has_value()
                    && PeerPresenceEvaluator::isOnlineOrAway(
                        *peerOpt,
                        QDateTime::currentMSecsSinceEpoch());
                memberEntries.push_back(GroupMemberListEntry{
                    memberId,
                    displayName,
                    memberId == ownerClientId,
                    isAdmin,
                    memberId == localClientId,
                    memberId == localClientId || peerOnline || onlineClients.contains(memberId),
                    avatarPath
                });
            }
            std::stable_sort(memberEntries.begin(), memberEntries.end(),
                             [](const GroupMemberListEntry& lhs, const GroupMemberListEntry& rhs) {
                                 if (lhs.isOwner != rhs.isOwner) {
                                     return lhs.isOwner;
                                 }
                                 if (lhs.isAdmin != rhs.isAdmin) {
                                     return lhs.isAdmin;
                                 }
                                 if (lhs.isSelf != rhs.isSelf) {
                                     return lhs.isSelf;
                                 }
                                 return lhs.displayName.localeAwareCompare(rhs.displayName) < 0;
                             });
            messageModel.setDisplayContext(localClientId, groupName);
            messageModel.setAvatarContext(cachedAvatarPathForClient(localClientId), QString());
            messageModel.setGroupActiveMemberCount(activeMemberCount);
            messageModel.setGroupMemberNames(memberNames);
            QHash<QString, QString> memberAvatars;
            memberAvatars.reserve(static_cast<int>(memberEntries.size()));
            for (const auto& memberEntry : memberEntries) {
                if (!memberEntry.avatarImagePath.trimmed().isEmpty()) {
                    memberAvatars.insert(memberEntry.clientId, memberEntry.avatarImagePath);
                }
            }
            messageModel.setGroupMemberAvatars(memberAvatars);
            // 更新右侧信息面板（公告 + 成员列表）+ 群聊头部 + 置顶消息
            const QString announcement = groupOpt
                ? QString::fromStdWString(groupOpt->announcement) : QString();
            const bool localIsOwnerOrAdmin = ownerClientId == localClientId
                || std::any_of(memberEntries.cbegin(), memberEntries.cend(),
                               [](const GroupMemberListEntry& e) {
                                   return e.isSelf && e.isAdmin;
                               });
            const bool isOwner = (ownerClientId == localClientId);

            if (deferGroupPanelRefresh) {
                // 会话切换路径：延迟面板/置顶消息重建到下一帧，让消息列表先渲染
                deferGroupPanelRefresh = false;
                const QString deferConvId = currentConversationId;
                QTimer::singleShot(0, m_mainWindow.get(), [&, deferConvId, announcement,
                                                            memberEntries, localIsOwnerOrAdmin,
                                                            groupName, activeMemberCount, isOwner]() {
                    if (currentConversationId != deferConvId) return;
                    m_mainWindow->setChatHeaderGroup(
                        deferConvId,
                        groupName,
                        activeMemberCount);
                    m_mainWindow->setGroupInfoPanel(announcement,
                                                    memberEntries,
                                                    localIsOwnerOrAdmin);
                    m_mainWindow->setCurrentUserIsGroupOwner(isOwner);
                    const auto allPins = conversationRepository.loadPinnedMessages(deferConvId);
                    if (!allPins.empty()) {
                        std::vector<PinnedCardInfo> cards;
                        cards.reserve(allPins.size());
                        for (const auto& p : allPins) {
                            cards.push_back({p.messageId, p.pinnedBody, p.authorName, p.pinnerName});
                        }
                        m_mainWindow->setPinnedMessageCards(cards);
                    } else {
                        m_mainWindow->clearPinnedMessageCards();
                    }
                });
            } else {
                // 非切换路径（定时刷新等）：同步更新
                m_mainWindow->setChatHeaderGroup(
                    currentConversationId,
                    groupName,
                    activeMemberCount);
                m_mainWindow->setGroupInfoPanel(announcement,
                                                memberEntries,
                                                localIsOwnerOrAdmin);
                m_mainWindow->setCurrentUserIsGroupOwner(isOwner);
                const auto allPins = conversationRepository.loadPinnedMessages(currentConversationId);
                if (!allPins.empty()) {
                    std::vector<PinnedCardInfo> cards;
                    cards.reserve(allPins.size());
                    for (const auto& p : allPins) {
                        cards.push_back({p.messageId, p.pinnedBody, p.authorName, p.pinnerName});
                    }
                    m_mainWindow->setPinnedMessageCards(cards);
                } else {
                    m_mainWindow->clearPinnedMessageCards();
                }
            }
            return;
        }

        if (currentTargetId.isEmpty()) {
            m_mainWindow->setChatHeader(QString(), QString());
            messageModel.setDisplayContext(localClientId, QString());
            messageModel.setAvatarContext(cachedAvatarPathForClient(localClientId), QString());
            messageModel.setGroupActiveMemberCount(0);
            m_mainWindow->clearPinnedMessageCards();
            m_mainWindow->setCurrentUserIsGroupOwner(false);
            return;
        }

        const QString resolvedTargetId = resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
        const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(resolvedTargetId));
        const bool remoteServiceOnline =
            remoteOnlineClientIds().contains(resolvedTargetId);
        if (peer.has_value()) {
            messageModel.setGroupActiveMemberCount(0);
            m_mainWindow->clearPinnedMessageCards();
            m_mainWindow->setCurrentUserIsGroupOwner(false);
            const QString sig = peerSignatures.value(resolvedTargetId, QString());
            PeerEndpoint headerPeer = *peer;
            if (remoteServiceOnline) {
                headerPeer.isConnected = true;
                headerPeer.presence = PeerPresenceStatus::Online;
                headerPeer.lastPresenceAtMs = QDateTime::currentMSecsSinceEpoch();
            }
            m_mainWindow->setChatHeaderDirect(
                displayNameForPeer(headerPeer),
                QStringLiteral("%1 \u00B7 %2:%3")
                    .arg(presenceTextForPeer(headerPeer),
                         QString::fromStdString(headerPeer.host),
                         QString::number(headerPeer.port)),
                sig,
                cachedAvatarPathForClient(resolvedTargetId));
            updateMessageDisplayContext();
            return;
        }

        m_mainWindow->setChatHeaderDirect(resolvedTargetId,
                                          remoteServiceOnline
                                              ? QStringLiteral("\u5728\u7EBF \u00B7 \u6D88\u606F\u670D\u52A1")
                                              : QStringLiteral("\u6682\u672A\u83B7\u53D6\u5230\u8054\u7CFB\u4EBA\u8BE6\u60C5"),
                                          QString(),
                                          cachedAvatarPathForClient(resolvedTargetId));
        updateMessageDisplayContext();
    };
    const auto syncSelectionState = [&]() {
        if (!m_mainWindow) {
            return;
        }
        m_mainWindow->setSelectedConversationId(currentConversationId);
        m_mainWindow->setSelectedContactId(
            resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId));
    };
    std::function<void()> scheduleReadReceiptUiRefresh;
    const auto flushReadReceipts = [&]() {
        if (currentConversationId.isEmpty()) {
            return;
        }
        const bool isGroupConv = groupService.isGroupConversation(currentConversationId);
        QMetaObject::invokeMethod(dbWorker, "flushReadReceipts",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, currentConversationId),
                                  Q_ARG(QString, localClientId),
                                  Q_ARG(bool, isGroupConv));
    };
    const auto flushReadReceiptsForIncomingConversations =
        [&](const QStringList& conversationIds) {
            if (!windowCanConsumeIncomingConversation()
                || currentConversationId.isEmpty()
                || !conversationIds.contains(currentConversationId)) {
                return;
            }
            flushReadReceipts();
        };
    QObject::connect(m_mainWindow.get(), &MainWindow::viewportReachedBottom, m_mainWindow.get(),
                     [&]() {
                         if (windowCanConsumeIncomingConversation()) {
                             flushReadReceipts();
                         }
                     });
    const auto pendingMessageCountForTarget = [&](const QString& targetClientId) {
        if (targetClientId.isEmpty()) {
            return 0;
        }
        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(localClientId, targetClientId);
        if (conversationId.isEmpty()) {
            return 0;
        }

        const auto pendingMessages =
            conversationRepository.loadPendingOutgoingMessages(conversationId.toStdWString(),
                                                               localClientId.toStdWString());
        return static_cast<int>(pendingMessages.size());
    };
    QTimer chatUiRefreshTimer(m_mainWindow.get());
    chatUiRefreshTimer.setSingleShot(true);
    bool conversationListRefreshPending = false;
    bool messageListRefreshPending = false;
    bool chatHeaderRefreshPending = false;
    bool selectionStateRefreshPending = false;
    bool contactListRefreshPending = false;
    bool startupInitialUiReady = false;
    const auto applyPendingRecoveryWhenReady = [&]() {
        if (!startupInitialUiReady || !pendingRecoveryState.has_value() || !m_mainWindow) {
            return;
        }

        const ClientRecoveryState state = *pendingRecoveryState;
        pendingRecoveryState.reset();
        if (!state.windowGeometry.isEmpty()) {
            m_mainWindow->restoreGeometry(state.windowGeometry);
        }
        m_mainWindow->navigateToRecoveryPage(state.navigationPageId);
        const auto recoveredConversationSummaries =
            conversationRepository.loadConversationSummaries();
        const bool recoveredConversationExists =
            !state.conversationId.isEmpty()
            && (conversationRepository.isKnownActiveGroupConversation(state.conversationId)
                || std::any_of(recoveredConversationSummaries.cbegin(),
                               recoveredConversationSummaries.cend(),
                               [&state](const ConversationSummary& summary) {
                                   return QString::fromStdWString(summary.conversationId)
                                       == state.conversationId;
                               }));
        if (recoveredConversationExists) {
            ComposerRecoveryContext composer;
            composer.composerHtml = state.composerHtml;
            composer.replyMessageId = state.replyMessageId;
            composer.replySenderId = state.replySenderId;
            composer.replySenderName = state.replySenderName;
            composer.replyBody = state.replyBody;
            composer.editingMessageId = state.editingMessageId;
            composer.editingBody = state.editingBody;
            m_mainWindow->stageRecoveredComposerContext(state.conversationId, composer);
            emit m_mainWindow->conversationSelected(state.conversationId);
        } else if (!state.conversationId.isEmpty()) {
            qWarning() << "[startup-recovery] conversation no longer exists:"
                       << state.conversationId;
        }

        switch (state.windowMode) {
        case RecoveryWindowMode::TrayHidden:
            m_mainWindow->hide();
            break;
        case RecoveryWindowMode::Minimized:
            m_mainWindow->showMinimized();
            break;
        case RecoveryWindowMode::Visible:
            if (state.windowMaximized) {
                m_mainWindow->showMaximized();
            } else {
                m_mainWindow->show();
            }
            m_mainWindow->showChatToast(QStringLiteral("LeyoChat 已自动恢复"), 3000);
            break;
        }
        if (state.windowMode != RecoveryWindowMode::Visible) {
            trayIcon->showMessage(appDisplayName,
                                  QStringLiteral("LeyoChat 已自动恢复"),
                                  QSystemTrayIcon::Information,
                                  3000);
        }
        qInfo() << "[startup-recovery] UI state restored";
        scheduleRecoverySnapshot();
    };
    // 用于 scheduler 路径的 header 节流：只在会话/目标变化时才执行 updateChatHeader
    QString lastScheduledHeaderConversationId;
    QString lastScheduledHeaderTargetId;
    const auto finishSplashWhenReady = [&]() {
        if (!startupInitialUiReady || !m_splashScreen) {
            return;
        }
        m_splashScreen->finish(m_mainWindow.get());
        m_splashScreen = nullptr;
        qInfo() << "[startup-perf] splash closed:" << startupTimer.elapsed() << "ms";
    };

    // ── 群组目录刷新（带节流，避免 GroupMeta 洪水时主线程卡死） ──
    QTimer directoryGroupsRefreshTimer(m_mainWindow.get());
    directoryGroupsRefreshTimer.setSingleShot(true);
    bool directoryGroupsRefreshPending = false;
    const auto refreshDirectoryGroupsImpl = [&]() {
        const auto groups = groupRepository.loadGroupsForMember(localClientId.toStdWString());
        QVector<GroupSummary> summaries;
        summaries.reserve(static_cast<int>(groups.size()));
        for (const auto& g : groups) {
            GroupSummary s;
            s.groupId    = QString::fromStdWString(g.groupId);
            s.groupName  = QString::fromStdWString(g.groupName);
            s.lastActiveMs = g.updatedAtMs;

            // 群主显示名
            const QString ownerId = QString::fromStdWString(g.ownerClientId);
            if (ownerId == localClientId) {
                s.ownerDisplayName = localDisplayName;
            } else {
                const auto ownerPeer = peerDirectoryService.findPeerByClientId(toUtf8(ownerId));
                s.ownerDisplayName = ownerPeer.has_value()
                    ? displayNameForPeer(*ownerPeer) : ownerId;
            }

            // 成员数
            const auto members = groupRepository.loadMembers(g.groupId);
            s.memberCount = static_cast<int>(members.size());

            // 未读数
            const QString convId = s.groupId;
            s.unreadCount = chatDataStore.unreadConversationIds().contains(convId) ? 1 : 0;

            summaries.append(s);
        }
        m_mainWindow->setDirectoryGroups(summaries);
    };
    QObject::connect(&directoryGroupsRefreshTimer, &QTimer::timeout, m_mainWindow.get(),
                     [&]() {
        directoryGroupsRefreshPending = false;
        refreshDirectoryGroupsImpl();
    });
    const auto refreshDirectoryGroups = [&]() {
        directoryGroupsRefreshPending = true;
        if (!directoryGroupsRefreshTimer.isActive()) {
            directoryGroupsRefreshTimer.start(500);
        }
    };

    // ── 组织架构刷新 ──
    const auto refreshDirectoryOrg = [&]() {
        QHash<QString, QVector<OrgContactEntry>> departments;
        const QSet<QString> onlineClients = remoteOnlineClientIds();
        const auto peers = peerDirectoryService.visiblePeers(toUtf8(localClientId));
        for (const auto& peer : peers) {
            const QString cid = QString::fromStdString(peer.clientId);

            OrgContactEntry entry;
            entry.clientId = cid;
            entry.displayName = displayNameForPeer(peer);
            entry.isOnline =
                PeerPresenceEvaluator::isOnlineOrAway(
                    peer,
                    QDateTime::currentMSecsSinceEpoch())
                || onlineClients.contains(cid);

            QString dept;
            if (peerProfiles.contains(cid)) {
                const auto& pp = peerProfiles.value(cid);
                dept = pp.department.trimmed();
                entry.jobTitle = pp.jobTitle;
            }
            if (dept.isEmpty()) dept = QStringLiteral("\u672A\u5206\u7EC4");
            departments[dept].append(entry);
        }
        // 每个部门内按名字排序（在线优先）
        for (auto it = departments.begin(); it != departments.end(); ++it) {
            std::sort(it->begin(), it->end(), [](const OrgContactEntry& a, const OrgContactEntry& b) {
                if (a.isOnline != b.isOnline) return a.isOnline;
                return a.displayName.localeAwareCompare(b.displayName) < 0;
            });
        }
        m_mainWindow->setDirectoryOrgData(departments);
    };

    // ──── DatabaseWorker ↔ ChatDataStore 信号连接 ────
    QObject::connect(dbWorker, &DatabaseWorker::allDataLoaded,
        &chatDataStore, [&](QVector<ConversationSummary> conversations,
                            QSet<QString> unreadIds,
                            QHash<QString, Group> groups,
                            QHash<QString, std::vector<GroupMember>> groupMembers,
                            QVector<PeerEndpoint> knownPeers) {
            QVector<ConversationSummary> decorated;
            decorated.reserve(conversations.size());
            for (const auto& c : conversations) {
                decorated.append(decorateConversationSummary(c, localClientId, peerDirectoryService));
            }

            // 在触发 model 更新前，先构建辅助数据（头像/在线状态），
            // 用 QSignalBlocker 抑制 setAvatarPaths/setOnlinePeerIds 自身的 dataChanged，
            // 避免 3 次全量刷新（原来: setItemsAndUnread + setAvatarPaths + setOnlinePeerIds）。
            {
                QHash<QString, QString> avatarPaths;
                avatarPaths.reserve(conversations.size());
                for (const auto& c : conversations) {
                    const QString convId = QString::fromStdWString(c.conversationId);
                    const QString peerId =
                        DirectConversationAddressing::otherParticipant(localClientId, convId);
                    if (!peerId.isEmpty()) {
                        const QString path = cachedAvatarPathForClient(peerId);
                        if (!path.isEmpty()) {
                            avatarPaths.insert(convId, path);
                        }
                    }
                }

                // O(n) 在线判定：遍历已连接 peer，通过反向索引定位会话
                QSet<QString> onlinePeerIds =
                    RemotePresenceUiAdapter::directConversationIdsForOnlineClients(
                        localClientId,
                        conversations,
                        remoteOnlineClientIds());
                onlinePeerIds.unite(
                    RemotePresenceUiAdapter::directConversationIdsForOnlinePeers(
                        localClientId,
                        conversations,
                        toPeerVector(peerDirectoryService.visiblePeers(toUtf8(localClientId))),
                        QDateTime::currentMSecsSinceEpoch()));

                // 静默设置辅助数据，不触发额外 dataChanged
                const QSignalBlocker blocker(&conversationModel);
                conversationModel.setAvatarPaths(avatarPaths);
                conversationModel.setOnlinePeerIds(onlinePeerIds);
            }

            // 触发 model 更新（setItemsAndUnread 会发出唯一的 dataChanged/modelReset）
            chatDataStore.bulkLoadConversations(std::move(decorated), std::move(unreadIds));
            chatDataStore.bulkLoadGroups(std::move(groups), std::move(groupMembers));

            // 仅首次启动时用 DB 已知 peer 初始化联系人列表。
            // 后续 loadAll 重载不能覆盖：PeerDirectoryService 才是运行时的权威来源，
            // DB 数据无法反映实时连接/在线状态，覆盖会导致联系人列表闪烁（在线→离线→在线循环）。
            if (!startupInitialUiReady) {
                chatDataStore.setContacts(knownPeers);
                // 同时将已知 peer 注入 PeerDirectoryService（作为 discovered/离线），
                // 确保 refreshContactList() → visiblePeers() 能在连接建立前显示它们
                for (const auto& p : knownPeers) {
                    PeerEndpoint restoredPeer = p;
                    restoredPeer.isConnected = false;
                    restoredPeer.presence = PeerPresenceStatus::Offline;
                    restoredPeer.lastPresenceAtMs = 0;
                    peerDirectoryService.upsertDiscoveredPeer(restoredPeer);
                }
            }

            if (!startupInitialUiReady) {
                startupInitialUiReady = true;
                qInfo() << "[startup-perf] first data loaded, scheduling splash+transfers";
                QTimer::singleShot(0, m_mainWindow.get(), [&]() {
                    applyPendingRecoveryWhenReady();
                    finishSplashWhenReady();
                });
                QTimer::singleShot(50, m_mainWindow.get(), [&]() {
                    refreshTransferList();
                    refreshDirectoryGroups();
                    qInfo() << "[startup-perf] transfers done (deferred):" << startupTimer.elapsed() << "ms";
                });
            }
        });
    QObject::connect(dbWorker, &DatabaseWorker::recentMessagesLoaded,
        &chatDataStore, [&](const QString& conversationId,
                            std::vector<ChatMessage> messages,
                            bool hasMoreBefore) {
            chatDataStore.setMessages(conversationId, std::move(messages), hasMoreBefore);
            if (conversationId == currentConversationId) {
                QHash<QString, MessageListModel::TransferVisualState> transferStates;
                const auto conversationTasks =
                    fileTransferService.loadTasksForConversation(conversationId);
                for (const auto& task : conversationTasks) {
                    MessageListModel::TransferVisualState visualState;
                    visualState.taskId = QString::fromStdWString(task.taskId);
                    visualState.state = task.state;
                    visualState.bytesCompleted = task.bytesCompleted;
                    visualState.fileSize = task.fileSize;
                    visualState.cancelable = task.direction == FileTransferDirection::Outgoing
                        && (task.state == FileTransferState::PendingOffer
                            || task.state == FileTransferState::WaitingAccept
                            || task.state == FileTransferState::ReadyToTransfer
                            || task.state == FileTransferState::Transferring
                            || task.state == FileTransferState::Paused
                            || task.state == FileTransferState::Interrupted);
                    transferStates.insert(visualState.taskId, visualState);
                }
                messageModel.setTransferStates(transferStates);
                if (groupService.isGroupConversation(conversationId)) {
                    const auto allPins = conversationRepository.loadPinnedMessages(conversationId);
                    if (!allPins.empty()) {
                        std::vector<PinnedCardInfo> cards;
                        cards.reserve(allPins.size());
                        for (const auto& p : allPins) {
                            cards.push_back({p.messageId, p.pinnedBody, p.authorName, p.pinnerName});
                        }
                        m_mainWindow->setPinnedMessageCards(cards);
                    } else {
                        m_mainWindow->clearPinnedMessageCards();
                    }
                }
            }
        });
    QObject::connect(dbWorker, &DatabaseWorker::olderMessagesLoaded,
        &chatDataStore, [&](const QString& conversationId,
                            const QString& beforeMessageId,
                            std::vector<ChatMessage> messages,
                            bool hasMoreBefore) {
            olderMessageLoadsInFlight.remove(conversationId + QLatin1Char('|') + beforeMessageId);
            if (conversationId != currentConversationId) {
                return;
            }
            chatDataStore.prependMessages(conversationId, std::move(messages), hasMoreBefore);
        });
    QObject::connect(m_mainWindow.get(), &MainWindow::olderMessagesRequested,
                     m_mainWindow.get(), [&](const QString& conversationId,
                                             const QString& beforeMessageId) {
        if (conversationId != currentConversationId || beforeMessageId.trimmed().isEmpty()) {
            return;
        }
        const QString requestKey = conversationId + QLatin1Char('|') + beforeMessageId;
        if (olderMessageLoadsInFlight.contains(requestKey)) {
            return;
        }
        olderMessageLoadsInFlight.insert(requestKey);
        QMetaObject::invokeMethod(dbWorker, "loadMessagesBefore",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, conversationId),
                                  Q_ARG(QString, beforeMessageId),
                                  Q_ARG(int, 80));
    });
    QObject::connect(dbWorker, &DatabaseWorker::readReceiptsFlushed,
                     m_mainWindow.get(), [&](const QString& conversationId,
                                             const QVector<QPair<QString, QString>>& targets,
                                             bool unreadConsumed) {
        // 仅处理当前会话的回执（切换会话后忽略过期结果）
        if (conversationId != currentConversationId) return;

        const bool isGroupConv = groupService.isGroupConversation(conversationId);
        const QString resolvedTargetId = resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
        PeerConnection* directConn =
            ConnectionRegistryUtils::connectedConnectionForTarget(connectionsByTargetId,
                                                                  peerIdsByConnection,
                                                                  resolvedTargetId);
        const bool canSendDirect = !isGroupConv && directConn && directConn->isConnected();

        for (const auto& [messageId, senderId] : targets) {
            const auto sendReceipt = [&](PeerConnection* conn, const QString& targetId) {
                if (!conn || !conn->isConnected()) return;
                MessageEnvelope receipt;
                receipt.messageId      = toUtf8(messageId);
                receipt.type           = MessageType::ReceiptRead;
                receipt.senderId       = toUtf8(localClientId);
                receipt.targetId       = toUtf8(targetId);
                receipt.conversationId = toUtf8(conversationId);
                receipt.createdAtMs    = QDateTime::currentMSecsSinceEpoch();
                conn->sendPayload(QByteArray::fromStdString(MessageCodec::encode(receipt)));
            };
            if (canSendDirect && senderId == resolvedTargetId) {
                sendReceipt(directConn, resolvedTargetId);
            }
            if (isGroupConv) {
                sendReceipt(ConnectionRegistryUtils::connectedConnectionForTarget(connectionsByTargetId,
                                                                                  peerIdsByConnection,
                                                                                  senderId),
                            senderId);
            }
        }

        if (!targets.isEmpty()) {
            const RemoteChatServiceSettings remoteSettings =
                RemoteChatServiceSettingsStore::load();
            if (remoteChatServiceRecentlyReachable(remoteSettings)) {
                ServerMessageClient readAckClient(remoteSettings, localClientId);
                MessageSyncService readAckSync(localClientId,
                                               &conversationRepository,
                                               &readAckClient);
                const PendingReadAckFlushResult readAckResult =
                    readAckSync.flushPendingReadAcks(100);
                if (readAckResult.success) {
                    if (readAckResult.acknowledgedCount > 0) {
                        qInfo().noquote()
                            << "[remote-read-ack] flushed conversation="
                            << conversationId
                            << "acked=" << readAckResult.acknowledgedCount
                            << "attempted=" << readAckResult.attemptedCount;
                    }
                } else {
                    qWarning().noquote()
                        << "[remote-read-ack] failed conversation="
                        << conversationId
                        << "attempted=" << readAckResult.attemptedCount
                        << "acked=" << readAckResult.acknowledgedCount
                        << "error=" << readAckResult.errorMessage;
                }
            }
        }

        if (unreadConsumed) {
            chatDataStore.markConversationRead(conversationId);
            QTimer::singleShot(100, m_mainWindow.get(), [&]() {
                if (scheduleReadReceiptUiRefresh) {
                    scheduleReadReceiptUiRefresh();
                }
            });
        }
    });
    dbWorkerThread.start();
    QMetaObject::invokeMethod(dbWorker, "loadAll", Qt::QueuedConnection);

    std::function<void()> flushScheduledChatUiRefresh;
    flushScheduledChatUiRefresh = [&]() {
        const bool doContacts       = contactListRefreshPending;
        const bool doConversations  = conversationListRefreshPending;
        const bool doMessages       = messageListRefreshPending;
        const bool doHeader         = chatHeaderRefreshPending;
        const bool doSelection      = selectionStateRefreshPending;
        contactListRefreshPending       = false;
        conversationListRefreshPending  = false;
        messageListRefreshPending       = false;
        chatHeaderRefreshPending        = false;
        selectionStateRefreshPending    = false;

        if (doContacts) {
            refreshContactList();
            refreshDirectoryOrg();
        }
        if (doHeader) {
            // 始终刷新群聊消息气泡的成员名映射，确保新到消息的发送者名称能即时解析。
            // setGroupMemberNames 内部有 hash 判等，无变化时不触发 dataChanged。
            if (!currentConversationId.isEmpty()
                && groupService.isGroupConversation(currentConversationId)) {
                const auto members = groupRepository.loadMembers(currentConversationId.toStdWString());
                QHash<QString, QString> memberNames;
                for (const auto& m : members) {
                    if (!m.isActive) continue;
                    const QString memberId = QString::fromStdWString(m.memberClientId);
                    QString displayName;
                    if (memberId == localClientId && !localDisplayName.trimmed().isEmpty()) {
                        displayName = localDisplayName.trimmed();
                    }
                    if (displayName.isEmpty()) {
                        const auto peerOpt = peerDirectoryService.findPeerByClientId(toUtf8(memberId));
                        if (peerOpt.has_value())
                            displayName = displayNameForPeer(*peerOpt);
                    }
                    if (displayName.isEmpty() && !m.memberDisplayNameSnapshot.empty()) {
                        displayName = QString::fromStdWString(m.memberDisplayNameSnapshot);
                    }
                    if (displayName.isEmpty()) {
                        displayName = (memberId == localClientId)
                            ? QStringLiteral("我") : QStringLiteral("未知成员");
                    }
                    memberNames.insert(memberId, displayName);
                }
                messageModel.setGroupMemberNames(memberNames);
            }

            // 面板/头部控件/置顶卡片：仅在会话切换时完整重建，避免滚动重置和闪烁
            const bool headerContextChanged =
                (lastScheduledHeaderConversationId != currentConversationId
                 || lastScheduledHeaderTargetId != currentTargetId);
            if (headerContextChanged) {
                lastScheduledHeaderConversationId = currentConversationId;
                lastScheduledHeaderTargetId = currentTargetId;
                updateChatHeader();
            }
        }
        if (doSelection) {
            syncSelectionState();
        }

        if (doConversations) {
            QMetaObject::invokeMethod(dbWorker, "loadAll", Qt::QueuedConnection);
        }
        if (doMessages && !currentConversationId.isEmpty()) {
            QMetaObject::invokeMethod(dbWorker, "loadMessagesForConversation",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, currentConversationId));
        }
    };
    QObject::connect(&chatUiRefreshTimer, &QTimer::timeout, m_mainWindow.get(),
                     [&]() {
                         if (flushScheduledChatUiRefresh) {
                             flushScheduledChatUiRefresh();
                         }
                     });
    const auto scheduleChatUiRefresh = [&](bool refreshConversations = true,
                                           bool refreshMessages = true,
                                           bool refreshHeader = true,
                                           bool refreshSelection = true,
                                           int delayMs = 300,
                                           bool refreshContacts = false) {
        conversationListRefreshPending = conversationListRefreshPending || refreshConversations;
        messageListRefreshPending = messageListRefreshPending || refreshMessages;
        chatHeaderRefreshPending = chatHeaderRefreshPending || refreshHeader;
        selectionStateRefreshPending = selectionStateRefreshPending || refreshSelection;
        contactListRefreshPending = contactListRefreshPending || refreshContacts;
        if (!chatUiRefreshTimer.isActive()) {
            chatUiRefreshTimer.start(qMax(0, delayMs));
        }
    };
    QTimer lanPeerUiRefreshTimer(m_mainWindow.get());
    lanPeerUiRefreshTimer.setSingleShot(true);
    QObject::connect(&lanPeerUiRefreshTimer, &QTimer::timeout, m_mainWindow.get(), [&]() {
        scheduleChatUiRefresh(false, false, false, false, 0, true);
    });

    // 联系人存活状态周期性重评估：
    // 由于 upsertConnectedPeer 不再因 lastPresenceAtMs 更新而报告 "UI changed"，
    // 需要定期触发联系人刷新以检测 Online→Away/Offline 阈值跨越。
    QTimer contactPresenceReevalTimer(m_mainWindow.get());
    contactPresenceReevalTimer.setInterval(30000); // 30 秒
    QObject::connect(&contactPresenceReevalTimer, &QTimer::timeout, m_mainWindow.get(), [&]() {
        scheduleChatUiRefresh(false, false, false, false, 0, true);
    });
    contactPresenceReevalTimer.start();

    scheduleReadReceiptUiRefresh = [&]() {
        scheduleChatUiRefresh(true, true, false, true, 100);
    };

    const IncomingStickerCacheCallback cacheRemoteSticker =
        [](const QString& packId,
           const QString& stickerId,
           const QByteArray& gifData) {
            return !StickerManager::instance()
                        .cacheReceivedSticker(packId, stickerId, gifData)
                        .isEmpty();
        };

    const auto processRemoteSyncedNotifications =
        [&](const QVector<IncomingMessageNotificationEvent>& notifications) {
        if (notifications.isEmpty()) {
            return;
        }

        QHash<QString, ConversationSummary> summariesById;
        for (const ConversationSummary& summary :
             conversationRepository.loadConversationSummaries()) {
            summariesById.insert(QString::fromStdWString(summary.conversationId), summary);
        }

        for (const IncomingMessageNotificationEvent& notification : notifications) {
            const QString conversationId = notification.conversationId.trimmed();
            if (conversationId.isEmpty()) {
                continue;
            }

            const auto summaryIt = summariesById.constFind(conversationId);
            const bool isGroup = groupService.isGroupConversation(conversationId);
            const QString senderId = notification.senderId.trimmed();
            const auto senderPeer =
                peerDirectoryService.findPeerByClientId(toUtf8(senderId));
            const QString senderName = senderPeer.has_value()
                ? displayNameForPeer(*senderPeer)
                : senderId;
            QString conversationTitle;
            if (isGroup) {
                const auto group =
                    groupRepository.findGroupById(conversationId.toStdWString());
                if (group.has_value()) {
                    conversationTitle =
                        QString::fromStdWString(group->groupName).trimmed();
                }
            } else {
                conversationTitle = senderName;
            }
            if (conversationTitle.trimmed().isEmpty()
                && summaryIt != summariesById.constEnd()) {
                conversationTitle =
                    QString::fromStdWString(summaryIt->title).trimmed();
            }

            if (notification.messageType == QStringLiteral("group_file_card")) {
                const QString fileId =
                    notification.payload.value(QStringLiteral("file_id")).toString();
                if (!fileId.isEmpty()) {
                    const GroupFileServiceConfig groupCfg =
                        effectiveGroupFileServiceConfigForGroup(conversationId);
                    QJsonObject resourceReference;
                    resourceReference[QStringLiteral("service_id")] =
                        QStringLiteral("remote-file-service");
                    resourceReference[QStringLiteral("workspace_id")] =
                        groupCfg.workspaceId;
                    resourceReference[QStringLiteral("resource_id")] = fileId;
                    resourceReference[QStringLiteral("resource_kind")] =
                        QStringLiteral("shared_file");
                    resourceReference[QStringLiteral("title")] =
                        notification.payload.value(QStringLiteral("file_name")).toString();
                    if (SharedFileResourceSync::syncIncomingSharedFileResource(
                            resourceReference,
                            QStringLiteral("remote-file-service"),
                            groupCfg,
                            stage2ResourceRepository)) {
                        refreshRuntimeArchitectureState();
                    }
                }
            }

            const QString notificationTitle = isGroup && !senderName.isEmpty()
                ? QStringLiteral("%1 · %2").arg(conversationTitle, senderName)
                : conversationTitle;
            if (notification.messageType == QStringLiteral("nudge")) {
                currentConversationId = conversationId;
                if (isGroup) {
                    currentTargetId.clear();
                    m_mainWindow->showGroupConversation(
                        conversationId, conversationTitle);
                } else {
                    currentTargetId = senderId;
                    m_mainWindow->showDirectConversation(
                        conversationId,
                        senderName.isEmpty() ? QStringLiteral("对方") : senderName);
                }
                restoreMainWindow();
                shakeMainWindow();
                notifyUnreadActivity(
                    isGroup ? QStringLiteral("群窗口抖动提醒")
                            : QStringLiteral("窗口抖动提醒"),
                    isGroup
                        ? QStringLiteral("%1 群有人提醒你").arg(conversationTitle)
                        : QStringLiteral("%1 正在提醒你").arg(
                              senderName.isEmpty() ? QStringLiteral("对方") : senderName));
                continue;
            }

            if (windowCanConsumeIncomingConversation()
                && currentConversationId == conversationId) {
                continue;
            }

            QString preview = notification.preview.trimmed();
            if (notification.messageType == QStringLiteral("group_file_card")) {
                const QString fileName = notification.payload
                    .value(QStringLiteral("file_name")).toString().trimmed();
                preview = QStringLiteral("发送了文件：%1").arg(
                    fileName.isEmpty() ? QStringLiteral("新文件") : fileName);
            }
            const bool mentionsMe =
                notification.mentionedIds.contains(localClientId)
                || notification.mentionedIds.contains(QStringLiteral("__all__"));
            if (mentionsMe) {
                preview.prepend(QStringLiteral("[有人@我] "));
            }
            qInfo().noquote() << "[remote-message-notify] conversation="
                              << conversationId
                              << "message=" << notification.messageId.left(8)
                              << "title=" << notificationTitle;
            notifyUnreadActivity(
                notificationTitle.trimmed().isEmpty()
                    ? QStringLiteral("新消息") : notificationTitle,
                preview.isEmpty() ? QStringLiteral("你收到一条新消息") : preview);
        }
    };

    QTimer remoteMessageHeartbeatTimer(m_mainWindow.get());
    remoteMessageHeartbeatTimer.setInterval(30000);
    const std::function<void()> sendRemoteMessageHeartbeat = [&]() {
        const RemoteChatServiceSettings settings =
            RemoteChatServiceSettingsStore::load();
        if (!remoteChatServiceProbeAllowed(settings)) {
            return;
        }

        QSettings remoteMessageSettings = AppSettings::createSettings();
        const QString remoteMessageDeviceId =
            RemoteMessageDeviceIdentity::loadOrCreate(&remoteMessageSettings,
                                                       localClientId);
        if (remoteMessageDeviceId.isEmpty()) {
            qWarning().noquote()
                << "[remote-message-heartbeat] device id unavailable";
            return;
        }

        const qint64 lastEventId =
            conversationRepository.loadRemoteMessageEventCursor(settings.workspaceId,
                                                                remoteMessageDeviceId);
        ServerMessageClient heartbeatClient(settings, localClientId);
        QString heartbeatError;
        const std::optional<ServerMessageSessionAck> ack =
            heartbeatClient.sendSessionHeartbeat(settings.workspaceId,
                                                 remoteMessageDeviceId,
                                                 lastEventId,
                                                 localAppVersion,
                                                 localRoutingCapabilities,
                                                 &heartbeatError);
        if (!ack || !ack->ok) {
            qWarning().noquote()
                << "[remote-message-heartbeat] failed"
                << "cursor=" << lastEventId
                << "error=" << heartbeatError;
        }
    };
    QObject::connect(&remoteMessageHeartbeatTimer,
                     &QTimer::timeout,
                     m_mainWindow.get(),
                     sendRemoteMessageHeartbeat);
    remoteMessageHeartbeatTimer.start();
    QTimer::singleShot(1000, m_mainWindow.get(), sendRemoteMessageHeartbeat);

    QTimer remoteMessageSyncDebounceTimer(m_mainWindow.get());
    remoteMessageSyncDebounceTimer.setSingleShot(true);
    QTimer remoteMessageSyncRetryTimer(m_mainWindow.get());
    remoteMessageSyncRetryTimer.setSingleShot(true);
    QTimer remoteMessageSyncPollTimer(m_mainWindow.get());
    remoteMessageSyncPollTimer.setInterval(5000);
    QString pendingRemoteMessageSyncReason;
    bool remoteMessageSyncInProgress = false;
    int remoteMessageSyncConsecutiveFailures = 0;
    constexpr qint64 remoteMessageReconciliationIntervalMs = 5 * 60 * 1000;
    qint64 lastRemoteMessageReconciliationAtMs = 0;
    std::function<void(const QString&, int)> scheduleRemoteMessageSync;
    std::function<void(const QString&)> runRemoteMessageSync;

    scheduleRemoteMessageSync = [&](const QString& reason, int delayMs) {
        pendingRemoteMessageSyncReason = reason.trimmed().isEmpty()
            ? QStringLiteral("unspecified")
            : reason.trimmed();
        if (!remoteMessageSyncDebounceTimer.isActive()) {
            remoteMessageSyncDebounceTimer.start(qMax(0, delayMs));
        }
    };

    runRemoteMessageSync = [&](const QString& reason) {
        if (remoteMessageSyncRetryTimer.isActive()
            && reason != QStringLiteral("retry")) {
            qInfo().noquote()
                << "[remote-message-sync] deferred by failure backoff"
                << "reason=" << reason
                << "remainingMs=" << remoteMessageSyncRetryTimer.remainingTime();
            return;
        }
        if (remoteMessageSyncInProgress) {
            scheduleRemoteMessageSync(reason, 2000);
            return;
        }

        remoteMessageSyncInProgress = true;
        const auto syncGuard = qScopeGuard([&]() {
            remoteMessageSyncInProgress = false;
        });

        const RemoteChatServiceSettings settings =
            RemoteChatServiceSettingsStore::load();
        if (!remoteChatServiceProbeAllowed(settings)) {
            remoteMessageSyncConsecutiveFailures = 0;
            remoteMessageSyncRetryTimer.stop();
            return;
        }

        QSettings remoteMessageSettings = AppSettings::createSettings();
        const QString remoteMessageDeviceId =
            RemoteMessageDeviceIdentity::loadOrCreate(&remoteMessageSettings,
                                                       localClientId);
        if (!remoteMessageDeviceId.isEmpty()) {
            ServerMessageClient eventServerClient(settings, localClientId);
            RemoteMessageEventConsumer eventConsumer(
                localClientId,
                settings.workspaceId,
                remoteMessageDeviceId,
                &conversationRepository,
                &eventServerClient,
                100,
                100,
                localAppVersion,
                localRoutingCapabilities,
                cacheRemoteSticker);
            const RemoteMessageEventConsumerResult eventResult =
                eventConsumer.consumeOnce();
            if (eventResult.success) {
                remoteMessageSyncConsecutiveFailures = 0;
                remoteMessageSyncRetryTimer.stop();
                if (eventResult.eventsSeen > 0
                    || eventResult.conversationsSynced > 0
                    || eventResult.sessionsSynced > 0) {
                    scheduleChatUiRefresh(true, true, true, true, 0, true);
                }
                processRemoteSyncedNotifications(eventResult.newIncomingNotifications);
                if (eventResult.newIncomingConversationIds.contains(currentConversationId)) {
                    qInfo().noquote()
                        << "[remote-read-ack] visible conversation synced; checking conversation="
                        << currentConversationId;
                }
                flushReadReceiptsForIncomingConversations(
                    eventResult.newIncomingConversationIds);
                qInfo().noquote() << "[remote-message-events] consumed reason="
                                  << reason
                                  << "events=" << eventResult.eventsSeen
                                  << "triggered=" << eventResult.conversationsTriggered
                                  << "synced=" << eventResult.conversationsSynced
                                  << "sessions=" << eventResult.sessionsSynced
                                  << "failed=" << eventResult.conversationsFailed
                                  << "deliveryAck=" << eventResult.pendingDeliveryAcksAcknowledged
                                  << "/" << eventResult.pendingDeliveryAcksAttempted
                                  << "readAck=" << eventResult.pendingReadAcksAcknowledged
                                  << "/" << eventResult.pendingReadAcksAttempted
                                  << "cursor=" << eventResult.previousEventId
                                  << "->" << eventResult.nextEventId;
                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                const bool reconciliationDue =
                    lastRemoteMessageReconciliationAtMs <= 0
                    || nowMs - lastRemoteMessageReconciliationAtMs
                           >= remoteMessageReconciliationIntervalMs;
                if (!reconciliationDue) {
                    return;
                }
                qInfo().noquote()
                    << "[remote-message-sync] periodic conversation reconciliation"
                    << "reason=" << reason;
            }

            if (!eventResult.success) {
                qWarning().noquote() << "[remote-message-events] failed reason="
                                     << reason
                                     << "events=" << eventResult.eventsSeen
                                     << "triggered=" << eventResult.conversationsTriggered
                                     << "synced=" << eventResult.conversationsSynced
                                     << "sessions=" << eventResult.sessionsSynced
                                     << "failed=" << eventResult.conversationsFailed
                                     << "cursor=" << eventResult.previousEventId
                                     << "->" << eventResult.nextEventId
                                     << "fallback=conversation-sync"
                                     << "error=" << eventResult.errorMessage;
            }
        } else {
            qWarning().noquote()
                << "[remote-message-events] device id unavailable"
                << "fallback=conversation-sync";
        }

        QStringList knownGroupConversationIds;
        for (const Group& group :
             groupRepository.loadGroupsForMember(localClientId.toStdWString())) {
            if (!group.isActive) {
                continue;
            }
            const QString groupId =
                QString::fromStdWString(group.groupId).trimmed();
            if (!groupId.isEmpty()) {
                knownGroupConversationIds.push_back(groupId);
            }
        }

        ServerMessageClient serverClient(settings, localClientId);
        QStringList serverConversationIds;
        QString listConversationError;
        const auto serverConversations =
            serverClient.listConversations(settings.workspaceId, 500,
                                           &listConversationError);
        if (serverConversations.has_value()) {
            for (const ServerConversationRecord& conversation :
                 *serverConversations) {
                const QString conversationId =
                    conversation.conversationId.trimmed();
                if (!conversationId.isEmpty()) {
                    serverConversationIds.push_back(conversationId);
                }
            }
        } else if (!listConversationError.trimmed().isEmpty()) {
            qWarning().noquote()
                << "[remote-message-sync] failed to list service conversations"
                << "reason=" << reason
                << "error=" << listConversationError;
        }

        serverConversationIds.removeDuplicates();
        const QStringList localFallbackConversationIds =
            RemoteMessageSyncCoordinator::serviceConversationIdsForLocalClient(
                localClientId,
                conversationRepository.loadConversationSummaries(),
                knownGroupConversationIds);
        QStringList conversationIds;
        if (serverConversations.has_value()) {
            conversationIds = serverConversationIds;
        } else {
            conversationIds = localFallbackConversationIds;
        }
        if (conversationIds.isEmpty()) {
            if (serverConversations.has_value()) {
                lastRemoteMessageReconciliationAtMs =
                    QDateTime::currentMSecsSinceEpoch();
                remoteMessageSyncConsecutiveFailures = 0;
                remoteMessageSyncRetryTimer.stop();
                return;
            }

            ++remoteMessageSyncConsecutiveFailures;
            const qint64 retryDelayMs =
                remoteMessageSyncRetryDelayMs(remoteMessageSyncConsecutiveFailures);
            qWarning().noquote()
                << "[remote-message-sync] failed to discover conversations"
                << "reason=" << reason
                << "retryMs=" << retryDelayMs
                << "error=" << listConversationError;
            remoteMessageSyncRetryTimer.start(static_cast<int>(retryDelayMs));
            return;
        }

        RemoteMessageSyncCoordinator coordinator(
            localClientId, &conversationRepository, &serverClient, 100, false,
            cacheRemoteSticker);
        const RemoteMessageSyncRunResult result =
            coordinator.syncDirectConversations(conversationIds);

        if (result.storedCount > 0 || result.skippedDuplicateCount > 0) {
            scheduleChatUiRefresh(true, true, false, true, 0);
        }
        processRemoteSyncedNotifications(result.newIncomingNotifications);
        flushReadReceiptsForIncomingConversations(result.newIncomingConversationIds);

        if (result.success) {
            if (serverConversations.has_value()) {
                lastRemoteMessageReconciliationAtMs =
                    QDateTime::currentMSecsSinceEpoch();
            }
            remoteMessageSyncConsecutiveFailures = 0;
            remoteMessageSyncRetryTimer.stop();
            qInfo().noquote() << "[remote-message-sync] completed reason="
                              << reason
                              << "conversations=" << result.attemptedCount
                              << "stored=" << result.storedCount
                              << "duplicates=" << result.skippedDuplicateCount
                              << "deliveryAck=" << result.pendingDeliveryAcksAcknowledged
                              << "/" << result.pendingDeliveryAcksAttempted
                              << "readAck=" << result.pendingReadAcksAcknowledged
                              << "/" << result.pendingReadAcksAttempted;
            return;
        }

        ++remoteMessageSyncConsecutiveFailures;
        const qint64 retryDelayMs =
            remoteMessageSyncRetryDelayMs(remoteMessageSyncConsecutiveFailures);
        qWarning().noquote() << "[remote-message-sync] failed reason="
                             << reason
                             << "attempted=" << result.attemptedCount
                             << "failed=" << result.failedCount
                             << "retryMs=" << retryDelayMs
                             << "error=" << result.errorMessage;
        remoteMessageSyncRetryTimer.start(static_cast<int>(retryDelayMs));
    };

    QObject::connect(&remoteMessageSyncDebounceTimer,
                     &QTimer::timeout,
                     m_mainWindow.get(),
                     [&]() {
                         const QString reason = pendingRemoteMessageSyncReason;
                         pendingRemoteMessageSyncReason.clear();
                         runRemoteMessageSync(reason);
                     });
    QObject::connect(&remoteMessageSyncRetryTimer,
                     &QTimer::timeout,
                     m_mainWindow.get(),
                     [&]() {
                         runRemoteMessageSync(QStringLiteral("retry"));
                     });
    QObject::connect(&remoteMessageSyncPollTimer,
                     &QTimer::timeout,
                     m_mainWindow.get(),
                     [&]() {
                         scheduleRemoteMessageSync(QStringLiteral("periodic"), 0);
                     });
    remoteMessageSyncPollTimer.start();

    // 通话结束时在聊天记录中插入一条通话记录消息
    QObject::connect(&callSession, &CallSession::callEnded, m_mainWindow.get(),
                     [&](const QString& /*callId*/, const QString& peerId,
                         const QString& result, qint64 durationMs) {
                         const QString conversationId =
                             DirectConversationAddressing::conversationIdForPeers(localClientId, peerId);
                         if (conversationId.isEmpty()) return;

                         const QString bodyJson = QStringLiteral(
                             "{\"result\":\"%1\",\"durationMs\":%2}")
                             .arg(result)
                             .arg(durationMs);

                         ChatMessage callMsg;
                         callMsg.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdWString();
                         callMsg.conversationId = conversationId.toStdWString();
                         callMsg.senderId = localClientId.toStdWString();
                         callMsg.body = bodyJson.toStdWString();
                         callMsg.messageType = L"call_record";
                         callMsg.createdAtMs = QDateTime::currentMSecsSinceEpoch();
                         callMsg.deliveryState = MessageDeliveryState::Sent;

                         conversationRepository.appendMessage(callMsg);
                         scheduleChatUiRefresh(true, currentConversationId == conversationId, false, false);
                     });

    QHash<QString, int> pendingDirectRetryFailureCounts;
    QHash<QString, qint64> pendingDirectRetryNotBeforeMs;
    const auto clearPendingDirectRetryBackoff = [&](const QString& messageId) {
        pendingDirectRetryFailureCounts.remove(messageId);
        pendingDirectRetryNotBeforeMs.remove(messageId);
    };
    const auto deferPendingDirectRetry = [&](const QString& messageId) {
        const int failures = pendingDirectRetryFailureCounts.value(messageId, 0) + 1;
        pendingDirectRetryFailureCounts.insert(messageId, failures);
        const qint64 delayMs = reliableDirectMessageRetryDelayMs(failures);
        pendingDirectRetryNotBeforeMs.insert(
            messageId, QDateTime::currentMSecsSinceEpoch() + delayMs);
        return delayMs;
    };

    const auto unsupportedPendingMessageCountForTarget = [&](const QString& targetClientId) {
        if (targetClientId.isEmpty()) {
            return 0;
        }
        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(localClientId, targetClientId);
        if (conversationId.isEmpty()) {
            return 0;
        }

        const auto pendingMessages =
            conversationRepository.loadPendingOutgoingMessages(conversationId.toStdWString(),
                                                               localClientId.toStdWString());
        int unsupportedCount = 0;
        for (const auto& message : pendingMessages) {
            if (!message.attachmentName.empty()) {
                ++unsupportedCount;
            }
        }

        return unsupportedCount;
    };
    const auto resumableOutgoingFileTaskCountForTarget = [&](const QString& targetClientId) {
        if (targetClientId.isEmpty()) {
            return 0;
        }

        int taskCount = 0;
        const auto resumableTasks = fileTransferService.loadResumableTasks();
        for (const auto& task : resumableTasks) {
            if (task.direction != FileTransferDirection::Outgoing) {
                continue;
            }

            if (QString::fromStdWString(task.peerClientId) != targetClientId) {
                continue;
            }

            ++taskCount;
        }

        return taskCount;
    };
    const auto flushPendingMessagesForTarget = [&](const QString& targetClientId, PeerConnection* connection) {
        if (targetClientId.isEmpty() || !connection || !connection->isConnected()) {
            return 0;
        }

        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(localClientId, targetClientId);
        if (conversationId.isEmpty()) {
            return 0;
        }

        const auto pendingMessages =
            conversationRepository.loadPendingOutgoingMessages(conversationId.toStdWString(),
                                                               localClientId.toStdWString());
        ReliableDirectMessageSender directSender(
            localClientId,
            &conversationRepository,
            nullptr,
             [&](const ReliableDirectMessageP2PRequest& p2pRequest,
                 QString* errorMessage) -> bool {
                 MessageEnvelope envelope;
                 QString buildError;
                 if (!ChatService::buildEnvelope(localClientId,
                                                  &conversationRepository,
                                                  p2pRequest.messageId,
                                                  p2pRequest.targetId,
                                                  &envelope,
                                                  &buildError)) {
                     if (errorMessage) {
                         *errorMessage = buildError.trimmed().isEmpty()
                             ? QStringLiteral("direct message envelope build failed")
                             : buildError;
                     }
                     return false;
                }
                if (!connection->isConnected()) {
                    if (errorMessage) {
                        *errorMessage = QStringLiteral("p2p peer is not connected");
                    }
                    return false;
                }
                return connection->sendPayload(
                    QByteArray::fromStdString(MessageCodec::encode(envelope)));
            });
        RemoteChatServiceSettings resendSettings =
            RemoteChatServiceSettingsStore::load();
        resendSettings.mode = RemoteChatTransportMode::P2POnly;
        resendSettings.allowP2PFallback = true;
         int resentCount = 0;
         for (const auto& message : pendingMessages) {
             if (message.messageType != L"text"
                 || !message.attachmentName.empty()) {
                 continue;
             }
             const QString messageId = QString::fromStdWString(message.messageId);
             if (messageId.isEmpty()) {
                 continue;
             }

             // A real reconnect is stronger evidence than a periodic poll, so
             // let it retry immediately even if an earlier route was backed off.
             clearPendingDirectRetryBackoff(messageId);

            ReliableDirectMessageSendRequest sendRequest;
            sendRequest.conversationId = conversationId;
            sendRequest.targetId = targetClientId;
            sendRequest.settings = resendSettings;
            sendRequest.serviceReachable = false;
            sendRequest.receiverServerCapable = false;
            sendRequest.p2pAvailable = true;
            sendRequest.requireP2PDeliveryReceipt = true;
            const ReliableDirectMessageSendResult sendResult =
                directSender.retryText(messageId, sendRequest);
            qInfo().noquote() << "[msg-resend] msgId=" << messageId.left(8)
                              << "sender=" << localClientId.left(8)
                              << "target=" << targetClientId.left(8)
                              << "sent=" << sendResult.success
                              << "channel=" << transportChannelName(sendResult.channelUsed);
             if (!sendResult.success) {
                 const qint64 retryDelayMs = deferPendingDirectRetry(messageId);
                // 发送失败（TLS 升级中或 socket 异常），保留 pending 状态以便下次重试
                qWarning().noquote() << "[msg-resend] reliable retry failed, keeping message pending msgId="
                                      << messageId.left(8)
                                      << "retryMs=" << retryDelayMs
                                      << "error=" << sendResult.errorMessage;
                 continue;
             }
             if (sendResult.channelUsed == TransportChannel::P2P) {
                 deferPendingDirectRetry(messageId);
             } else {
                 clearPendingDirectRetryBackoff(messageId);
             }
             ++resentCount;
        }

        if (resentCount > 0) {
            scheduleChatUiRefresh(true, currentConversationId == conversationId, false, false);
        }
        return resentCount;
    };
    QHash<QString, qint64> pendingGroupDrainCursorByTarget;
    const auto flushPendingGroupEnvelopesForTarget =
        [&](const QString& targetClientId, PeerConnection* connection) {
            const QString normalizedTargetId = targetClientId.trimmed();
            if (normalizedTargetId.isEmpty()
                || !connection
                || !connection->isConnected()) {
                return 0;
            }

            constexpr int kBatchSize = 200;
            constexpr int kMaxBatchesPerPass = 5;
            const bool retainUntilDeliveryReceipt =
                peerDeliveryReceiptCapabilities.value(normalizedTargetId, false);
            qint64 afterId = pendingGroupDrainCursorByTarget.value(normalizedTargetId, 0);
            int sentCount = 0;

            for (int batchIndex = 0;
                 batchIndex < kMaxBatchesPerPass;
                 ++batchIndex) {
                const auto pendingEnvelopes =
                    conversationRepository.loadPendingGroupEnvelopesAfterId(
                        normalizedTargetId, afterId, kBatchSize);
                if (pendingEnvelopes.empty()) {
                    pendingGroupDrainCursorByTarget.insert(normalizedTargetId, 0);
                    break;
                }

                QVector<qint64> deleteIds;
                deleteIds.reserve(static_cast<int>(pendingEnvelopes.size()));
                bool writeFailed = false;
                for (const auto& pending : pendingEnvelopes) {
                    if (!connection->isConnected()
                        || !connection->sendPayload(pending.envelopeBlob)) {
                        qWarning().noquote()
                            << "[group-pending] sendPayload failed for envelope id="
                            << pending.id
                            << "target=" << normalizedTargetId.left(8)
                            << ", keeping in pending queue";
                        writeFailed = true;
                        break;
                    }

                    ++sentCount;
                    afterId = pending.id;
                    if (retainUntilDeliveryReceipt) {
                        qInfo().noquote()
                            << "[group-pending] retaining envelope until delivery receipt"
                            << "id=" << pending.id
                            << "target=" << normalizedTargetId.left(8);
                    } else {
                        deleteIds.append(pending.id);
                    }
                }

                if (!deleteIds.isEmpty()
                    && !conversationRepository.deletePendingGroupEnvelopes(deleteIds)) {
                    qWarning().noquote()
                        << "[group-pending] failed to delete delivered rows target="
                        << normalizedTargetId.left(8)
                        << "count=" << deleteIds.size();
                    pendingGroupDrainCursorByTarget.insert(normalizedTargetId, 0);
                    break;
                }
                if (writeFailed) {
                    pendingGroupDrainCursorByTarget.insert(normalizedTargetId, 0);
                    break;
                }

                pendingGroupDrainCursorByTarget.insert(normalizedTargetId, afterId);
                if (pendingEnvelopes.size()
                    < static_cast<std::size_t>(kBatchSize)) {
                    // The next pass starts from the oldest still-unconfirmed row.
                    // This rotates receipt-capable backlogs without starving rows
                    // beyond the first page.
                    pendingGroupDrainCursorByTarget.insert(normalizedTargetId, 0);
                    break;
                }
            }
            return sentCount;
        };
    const auto requestFileTransferResumeForTarget = [&](const QString& targetClientId, PeerConnection* connection) {
        if (targetClientId.isEmpty() || !connection || !connection->isConnected()) {
            return 0;
        }

        int requestedCount = 0;
        const auto resumableTasks = fileTransferService.loadResumableTasks();
        for (const auto& task : resumableTasks) {
            const QString peerClientId = QString::fromStdWString(task.peerClientId);
            if (peerClientId != targetClientId || task.state == FileTransferState::WaitingAccept) {
                continue;
            }

            MessageEnvelope resumeEnvelope;
            if (!fileTransferService.buildResumeRequestEnvelope(QString::fromStdWString(task.taskId),
                                                               localClientId,
                                                               targetClientId,
                                                               &resumeEnvelope)) {
                continue;
            }

            connection->sendPayload(QByteArray::fromStdString(MessageCodec::encode(resumeEnvelope)));
            ++requestedCount;
        }

        return requestedCount;
    };
    QSet<QString> activeOutgoingDataPumpTasks;
    const auto isOutgoingTransferKickableState = [](FileTransferState state) {
        return state == FileTransferState::WaitingAccept
               || state == FileTransferState::ReadyToTransfer
               || state == FileTransferState::Interrupted
               || state == FileTransferState::PendingOffer;
    };
    const auto isOutgoingTransferInFlightState = [](FileTransferState state) {
        return state == FileTransferState::Transferring
               || state == FileTransferState::Completing
               || state == FileTransferState::Completed;
    };
    std::function<void(const QString&, const QString&)> scheduleOptimisticDataPump;
    const auto resendPendingFileOffersForTarget = [&](const QString& targetClientId, PeerConnection* connection) {
        if (targetClientId.isEmpty() || !connection || !connection->isConnected()) {
            return 0;
        }

        int resentCount = 0;
        const auto resumableTasks = fileTransferService.loadResumableTasks();
        for (const auto& task : resumableTasks) {
            const QString peerClientId = QString::fromStdWString(task.peerClientId);
            if (peerClientId != targetClientId
                || (task.state != FileTransferState::WaitingAccept
                    && task.state != FileTransferState::PendingOffer)) {
                continue;
            }

            MessageEnvelope offerEnvelope;
            if (!fileTransferService.buildOfferEnvelope(task,
                                                        localClientId,
                                                        targetClientId,
                                                        QString(),
                                                        activeFileTransferPort,
                                                        &offerEnvelope)) {
                continue;
            }

            connection->sendPayload(QByteArray::fromStdString(MessageCodec::encode(offerEnvelope)));
            if (scheduleOptimisticDataPump) {
                scheduleOptimisticDataPump(QString::fromStdWString(task.taskId), targetClientId);
            }
            ++resentCount;
        }

        return resentCount;
    };
    const auto ensureFileTransferMessage = [&](const FileTransferTask& task,
                                               const QString& senderId,
                                               MessageDeliveryState state,
                                               qint64 createdAtMs) {
        const QString taskId = QString::fromStdWString(task.taskId);
        if (taskId.isEmpty()) {
            return false;
        }

        ChatMessage existingMessage;
        if (conversationRepository.findMessageById(taskId, &existingMessage)) {
            return true;
        }

        const QString conversationId = QString::fromStdWString(task.conversationId);
        const QString fileName = QString::fromStdWString(task.fileName);
        const QString localFilePath = senderId == localClientId ? QString::fromStdWString(task.sourcePath)
                                                                : QString::fromStdWString(task.targetPath);
        QString conversationTitle = QString::fromStdWString(task.peerClientId);
        const QString taskGroupId = QString::fromStdWString(task.groupId).trimmed();
        const QString resolvedGroupId = taskGroupId.isEmpty() ? conversationId.trimmed() : taskGroupId;
        if (!resolvedGroupId.isEmpty()) {
            if (const auto groupOpt = groupRepository.findGroupById(resolvedGroupId.toStdWString());
                groupOpt.has_value()) {
                conversationTitle = QString::fromStdWString(groupOpt->groupName);
            }
        }
        if (conversationTitle.trimmed().isEmpty()) {
            conversationTitle = conversationId;
        }
        const ChatMessage message{
            task.taskId,
            task.conversationId,
            senderId.toStdWString(),
            fileTransferStatusText(fileName,
                                   task.state,
                                   task.direction,
                                   displayBytesForTransferState(task, task.state),
                                   task.fileSize)
                .toStdWString(),
            createdAtMs,
            state,
            task.fileName,
            localFilePath.toStdWString(),
            std::wstring(L"text"),
            std::wstring()
        };
        if (!conversationRepository.appendMessage(message)) {
            return false;
        }

        upsertConversationSummaryPreservingLatest(
            conversationRepository,
            ConversationSummary{
                task.conversationId,
                conversationTitle.toStdWString(),
                fileTransferStatusText(fileName,
                                       task.state,
                                       task.direction,
                                       displayBytesForTransferState(task, task.state),
                                       task.fileSize)
                    .toStdWString(),
                createdAtMs
            });
        return true;
    };
    const auto syncFileTransferMessageState = [&](const FileTransferTask& task,
                                                  FileTransferState state,
                                                  MessageDeliveryState deliveryState,
                                                  const QString& localFilePath = QString()) {
        const QString taskId = QString::fromStdWString(task.taskId);
        const QString fileName = QString::fromStdWString(task.fileName);
        const QString body =
            fileTransferStatusText(fileName,
                                   state,
                                   task.direction,
                                   displayBytesForTransferState(task, state),
                                   task.fileSize);
        QString conversationTitle = QString::fromStdWString(task.peerClientId);
        const QString taskGroupId = QString::fromStdWString(task.groupId).trimmed();
        const QString resolvedGroupId = taskGroupId.isEmpty()
                                            ? QString::fromStdWString(task.conversationId).trimmed()
                                            : taskGroupId;
        if (!resolvedGroupId.isEmpty()) {
            if (const auto groupOpt = groupRepository.findGroupById(resolvedGroupId.toStdWString());
                groupOpt.has_value()) {
                conversationTitle = QString::fromStdWString(groupOpt->groupName);
            }
        }
        if (conversationTitle.trimmed().isEmpty()) {
            conversationTitle = QString::fromStdWString(task.conversationId);
        }
        conversationRepository.updateMessageBody(taskId, body);
        const MessageDeliveryState effectiveDeliveryState =
            effectiveFileTransferDeliveryState(
                task.direction,
                currentConversationId == QString::fromStdWString(task.conversationId),
                deliveryState);
        conversationRepository.updateDeliveryStatePreservingRead(taskId, effectiveDeliveryState);
        if (!localFilePath.isEmpty()) {
            conversationRepository.updateAttachmentMetadata(taskId, fileName, localFilePath);
        }
        QString storedConversationId;
        QString storedBody;
        qint64 messageCreatedAtMs = task.createdAtMs;
        QString storedAttachmentName;
        QString storedLocalFilePath;
        conversationRepository.findMessageStorageRecordById(taskId,
                                                            &storedConversationId,
                                                            &storedBody,
                                                            &messageCreatedAtMs,
                                                            &storedAttachmentName,
                                                            &storedLocalFilePath);
        const QString summaryConversationId = storedConversationId.trimmed().isEmpty()
                                                  ? QString::fromStdWString(task.conversationId)
                                                  : storedConversationId.trimmed();
        const bool taskMessageStillLatest =
            conversationRepository.loadLatestMessageIdForConversation(summaryConversationId) == taskId;
        if (taskMessageStillLatest) {
            upsertConversationSummaryPreservingLatest(
                conversationRepository,
                ConversationSummary{
                    summaryConversationId.toStdWString(),
                    conversationTitle.toStdWString(),
                    body.toStdWString(),
                    messageCreatedAtMs
                });
        }
        scheduleDeferredTransferRefresh(
            state != FileTransferState::Transferring
                && currentConversationId == QString::fromStdWString(task.conversationId),
            state != FileTransferState::Transferring,
            true);
    };
    const auto interruptFileTransfersForTargets = [&](const QStringList& targetIds) {
        if (targetIds.isEmpty()) {
            return;
        }

        const auto resumableTasks = fileTransferService.loadResumableTasks();
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        for (const auto& task : resumableTasks) {
            const QString peerClientId = QString::fromStdWString(task.peerClientId);
            if (!targetIds.contains(peerClientId) || task.state == FileTransferState::WaitingAccept) {
                continue;
            }

            fileTransferService.markTaskState(QString::fromStdWString(task.taskId),
                                              FileTransferState::Interrupted,
                                              task.bytesCompleted,
                                              task.lastChunkIndex,
                                              QStringLiteral("peer_disconnected"),
                                              QStringLiteral("\u5BF9\u7AEF\u8FDE\u63A5\u5DF2\u65AD\u5F00"),
                                              nowMs);
            syncFileTransferMessageState(task,
                                         FileTransferState::Interrupted,
                                         task.direction == FileTransferDirection::Outgoing
                                             ? MessageDeliveryState::Pending
                                             : MessageDeliveryState::Received,
                                         task.direction == FileTransferDirection::Outgoing
                                             ? QString::fromStdWString(task.sourcePath)
                                             : QString::fromStdWString(task.tempPath));
        }
    };
    const auto restoreResumableFileTransferMessages = [&]() {
        const auto resumableTasks = fileTransferService.loadResumableTasks();
        // 群文件 fan-out 会为每位成员创建独立 task（taskId 各不相同），
        // 恢复时只需为同一群+同一源文件创建一条可见消息，避免 UI 出现重复卡片。
        QSet<QString> seenGroupFileKeys;
        for (const auto& task : resumableTasks) {
            const bool outgoing = task.direction == FileTransferDirection::Outgoing;
            const QString senderId = outgoing ? localClientId : QString::fromStdWString(task.peerClientId);
            const MessageDeliveryState deliveryState = outgoing ? MessageDeliveryState::Pending
                                                                : MessageDeliveryState::Received;
            const QString localFilePath = outgoing ? QString::fromStdWString(task.sourcePath)
                                                   : QString::fromStdWString(task.tempPath);
            const QString groupId = QString::fromStdWString(task.groupId).trimmed();
            if (outgoing && !groupId.isEmpty()) {
                const QString key = groupId + QChar('|') + QString::fromStdWString(task.sourcePath);
                if (seenGroupFileKeys.contains(key)) {
                    continue;  // 同一群文件的 fan-out 副本，跳过
                }
                seenGroupFileKeys.insert(key);
            }
            ensureFileTransferMessage(task, senderId, deliveryState, task.createdAtMs);
            syncFileTransferMessageState(task, task.state, deliveryState, localFilePath);
        }
    };
    const auto sendFileControlPayload = [&](const FileControlPayload& payload, PeerConnection* connection) {
        if (!connection || !connection->isConnected()) {
            return false;
        }

        MessageEnvelope envelope;
        if (!FileTransferService::envelopeFromPayload(payload, &envelope)) {
            return false;
        }

        return connection->sendPayload(QByteArray::fromStdString(MessageCodec::encode(envelope)));
    };
    const auto notifyFileTransferFailureToSender =
        [&](const FileTransferTask& task, const QString& errorText) {
        const QString peerClientId = QString::fromStdWString(task.peerClientId);
        if (peerClientId.trimmed().isEmpty()) {
            return false;
        }
        PeerConnection* controlConnection = connectionsByTargetId.value(peerClientId, nullptr);
        if (!controlConnection || !controlConnection->isConnected()) {
            return false;
        }

        FileControlPayload failPayload;
        failPayload.type = FileControlType::Fail;
        failPayload.taskId = toUtf8(QString::fromStdWString(task.taskId));
        failPayload.senderId = toUtf8(localClientId);
        failPayload.targetId = toUtf8(peerClientId);
        failPayload.conversationId = toUtf8(QString::fromStdWString(task.conversationId));
        failPayload.fileName = toUtf8(QString::fromStdWString(task.fileName));
        failPayload.fileHash = toUtf8(QString::fromStdWString(task.fileHash));
        failPayload.reason = toUtf8(errorText);
        return sendFileControlPayload(failPayload, controlConnection);
    };
    const auto finalizeIncomingTask = [&](const FileTransferTask& task) {
        const QString taskId = QString::fromStdWString(task.taskId);
        const QString tempPath = QString::fromStdWString(task.tempPath);
        const QString targetPath = QString::fromStdWString(task.targetPath);
        if (taskId.isEmpty() || tempPath.isEmpty() || targetPath.isEmpty()) {
            return;
        }

        const qint64 tempFileSize = QFileInfo(tempPath).size();
        qInfo() << "[FileTransfer] finalizeIncomingTask: taskId=" << taskId
                << "tempPath=" << tempPath << "targetPath=" << targetPath
                << "tempFileSize=" << tempFileSize << "expectedSize=" << task.fileSize;
        if (!QFileInfo::exists(tempPath) || tempFileSize != task.fileSize) {
            qWarning() << "[FileTransfer] SIZE MISMATCH: temp=" << tempFileSize
                       << "expected=" << task.fileSize
                       << "diff=" << (task.fileSize - tempFileSize) << "bytes";
            const QString errorText = QStringLiteral("\u6587\u4EF6\u5927\u5C0F\u6821\u9A8C\u5931\u8D25");
            fileTransferService.markTaskState(taskId,
                                              FileTransferState::Failed,
                                              task.bytesCompleted,
                                              task.lastChunkIndex,
                                              QStringLiteral("size_mismatch"),
                                              errorText,
                                              QDateTime::currentMSecsSinceEpoch());
            syncFileTransferMessageState(task,
                                         FileTransferState::Failed,
                                         MessageDeliveryState::Failed,
                                         tempPath);
            notifyFileTransferFailureToSender(task, errorText);
            m_mainWindow->setStatusMessage(
                QStringLiteral("\u6587\u4EF6 %1 \u63A5\u6536\u4E0D\u5B8C\u6574")
                    .arg(QString::fromStdWString(task.fileName)),
                4000);
            scheduleDeferredTransferRefresh();
            return;
        }
        if (QFileInfo::exists(targetPath)) {
            QFile::remove(targetPath);
        }
        if (!QFile::rename(tempPath, targetPath)) {
            const QString errorText = QStringLiteral("\u65E0\u6CD5\u5B8C\u6210\u6587\u4EF6\u843D\u76D8");
            fileTransferService.markTaskState(taskId,
                                              FileTransferState::Failed,
                                              task.bytesCompleted,
                                              task.lastChunkIndex,
                                              QStringLiteral("finalize_failed"),
                                              errorText,
                                              QDateTime::currentMSecsSinceEpoch());
            syncFileTransferMessageState(task,
                                         FileTransferState::Failed,
                                         MessageDeliveryState::Failed,
                                         tempPath);
            notifyFileTransferFailureToSender(task, errorText);
            m_mainWindow->setStatusMessage(
                QStringLiteral("\u6587\u4EF6 %1 \u843D\u76D8\u5931\u8D25")
                    .arg(QString::fromStdWString(task.fileName)),
                4000);
            scheduleDeferredTransferRefresh();
            return;
        }

        conversationRepository.updateAttachmentMetadata(taskId,
                                                        QString::fromStdWString(task.fileName),
                                                        targetPath);
        fileTransferService.markTaskState(taskId,
                                          FileTransferState::Completed,
                                          task.fileSize,
                                          task.chunkCount > 0 ? task.chunkCount - 1 : -1,
                                          QString(),
                                          QString(),
                                          QDateTime::currentMSecsSinceEpoch());
        syncFileTransferMessageState(task,
                                     FileTransferState::Completed,
                                     MessageDeliveryState::Received,
                                     targetPath);

        const QString peerClientId = QString::fromStdWString(task.peerClientId);
        if (PeerConnection* controlConnection = connectionsByTargetId.value(peerClientId, nullptr)) {
            FileControlPayload completePayload;
            completePayload.type = FileControlType::Complete;
            completePayload.taskId = toUtf8(taskId);
            completePayload.senderId = toUtf8(localClientId);
            completePayload.targetId = toUtf8(peerClientId);
            completePayload.conversationId = toUtf8(QString::fromStdWString(task.conversationId));
            completePayload.fileName = toUtf8(QString::fromStdWString(task.fileName));
            completePayload.fileHash = toUtf8(QString::fromStdWString(task.fileHash));
            sendFileControlPayload(completePayload, controlConnection);
        }

        scheduleDeferredTransferRefresh();
    };

    // ──── 文件接收：worker 线程完成后的主线程回调 ────
    QObject::connect(fileReceiveWorker, &FileReceiveWorker::chunkProcessed, m_mainWindow.get(),
                     [&](const QString& taskId,
                         const FileTransferTask& task,
                         qint64 bytesCompleted,
                         bool transferCompleted,
                         bool shouldSendProgress,
                         const std::vector<int>& completedChunks) {
        Q_UNUSED(bytesCompleted)
        const QString tempPath = QString::fromStdWString(task.tempPath);
        if (shouldSendProgress) {
            syncFileTransferMessageState(task,
                                         transferCompleted ? FileTransferState::Completing
                                                           : FileTransferState::Transferring,
                                         MessageDeliveryState::Received,
                                         tempPath);
        }
        const QString peerClientId = QString::fromStdWString(task.peerClientId);
        if (shouldSendProgress) {
            if (PeerConnection* controlConnection = connectionsByTargetId.value(peerClientId, nullptr)) {
                FileControlPayload progressPayload;
                progressPayload.type = FileControlType::Progress;
                progressPayload.taskId = toUtf8(taskId);
                progressPayload.senderId = toUtf8(localClientId);
                progressPayload.targetId = toUtf8(peerClientId);
                progressPayload.conversationId = toUtf8(QString::fromStdWString(task.conversationId));
                progressPayload.completedChunks = completedChunks;
                sendFileControlPayload(progressPayload, controlConnection);
            }
        }
        if (transferCompleted) {
            finalizeIncomingTask(task);
        }
    });
    QObject::connect(fileReceiveWorker, &FileReceiveWorker::chunkFailed, m_mainWindow.get(),
                     [&](const QString& taskId, int chunkIndex, const QString& reason) {
        qWarning() << "[FileReceiveWorker] chunk failed: taskId=" << taskId
                   << "chunk=" << chunkIndex << "reason=" << reason;
        FileTransferTask failedTask;
        if (!fileTransferService.loadTask(taskId, &failedTask)) {
            return;
        }

        fileTransferService.markTaskState(taskId,
                                          FileTransferState::Failed,
                                          failedTask.bytesCompleted,
                                          failedTask.lastChunkIndex,
                                          QStringLiteral("chunk_failed"),
                                          reason,
                                          QDateTime::currentMSecsSinceEpoch());
        syncFileTransferMessageState(failedTask,
                                     FileTransferState::Failed,
                                     MessageDeliveryState::Failed,
                                     QString::fromStdWString(failedTask.tempPath));
        notifyFileTransferFailureToSender(failedTask, reason);
        scheduleDeferredTransferRefresh();
    });

    const auto handleIncomingChunk = [&](const FileTransferChunkHeader& header, const QByteArray& payload) {
        // 将 chunk 处理分派到文件接收工作线程，避免主线程磁盘 I/O 阻塞
        QMetaObject::invokeMethod(fileReceiveWorker, "processChunk",
                                  Qt::QueuedConnection,
                                  Q_ARG(FileTransferChunkHeader, header),
                                  Q_ARG(QByteArray, payload));
    };
    const auto lastCompletedChunkIndex = [](const std::vector<int>& completedChunks) {
        if (completedChunks.empty()) {
            return -1;
        }
        return *std::max_element(completedChunks.begin(), completedChunks.end());
    };
    const auto markOutgoingAwaitingCompletion =
        [&](const FileTransferTask& task,
            const QString& taskId,
            const QString& sourceFilePath,
            const std::vector<int>& remoteCompletedChunkVector) {
        activeOutgoingDataPumpTasks.remove(taskId);

        const qint64 acknowledgedBytes =
            std::clamp(completedBytesForTask(task, remoteCompletedChunkVector), 0LL, task.fileSize);
        const int acknowledgedLastChunkIndex = lastCompletedChunkIndex(remoteCompletedChunkVector);
        const qint64 completingStartedAtMs = QDateTime::currentMSecsSinceEpoch();
        fileTransferService.markTaskState(taskId,
                                          FileTransferState::Completing,
                                          acknowledgedBytes,
                                          acknowledgedLastChunkIndex,
                                          QString(), QString(),
                                          completingStartedAtMs);

        FileTransferTask refreshedTask = task;
        refreshedTask.bytesCompleted = acknowledgedBytes;
        refreshedTask.lastChunkIndex = acknowledgedLastChunkIndex;
        syncFileTransferMessageState(refreshedTask, FileTransferState::Completing,
                                     MessageDeliveryState::Sent, sourceFilePath);
        scheduleDeferredTransferRefresh();

        constexpr int kOutgoingCompleteTimeoutMs = 120000;
        QTimer::singleShot(kOutgoingCompleteTimeoutMs,
                           m_mainWindow.get(),
                           [&, taskId, sourceFilePath]() {
            FileTransferTask timeoutTask;
            if (!fileTransferService.loadTask(taskId, &timeoutTask)
                || timeoutTask.direction != FileTransferDirection::Outgoing
                || timeoutTask.state != FileTransferState::Completing) {
                return;
            }

            const QString timeoutMessage =
                QStringLiteral("等待对方完成确认超时");
            fileTransferService.markTaskState(taskId,
                                              FileTransferState::Interrupted,
                                              timeoutTask.bytesCompleted,
                                              timeoutTask.lastChunkIndex,
                                              QStringLiteral("complete_timeout"),
                                              timeoutMessage,
                                              QDateTime::currentMSecsSinceEpoch());
            syncFileTransferMessageState(timeoutTask,
                                         FileTransferState::Interrupted,
                                         MessageDeliveryState::Pending,
                                         sourceFilePath);
            scheduleDeferredTransferRefresh();
        });
    };
    const auto startOutgoingDataTransfer = [&](const QString& taskId,
                                               const QString& targetClientId,
                                               PeerConnection* controlConnection) -> bool {
        if (taskId.isEmpty() || targetClientId.isEmpty() || !controlConnection) {
            return false;
        }

        FileTransferTask task;
        if (!fileTransferService.loadTask(taskId, &task)
            || task.direction != FileTransferDirection::Outgoing) {
            return false;
        }
        if (isOutgoingTransferInFlightState(task.state)) {
            return true;
        }
        if (activeOutgoingDataPumpTasks.contains(taskId)) {
            return true;
        }
        // 限制同时活跃的发送数据泵数量，避免多路群文件传输并发导致主线程拥塞
        constexpr int kMaxConcurrentOutgoingPumps = 2;
        if (activeOutgoingDataPumpTasks.size() >= kMaxConcurrentOutgoingPumps) {
            return false;
        }
        activeOutgoingDataPumpTasks.insert(taskId);

        const QString sourceFilePath = QString::fromStdWString(task.sourcePath);

        const auto completedChunkVector = fileTransferRepository.loadCompletedChunkIndexes(taskId);
        QSet<int> completedChunks;
        for (const int ci : completedChunkVector) {
            completedChunks.insert(ci);
        }

        if (task.chunkCount > 0 && completedChunks.size() >= task.chunkCount) {
            markOutgoingAwaitingCompletion(task,
                                           taskId,
                                           sourceFilePath,
                                           completedChunkVector);
            return true;
        }

        fileTransferService.markTaskState(taskId,
                                          FileTransferState::Transferring,
                                          task.bytesCompleted,
                                          task.lastChunkIndex,
                                          QString(), QString(),
                                          QDateTime::currentMSecsSinceEpoch());
        syncFileTransferMessageState(task, FileTransferState::Transferring,
                                     MessageDeliveryState::Sent, sourceFilePath);

        auto remoteCompletedChunkSet = std::make_shared<QSet<int>>(completedChunks);
        auto dispatchedChunkSet = std::make_shared<QSet<int>>(completedChunks);
        auto nextChunkIndex = std::make_shared<int>(0);
        while (*nextChunkIndex < task.chunkCount && dispatchedChunkSet->contains(*nextChunkIndex)) {
            ++(*nextChunkIndex);
        }
        auto batchInFlight = std::make_shared<bool>(false);
        auto completedBytesFromSet = std::make_shared<std::function<qint64(const QSet<int>&)>>([task](const QSet<int>& chunkSet) {
            std::vector<int> chunkVector;
            chunkVector.reserve(static_cast<std::size_t>(chunkSet.size()));
            for (const int chunkIndex : chunkSet) {
                chunkVector.push_back(chunkIndex);
            }
            return completedBytesForTask(task, chunkVector);
        });
        auto pumpLoop = std::make_shared<std::function<void()>>();
        auto failTransfer = std::make_shared<
            std::function<void(qint64, int, const QString&, const QString&)>>(
            [&, taskId, sourceFilePath, task](qint64 bytesCompleted,
                                             int lastChunkIndex,
                                             const QString& errorCode,
                                             const QString& errorText) {
            activeOutgoingDataPumpTasks.remove(taskId);
            fileTransferService.markTaskState(taskId,
                                              FileTransferState::Interrupted,
                                              bytesCompleted,
                                              lastChunkIndex,
                                              errorCode,
                                              errorText,
                                              QDateTime::currentMSecsSinceEpoch());
            FileTransferTask refreshedTask = task;
            refreshedTask.bytesCompleted = bytesCompleted;
            refreshedTask.lastChunkIndex = lastChunkIndex;
            syncFileTransferMessageState(refreshedTask, FileTransferState::Interrupted,
                                         MessageDeliveryState::Pending, sourceFilePath);
            scheduleDeferredTransferRefresh();
        });
        constexpr int kChunksPerPumpTick = 4;
        QPointer<PeerConnection> safeControlConnection(controlConnection);
        *pumpLoop = [&, taskId, targetClientId, safeControlConnection, sourceFilePath,
                     remoteCompletedChunkSet, dispatchedChunkSet, nextChunkIndex,
                     batchInFlight, completedBytesFromSet,
                     failTransfer, pumpLoop, task, kChunksPerPumpTick]() {
            if (*batchInFlight) {
                return;
            }
            *batchInFlight = true;

            const QSet<int> dispatchedSnapshot = *dispatchedChunkSet;
            const int startChunkIndex = *nextChunkIndex;
            auto* watcher =
                new QFutureWatcher<FileTransferService::PreparedOutgoingChunkBatch>(m_mainWindow.get());
            QObject::connect(
                watcher,
                &QFutureWatcher<FileTransferService::PreparedOutgoingChunkBatch>::finished,
                m_mainWindow.get(),
                [&, watcher, taskId, targetClientId, safeControlConnection, sourceFilePath,
                 remoteCompletedChunkSet, dispatchedChunkSet, nextChunkIndex,
                 batchInFlight, completedBytesFromSet,
                 failTransfer, pumpLoop, task]() {
                    watcher->deleteLater();
                    *batchInFlight = false;
                    if (!activeOutgoingDataPumpTasks.contains(taskId)) {
                        return;
                    }

                    const auto batch = watcher->result();
                    if (!batch.ok) {
                        (*failTransfer)((*completedBytesFromSet)(*remoteCompletedChunkSet),
                                        *nextChunkIndex > 0 ? *nextChunkIndex - 1 : task.lastChunkIndex,
                                        batch.errorCode.isEmpty()
                                            ? QStringLiteral("send_incomplete")
                                            : batch.errorCode,
                                        batch.errorText.isEmpty()
                                            ? QStringLiteral("\u6570\u636e\u53d1\u9001\u4e2d\u65ad")
                                            : batch.errorText);
                        return;
                    }

                    if (!safeControlConnection || !safeControlConnection->isConnected()) {
                        (*failTransfer)((*completedBytesFromSet)(*remoteCompletedChunkSet),
                                        *nextChunkIndex > 0 ? *nextChunkIndex - 1 : task.lastChunkIndex,
                                        QStringLiteral("send_incomplete"),
                                        QStringLiteral("\u6570\u636e\u53d1\u9001\u4e2d\u65ad"));
                        return;
                    }

                    *nextChunkIndex = batch.nextChunkIndex;
                    int lastChunkIndex = task.lastChunkIndex;
                    const int chunkCount = static_cast<int>(batch.chunks.size());
                    for (int ci = 0; ci < chunkCount; ++ci) {
                        const auto& preparedChunk = batch.chunks[static_cast<size_t>(ci)];
                        MessageEnvelope chunk;
                        chunk.type = MessageType::FileChunk;
                        chunk.senderId = toUtf8(localClientId);
                        chunk.targetId = toUtf8(targetClientId);
                        chunk.fileTaskId = toUtf8(taskId);
                        chunk.chunkIndex = preparedChunk.chunkIndex;
                        chunk.body = preparedChunk.encodedBody.toStdString();
                        // 同批最后一个 chunk 才 flush，减少主线程阻塞
                        const bool isLast = (ci == chunkCount - 1);
                        if (!safeControlConnection->sendPayload(
                                QByteArray::fromStdString(MessageCodec::encode(chunk)),
                                /*deferFlush=*/!isLast)) {
                            qWarning().noquote()
                                << "File transfer chunk send failed for task" << taskId
                                << "chunk" << preparedChunk.chunkIndex
                                << "to" << targetClientId;
                            (*failTransfer)((*completedBytesFromSet)(*remoteCompletedChunkSet),
                                            lastChunkIndex,
                                            QStringLiteral("send_incomplete"),
                                            QStringLiteral("\u6570\u636e\u53d1\u9001\u4e2d\u65ad"));
                            return;
                        }

                        dispatchedChunkSet->insert(preparedChunk.chunkIndex);
                        lastChunkIndex = preparedChunk.chunkIndex;
                    }

                    while (*nextChunkIndex < task.chunkCount
                           && dispatchedChunkSet->contains(*nextChunkIndex)) {
                        ++(*nextChunkIndex);
                    }

                    if (dispatchedChunkSet->size() >= task.chunkCount) {
                        const auto remoteCompletedChunkVector =
                            fileTransferRepository.loadCompletedChunkIndexes(taskId);
                        markOutgoingAwaitingCompletion(task,
                                                       taskId,
                                                       sourceFilePath,
                                                       remoteCompletedChunkVector);
                        return;
                    }

                    if (batch.chunks.empty() && batch.reachedEnd) {
                        (*failTransfer)((*completedBytesFromSet)(*remoteCompletedChunkSet),
                                        lastChunkIndex,
                                        QStringLiteral("send_incomplete"),
                                        QStringLiteral("\u6570\u636e\u53d1\u9001\u4e2d\u65ad"));
                        return;
                    }

                    // 背压自适应：socket 积压 > 8MB 时放慢到 100ms，否则 10ms
                    const qint64 pending = safeControlConnection
                        ? safeControlConnection->pendingBytes() : 0;
                    const int pumpDelayMs = pending > 8 * 1024 * 1024 ? 100 : 10;
                    QTimer::singleShot(pumpDelayMs, m_mainWindow.get(), [pumpLoop]() { (*pumpLoop)(); });
                });

            watcher->setFuture(QtConcurrent::run(
                [sourceFilePath, task, dispatchedSnapshot, startChunkIndex, kChunksPerPumpTick]() {
                    return FileTransferService::prepareOutgoingChunkBatch(sourceFilePath,
                                                                          task.chunkSize,
                                                                          task.chunkCount,
                                                                          startChunkIndex,
                                                                          dispatchedSnapshot,
                                                                          kChunksPerPumpTick,
                                                                          task.fileSize);
                }));
        };

        QTimer::singleShot(0, m_mainWindow.get(), [pumpLoop]() { (*pumpLoop)(); });
        return true;
    };

    const auto scheduleOutgoingOfferRetry = [&](const QString& taskId, const QString& targetClientId) {
        if (taskId.trimmed().isEmpty() || targetClientId.trimmed().isEmpty()) {
            return;
        }

        auto offerRetryAttempts = std::make_shared<int>(0);
        auto retryLoop = std::make_shared<std::function<void()>>();
        *retryLoop = [&, taskId, targetClientId, offerRetryAttempts, retryLoop]() {
            if (*offerRetryAttempts >= 4) {
                return;
            }

            const int attemptIndex = *offerRetryAttempts;
            ++(*offerRetryAttempts);
            const int delayMs = 1500 + attemptIndex * 1500;
            QTimer::singleShot(delayMs, m_mainWindow.get(),
                               [&, taskId, targetClientId, retryLoop]() {
                FileTransferTask pendingTask;
                if (!fileTransferService.loadTask(taskId, &pendingTask)
                    || pendingTask.direction != FileTransferDirection::Outgoing
                    || (pendingTask.state != FileTransferState::WaitingAccept
                        && pendingTask.state != FileTransferState::PendingOffer)) {
                    return;
                }

                PeerConnection* retryConnection = connectionsByTargetId.value(targetClientId, nullptr);
                if (!retryConnection || !retryConnection->isConnected()) {
                    return;
                }

                MessageEnvelope retryOffer;
                if (fileTransferService.buildOfferEnvelope(pendingTask,
                                                           localClientId,
                                                           targetClientId,
                                                           QString(),
                                                           activeFileTransferPort,
                                                           &retryOffer)) {
                    retryConnection->sendPayload(
                        QByteArray::fromStdString(MessageCodec::encode(retryOffer)));
                }

                FileTransferTask refreshedTask;
                if (fileTransferService.loadTask(taskId, &refreshedTask)
                    && refreshedTask.direction == FileTransferDirection::Outgoing
                    && isOutgoingTransferKickableState(refreshedTask.state)
                    && !activeOutgoingDataPumpTasks.contains(taskId)) {
                    startOutgoingDataTransfer(taskId, targetClientId, retryConnection);
                }

                if (retryLoop) {
                    (*retryLoop)();
                }
            });
        };
        (*retryLoop)();
    };
    scheduleOptimisticDataPump = [&](const QString& taskId, const QString& targetClientId) {
        if (taskId.trimmed().isEmpty() || targetClientId.trimmed().isEmpty()) {
            return;
        }

        auto kickAttempts = std::make_shared<int>(0);
        auto retryLoop = std::make_shared<std::function<void()>>();
        *retryLoop = [&, taskId, targetClientId, kickAttempts, retryLoop]() {
            if (*kickAttempts >= 3) {
                return;
            }

            const int delayMs = 250 + (*kickAttempts * 350);
            ++(*kickAttempts);
            QTimer::singleShot(delayMs, m_mainWindow.get(),
                               [&, taskId, targetClientId, retryLoop]() {
                FileTransferTask pendingTask;
                if (!fileTransferService.loadTask(taskId, &pendingTask)
                    || pendingTask.direction != FileTransferDirection::Outgoing
                    || !isOutgoingTransferKickableState(pendingTask.state)) {
                    return;
                }
                if (activeOutgoingDataPumpTasks.contains(taskId)) {
                    return;
                }

                PeerConnection* retryConnection = connectionsByTargetId.value(targetClientId, nullptr);
                if (!retryConnection || !retryConnection->isConnected()) {
                    return;
                }
                startOutgoingDataTransfer(taskId, targetClientId, retryConnection);
                if (retryLoop) {
                    (*retryLoop)();
                }
            });
        };
        (*retryLoop)();
    };

    std::function<void(PeerConnection*, const QString&, const QString&, quint16)> attachConnection;
    QVector<SystemNotificationItem> systemNotificationItems;
    std::function<void(const SystemNotificationItem&)> appendSystemNotification;
    std::function<void(const QString&)> markSystemNotificationRead;
    std::function<void(const QString&)> archiveSystemNotification;
    std::function<void()> markAllSystemNotificationsRead;
    std::function<void(const AzureDevOpsNotificationEvent&)> enqueueAzureDevOpsNotification;
    std::function<void(const OutlookNotificationEvent&)> enqueueOutlookNotificationEvent;
    std::function<void()> refreshAzureDevOpsBuildNotificationTimer;
    std::function<void()> refreshOutlookNotificationTimer;
    std::function<void(const OutlookConnectionSettings&)> restartOutlookStreaming;
    std::function<void()> stopOutlookStreaming;
    QString devOpsFailureAnnouncementKey;
    QString outlookFailureAnnouncementKey;
    std::function<void(const QString&,
                       const QString&,
                       const QString&,
                       const QString&,
                       const QString&,
                       int,
                       int,
                       QString&)> publishIntegrationFailureNotification;
    std::function<void(const QString&, const QString&, QString&)> publishIntegrationRecoveryNotification;

    appendSystemNotification = [&](const SystemNotificationItem& item) {
        if (item.notificationId.trimmed().isEmpty()) {
            return;
        }
        qInfo().noquote() << QStringLiteral("[notifications][center] append %1 %2")
                                 .arg(item.sourceKey.trimmed(), item.title.trimmed());
        systemNotificationItems.prepend(item);
        while (systemNotificationItems.size() > 200) {
            systemNotificationItems.removeLast();
        }
        m_mainWindow->setNotificationItems(systemNotificationItems);
        if (item.unread) {
            ++systemUnreadReminderCount;
            lastUnreadTitle = item.title.trimmed().isEmpty()
                ? item.sourceLabel.trimmed()
                : item.title.trimmed();
            updateTrayUnreadPresentation();
            notifyUnreadActivity(lastUnreadTitle,
                                 item.summary.trimmed().isEmpty()
                                     ? QStringLiteral("你有一条新的系统提醒")
                                     : item.summary.trimmed(),
                                 false);
        }
    };
    markSystemNotificationRead = [&](const QString& notificationId) {
        if (notificationId.trimmed().isEmpty()) {
            return;
        }
        bool changed = false;
        for (SystemNotificationItem& item : systemNotificationItems) {
            if (item.notificationId == notificationId && item.unread) {
                item.unread = false;
                systemUnreadReminderCount = qMax(0, systemUnreadReminderCount - 1);
                changed = true;
                break;
            }
        }
        if (changed) {
            qInfo().noquote() << QStringLiteral("[notifications][center] mark-read %1")
                                     .arg(notificationId.trimmed());
            updateTrayUnreadPresentation();
            m_mainWindow->setNotificationItems(systemNotificationItems);
        }
    };
    archiveSystemNotification = [&](const QString& notificationId) {
        if (notificationId.trimmed().isEmpty()) {
            return;
        }
        const auto it = std::remove_if(systemNotificationItems.begin(),
                                       systemNotificationItems.end(),
                                       [&](const SystemNotificationItem& item) {
                                           if (item.notificationId == notificationId && item.unread) {
                                               systemUnreadReminderCount =
                                                   qMax(0, systemUnreadReminderCount - 1);
                                           }
                                           return item.notificationId == notificationId;
                                       });
         if (it != systemNotificationItems.end()) {
              systemNotificationItems.erase(it, systemNotificationItems.end());
              qInfo().noquote() << QStringLiteral("[notifications][center] archive %1")
                                       .arg(notificationId.trimmed());
              updateTrayUnreadPresentation();
              m_mainWindow->setNotificationItems(systemNotificationItems);
          }
    };
    markAllSystemNotificationsRead = [&]() {
        bool changed = false;
        for (SystemNotificationItem& item : systemNotificationItems) {
            if (item.unread) {
                item.unread = false;
                changed = true;
            }
        }
        if (changed) {
            systemUnreadReminderCount = 0;
            updateTrayUnreadPresentation();
            m_mainWindow->setNotificationItems(systemNotificationItems);
        }
    };
    publishIntegrationFailureNotification =
        [&](const QString& sourceKey,
            const QString& sourceLabel,
            const QString& category,
            const QString& errorMessage,
            const QString& detail,
            int consecutiveFailures,
            int nextPollMinutes,
            QString& announcedKey) {
            Q_UNUSED(detail);
            const QString normalizedError = errorMessage.trimmed();
            if (normalizedError.isEmpty()) {
                return;
            }
            const QString normalizedCategory =
                category.trimmed().isEmpty() ? QStringLiteral("other") : category.trimmed();
            const QString key = QStringLiteral("%1|%2|%3")
                                    .arg(sourceKey.trimmed(), normalizedCategory, normalizedError);
            if (announcedKey == key) {
                return;
            }
            announcedKey = key;
            // 集成同步错误只在状态栏临时提示，不插入通知中心
            m_mainWindow->setStatusMessage(
                IntegrationNotificationPresentation::integrationFailureSummary(
                    sourceLabel, normalizedCategory, consecutiveFailures, nextPollMinutes),
                8000);
        };
    publishIntegrationRecoveryNotification =
        [&](const QString& sourceKey, const QString& sourceLabel, QString& announcedKey) {
            Q_UNUSED(sourceKey);
            if (announcedKey.trimmed().isEmpty()) {
                return;
            }
            announcedKey.clear();
            // 恢复通知只在状态栏临时提示
            m_mainWindow->setStatusMessage(
                IntegrationNotificationPresentation::integrationRecoverySummary(sourceLabel),
                5000);
        };
    const auto refreshActiveReminderSurface = [&]() {
        QVector<ReminderItem> reminders;
        const std::vector<ReminderItem> activeReminders =
            reminderRepository.loadActiveReminders();
        reminders.reserve(static_cast<qsizetype>(activeReminders.size()));
        for (const ReminderItem& reminder : activeReminders) {
            reminders.push_back(reminder);
        }
        m_mainWindow->setActiveReminders(reminders);
    };
    enqueueAzureDevOpsNotification = [&](const AzureDevOpsNotificationEvent& event) {
        QString detail = event.status.trimmed();
        if (!event.actor.trimmed().isEmpty()) {
            detail = detail.isEmpty()
                         ? event.actor.trimmed()
                         : QStringLiteral("%1 · %2").arg(event.status.trimmed(), event.actor.trimmed());
        }
        appendSystemNotification(SystemNotificationItem{
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            QStringLiteral("azure-devops"),
            QStringLiteral("Azure DevOps"),
            sanitizeNotificationText(event.title, QStringLiteral("Azure DevOps 提醒")),
            sanitizeNotificationText(event.summary, QStringLiteral("收到一条新的 DevOps 系统通知。")),
            sanitizeNotificationText(detail, QStringLiteral("可打开原始链接查看详情。")),
            QStringLiteral("打开原始页面"),
            event.webUrl.trimmed(),
            QString(),
            QDateTime::currentMSecsSinceEpoch(),
            true,
        });
    };
    enqueueOutlookNotificationEvent = [&](const OutlookNotificationEvent& event) {
        const QString detail = event.actor.trimmed().isEmpty()
            ? event.status.trimmed()
            : (event.status.trimmed().isEmpty()
                   ? event.actor.trimmed()
                   : QStringLiteral("%1 · %2").arg(event.status.trimmed(), event.actor.trimmed()));
        appendSystemNotification(SystemNotificationItem{
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            QStringLiteral("outlook"),
            QStringLiteral("Outlook"),
            sanitizeNotificationText(event.title, QStringLiteral("Outlook 提醒")),
            sanitizeNotificationText(event.summary, QStringLiteral("收到一条新的 Outlook 系统通知。")),
            sanitizeNotificationText(detail, QStringLiteral("可打开原始链接查看详情。")),
            event.kind == OutlookNotificationKind::CalendarReminder
                ? QStringLiteral("打开日程")
                : QStringLiteral("打开邮件"),
            event.webUrl.trimmed(),
            event.htmlBody,
            QDateTime::currentMSecsSinceEpoch(),
            true,
        });
    };

    QObject::connect(&reminderService,
                     &ReminderService::reminderDue,
                     m_mainWindow.get(),
                     [&](const ReminderItem& reminder) {
                         appendSystemNotification(notificationFromReminder(reminder));
                     });
    QObject::connect(&reminderService,
                     &ReminderService::remindersChanged,
                     m_mainWindow.get(),
                     refreshActiveReminderSurface);
    QObject::connect(m_mainWindow.get(),
                     &MainWindow::reminderDoneRequested,
                     m_mainWindow.get(),
                     [&](const QString& reminderId) {
                         if (reminderService.markDone(reminderId.trimmed())) {
                             m_mainWindow->setStatusMessage(QStringLiteral("已完成提醒"), 2500);
                         }
                     });
    QObject::connect(m_mainWindow.get(),
                     &MainWindow::reminderSnoozeRequested,
                     m_mainWindow.get(),
                     [&](const QString& reminderId, int minutes) {
                         const int normalizedMinutes = qMax(1, minutes);
                         const qint64 dueAtMs =
                             QDateTime::currentDateTime().addSecs(normalizedMinutes * 60).toMSecsSinceEpoch();
                         if (reminderService.snooze(reminderId.trimmed(), dueAtMs)) {
                             m_mainWindow->setStatusMessage(
                                 QStringLiteral("已稍后 %1 分钟提醒").arg(normalizedMinutes),
                                 2500);
                         }
                     });
    QObject::connect(m_mainWindow.get(),
                     &MainWindow::messageReminderRequested,
                     m_mainWindow.get(),
                     [&](const QString& messageId,
                         const QString& conversationId,
                         const QString& titleSnapshot,
                         const QString& previewSnapshot) {
                         ReminderDialog dialog(m_mainWindow.get());
                         dialog.setContextPreview(titleSnapshot, previewSnapshot);
                         if (dialog.exec() != QDialog::Accepted) {
                             return;
                         }

                         const QDateTime now = QDateTime::currentDateTime();
                         const auto item = makeMessageReminderItem(
                             messageId,
                             conversationId,
                             titleSnapshot,
                             previewSnapshot,
                             dialog.note(),
                             dialog.selectedDueTime().toMSecsSinceEpoch(),
                             now);

                         QString error;
                         if (!item.has_value() || !reminderService.scheduleReminder(*item, &error)) {
                             m_mainWindow->setStatusMessage(
                                 error.trimmed().isEmpty()
                                     ? QStringLiteral("提醒创建失败")
                                     : QStringLiteral("提醒创建失败：%1").arg(error.trimmed()),
                                 4000);
                             return;
                         }
                         m_mainWindow->setStatusMessage(QStringLiteral("已设置提醒"), 2500);
                     });
    QObject::connect(m_mainWindow.get(),
                     &MainWindow::contactReminderRequested,
                     m_mainWindow.get(),
                     [&](const QString& contactId,
                         const QString& displayName,
                         const QString& previewSnapshot) {
                         const QString trimmedContactId = contactId.trimmed();
                         if (trimmedContactId.isEmpty()) {
                             m_mainWindow->setStatusMessage(QStringLiteral("提醒创建失败：联系人为空"), 3000);
                             return;
                         }

                         const QDateTime now = QDateTime::currentDateTime();
                         const auto item =
                             makeContactReminderItem(trimmedContactId, displayName, previewSnapshot, now);

                         QString error;
                         if (!item.has_value() || !reminderService.scheduleReminder(*item, &error)) {
                             m_mainWindow->setStatusMessage(
                                 error.trimmed().isEmpty()
                                     ? QStringLiteral("提醒创建失败")
                                     : QStringLiteral("提醒创建失败：%1").arg(error.trimmed()),
                                 4000);
                             return;
                         }
                         m_mainWindow->setStatusMessage(QStringLiteral("已设置明天跟进"), 2500);
                     });
    QObject::connect(m_mainWindow.get(),
                     &MainWindow::groupAnnouncementReminderRequested,
                     m_mainWindow.get(),
                     [&](const QString& groupId,
                         const QString& groupName,
                         const QString& announcement) {
                         const QString trimmedGroupId = groupId.trimmed();
                         if (trimmedGroupId.isEmpty()) {
                             m_mainWindow->setStatusMessage(QStringLiteral("提醒创建失败：群为空"), 3000);
                             return;
                         }

                         ReminderDialog dialog(m_mainWindow.get());
                         const QString titleSnapshot = groupName.trimmed().isEmpty()
                                                           ? trimmedGroupId
                                                           : groupName.trimmed();
                         const QString previewSnapshot = announcement.trimmed().isEmpty()
                                                             ? QStringLiteral("群公告提醒")
                                                             : announcement.trimmed().left(160);
                         dialog.setContextPreview(titleSnapshot, previewSnapshot);
                         if (dialog.exec() != QDialog::Accepted) {
                             return;
                         }

                         const QDateTime now = QDateTime::currentDateTime();
                         const auto item = makeGroupAnnouncementReminderItem(
                             trimmedGroupId,
                             groupName,
                             announcement,
                             dialog.note(),
                             dialog.selectedDueTime().toMSecsSinceEpoch(),
                             now);

                         QString error;
                         if (!item.has_value() || !reminderService.scheduleReminder(*item, &error)) {
                             m_mainWindow->setStatusMessage(
                                 error.trimmed().isEmpty()
                                     ? QStringLiteral("提醒创建失败")
                                     : QStringLiteral("提醒创建失败：%1").arg(error.trimmed()),
                                 4000);
                             return;
                         }
                         m_mainWindow->setStatusMessage(QStringLiteral("已设置群公告提醒"), 2500);
                     });
    reminderService.start();
    refreshActiveReminderSurface();

    const auto rememberPeer = [&](const QString& clientId,
                                  const QString& displayName,
                                  const QString& host,
                                  quint16 port,
                                  bool connected,
                                  PeerPresenceStatus presence = PeerPresenceStatus::Online,
                                  const QString& avatarBase64 = QString()) {
        const QString normalizedClientId = clientId.trimmed();
        const QString normalizedHost = normalizeHost(host).trimmed();
        if (normalizedClientId.isEmpty() || normalizedClientId == localClientId || normalizedHost.isEmpty()
            || port == 0) {
            return false;
        }

        if (!avatarBase64.trimmed().isEmpty()) {
            persistAvatarBase64ForClient(normalizedClientId, avatarBase64);
        }

        const qint64 observedAtMs = QDateTime::currentMSecsSinceEpoch();
        const PeerEndpoint endpoint{
            toUtf8(normalizedClientId),
            toUtf8(displayName.trimmed()),
            toUtf8(normalizedHost),
            port,
            connected,
            presence,
            presence == PeerPresenceStatus::Offline ? 0 : observedAtMs
        };
        if (connected) {
            const bool changed = peerDirectoryService.upsertConnectedPeer(endpoint);
            // 连接成功时持久化，重启后可恢复
            // 去抖：同一 peer 60 秒内不重复写 DB，避免重连风暴时 N*saveKnownPeer
            {
                static QHash<QString, qint64> lastSavedAtMs;
                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                const qint64 lastMs = lastSavedAtMs.value(normalizedClientId, 0);
                if (nowMs - lastMs > 60000) {
                    conversationRepository.saveKnownPeer(endpoint);
                    lastSavedAtMs.insert(normalizedClientId, nowMs);
                }
            }
            return changed;
        }
        return peerDirectoryService.upsertDiscoveredPeer(endpoint);
    };

    const auto sendPeerDirectorySnapshot = [&](PeerConnection* connection, const QString& excludeClientId = {}) {
        if (!connection || !connection->isConnected()) {
            return;
        }

        auto peers = peerDirectoryService.connectedPeers(toUtf8(localClientId));
        if (!excludeClientId.trimmed().isEmpty()) {
            const std::string excludedClientId = toUtf8(excludeClientId.trimmed());
            peers.erase(std::remove_if(peers.begin(),
                                       peers.end(),
                                       [&](const PeerEndpoint& peer) { return peer.clientId == excludedClientId; }),
                        peers.end());
        }

        const MessageEnvelope snapshotEnvelope =
            PeerHandshake::buildDirectorySnapshotEnvelope(localClientId, peers);
        connection->sendPayload(QByteArray::fromStdString(MessageCodec::encode(snapshotEnvelope)));
    };

    const auto activeGroupMemberIds = [&](const QString& groupId) {
        return groupService.activeMemberIds(groupId);
    };

    const auto removeGroupConversationLocally = [&](const QString& groupId) {
        conversationRepository.deleteConversation(groupId);
        if (currentConversationId == groupId) {
            currentConversationId.clear();
            currentTargetId.clear();
            m_mainWindow->clearCurrentConversationView();
        }
        scheduleChatUiRefresh(true, true, true, true);
    };

    const auto isLikelyLegacyGroupRecord = [&](const QString& groupId) {
        const auto groupOpt = groupRepository.findGroupById(groupId.toStdWString());
        if (!groupOpt.has_value()) {
            return true;
        }

        const QString ownerId = QString::fromStdWString(groupOpt->ownerClientId).trimmed();
        const auto members = groupRepository.loadMembers(groupOpt->groupId);
        bool hasActiveMember = false;
        bool localMemberActive = false;
        for (const auto& member : members) {
            if (!member.isActive) {
                continue;
            }
            hasActiveMember = true;
            if (QString::fromStdWString(member.memberClientId).trimmed() == localClientId) {
                localMemberActive = true;
            }
        }

        return ownerId.isEmpty() || !hasActiveMember || !localMemberActive;
    };

    const auto tryRemoveLegacyGroupLocally = [&](const QString& groupId, const QString& actionLabel) {
        if (!isLikelyLegacyGroupRecord(groupId)) {
            return false;
        }
        removeGroupConversationLocally(groupId);
        m_mainWindow->setStatusMessage(
            QStringLiteral("检测到旧版本群数据，已本地%1（不影响其他成员）").arg(actionLabel),
            4200);
        return true;
    };

    const auto broadcastGroupMeta = [&](const Group& group,
                                        const QStringList& snapshotMemberIds,
                                        const QStringList& recipientIds,
                                        const QString& eventType,
                                        const QString& affectedMemberId = QString()) {
        const auto metaEnvelopes = groupService.buildGroupMetaFanOut(localClientId,
                                                                     group,
                                                                     snapshotMemberIds,
                                                                     recipientIds,
                                                                     eventType,
                                                                     affectedMemberId);
        for (const auto& env : metaEnvelopes) {
            const QString targetId = QString::fromStdString(env.targetId);
            if (PeerConnection* conn = connectionsByTargetId.value(targetId, nullptr)) {
                if (conn->isConnected()) {
                    conn->sendPayload(QByteArray::fromStdString(MessageCodec::encode(env)));
                }
            }
        }
    };

    const auto broadcastLocalPresence = [&]() {
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        const QString localAvatarPath =
            cfg.value(QStringLiteral("avatar/") + localClientId,
                      cfg.value(QStringLiteral("avatar/self"))).toString();
        const QString localAvatarBase64 = avatarBase64ForImagePath(localAvatarPath);
        for (auto it = connectionsByTargetId.begin(); it != connectionsByTargetId.end(); ++it) {
            PeerConnection* connection = it.value();
            if (!connection || !connection->isConnected()) {
                continue;
            }
            const MessageEnvelope helloEnvelope = PeerHandshake::buildHelloEnvelope(PeerHello{
                localClientId,
                localDisplayName,
                activeListenPort,
                QString::fromStdWString(profile->signature),
                localPresence,
                localAvatarBase64,
                TlsHelper::isAvailable(),
                QString::fromStdWString(profile->department),
                QString::fromStdWString(profile->jobTitle),
                QString::fromStdWString(profile->phoneNumber),
                QString::fromStdWString(profile->gender),
                QString::fromStdWString(profile->email),
                localAppVersion,
                localRoutingCapabilities
            });
            connection->sendPayload(QByteArray::fromStdString(MessageCodec::encode(helloEnvelope)));
            sendPeerDirectorySnapshot(connection);
        }
    };

    const auto refreshLocalPresence = [&](bool forceBroadcast = false) {
        PresenceHeuristicInputs inputs;
        inputs.workstationLocked = workstationLocked();
        inputs.idleMilliseconds = systemIdleMilliseconds();
        const PeerPresenceStatus nextPresence = determineLocalPresence(inputs);
        if (!forceBroadcast && nextPresence == localPresence) {
            return;
        }
        localPresence = nextPresence;
        broadcastLocalPresence();
    };

    // 全局 pending 连接计数器：限制同时处于 connecting/TLS 握手中的 socket 数量，
    // 避免大量 QSslSocket 的 SChannel 内部 HWND/Timer 瞬间耗尽 USER 对象配额(10000)
    // 千人规模部署需要更大的并发窗口以应对早高峰批量上线
    constexpr int kMaxGlobalPendingConnections = 64;
    int globalPendingConnections = 0;
    struct DeferredPeer { QString clientId; QString host; quint16 port; };
    QQueue<DeferredPeer> pendingConnectQueue;
    QSet<QString> pendingQueuedClientIds;  // 队列去重：避免同一 peer 被 LAN 广播重复入队数百次

    // 连接失败冷却表：记录每个 clientId 最近一次失败时间戳，
    // 30 秒内不重试同一 clientId，避免离线 peer 被 LAN 广播每 3s 重复连接
    QHash<QString, qint64> connectFailedCooldown;
    constexpr qint64 kConnectCooldownMs = 30000;  // 30 秒冷却期

    // tryAutoConnect SKIP 日志采样：每个 peer 10 秒内最多打印一次，
    // 40+ peer × 每 3s 广播 = 海量噪音日志，可把 911MB/天的日志文件撑满并阻塞主线程
    QHash<QString, qint64> skipLogLastPrintedMs;
    constexpr qint64 kSkipLogSampleIntervalMs = 10000;

    // HELLO 去重限速：同一 clientId 5 秒内只处理一次完整 HELLO，
    // 防止连接震荡导致 HELLO 洪水阻塞主线程
    QHash<QString, qint64> helloLastProcessedMs;
    constexpr qint64 kHelloDedupIntervalMs = 5000;

    // 前向声明 drainPendingConnectQueue
    std::function<void()> drainPendingConnectQueue;

    const auto tryAutoConnectPeer = [&](const QString& clientId, const QString& host, quint16 port) {
        const QString normalizedClientId = clientId.trimmed();
        const QString normalizedHost = normalizeHost(host).trimmed();
        if (normalizedClientId.isEmpty() || normalizedClientId == localClientId || normalizedHost.isEmpty()
            || port == 0) {
            return false;
        }

        // 冷却期检查：最近 30 秒内连接失败过的 peer 不重试，
        // 避免离线 peer 被 LAN 广播每 3s 触发无效连接消耗 USER 对象
        {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const auto it = connectFailedCooldown.constFind(normalizedClientId);
            if (it != connectFailedCooldown.constEnd() && (now - it.value()) < kConnectCooldownMs) {
                return false;
            }
        }

        // 已有连接条目（含正在连接中的）则跳过，避免重复创建 socket
        // 断开的连接由 disconnected 信号处理器自动移除
        if (connectionsByTargetId.contains(normalizedClientId)) {
            PeerConnection* existing = connectionsByTargetId.value(normalizedClientId);
            if (existing && (existing->isConnected() || existing->isConnecting())) {
                // 采样日志：每个 peer 10 秒内最多打印一次 SKIP，避免日志洪水
                const qint64 nowSkip = QDateTime::currentMSecsSinceEpoch();
                const auto skipIt = skipLogLastPrintedMs.constFind(normalizedClientId);
                if (skipIt == skipLogLastPrintedMs.constEnd()
                    || (nowSkip - skipIt.value()) >= kSkipLogSampleIntervalMs) {
                    skipLogLastPrintedMs.insert(normalizedClientId, nowSkip);
                    qDebug().noquote() << "[peer-diag] tryAutoConnect SKIP (already connected/connecting):"
                                      << normalizedHost << ":" << port
                                      << "clientId=" << normalizedClientId.left(8);
                }
                return false;
            }
            // 连接已死（对端被强杀/断网等）或 QPointer 已空：清理旧条目并重连
            qInfo().noquote() << "[peer-diag] tryAutoConnect STALE-CLEANUP:"
                              << normalizedHost << ":" << port
                              << "clientId=" << normalizedClientId.left(8)
                              << "existing=" << (existing ? "dead" : "null");
            if (existing) {
                // 先断开所有信号，防止 deleteLater 延迟触发 disconnected handler
                // 误将新注册的连接从 connectionsByTargetId 中移除或标记 peer 离线
                peerIdsByConnection.remove(existing);
                existing->disconnect();
                existing->deleteLater();
            }
            connectionsByTargetId.remove(normalizedClientId);
        }

        // 超过并发限制时排队延迟连接
        if (globalPendingConnections >= kMaxGlobalPendingConnections) {
            // 去重：同一 clientId 只入队一次，避免 LAN 广播每 3s 重复入队
            if (pendingQueuedClientIds.contains(normalizedClientId)) {
                return true;
            }
            // 队列上限保护，避免无限膨胀
            if (pendingConnectQueue.size() >= 500) {
                return true;
            }
            pendingConnectQueue.enqueue({normalizedClientId, normalizedHost, port});
            pendingQueuedClientIds.insert(normalizedClientId);
            qInfo().noquote() << "[peer-diag] tryAutoConnect QUEUED:"
                              << normalizedHost << ":" << port
                              << "pending=" << globalPendingConnections
                              << "queueSize=" << pendingConnectQueue.size();
            return true;
        }

        QHostAddress address;
        if (!address.setAddress(normalizedHost)) {
            qWarning().noquote() << "[peer-diag] tryAutoConnect BAD-ADDRESS:" << normalizedHost;
            return false;
        }

        PeerConnection* outbound = sessionManager.connectToPeer(address, port);
        if (attachConnection) {
            attachConnection(outbound, normalizedClientId, normalizedHost, port);
        } else {
            qWarning().noquote() << "[peer-diag] BUG: attachConnection not yet assigned, skipping";
        }
        ++globalPendingConnections;
        qInfo().noquote() << "[peer-diag] tryAutoConnect CONNECTING:"
                          << normalizedHost << ":" << port
                          << "clientId=" << normalizedClientId.left(8)
                          << "pending=" << globalPendingConnections;

        // 连接完成（成功或失败）后减少计数并排空队列
        // settled 标志防止 connected+disconnected 双重触发
        auto globalSettled = std::make_shared<bool>(false);
        auto onGlobalSettled = [&, globalSettled, normalizedHost, normalizedClientId, appShuttingDown](bool success) {
            if (*appShuttingDown) {
                return;
            }
            if (*globalSettled) return;
            *globalSettled = true;
            --globalPendingConnections;
            if (!success) {
                // 连接失败：记录冷却时间，30 秒内不重试此 peer
                connectFailedCooldown.insert(normalizedClientId, QDateTime::currentMSecsSinceEpoch());
            } else {
                // 连接成功：清除冷却记录
                connectFailedCooldown.remove(normalizedClientId);
            }
            qInfo().noquote() << "[peer-diag] onGlobalSettled:"
                              << normalizedHost
                              << "clientId=" << normalizedClientId.left(8)
                              << "success=" << success
                              << "pending=" << globalPendingConnections
                              << "queueSize=" << pendingConnectQueue.size();
            if (drainPendingConnectQueue) {
                drainPendingConnectQueue();
            }
        };
        QObject::connect(outbound, &PeerConnection::connected, m_mainWindow.get(),
                         [onGlobalSettled]() { onGlobalSettled(true); });
        QObject::connect(outbound, &PeerConnection::disconnected, m_mainWindow.get(),
                         [onGlobalSettled]() { onGlobalSettled(false); });
        return true;
    };

    drainPendingConnectQueue = [&]() {
        int drained = 0;
        while (!pendingConnectQueue.isEmpty()
               && globalPendingConnections < kMaxGlobalPendingConnections) {
            const DeferredPeer dp = pendingConnectQueue.dequeue();
            pendingQueuedClientIds.remove(dp.clientId);
            tryAutoConnectPeer(dp.clientId, dp.host, dp.port);
            ++drained;
        }
        if (drained > 0) {
            qInfo().noquote() << "[peer-diag] drainQueue: drained=" << drained
                              << "remaining=" << pendingConnectQueue.size()
                              << "pending=" << globalPendingConnections;
        }
    };

    const auto ensureLegacyP2PConnectionForTarget =
        [&](const QString& rawTargetId,
            const RemoteChatServiceSettings& settings,
            bool receiverServerCapable,
            bool p2pAvailable,
            const QString& reason) -> bool {
            const QString targetId =
                resolvedTargetIdsByAlias.value(rawTargetId, rawTargetId).trimmed();
            if (targetId.isEmpty() || targetId == localClientId) {
                return false;
            }
            const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(targetId));
            const bool hasEndpoint =
                peer.has_value() && !peer->host.empty() && peer->port != 0;
            if (!P2PConnectionPolicy::shouldPreflightLegacyPeerConnection(
                    settings,
                    receiverServerCapable,
                    p2pAvailable,
                    hasEndpoint)) {
                return false;
            }

            const QString peerHost = QString::fromUtf8(
                peer->host.data(),
                static_cast<int>(peer->host.size()));
            const bool started = tryAutoConnectPeer(targetId, peerHost, peer->port);
            qInfo().noquote()
                << "[legacy-p2p] preflight"
                << "reason=" << reason
                << "target=" << targetId.left(8)
                << "host=" << peerHost
                << "port=" << peer->port
                << "started=" << started;
            return started;
        };

    const auto preflightLegacyP2PForDirectTarget =
        [&](const QString& rawTargetId, const QString& reason) -> bool {
            const QString targetId =
                resolvedTargetIdsByAlias.value(rawTargetId, rawTargetId).trimmed();
            if (targetId.isEmpty() || targetId == localClientId) {
                return false;
            }

            const RemoteChatServiceSettings settings =
                RemoteChatServiceSettingsStore::load();
            const RecipientCapabilityDecision capability =
                recipientCapabilityResolver.resolveFromCacheOnly(
                    targetId,
                    QDateTime::currentMSecsSinceEpoch());
            const bool receiverServerCapable =
                capability.status == RecipientCapabilityStatus::ServerReceiveCapable;
            PeerConnection* connected =
                ConnectionRegistryUtils::connectedConnectionForTarget(
                    connectionsByTargetId,
                    peerIdsByConnection,
                    targetId);
            return ensureLegacyP2PConnectionForTarget(
                targetId,
                settings,
                receiverServerCapable,
                connected && connected->isConnected(),
                reason);
        };

    const auto preflightDirectFileP2PForTarget =
        [&](const QString& rawTargetId, const QString& reason) -> bool {
            const QString targetId =
                resolvedTargetIdsByAlias.value(rawTargetId, rawTargetId).trimmed();
            if (targetId.isEmpty() || targetId == localClientId) {
                return false;
            }

            if (PeerConnection* connected =
                    ConnectionRegistryUtils::connectedConnectionForTarget(
                        connectionsByTargetId,
                        peerIdsByConnection,
                        targetId)) {
                return connected->isConnected();
            }

            const RemoteChatServiceSettings settings =
                RemoteChatServiceSettingsStore::load();
            if (!P2PConnectionPolicy::shouldStartPeerConnection(
                    settings,
                    P2PConnectionTrigger::ExplicitUserAction)) {
                return false;
            }

            const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(targetId));
            if (!peer.has_value() || peer->host.empty() || peer->port == 0) {
                qInfo().noquote()
                    << "[direct-file-p2p] preflight skipped; no endpoint"
                    << "reason=" << reason
                    << "target=" << targetId.left(8);
                return false;
            }

            const QString peerHost = QString::fromUtf8(
                peer->host.data(),
                static_cast<int>(peer->host.size()));
            const bool started = tryAutoConnectPeer(targetId, peerHost, peer->port);
            qInfo().noquote()
                << "[direct-file-p2p] preflight"
                << "reason=" << reason
                << "target=" << targetId.left(8)
                << "host=" << peerHost
                << "port=" << peer->port
                << "started=" << started;
            return started;
        };

    attachConnection = [&](PeerConnection* connection,
                           const QString& fallbackTargetId,
                           const QString& fallbackHost,
                           quint16 fallbackPort) {
        if (!connection) {
            return;
        }

        if (!fallbackTargetId.isEmpty()) {
            connectionsByTargetId.insert(fallbackTargetId, connection);
        }

        const auto sendHello = [&, connection]() {
            QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
            const QString localAvatarPath =
                cfg.value(QStringLiteral("avatar/") + localClientId,
                          cfg.value(QStringLiteral("avatar/self"))).toString();
            const MessageEnvelope helloEnvelope = PeerHandshake::buildHelloEnvelope(PeerHello{
                localClientId,
                localDisplayName,
                activeListenPort,
                QString::fromStdWString(profile->signature),
                localPresence,
                avatarBase64ForImagePath(localAvatarPath),
                TlsHelper::isAvailable(),
                QString::fromStdWString(profile->department),
                QString::fromStdWString(profile->jobTitle),
                QString::fromStdWString(profile->phoneNumber),
                QString::fromStdWString(profile->gender),
                QString::fromStdWString(profile->email),
                localAppVersion,
                localRoutingCapabilities
            });
            connection->sendPayload(QByteArray::fromStdString(MessageCodec::encode(helloEnvelope)));
        };

        auto everConnected = std::make_shared<bool>(connection->isConnected());
        QObject::connect(connection, &PeerConnection::connected, m_mainWindow.get(),
                         [everConnected]() {
                             *everConnected = true;
                         });
        QObject::connect(connection, &PeerConnection::connected, m_mainWindow.get(), sendHello);
        if (connection->isConnected()) {
            sendHello();
        }
        QObject::connect(connection, &QObject::destroyed, m_mainWindow.get(),
                         [&, appShuttingDown](QObject* destroyedObject) {
                             if (*appShuttingDown) {
                                 return;
                             }
                             auto* destroyedConnection = static_cast<PeerConnection*>(destroyedObject);
                             peerIdsByConnection.remove(destroyedConnection);
                             ConnectionRegistryUtils::removePointerEntries(connectionsByTargetId,
                                                                           destroyedConnection);
                          });
        QObject::connect(connection, &PeerConnection::disconnected, m_mainWindow.get(),
                         [&, connection, everConnected, fallbackHost, appShuttingDown]() {
            if (*appShuttingDown) {
                return;
            }
            peerIdsByConnection.remove(connection);
            const QStringList removedTargetIds =
                ConnectionRegistryUtils::removePointerEntries(connectionsByTargetId, connection);
            if (!*everConnected) {
                qInfo().noquote() << "[peer-diag] attachConnection-disconnected: failed-probe removed="
                                  << removedTargetIds.join(',')
                                  << "host=" << fallbackHost
                                  << "remaining-connections=" << connectionsByTargetId.size();
                connection->deleteLater();
                return;
            }
            qInfo().noquote() << "[peer-diag] attachConnection-disconnected: removed="
                              << removedTargetIds.join(',')
                              << "host=" << connection->peerHost()
                              << "remaining-connections=" << connectionsByTargetId.size();

            bool contactListChanged = false;
            QStringList disconnectedTargetIds;
            for (const auto& targetId : removedTargetIds) {
                const QString resolvedTargetId = resolvedTargetIdsByAlias.value(targetId, targetId);
                if (PeerConnection* retainedConnection =
                        ConnectionRegistryUtils::connectedConnectionForTarget(
                            connectionsByTargetId,
                            peerIdsByConnection,
                            resolvedTargetId,
                            connection)) {
                    connectionsByTargetId.insert(resolvedTargetId, retainedConnection);
                    qInfo().noquote()
                        << "[peer-diag] disconnect skipped offline mark; promoted retained connection for"
                        << resolvedTargetId.left(8);
                    continue;
                }
                contactListChanged = peerDirectoryService.markPeerDisconnected(toUtf8(resolvedTargetId)) || contactListChanged;
                if (!disconnectedTargetIds.contains(targetId)) {
                    disconnectedTargetIds.push_back(targetId);
                }
                if (resolvedTargetId != targetId
                    && !disconnectedTargetIds.contains(resolvedTargetId)) {
                    disconnectedTargetIds.push_back(resolvedTargetId);
                }
                peerDeliveryReceiptCapabilities.remove(resolvedTargetId);
            }
            interruptFileTransfersForTargets(disconnectedTargetIds);

            // 对端断连时自动挂断通话，避免音频/共享资源泄漏
            if (callSession.state() != CallSession::State::Idle
                && callSession.state() != CallSession::State::Ended) {
                const QString callPeerId = callSession.peerId();
                if (disconnectedTargetIds.contains(callPeerId)) {
                    callSession.hangup();
                }
            }

            scheduleChatUiRefresh(true, false, true, true, 150, contactListChanged);

            // 释放死连接及其 QSslSocket，归还 Windows USER 对象
            connection->deleteLater();
        });

        QObject::connect(connection, &PeerConnection::payloadReceived, m_mainWindow.get(),
                         [&, connection, fallbackTargetId, fallbackHost, fallbackPort](const QByteArray& payload) {
                             const auto decoded = MessageCodec::decode(
                                 std::string_view(payload.constData(),
                                                  static_cast<std::size_t>(payload.size())));
                             if (!decoded.has_value()) {
                                 return;
                             }

                             if (decoded->type == MessageType::ReceiptReceived) {
                                 const QString receiptSenderId =
                                     QString::fromUtf8(decoded->senderId.data(),
                                                       static_cast<int>(decoded->senderId.size())).trimmed();
                                 const QString receiptMessageId =
                                     QString::fromUtf8(decoded->messageId.data(),
                                                       static_cast<int>(decoded->messageId.size())).trimmed();
                                 const QString receiptTargetId =
                                     QString::fromUtf8(decoded->targetId.data(),
                                                       static_cast<int>(decoded->targetId.size())).trimmed();
                                 const QString receiptConversationId =
                                     QString::fromUtf8(decoded->conversationId.data(),
                                                       static_cast<int>(decoded->conversationId.size())).trimmed();
                                 QString mappedPeerId;
                                 for (auto it = connectionsByTargetId.constBegin();
                                      it != connectionsByTargetId.constEnd(); ++it) {
                                     if (it.value() == connection) {
                                         mappedPeerId = it.key();
                                         break;
                                     }
                                 }
                                 if (mappedPeerId.isEmpty()) {
                                     mappedPeerId = peerIdsByConnection.value(connection).trimmed();
                                 }

                                 if (receiptSenderId.isEmpty()
                                     || receiptMessageId.isEmpty()
                                     || receiptTargetId != localClientId
                                     || mappedPeerId != receiptSenderId) {
                                     qWarning().noquote() << "[receipt-recv] rejected receipt msgId="
                                                          << receiptMessageId.left(8)
                                                          << "sender=" << receiptSenderId.left(8)
                                                          << "mappedPeer=" << mappedPeerId.left(8)
                                                          << "target=" << receiptTargetId.left(8);
                                     return;
                                 }

                                 conversationRepository.deletePendingGroupEnvelopeForTargetMessage(
                                     receiptSenderId, receiptMessageId);
                                 if (!groupService.isGroupConversation(receiptConversationId)) {
                                     ChatService::storeIncomingEnvelope(localClientId, &conversationRepository, *decoded);
                                 }
                                 scheduleChatUiRefresh(true, true, false, false);
                                 return;
                             }

                             if (decoded->type == MessageType::HandshakeHello) {
                                 const auto hello = PeerHandshake::parseHelloEnvelope(*decoded);
                                 if (!hello.has_value()) {
                                     return;
                                 }

                                 connection->markHelloReceived();

                                 const QString actualTargetId = hello->clientId;
                                 peerDeliveryReceiptCapabilities.insert(
                                     actualTargetId,
                                     MessageRoutingCapabilities::hasP2PDeliveryReceiptV1(
                                         hello->capabilities));
                                 recipientCapabilityResolver.rememberLocalObservation(
                                     actualTargetId,
                                     hello->capabilities,
                                     QDateTime::currentMSecsSinceEpoch());

                                 // Duplicate TCP arbitration must run before HELLO throttling.
                                 const QString aliasTargetId =
                                     fallbackTargetId.isEmpty() ? actualTargetId : fallbackTargetId;
                                 const QString resolvedHost =
                                     !fallbackHost.isEmpty() ? fallbackHost : normalizeHost(connection->peerHost());
                                 const quint16 resolvedPort =
                                     hello->listenPort != 0 ? hello->listenPort
                                                            : (fallbackPort != 0 ? fallbackPort : connection->peerPort());

                                 // 避免破损/重复入站连接覆盖已有的正常连接
                                 // 确定性裁决：两端互连时必须约定保留同一条 TCP 通道，
                                 // 否则双方各丢对方的入站 → 自己的出站也断 → 死循环。
                                 //
                                 // 角色感知规则：双方约定保留 min(localId, targetId) 发起的出站连接。
                                 // 由于 existing 可能是入站也可能是出站，不能仅凭 clientId 大小决定。
                                 // 必须同时考虑 existing 的角色：
                                 //   - existing 是 Client（出站）且 local < target → 这就是约定保留的连接 → keepExisting
                                 //   - existing 是 Server（入站）且 local > target → 对端出站 = 本端入站 = existing → keepExisting
                                 //   - 其他情况 → 丢弃 existing，保留新连接
                                 bool replacedDuplicateConnection = false;
                                 bool registerHelloConnectionForTarget = true;
                                 if (connectionsByTargetId.contains(actualTargetId)) {
                                     PeerConnection* existing = connectionsByTargetId.value(actualTargetId);
                                     if (existing && existing != connection && existing->isConnected()) {
                                         const bool existingIsLocalOutbound =
                                             (existing->connectionRole() == ConnectionRole::Client);
                                         const auto duplicateAction =
                                             ConnectionRegistryUtils::duplicatePeerConnectionAction(
                                                 localClientId,
                                                 actualTargetId,
                                                 existingIsLocalOutbound,
                                                 hello->capabilities);
                                         const bool keepExisting =
                                             duplicateAction == ConnectionRegistryUtils::DuplicatePeerConnectionAction::KeepExistingDropNew
                                             || duplicateAction == ConnectionRegistryUtils::DuplicatePeerConnectionAction::KeepBothPreferExisting;
                                         const bool keepBoth =
                                             duplicateAction == ConnectionRegistryUtils::DuplicatePeerConnectionAction::KeepBothPreferExisting
                                             || duplicateAction == ConnectionRegistryUtils::DuplicatePeerConnectionAction::KeepBothPreferNew;
                                         // 两端必须保留同一条 TCP：min(id) 发起的出站
                                         qInfo().noquote() << "[peer-diag] DUPLICATE connection for"
                                                           << actualTargetId.left(8)
                                                           << "action="
                                                           << ConnectionRegistryUtils::duplicatePeerConnectionActionName(duplicateAction)
                                                           << "localId=" << localClientId.left(8)
                                                           << "targetId=" << actualTargetId.left(8)
                                                           << "peerCaps=" << hello->capabilities.join('|')
                                                           << "existingRole=" << (existingIsLocalOutbound ? "Client" : "Server")
                                                           << "newRole=" << (connection->connectionRole() == ConnectionRole::Client ? "Client" : "Server");
                                         if (keepBoth && keepExisting) {
                                             ConnectionRegistryUtils::removePointerEntries(connectionsByTargetId,
                                                                                          connection);
                                             registerHelloConnectionForTarget = false;
                                         } else if (keepBoth) {
                                             ConnectionRegistryUtils::removePointerEntries(connectionsByTargetId,
                                                                                          existing);
                                             replacedDuplicateConnection = true;
                                         } else if (keepExisting) {
                                             ConnectionRegistryUtils::removePointerEntries(connectionsByTargetId,
                                                                                          connection);
                                             // 先断开 loser 的所有信号，防止 deleteLater 触发 disconnected handler
                                             // 误删赢家连接或产生副作用（markPeerDisconnected 等）
                                             connection->disconnect();
                                             connection->deleteLater();
                                             return;
                                         } else {
                                             ConnectionRegistryUtils::removePointerEntries(connectionsByTargetId,
                                                                                          existing);
                                             peerIdsByConnection.remove(existing);
                                             existing->disconnect();
                                             existing->deleteLater();
                                             replacedDuplicateConnection = true;
                                             // 继续执行，让 connection 注册为新的活跃连接
                                         }
                                     }
                                 }
                                 const bool repeatedHelloOnRegisteredConnection =
                                     !replacedDuplicateConnection
                                     && registerHelloConnectionForTarget
                                     && connectionsByTargetId.value(actualTargetId) == connection;
                                 const qint64 nowHello = QDateTime::currentMSecsSinceEpoch();
                                 const auto helloIt = helloLastProcessedMs.constFind(actualTargetId);
                                 const bool hasLastProcessedHello =
                                     helloIt != helloLastProcessedMs.constEnd();
                                 if (ConnectionRegistryUtils::shouldThrottlePeerHello(
                                         repeatedHelloOnRegisteredConnection,
                                         hasLastProcessedHello,
                                         nowHello,
                                         hasLastProcessedHello ? helloIt.value() : 0,
                                         kHelloDedupIntervalMs)) {
                                     return;
                                 }
                                 helloLastProcessedMs.insert(actualTargetId, nowHello);

                                 const bool peerUiChanged = rememberPeer(actualTargetId,
                                                                         hello->displayName,
                                                                         resolvedHost,
                                                                         resolvedPort,
                                                                         true,
                                                                         hello->presence,
                                                                         hello->avatarBase64);
                                 qInfo().noquote() << "[peer-diag] HELLO received from"
                                                   << resolvedHost << ":" << resolvedPort
                                                   << "actualId=" << actualTargetId.left(8)
                                                   << "name=" << hello->displayName
                                                   << "role=" << (connection->connectionRole() == ConnectionRole::Client ? "Client" : "Server");
                                 peerIdsByConnection.insert(connection, actualTargetId);
                                 if (registerHelloConnectionForTarget) {
                                     connectionsByTargetId.insert(actualTargetId, connection);
                                 }
                                 if (!hello->signature.isEmpty()) {
                                     peerSignatures.insert(actualTargetId, hello->signature);
                                 }
                                 // 存储对端个人资料
                                 {
                                     PeerProfileInfo pi;
                                     pi.department   = hello->department;
                                     pi.jobTitle     = hello->jobTitle;
                                     pi.phoneNumber  = hello->phoneNumber;
                                     pi.gender       = hello->gender;
                                     pi.email        = hello->email;
                                     peerProfiles.insert(actualTargetId, pi);
                                 }
                                 if (!fallbackTargetId.isEmpty() && fallbackTargetId != actualTargetId) {
                                     resolvedTargetIdsByAlias.insert(fallbackTargetId, actualTargetId);
                                     if (registerHelloConnectionForTarget) {
                                         connectionsByTargetId.remove(fallbackTargetId);
                                     }
                                 }
                                 if (registerHelloConnectionForTarget) {
                                     sendPeerDirectorySnapshot(connection, actualTargetId);
                                 }

                                 const QString canonicalConversationId =
                                     DirectConversationAddressing::conversationIdForPeers(localClientId, actualTargetId);
                                 // 不在握手阶段自动创建会话 —— 会话仅在收发消息时才创建，
                                 // 避免级联 PeerDirectorySnapshot 导致会话列表被空会话淹没。
                                 ConversationSummary existingSummary;
                                 const bool hasExistingSummary = tryLoadConversationSummary(
                                     conversationRepository,
                                     canonicalConversationId,
                                     &existingSummary);
                                 if (hasExistingSummary
                                     && QString::fromStdWString(existingSummary.title).trimmed().isEmpty()
                                     && !hello->displayName.trimmed().isEmpty()) {
                                     existingSummary.title = hello->displayName.toStdWString();
                                     conversationRepository.upsertConversation(existingSummary);
                                 }

                                 if (!currentTargetId.isEmpty()
                                     && (currentTargetId == aliasTargetId
                                         || currentTargetId == actualTargetId)) {
                                     currentTargetId = actualTargetId;
                                     currentConversationId = canonicalConversationId;
                                 }

                                 scheduleChatUiRefresh(true, true, true, true, 150, peerUiChanged);
                                 if (!registerHelloConnectionForTarget) {
                                     qInfo().noquote()
                                         << "[peer-diag] retained non-primary legacy connection for"
                                         << actualTargetId.left(8);
                                     return;
                                 }
                                 resendPendingFileOffersForTarget(actualTargetId, connection);
                                 flushPendingMessagesForTarget(actualTargetId, connection);
                                 requestFileTransferResumeForTarget(actualTargetId, connection);
                                 scheduleRemoteMessageSync(QStringLiteral("peer-hello"), 2000);

                                 // TLS 升级协商：如果双方都支持 TLS 且连接尚未加密，
                                 // 由客户端一方发起 TlsUpgrade 请求（两阶段协议）。
                                 // 客户端先发 request，等收到 ready 后再 startClientEncryption。
                                 if (hello->supportsTls && TlsHelper::isAvailable()
                                     && !connection->isEncrypted()
                                     && connection->connectionRole() == ConnectionRole::Client) {
                                     MessageEnvelope tlsReq;
                                     tlsReq.messageId = QStringLiteral("tls-%1").arg(localClientId).toStdString();
                                     tlsReq.type = MessageType::TlsUpgrade;
                                     tlsReq.senderId = toUtf8(localClientId);
                                     tlsReq.body = "request";
                                     connection->sendPayload(
                                         QByteArray::fromStdString(MessageCodec::encode(tlsReq)));
                                     // 不在这里调用 upgradeToTls()，等服务端回复 ready 再启动
                                 }

                                 // 离线补发：分页连续排空，不再只处理前 200 条，也不按
                                 // 年龄静默删除未确认消息。新客户端等待持久化回执，旧
                                 // 客户端保持写成功后清理的兼容行为。
                                 flushPendingGroupEnvelopesForTarget(
                                     actualTargetId, connection);

                                 // 离线补发：对方重连后，检查哪些群有该成员，补发群元数据
                                 {
                                     const auto groupsForPeer = groupRepository.loadGroupsForMember(
                                         actualTargetId.toStdWString());
                                     for (const auto& g : groupsForPeer) {
                                         if (!g.isActive) {
                                             continue;
                                         }
                                         const QStringList allMemberIds =
                                             groupService.activeMemberIds(QString::fromStdWString(g.groupId));
                                         if (!allMemberIds.contains(actualTargetId)) {
                                             continue;
                                         }
                                         const auto metaEnvs = groupService.buildGroupCreateMetaFanOut(
                                             localClientId, g, allMemberIds);
                                         for (const auto& env : metaEnvs) {
                                             if (QString::fromStdString(env.targetId) == actualTargetId) {
                                                 connection->sendPayload(
                                                     QByteArray::fromStdString(MessageCodec::encode(env)));
                                                 break;
                                             }
                                         }
                                     }
                                 }

                                 // 离线补发：对方重连后，如果本地用户是群管理员/群主，
                                 // 补发群文件服务配置（解决对方离线时错过配置同步的问题）
                                 {
                                     const auto groupsForPeer = groupRepository.loadGroupsForMember(
                                         actualTargetId.toStdWString());
                                     for (const auto& g : groupsForPeer) {
                                         if (!g.isActive) continue;
                                         const QString gid = QString::fromStdWString(g.groupId);
                                         // 只有管理员/群主才补发配置
                                         const auto members = groupRepository.loadMembers(g.groupId);
                                         bool localIsAdmin = false;
                                         for (const auto& m : members) {
                                             if (QString::fromStdWString(m.memberClientId) == localClientId
                                                 && (m.role == L"owner" || m.role == L"admin")) {
                                                 localIsAdmin = true;
                                                 break;
                                             }
                                         }
                                         if (!localIsAdmin) continue;
                                         const auto fsCfg = effectiveGroupFileServiceConfigForGroup(gid);
                                         if (!fsCfg.enabled || fsCfg.baseUrl.trimmed().isEmpty()) continue;
                                         // 构建单条配置同步消息发给刚连接的 peer
                                         const auto cfgEnvs = groupService.buildGroupFileServiceConfigFanOut(
                                             localClientId, gid, fsCfg);
                                         for (const auto& env : cfgEnvs) {
                                             if (QString::fromStdString(env.targetId) == actualTargetId) {
                                                 connection->sendPayload(
                                                     QByteArray::fromStdString(MessageCodec::encode(env)));
                                                 qInfo() << "[group-fs-sync] resent config to reconnected peer"
                                                         << actualTargetId.left(8) << "groupId=" << gid;
                                                 break;
                                             }
                                         }
                                     }
                                 }

                                 return;
                             }

                             // TLS 两阶段协议处理
                             if (decoded->type == MessageType::TlsUpgrade
                                 && !connection->isEncrypted()
                                 && TlsHelper::isAvailable()) {
                                 const QString tlsBody = QString::fromUtf8(
                                     decoded->body.data(),
                                     static_cast<int>(decoded->body.size()));

                                 // 服务端：收到 request → 回复 ready → 启动加密
                                 if (tlsBody == QStringLiteral("request")
                                     && connection->connectionRole() == ConnectionRole::Server) {
                                     MessageEnvelope tlsReady;
                                     tlsReady.messageId = QStringLiteral("tls-ready-%1").arg(localClientId).toStdString();
                                     tlsReady.type = MessageType::TlsUpgrade;
                                     tlsReady.senderId = toUtf8(localClientId);
                                     tlsReady.body = "ready";
                                     connection->sendPayload(
                                         QByteArray::fromStdString(MessageCodec::encode(tlsReady)));
                                     connection->upgradeToTls();
                                     return;
                                 }

                                 // 客户端：收到 ready → 启动加密
                                 if (tlsBody == QStringLiteral("ready")
                                     && connection->connectionRole() == ConnectionRole::Client) {
                                     connection->upgradeToTls();
                                     return;
                                 }
                             }

                             if (decoded->type == MessageType::PeerDirectorySnapshot) {
                                 const auto snapshot = PeerHandshake::parseDirectorySnapshotEnvelope(*decoded);
                                 if (!snapshot.has_value()) {
                                     return;
                                 }

                                 bool discoveredPeerAdded = false;
                                 struct DeferredPeer { QString id; QString host; quint16 port; };
                                 QList<DeferredPeer> peersToConnect;
                                 for (const auto& peer : *snapshot) {
                                     const QString peerClientId = QString::fromStdString(peer.clientId);
                                     const QString peerDisplayName = QString::fromStdString(peer.displayName);
                                     const QString peerHost = QString::fromStdString(peer.host);
                                     if (peerClientId.isEmpty() || peerClientId == localClientId) {
                                         continue;
                                     }

                                     discoveredPeerAdded = rememberPeer(peerClientId,
                                                                        peerDisplayName,
                                                                        peerHost,
                                                                        peer.port,
                                                                        false,
                                                                        peer.presence) || discoveredPeerAdded;
                                     peersToConnect.append({peerClientId, peerHost, peer.port});
                                 }

                                 if (discoveredPeerAdded) {
                                     scheduleChatUiRefresh(true, false, true, true, 150, true);
                                 }

                                 // 目录快照只刷新本地目录；默认不再级联连接快照里的所有 peer。
                                 // 千人规模下这条路径会把拓扑推向全连接，只有显式兼容开关允许时才恢复旧行为。
                                 const bool mayAutoConnectSnapshotPeers =
                                     P2PConnectionPolicy::shouldStartPeerConnection(
                                         RemoteChatServiceSettingsStore::load(),
                                         P2PConnectionTrigger::PeerDirectorySnapshot);
                                 if (!peersToConnect.isEmpty() && mayAutoConnectSnapshotPeers) {
                                     QTimer::singleShot(0, m_mainWindow.get(),
                                         [&, peers = std::move(peersToConnect)]() {
                                             for (const auto& p : peers) {
                                                 tryAutoConnectPeer(p.id, p.host, p.port);
                                             }
                                         });
                                 } else if (!peersToConnect.isEmpty()) {
                                     qInfo().noquote()
                                         << "[peer-diag] peer directory snapshot stored directory-only:"
                                         << "peers=" << peersToConnect.size();
                                 }
                                 return;
                             }

                             if (decoded->type == MessageType::CallControl) {
                                 CallControlPayload payload = callPayloadFromEnvelope(*decoded);
                                 if (payload.senderId.empty()) {
                                     payload.senderId = decoded->senderId;
                                 }
                                 if (payload.targetId.empty()) {
                                     payload.targetId = decoded->targetId;
                                 }
                                 if (payload.callId.empty()) {
                                     return;
                                 }
                                 // 特殊处理：ScreenShareStart 携带 TCP 端口，需要打开观看窗口
                                 if (payload.type == CallControlType::ScreenShareStart
                                     && payload.screenTcpPort > 0) {
                                     const QString peerHost = normalizeHost(connection->peerHost());
                                     qInfo() << "[ScreenShare-RX] ScreenShareStart from"
                                             << QString::fromStdString(payload.senderId)
                                             << "host=" << peerHost
                                             << "port=" << payload.screenTcpPort;
                                     screenViewer = std::make_unique<ScreenViewerWidget>();
                                     screenViewer->setWindowTitle(
                                         QStringLiteral("桌面共享 - %1")
                                             .arg(QString::fromStdString(payload.senderId).left(8)));
                                     screenViewer->resize(1280, 720);
                                     screenViewer->show();
                                     screenViewer->connectToHost(peerHost, payload.screenTcpPort);
                                     // 连接远程控制事件信号 → 通过信令转发给共享端
                                     QObject::connect(screenViewer.get(), &ScreenViewerWidget::remoteMouseEvent,
                                                      m_mainWindow.get(), [&](int type, int x, int y, int button) {
                                         CallControlPayload mp;
                                         mp.type = CallControlType::RemoteMouseInput;
                                         mp.callId = callSession.callId().toStdString();
                                         mp.targetId = callSession.peerId().toStdString();
                                         mp.inputType = type;
                                         mp.inputX = x;
                                         mp.inputY = y;
                                         mp.inputButton = button;
                                         emit callSession.outgoingSignal(mp);
                                     });
                                     QObject::connect(screenViewer.get(), &ScreenViewerWidget::remoteKeyEvent,
                                                      m_mainWindow.get(), [&](int type, int key, int modifiers) {
                                         CallControlPayload kp;
                                         kp.type = CallControlType::RemoteKeyInput;
                                         kp.callId = callSession.callId().toStdString();
                                         kp.targetId = callSession.peerId().toStdString();
                                         kp.inputType = type;
                                         kp.inputButton = key;
                                         kp.inputModifiers = modifiers;
                                         emit callSession.outgoingSignal(kp);
                                     });
                                 }
                                 // 接收端处理远程输入事件 → 注入本地系统
                                 if (payload.type == CallControlType::RemoteMouseInput) {
                                     if (!remoteInput.isEnabled()) {
                                         qWarning() << "[RemoteControl] mouse input received but remoteInput is DISABLED";
                                     }
                                     remoteInput.injectMouseEvent(
                                         payload.inputType, payload.inputX,
                                         payload.inputY, payload.inputButton);
                                     return;
                                 }
                                 if (payload.type == CallControlType::RemoteKeyInput) {
                                     if (!remoteInput.isEnabled()) {
                                         qWarning() << "[RemoteControl] key input received but remoteInput is DISABLED";
                                     }
                                     remoteInput.injectKeyEvent(
                                         payload.inputType, payload.inputButton,
                                         payload.inputModifiers);
                                     return;
                                 }
                                 callSession.handleCallControl(payload);
                                 return;
                             }

                             if (decoded->type == MessageType::FileControl) {
                                 FileControlPayload controlPayload;
                                 if (!FileTransferService::payloadFromEnvelope(*decoded, &controlPayload)) {
                                     return;
                                 }

                                 const QString senderId =
                                     QString::fromUtf8(decoded->senderId.data(),
                                                       static_cast<int>(decoded->senderId.size()));
                                 const auto senderPeer =
                                     peerDirectoryService.findPeerByClientId(toUtf8(senderId));
                                 const QString senderFolderName = senderPeer.has_value()
                                     ? displayNameForPeer(*senderPeer)
                                     : (senderId.isEmpty() ? QStringLiteral("unknown") : senderId);
                                 if (!senderId.isEmpty()) {
                                     connectionsByTargetId.insert(senderId, connection);
                                     peerIdsByConnection.insert(connection, senderId);
                                 }

                                 const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                                 switch (controlPayload.type) {
                                 case FileControlType::Offer: {
                                     FileTransferTask acceptedTask;
                                     if (!fileTransferService.acceptIncomingOffer(controlPayload,
                                                                                 ensureIncomingFilesDirectoryForSender(senderFolderName),
                                                                                 nowMs,
                                                                                 &acceptedTask)) {
                                         m_mainWindow->setStatusMessage(
                                             QStringLiteral("\u6587\u4EF6\u4EFB\u52A1\u521B\u5EFA\u5931\u8D25"),
                                             3000);
                                         return;
                                     }

                                     ensureFileTransferMessage(acceptedTask,
                                                               senderId,
                                                               MessageDeliveryState::Received,
                                                               nowMs);
                                     syncFileTransferMessageState(acceptedTask,
                                                                  acceptedTask.state,
                                                                  MessageDeliveryState::Received,
                                                                  QString::fromStdWString(acceptedTask.tempPath));
                                     if (FileTransferService::shouldSendReadyEnvelope(fileTransferListening)) {
                                         MessageEnvelope acceptEnvelope;
                                         if (fileTransferService.buildReadyEnvelope(
                                                 FileControlType::Accept,
                                                 QString::fromStdWString(acceptedTask.taskId),
                                                 localClientId,
                                                 senderId,
                                                 QString(),
                                                 fileTransferListening ? activeFileTransferPort : 0,
                                                 &acceptEnvelope)) {
                                             connection->sendPayload(
                                                 QByteArray::fromStdString(MessageCodec::encode(acceptEnvelope)));
                                         }
                                     }

                                     const QString acceptedTaskId =
                                         QString::fromStdWString(acceptedTask.taskId);
                                    auto readyRetryAttempts = std::make_shared<int>(0);
                                    const auto scheduleReadyRetry = std::make_shared<std::function<void()>>();
                                    *scheduleReadyRetry = [&, acceptedTaskId, senderId, readyRetryAttempts, scheduleReadyRetry]() {
                                        if (*readyRetryAttempts >= 4) {
                                            return;
                                        }

                                        const int attemptIndex = *readyRetryAttempts;
                                        ++(*readyRetryAttempts);
                                        const int delayMs = 1200 + attemptIndex * 1200;
                                        QTimer::singleShot(delayMs, m_mainWindow.get(),
                                                           [&, acceptedTaskId, senderId, readyRetryAttempts, scheduleReadyRetry]() {
                                            FileTransferTask pendingTask;
                                            if (!fileTransferService.loadTask(acceptedTaskId, &pendingTask)
                                                || pendingTask.direction != FileTransferDirection::Incoming
                                                || pendingTask.state != FileTransferState::ReadyToTransfer) {
                                                return;
                                            }
                                            PeerConnection* retryConnection =
                                                connectionsByTargetId.value(senderId, nullptr);
                                            if (!retryConnection || !retryConnection->isConnected()) {
                                                return;
                                            }
                                            MessageEnvelope retryEnvelope;
                                            if (fileTransferService.buildReadyEnvelope(
                                                    FileControlType::ResumeResponse,
                                                    acceptedTaskId,
                                                    localClientId,
                                                    senderId,
                                                    QString(),
                                                    fileTransferListening ? activeFileTransferPort : 0,
                                                    &retryEnvelope)) {
                                                retryConnection->sendPayload(
                                                    QByteArray::fromStdString(MessageCodec::encode(retryEnvelope)));
                                            }
                                            if (scheduleReadyRetry) {
                                                (*scheduleReadyRetry)();
                                            }
                                        });
                                    };
                                    (*scheduleReadyRetry)();

                                     scheduleDeferredTransferRefresh();

                                     // 文件接收通知：任务栏闪烁 + 托盘气泡
                                     {
                                         const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(senderId));
                                         const QString senderName = peer.has_value() ? displayNameForPeer(*peer) : senderId;
                                         const QString fileName = QString::fromStdWString(acceptedTask.fileName);
                                         notifyUnreadActivity(
                                             senderName,
                                             QStringLiteral("向你发送了文件：%1").arg(fileName));
                                     }
                                     return;
                                 }
                                 case FileControlType::ResumeRequest: {
                                    if (!FileTransferService::shouldSendReadyEnvelope(fileTransferListening)) {
                                         return;
                                     }

                                     MessageEnvelope resumeEnvelope;
                                     if (fileTransferService.buildReadyEnvelope(
                                             FileControlType::ResumeResponse,
                                             QString::fromUtf8(controlPayload.taskId.data(),
                                                               static_cast<int>(controlPayload.taskId.size())),
                                             localClientId,
                                             senderId,
                                             QString(),
                                             fileTransferListening ? activeFileTransferPort : 0,
                                             &resumeEnvelope)) {
                                         connection->sendPayload(
                                             QByteArray::fromStdString(MessageCodec::encode(resumeEnvelope)));
                                         scheduleDeferredTransferRefresh();
                                     }
                                     return;
                                 }
                                 case FileControlType::Accept:
                                 case FileControlType::ResumeResponse: {
                                     const QString taskId =
                                         QString::fromUtf8(controlPayload.taskId.data(),
                                                           static_cast<int>(controlPayload.taskId.size()));
                                      const bool readyApplied =
                                          fileTransferService.applyRemoteReady(controlPayload, nowMs);
                                      FileTransferTask currentTask;
                                      const bool loadedCurrentTask =
                                          fileTransferService.loadTask(taskId, &currentTask);
                                      const bool transferAlreadyInFlight =
                                          loadedCurrentTask
                                          && (isOutgoingTransferInFlightState(currentTask.state)
                                              || activeOutgoingDataPumpTasks.contains(taskId));
                                      const bool canKickTransfer =
                                          loadedCurrentTask
                                          && currentTask.direction == FileTransferDirection::Outgoing
                                          && QString::fromStdWString(currentTask.peerClientId).trimmed() == senderId
                                          && !transferAlreadyInFlight
                                          && (readyApplied || isOutgoingTransferKickableState(currentTask.state));
                                      if (canKickTransfer) {
                                          syncFileTransferMessageState(currentTask,
                                                                       FileTransferState::ReadyToTransfer,
                                                                       MessageDeliveryState::Sent,
                                                                       QString::fromStdWString(currentTask.sourcePath));
                                          startOutgoingDataTransfer(taskId,
                                                                    senderId,
                                                                    connection);
                                         auto transferKickAttempts = std::make_shared<int>(0);
                                         const auto transferKickLoop =
                                             std::make_shared<std::function<void()>>();
                                         *transferKickLoop = [&, taskId, senderId, transferKickAttempts, transferKickLoop]() {
                                             if (*transferKickAttempts >= 3) {
                                                 return;
                                             }
                                             ++(*transferKickAttempts);
                                             QTimer::singleShot(900, m_mainWindow.get(),
                                                                [&, taskId, senderId, transferKickLoop]() {
                                                 FileTransferTask pendingTask;
                                                  if (!fileTransferService.loadTask(taskId, &pendingTask)
                                                      || pendingTask.direction != FileTransferDirection::Outgoing
                                                      || !isOutgoingTransferKickableState(pendingTask.state)
                                                      || activeOutgoingDataPumpTasks.contains(taskId)) {
                                                      return;
                                                  }
                                                  PeerConnection* retryConnection =
                                                     connectionsByTargetId.value(senderId, nullptr);
                                                 if (!retryConnection || !retryConnection->isConnected()) {
                                                     return;
                                                 }
                                                 startOutgoingDataTransfer(taskId, senderId, retryConnection);
                                                 if (transferKickLoop) {
                                                     (*transferKickLoop)();
                                                 }
                                             });
                                         };
                                         (*transferKickLoop)();
                                           scheduleDeferredTransferRefresh();
                                       }
                                      return;
                                  }
                                case FileControlType::Reject:
                                case FileControlType::Fail:
                                case FileControlType::Cancel: {
                                     const FileTransferState state =
                                         controlPayload.type == FileControlType::Cancel ? FileTransferState::Canceled
                                                                                        : FileTransferState::Failed;
                                     FileTransferTask currentTask;
                                     const QString taskId =
                                         QString::fromUtf8(controlPayload.taskId.data(),
                                                           static_cast<int>(controlPayload.taskId.size()));
                                     const bool loadedCurrentTask = fileTransferService.loadTask(taskId, &currentTask);
                                     fileTransferService.markTaskState(
                                         taskId,
                                         state,
                                         loadedCurrentTask ? currentTask.bytesCompleted : 0,
                                         loadedCurrentTask ? currentTask.lastChunkIndex : -1,
                                         controlPayload.type == FileControlType::Reject
                                             ? QStringLiteral("rejected")
                                             : (controlPayload.type == FileControlType::Cancel
                                                    ? QStringLiteral("canceled")
                                                    : QStringLiteral("failed")),
                                         QString::fromUtf8(controlPayload.reason.data(),
                                                           static_cast<int>(controlPayload.reason.size())),
                                         nowMs);
                                     if (loadedCurrentTask) {
                                         syncFileTransferMessageState(
                                             currentTask,
                                             state,
                                             state == FileTransferState::Canceled
                                                 ? MessageDeliveryState::Pending
                                                 : MessageDeliveryState::Failed,
                                             currentTask.direction == FileTransferDirection::Outgoing
                                                 ? QString::fromStdWString(currentTask.sourcePath)
                                                 : QString::fromStdWString(currentTask.tempPath));
                                     }
                                     m_mainWindow->setStatusMessage(
                                         QStringLiteral("\u6587\u4EF6\u4EFB\u52A1 %1 \u5DF2\u7ED3\u675F")
                                             .arg(QString::fromUtf8(controlPayload.taskId.data(),
                                                                    static_cast<int>(controlPayload.taskId.size()))),
                                         3000);
                                     scheduleDeferredTransferRefresh();
                                     return;
                                 }
                                 case FileControlType::Progress:
                                     if (fileTransferService.applyRemoteProgress(controlPayload, nowMs)) {
                                         FileTransferTask currentTask;
                                         const QString taskId =
                                             QString::fromUtf8(controlPayload.taskId.data(),
                                                               static_cast<int>(controlPayload.taskId.size()));
                                         if (fileTransferService.loadTask(taskId, &currentTask)) {
                                             syncFileTransferMessageState(currentTask,
                                                                          currentTask.state,
                                                                          MessageDeliveryState::Sent,
                                                                          QString::fromStdWString(currentTask.sourcePath));
                                         }
                                         // 传输进行中只刷新传输列表，避免频繁全量刷新阻塞事件循环
                                         scheduleDeferredTransferRefresh(false, false, true);
                                     }
                                     return;
                                 case FileControlType::Complete:
                                     {
                                         FileTransferTask completedTask;
                                         const QString completedTaskId =
                                             QString::fromUtf8(controlPayload.taskId.data(),
                                                               static_cast<int>(controlPayload.taskId.size()));
                                         qint64 bytesCompleted = 0;
                                         int lastChunkIndex = -1;
                                         if (fileTransferService.loadTask(completedTaskId, &completedTask)) {
                                             bytesCompleted = completedTask.fileSize;
                                             lastChunkIndex =
                                                 completedTask.chunkCount > 0 ? completedTask.chunkCount - 1 : -1;
                                         }
                                     fileTransferService.markTaskState(
                                         completedTaskId,
                                         FileTransferState::Completed,
                                         bytesCompleted,
                                         lastChunkIndex,
                                         QString(),
                                         QString(),
                                         nowMs);
                                         if (bytesCompleted > 0) {
                                             syncFileTransferMessageState(completedTask,
                                                                          FileTransferState::Completed,
                                                                          MessageDeliveryState::Sent,
                                                                          QString::fromStdWString(completedTask.sourcePath));
                                         }
                                     {
                                         const QString peerCid = QString::fromStdWString(completedTask.peerClientId);
                                         const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(peerCid));
                                         const QString peerName = peer.has_value() ? displayNameForPeer(*peer) : peerCid;
                                         const QString fileName = QString::fromStdWString(completedTask.fileName);
                                         const QString fileSizeText = QLocale().formattedDataSize(completedTask.fileSize);

                                         // 图片文件不生成回执通知（发图频率高，回执会淹没消息）
                                         if (isReceiptQuietImageFileName(fileName)) {
                                             break;
                                         }

                                         const QString notifyBody =
                                             QStringLiteral("%1\u5DF2\u6210\u529F\u63A5\u6536\u4E86\u60A8\u53D1\u9001\u7684\u6587\u4EF6 \u201C%2\u201D (%3)\u3002")
                                                 .arg(peerName, fileName, fileSizeText);

                                         // 写入聊天记录作为持久化通知
                                         const QString taskConversationId = QString::fromStdWString(completedTask.conversationId);
                                         const QString notifyMsgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                                         const qint64 nowMsNotify = QDateTime::currentMSecsSinceEpoch();
                                         ChatMessage notifyMsg{
                                             notifyMsgId.toStdWString(),
                                             taskConversationId.toStdWString(),
                                             localClientId.toStdWString(),
                                             notifyBody.toStdWString(),
                                             nowMsNotify,
                                             MessageDeliveryState::Read,
                                             {}, {},
                                             L"system",
                                             {}
                                         };
                                         conversationRepository.appendMessage(notifyMsg);
                                         // 更新会话摘要，让会话列表显示最新通知
                                         {
                                             QString convTitle;
                                             const QString taskGroupId = QString::fromStdWString(completedTask.groupId).trimmed();
                                             const QString resolvedGroupId = taskGroupId.isEmpty()
                                                 ? taskConversationId.trimmed()
                                                 : taskGroupId;
                                             if (!resolvedGroupId.isEmpty()) {
                                                 if (const auto gOpt = groupRepository.findGroupById(resolvedGroupId.toStdWString());
                                                     gOpt.has_value()) {
                                                     convTitle = QString::fromStdWString(gOpt->groupName);
                                                 }
                                             }
                                             if (convTitle.isEmpty()) {
                                                 convTitle = peerName;
                                             }
                                             upsertConversationSummaryPreservingLatest(
                                                 conversationRepository,
                                                 ConversationSummary{
                                                     taskConversationId.toStdWString(),
                                                     convTitle.toStdWString(),
                                                     notifyBody.toStdWString(),
                                                     nowMsNotify
                                                 });
                                         }

                                         if (QSystemTrayIcon::isSystemTrayAvailable()) {
                                             QSettings trayNotifCheck(AppSettings::organizationName(), AppSettings::applicationName());
                                             if (trayNotifCheck.value(QStringLiteral("notification/trayPopupEnabled"), false).toBool()) {
                                                 trayIcon->showMessage(
                                                     QStringLiteral("\u6587\u4EF6\u4F20\u8F93\u5B8C\u6210"),
                                                     notifyBody,
                                                     QSystemTrayIcon::Information,
                                                     8000);
                                             }
                                         }
                                     }
                                     scheduleDeferredTransferRefresh();
                                     scheduleChatUiRefresh(true, true, true, true);
                                     return;
                                     }
                                 }
                             }

                             const QString senderId =
                                 QString::fromUtf8(decoded->senderId.data(),
                                                   static_cast<int>(decoded->senderId.size()));
                             const QString conversationId =
                                 DirectConversationAddressing::conversationIdForPeers(localClientId, senderId);
                             // 关键诊断日志：记录每条入站消息的核心字段，用于追踪 senderId 覆盖问题
                             qInfo().noquote() << "[msg-in] type=" << static_cast<int>(decoded->type)
                                               << "msgId=" << QString::fromUtf8(decoded->messageId.data(),
                                                                                static_cast<int>(decoded->messageId.size())).left(8)
                                               << "sender=" << senderId.left(8)
                                               << "conv=" << conversationId.left(12);
                             const auto senderPeer =
                                 peerDirectoryService.findPeerByClientId(toUtf8(senderId));
                             const QString senderFolderName = senderPeer.has_value()
                                 ? displayNameForPeer(*senderPeer)
                                 : (senderId.isEmpty() ? QStringLiteral("unknown") : senderId);

                             if (!senderId.isEmpty()) {
                                 connectionsByTargetId.insert(senderId, connection);
                                 peerIdsByConnection.insert(connection, senderId);
                                 // 收到消息即证明对端在线，立刻刷新 lastPresenceAtMs
                                 // 避免 Header 在消息到达时仍显示"离开"
                                 peerDirectoryService.touchPresence(
                                     toUtf8(senderId),
                                     QDateTime::currentMSecsSinceEpoch());
                             }

                             // GroupMeta锛氳鎷夊叆鏂扮兢鏃跺悓姝ョ兢淇℃伅
                             if (decoded->type == MessageType::GroupMeta) {
                                 const QByteArray rawBody(decoded->body.data(),
                                                          static_cast<int>(decoded->body.size()));
                                 const QJsonObject json =
                                     QJsonDocument::fromJson(rawBody).object();
                                 const QString eventType =
                                     json.value(QStringLiteral("event_type")).toString(QStringLiteral("create"));
                                 const QString groupId =
                                     json.value(QStringLiteral("group_id")).toString();
                                 const QString groupName =
                                     json.value(QStringLiteral("group_name")).toString();
                                 const QString ownerClientId =
                                     json.value(QStringLiteral("owner_client_id")).toString();
                                 const QString announcement =
                                     json.value(QStringLiteral("announcement")).toString();
                                 const QString affectedMemberId =
                                     json.value(QStringLiteral("affected_member_id")).toString().trimmed();
                                 const bool hasExplicitVersion =
                                     json.contains(QStringLiteral("version"))
                                     && json.value(QStringLiteral("version")).isDouble();
                                 const int incomingVersion =
                                     json.value(QStringLiteral("version")).toInt();
                                 const qint64 createdAtMs =
                                     static_cast<qint64>(json.value(QStringLiteral("created_at_ms")).toDouble());
                                 const qint64 updatedAtMs =
                                     static_cast<qint64>(json.value(QStringLiteral("updated_at_ms")).toDouble());

                                 if (!groupId.isEmpty() && !groupName.isEmpty()) {
                                     const qint64 incomingUpdatedAtMs = updatedAtMs > 0 ? updatedAtMs : createdAtMs;
                                     if (!groupService.shouldApplyIncomingMeta(groupId,
                                                                               hasExplicitVersion ? incomingVersion : 0,
                                                                               incomingUpdatedAtMs)) {
                                         return;
                                     }

                                     const auto existingGroupOpt =
                                         groupRepository.findGroupById(groupId.toStdWString());
                                     Group newGroup;
                                     newGroup.groupId       = groupId.toStdWString();
                                     newGroup.groupName     = groupName.toStdWString();
                                     newGroup.ownerClientId = ownerClientId.toStdWString();
                                     newGroup.announcement  = announcement.toStdWString();
                                     newGroup.version       = hasExplicitVersion && incomingVersion > 0
                                                                  ? incomingVersion
                                                                  : (existingGroupOpt.has_value()
                                                                         ? std::max(existingGroupOpt->version, 1)
                                                                         : 1);
                                     newGroup.createdAtMs   = createdAtMs;
                                     newGroup.updatedAtMs   = incomingUpdatedAtMs;
                                     newGroup.isActive      = eventType != QStringLiteral("disband");

                                     const bool hasMembersSnapshot =
                                         json.contains(QStringLiteral("members"))
                                         && json.value(QStringLiteral("members")).isArray();
                                     const QJsonArray membersArray =
                                         json.value(QStringLiteral("members")).toArray();
                                     std::vector<GroupMember> members;
                                     QStringList memberIds;
                                     const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                                     // 预先加载已有成员角色，用于 role 缺失时保留
                                     QHash<QString, QString> existingRoles;
                                     if (hasMembersSnapshot) {
                                         const auto existingMembers =
                                             groupRepository.loadMembers(newGroup.groupId);
                                         for (const auto& em : existingMembers) {
                                             existingRoles.insert(
                                                 QString::fromStdWString(em.memberClientId),
                                                 QString::fromStdWString(em.role));
                                         }
                                     }
                                     for (const auto& v : membersArray) {
                                         QString mid;
                                         QString displayName;
                                         QString role;
                                         if (v.isObject()) {
                                             const QJsonObject memberObject = v.toObject();
                                             mid = memberObject.value(QStringLiteral("client_id")).toString();
                                             displayName =
                                                 memberObject.value(QStringLiteral("display_name")).toString();
                                             role = memberObject.value(QStringLiteral("role")).toString();
                                         } else {
                                             mid = v.toString();
                                         }
                                         if (!mid.isEmpty()) {
                                             memberIds.append(mid.trimmed());
                                             GroupMember gm;
                                             gm.groupId                    = newGroup.groupId;
                                             gm.memberClientId             = mid.trimmed().toStdWString();
                                             gm.memberDisplayNameSnapshot  = displayName.toStdWString();
                                             gm.joinedAtMs                 = nowMs;
                                             gm.isActive                   = true;
                                             if (!role.isEmpty()) {
                                                 gm.role = role.toStdWString();
                                             } else {
                                                 // 网络消息未携带 role：保留数据库中已有角色，避免 admin 被降级
                                                 const QString existing = existingRoles.value(mid.trimmed());
                                                 if (!existing.isEmpty()) {
                                                     gm.role = existing.toStdWString();
                                                 } else {
                                                     gm.role = (mid.trimmed() == ownerClientId)
                                                         ? std::wstring(L"owner")
                                                         : std::wstring(L"member");
                                                 }
                                             }
                                             members.push_back(std::move(gm));
                                         }
                                     }
                                     // 接收端保护：remove_member 事件中，确保被移除者不出现在活跃成员快照中
                                     if (eventType == QStringLiteral("remove_member")
                                         && !affectedMemberId.isEmpty()) {
                                         const std::wstring affectedWide = affectedMemberId.toStdWString();
                                         members.erase(
                                             std::remove_if(members.begin(), members.end(),
                                                           [&](const GroupMember& gm) {
                                                               return gm.memberClientId == affectedWide;
                                                           }),
                                             members.end());
                                         memberIds.removeAll(affectedMemberId);
                                     }

                                     groupRepository.upsertGroup(newGroup);
                                     if (hasMembersSnapshot) {
                                         groupRepository.replaceMembers(newGroup.groupId, members);
                                     } else if (eventType == QStringLiteral("remove_member")
                                                && !affectedMemberId.isEmpty()) {
                                         // 即使快照缺失，也需确保被移除者在本地标记为非活跃
                                         auto localMembers = groupRepository.loadMembers(newGroup.groupId);
                                         const std::wstring affWide = affectedMemberId.toStdWString();
                                         bool deactivated = false;
                                         for (auto& lm : localMembers) {
                                             if (lm.memberClientId == affWide && lm.isActive) {
                                                 lm.isActive = false;
                                                 deactivated = true;
                                                 break;
                                             }
                                         }
                                         if (deactivated) {
                                             groupRepository.replaceMembers(newGroup.groupId, localMembers);
                                         }
                                     }

                                     memberIds.removeAll(QString());
                                     memberIds.removeDuplicates();
                                     const bool localStillMember =
                                         !hasMembersSnapshot || memberIds.contains(localClientId);
                                     if (eventType == QStringLiteral("disband")) {
                                         removeGroupConversationLocally(groupId);
                                         m_mainWindow->setStatusMessage(
                                             QStringLiteral("\u7FA4\u300C%1\u300D\u5DF2\u89E3\u6563").arg(groupName),
                                             4000);
                                         scheduleChatUiRefresh(true, true, true, true);
                                         refreshDirectoryGroups();
                                         return;
                                     }

                                     if (!localStillMember && eventType == QStringLiteral("remove_member")
                                         && affectedMemberId == localClientId) {
                                         removeGroupConversationLocally(groupId);
                                         m_mainWindow->setStatusMessage(
                                             QStringLiteral("\u4F60\u5DF2\u88AB\u79FB\u51FA\u7FA4\u300C%1\u300D")
                                                 .arg(groupName),
                                             4000);
                                         scheduleChatUiRefresh(true, true, true, true);
                                         refreshDirectoryGroups();
                                         return;
                                     }

                                     std::wstring previewText;
                                     qint64 lastMessageAtMs = QDateTime::currentMSecsSinceEpoch();
                                     ConversationSummary existingSummary;
                                     if (tryLoadConversationSummary(conversationRepository, groupId, &existingSummary)) {
                                         previewText = existingSummary.lastMessagePreview;
                                         lastMessageAtMs = existingSummary.lastMessageAtMs;
                                     }
                                     if (previewText.empty()) {
                                         previewText = eventType == QStringLiteral("create")
                                                           ? std::wstring(L"\u4F60\u88AB\u9080\u8BF7\u52A0\u5165\u7FA4")
                                                           : QStringLiteral("\u7FA4\u4FE1\u606F\u5DF2\u66F4\u65B0")
                                                                 .toStdWString();
                                     }
                                     if (lastMessageAtMs <= 0) {
                                         lastMessageAtMs = QDateTime::currentMSecsSinceEpoch();
                                     }

                                     upsertConversationSummaryPreservingLatest(
                                         conversationRepository,
                                         ConversationSummary{
                                             newGroup.groupId,
                                             newGroup.groupName,
                                             previewText,
                                             lastMessageAtMs
                                         });

                                     if (eventType == QStringLiteral("create") && !existingGroupOpt.has_value()) {
                                         m_mainWindow->setStatusMessage(
                                             QStringLiteral("\u4F60\u88AB\u9080\u8BF7\u52A0\u5165\u7FA4\u300C%1\u300D")
                                                 .arg(groupName),
                                             4000);
                                     } else if (eventType == QStringLiteral("announcement")) {
                                         m_mainWindow->setStatusMessage(QStringLiteral("\u7FA4\u516C\u544A\u5DF2\u66F4\u65B0"),
                                                                        3000);
                                     } else if (eventType == QStringLiteral("rename")) {
                                         m_mainWindow->setStatusMessage(
                                             QStringLiteral("\u7FA4\u540D\u79F0\u5DF2\u66F4\u65B0\u4E3A\u300C%1\u300D")
                                                 .arg(groupName),
                                             3000);
                                     } else if (eventType == QStringLiteral("remove_member")) {
                                         // 解析退出成员的显示名
                                         QString affectedDisplayName;
                                         if (!affectedMemberId.isEmpty()) {
                                             const auto affectedPeer =
                                                 peerDirectoryService.findPeerByClientId(toUtf8(affectedMemberId));
                                             if (affectedPeer.has_value()) {
                                                 affectedDisplayName = displayNameForPeer(*affectedPeer);
                                             }
                                         }
                                         if (affectedDisplayName.isEmpty()) {
                                             affectedDisplayName = (affectedMemberId == senderId)
                                                 ? senderFolderName : affectedMemberId.left(8);
                                         }

                                         // 插入系统消息 "xxx 退出了群聊"
                                         {
                                             const QString sysMsgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                                             const qint64 sysNowMs = QDateTime::currentMSecsSinceEpoch();
                                             const QString sysBody =
                                                 QStringLiteral("%1 \u9000\u51FA\u4E86\u7FA4\u804A")
                                                     .arg(affectedDisplayName);
                                             ChatMessage sysMsg{
                                                 sysMsgId.toStdWString(),
                                                 groupId.toStdWString(),
                                                 affectedMemberId.toStdWString(),
                                                 sysBody.toStdWString(),
                                                 sysNowMs,
                                                 MessageDeliveryState::Read,
                                                 {}, {},
                                                 L"system",
                                                 {}
                                             };
                                             conversationRepository.appendMessage(sysMsg);
                                             // 更新会话摘要预览
                                             upsertConversationSummaryPreservingLatest(
                                                 conversationRepository,
                                                 ConversationSummary{
                                                     groupId.toStdWString(),
                                                     newGroup.groupName,
                                                     sysBody.toStdWString(),
                                                     sysNowMs
                                                 });
                                         }

                                         // 强制刷新 header（绕过 headerContextChanged 守卫）
                                         if (groupId == currentConversationId) {
                                             lastScheduledHeaderConversationId.clear();
                                         }

                                         m_mainWindow->setStatusMessage(
                                             QStringLiteral("%1 \u9000\u51FA\u4E86\u7FA4\u804A")
                                                 .arg(affectedDisplayName),
                                             3000);
                                     } else if (eventType == QStringLiteral("set_admin")) {
                                         if (affectedMemberId == localClientId) {
                                             m_mainWindow->setStatusMessage(
                                                 QStringLiteral("\u4F60\u5DF2\u88AB\u8BBE\u4E3A\u7FA4\u300C%1\u300D\u7684\u7BA1\u7406\u5458")
                                                     .arg(groupName),
                                                 4000);
                                         } else {
                                             m_mainWindow->setStatusMessage(QStringLiteral("\u7FA4\u7BA1\u7406\u5458\u5DF2\u66F4\u65B0"),
                                                                            3000);
                                         }
                                     } else if (eventType == QStringLiteral("unset_admin")) {
                                         if (affectedMemberId == localClientId) {
                                             m_mainWindow->setStatusMessage(
                                                 QStringLiteral("\u4F60\u5DF2\u88AB\u53D6\u6D88\u7FA4\u300C%1\u300D\u7684\u7BA1\u7406\u5458\u8EAB\u4EFD")
                                                     .arg(groupName),
                                                 4000);
                                         } else {
                                             m_mainWindow->setStatusMessage(QStringLiteral("\u7FA4\u7BA1\u7406\u5458\u5DF2\u66F4\u65B0"),
                                                                            3000);
                                         }
                                     }
                                     scheduleChatUiRefresh(true, true, true, true);
                                     refreshDirectoryGroups();

                                     // 同步置顶消息快照（新成员加入时 GroupMeta 携带历史置顶）
                                     if (json.contains(QStringLiteral("pinned_messages"))
                                         && json.value(QStringLiteral("pinned_messages")).isArray()) {
                                         const QJsonArray pinnedArr =
                                             json.value(QStringLiteral("pinned_messages")).toArray();
                                         for (const QJsonValue& v : pinnedArr) {
                                             const QJsonObject pin = v.toObject();
                                             const QString pinMsgId =
                                                 pin.value(QStringLiteral("message_id")).toString().trimmed();
                                             const QString pinBody =
                                                 pin.value(QStringLiteral("pinned_body")).toString();
                                             const QString pinAuthor =
                                                 pin.value(QStringLiteral("author_name")).toString();
                                             const QString pinPinner =
                                                 pin.value(QStringLiteral("pinner_name")).toString();
                                             const qint64 pinAtMs = static_cast<qint64>(
                                                 pin.value(QStringLiteral("pinned_at_ms")).toDouble());
                                             if (!pinMsgId.isEmpty()) {
                                                 conversationRepository.pinMessageForConversation(
                                                     groupId, pinMsgId, QString(),
                                                     pinPinner, pinAuthor,
                                                     pinBody,
                                                     pinAtMs > 0 ? pinAtMs
                                                                 : QDateTime::currentMSecsSinceEpoch());
                                             }
                                         }
                                         // 若当前会话正是该群，刷新置顶栏
                                         if (groupId == currentConversationId) {
                                             const auto allPins =
                                                 conversationRepository.loadPinnedMessages(groupId);
                                             if (!allPins.empty()) {
                                                 std::vector<PinnedCardInfo> cards;
                                                 cards.reserve(allPins.size());
                                                 for (const auto& p : allPins) {
                                                     cards.push_back({p.messageId, p.pinnedBody,
                                                                      p.authorName, p.pinnerName});
                                                 }
                                                 m_mainWindow->setPinnedMessageCards(cards);
                                             }
                                         }
                                     }
                                 }
                                 return;
                             }

                             // 鏂囦欢鏁版嵁鍧楀鐞嗭紙閫氳繃鎺у埗杩炴帴鍐呰仈鍙戦€侊紝鏃犻渶棰濆 TCP 绔彛锛?
                             // 正在输入指示器（仅单聊）
                             if (decoded->type == MessageType::TypingIndicator) {
                                 if (senderId == currentTargetId
                                     && !groupService.isGroupConversation(currentConversationId)) {
                                     if (m_mainWindow->chatHeaderWidget()) {
                                         m_mainWindow->chatHeaderWidget()->showTypingIndicator();
                                     }
                                 }
                                 return;
                             }

                                                          if (decoded->type == MessageType::FileChunk) {
                                 const QString taskId = QString::fromUtf8(
                                     decoded->fileTaskId.data(),
                                     static_cast<int>(decoded->fileTaskId.size()));
                                 const QByteArray chunkData =
                                     QByteArray::fromBase64(
                                         QByteArray(decoded->body.data(),
                                                    static_cast<int>(decoded->body.size())));
                                 FileTransferChunkHeader chunkHeader;
                                 chunkHeader.taskId = decoded->fileTaskId;
                                 chunkHeader.chunkIndex = decoded->chunkIndex;
                                 chunkHeader.payloadSize = chunkData.size();
                                 handleIncomingChunk(chunkHeader, chunkData);
                                 return;
                             }

                             // Message reaction handler
                             if (decoded->type == MessageType::MessageReaction) {
                                 const QByteArray payloadBytes(decoded->payloadJson.data(),
                                     static_cast<int>(decoded->payloadJson.size()));
                                 const QJsonObject reactionObj = QJsonDocument::fromJson(payloadBytes).object();
                                 const QString targetMessageId = reactionObj.value(QStringLiteral("targetMessageId")).toString();
                                 const QString emoji = reactionObj.value(QStringLiteral("emoji")).toString();
                                 if (!targetMessageId.isEmpty()) {
                                     ChatService::applyReaction(&conversationRepository, targetMessageId, senderId, emoji);
                                     scheduleChatUiRefresh(true, false, false, false, 50);
                                 }
                                 return;
                             }

                             // Pin message handler
                             if (decoded->type == MessageType::PinMessage) {
                                 const QByteArray payloadBytes(decoded->payloadJson.data(),
                                     static_cast<int>(decoded->payloadJson.size()));
                                 const QJsonObject pinObj = QJsonDocument::fromJson(payloadBytes).object();
                                 const QString groupId = pinObj.value(QStringLiteral("group_id")).toString().trimmed();
                                 const QString pinnedMsgId = pinObj.value(QStringLiteral("message_id")).toString().trimmed();
                                 const QString pinnedBody = pinObj.value(QStringLiteral("pinned_body")).toString();
                                 const QString pinnerNameVal = pinObj.value(QStringLiteral("pinner_name")).toString();
                                 const QString authorNameVal = pinObj.value(QStringLiteral("author_name")).toString();
                                 const QString action = pinObj.value(QStringLiteral("action")).toString();
                                 if (action == QStringLiteral("unpin")) {
                                     conversationRepository.unpinMessageForConversation(groupId, pinnedMsgId);
                                 } else if (!pinnedMsgId.isEmpty()) {
                                     conversationRepository.pinMessageForConversation(
                                         groupId, pinnedMsgId, senderId,
                                         pinnerNameVal, authorNameVal,
                                         pinnedBody, QDateTime::currentMSecsSinceEpoch());
                                 }

                                 // 插入系统消息留痕
                                 {
                                     const QString preview = pinnedBody.left(30);
                                     QString sysBody;
                                     if (action == QStringLiteral("unpin")) {
                                         sysBody = preview.isEmpty()
                                             ? QStringLiteral("%1 \u53D6\u6D88\u4E86\u4E00\u6761\u7F6E\u9876\u6D88\u606F")
                                                   .arg(pinnerNameVal)
                                             : QStringLiteral("%1 \u53D6\u6D88\u4E86\u4E00\u6761\u7F6E\u9876\u6D88\u606F\uFF1A%2")
                                                   .arg(pinnerNameVal, preview);
                                     } else {
                                         sysBody = QStringLiteral("%1 \U0001F4CC \u7F6E\u9876\u4E86\u4E00\u6761\u6D88\u606F\uFF1A%2")
                                             .arg(pinnerNameVal, preview);
                                     }
                                     const QString sysMsgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                                     const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                                     ChatMessage sysMsg{
                                         sysMsgId.toStdWString(),
                                         groupId.toStdWString(),
                                         senderId.toStdWString(),
                                         sysBody.toStdWString(),
                                         nowMs,
                                         MessageDeliveryState::Read,
                                         {}, {},
                                         L"system",
                                         {}
                                     };
                                     conversationRepository.appendMessage(sysMsg);
                                 }

                                 if (groupId == currentConversationId) {
                                     const auto allPins = conversationRepository.loadPinnedMessages(groupId);
                                     if (!allPins.empty()) {
                                         std::vector<PinnedCardInfo> cards;
                                         cards.reserve(allPins.size());
                                         for (const auto& p : allPins) {
                                             cards.push_back({p.messageId, p.pinnedBody, p.authorName, p.pinnerName});
                                         }
                                         m_mainWindow->setPinnedMessageCards(cards);
                                     } else {
                                         m_mainWindow->clearPinnedMessageCards();
                                     }
                                 }
                                 scheduleChatUiRefresh(true, true, true, true);
                                 return;
                             }

                             if (decoded->type == MessageType::GroupMessage
                                 || decoded->type == MessageType::ResourceReference) {
                                 const QJsonObject bodyJson = QJsonDocument::fromJson(
                                     QByteArray::fromStdString(decoded->body)).object();
                                 const QString envelopeGroupId = QString::fromUtf8(
                                     decoded->conversationId.data(),
                                     static_cast<int>(decoded->conversationId.size())).trimmed();
                                 const QString bodyGroupId =
                                     bodyJson.value(QStringLiteral("group_id")).toString().trimmed();
                                 const QString groupId = envelopeGroupId.isEmpty() ? bodyGroupId : envelopeGroupId;

                                 // Intercept file service config sync message ASAP.
                                 // It must not depend on local group metadata readiness.
                                 if (bodyJson.value(QStringLiteral("message_kind")).toString()
                                         == QStringLiteral("group_file_service_config")) {
                                     const QJsonObject cfgJson =
                                         bodyJson.value(QStringLiteral("file_service_config")).toObject();
                                     GroupFileServiceConfig cfg;
                                     cfg.groupId     = groupId;
                                     cfg.enabled     = cfgJson.value(QStringLiteral("enabled")).toBool();
                                     cfg.baseUrl     = cfgJson.value(QStringLiteral("base_url")).toString().trimmed();
                                     cfg.bearerToken = cfgJson.value(QStringLiteral("bearer_token")).toString();
                                     cfg.workspaceId = cfgJson.value(QStringLiteral("workspace_id")).toString().trimmed();

                                     if (cfg.groupId.isEmpty()) {
                                         qWarning() << "[group-fs-sync] skip empty groupId in incoming config";
                                         return;
                                     }

                                     qInfo() << "[group-fs-sync] received config groupId=" << cfg.groupId
                                             << "enabled=" << cfg.enabled
                                             << "baseUrl=" << cfg.baseUrl
                                             << "workspaceId=" << cfg.workspaceId;

                                     GroupFileServiceSettingsStore::save(cfg);
                                     // 同步更新运行时架构绑定表，让"未绑定群服务"变为已绑定
                                     {
                                         QVector<GroupServiceBindingSnapshot> existingBindings =
                                             stage2BindingRepository.loadGroupBindings();
                                         existingBindings.erase(
                                             std::remove_if(existingBindings.begin(), existingBindings.end(),
                                                            [&cfg](const GroupServiceBindingSnapshot& b) {
                                                                return b.groupId == cfg.groupId;
                                                            }),
                                             existingBindings.end());
                                         if (cfg.enabled && !cfg.baseUrl.isEmpty()) {
                                             QString groupDisplayName = cfg.groupId;
                                             if (const auto grpOpt = groupRepository.findGroupById(cfg.groupId.toStdWString())) {
                                                 groupDisplayName = QString::fromStdWString(grpOpt->groupName);
                                             }
                                             GroupServiceBindingSnapshot binding;
                                             binding.groupId   = cfg.groupId;
                                             binding.groupName = groupDisplayName;
                                             binding.enabled   = true;
                                             binding.binding.boundServiceId     = cfg.baseUrl;
                                             binding.binding.sharedFilesEnabled = true;
                                             binding.primaryResource.serviceId   = cfg.baseUrl;
                                             binding.primaryResource.workspaceId = cfg.workspaceId;
                                             binding.discoverySnapshot.observedAtMs = QDateTime::currentMSecsSinceEpoch();
                                             existingBindings.push_back(binding);
                                         }
                                         stage2BindingRepository.replaceGroupBindings(existingBindings);
                                         refreshRuntimeArchitectureState();
                                     }
                                     // Replay resource_reference messages that arrived
                                     // before this config sync. Those were stored in the
                                     // conversation but skipped for stage2 sync because
                                     // workspaceId was not yet known.
                                     {
                                         const QString convId = cfg.groupId;
                                         const int replayedCount =
                                             SharedFileResourceSync::replaySharedFileResourcesForConversation(
                                                 convId,
                                                 QStringLiteral("remote-file-service"),
                                                 cfg,
                                                 conversationRepository,
                                                 stage2ResourceRepository);
                                         if (replayedCount > 0) {
                                             refreshRuntimeArchitectureState();
                                         }
                                     }
                                     // Refresh panel if viewing this group
                                     if (auto* panel = m_mainWindow->groupInfoPanel()) {
                                         if (currentConversationId == cfg.groupId) {
                                             panel->setGroupFileServiceConfig(cfg, false);
                                         }
                                     }
                                     return;
                                 }

                                  const auto groupOpt = groupRepository.findGroupById(
                                      groupId.toStdWString());
                                  if (groupOpt.has_value()) {
                                      const QString groupTitle =
                                          QString::fromStdWString(groupOpt->groupName);
                                      const bool isGroupNudge =
                                          decoded->contentType == "nudge";
                                       QString inlineAttachmentName;
                                       QByteArray inlineAttachmentPayload;
                                       QString inlinePreviewText;
                                       const bool hasInlineAttachment = tryExtractInlineGroupAttachment(
                                           *decoded,
                                           &inlineAttachmentName,
                                           &inlineAttachmentPayload,
                                           &inlinePreviewText);
                                       const QString savedInlineAttachmentPath =
                                           hasInlineAttachment
                                               ? saveIncomingAttachmentPayload(inlineAttachmentPayload,
                                                                               inlineAttachmentName,
                                                                               senderFolderName)
                                               : QString();

                                       // 拦截 p2p_file_request — 接收方请求下载 P2P 文件
                                       if (decoded->messageSubtype == "p2p_file_request"
                                           && !decoded->payloadJson.empty()) {
                                           const QJsonObject reqObj = QJsonDocument::fromJson(
                                               QByteArray::fromStdString(decoded->payloadJson)).object();
                                           const QString requesterId = reqObj.value(QStringLiteral("requester_id")).toString();
                                           const QString senderFilePath = reqObj.value(QStringLiteral("sender_file_path")).toString();
                                           if (requesterId.isEmpty() || senderFilePath.isEmpty()) return;
                                           if (!QFile::exists(senderFilePath)) {
                                               qWarning() << "[p2p-file-request] file not found:" << senderFilePath;
                                               return;
                                           }
                                           // 为该请求者创建单个 P2P 传输任务
                                           const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                                           FileTransferTask transferTask;
                                           if (!fileTransferService.createOutgoingTask(
                                                   groupId, requesterId, groupId,
                                                   senderFilePath, nowMs, &transferTask)) {
                                               qWarning() << "[p2p-file-request] task creation failed";
                                               return;
                                           }
                                           PeerConnection* requesterConn = connectionsByTargetId.value(requesterId, nullptr);
                                           if (!requesterConn || !requesterConn->isConnected()) return;
                                           MessageEnvelope offerEnvelope;
                                           if (fileTransferService.buildOfferEnvelope(transferTask,
                                                   localClientId, requesterId, QString(),
                                                   activeFileTransferPort, &offerEnvelope)) {
                                               requesterConn->sendPayload(
                                                   QByteArray::fromStdString(MessageCodec::encode(offerEnvelope)));
                                               if (scheduleOptimisticDataPump) {
                                                   scheduleOptimisticDataPump(
                                                       QString::fromStdWString(transferTask.taskId), requesterId);
                                               }
                                           }
                                           return;
                                       }

                                       // 拦截 group_file_card 消息 — 存储 file_card_json
                                       if (decoded->messageSubtype == "group_file_card"
                                           && !decoded->payloadJson.empty()) {
                                           // 去重检查：防止 INSERT OR REPLACE 覆盖已有消息
                                           {
                                               const QString fcMsgId = QString::fromUtf8(decoded->messageId.data(),
                                                                                          static_cast<int>(decoded->messageId.size()));
                                               ChatMessage existingFc;
                                               if (conversationRepository.findMessageById(fcMsgId, &existingFc)) {
                                                   qInfo().noquote() << "[group_file_card-DEDUP] Skipped duplicate msgId=" << fcMsgId;
                                                   sendDeliveryReceipt(connection, localClientId, *decoded);
                                                   scheduleChatUiRefresh();
                                                   return;
                                               }
                                           }
                                           ChatMessage fileCardMsg;
                                           fileCardMsg.messageId = std::wstring(decoded->messageId.begin(), decoded->messageId.end());
                                           fileCardMsg.conversationId = groupId.toStdWString();
                                           fileCardMsg.senderId = std::wstring(decoded->senderId.begin(), decoded->senderId.end());
                                           fileCardMsg.body = QString::fromUtf8(decoded->body.data(),
                                               static_cast<int>(decoded->body.size())).toStdWString();
                                           fileCardMsg.createdAtMs = decoded->createdAtMs;
                                           fileCardMsg.deliveryState = MessageDeliveryState::Received;
                                           fileCardMsg.messageType = QStringLiteral("group_file_card").toStdWString();
                                           fileCardMsg.fileCardJson = QString::fromStdString(decoded->payloadJson).toStdWString();
                                           if (!conversationRepository.appendMessage(fileCardMsg, QDateTime::currentMSecsSinceEpoch())) {
                                               qWarning().noquote() << "[group_file_card] failed to store incoming msgId="
                                                                    << QString::fromUtf8(decoded->messageId.data(),
                                                                                         static_cast<int>(decoded->messageId.size()));
                                               return;
                                           }
                                           conversationRepository.upsertConversation(ConversationSummary{
                                               groupId.toStdWString(),
                                               QString::fromStdWString(groupOpt->groupName).toStdWString(),
                                               fileCardMsg.body,
                                               fileCardMsg.createdAtMs
                                           });
                                           sendDeliveryReceipt(connection, localClientId, *decoded);

                                           // 同步 shared_file resource 到 stage2 资源目录
                                           {
                                               const QJsonObject cardObj = QJsonDocument::fromJson(
                                                   QString::fromStdString(decoded->payloadJson).toUtf8()).object();
                                               const QString fileId = cardObj.value(QStringLiteral("file_id")).toString();
                                               if (!fileId.isEmpty()) {
                                                   const GroupFileServiceConfig groupCfg =
                                                       effectiveGroupFileServiceConfigForGroup(groupId);
                                                   QJsonObject resRef;
                                                   resRef[QStringLiteral("service_id")]    = QStringLiteral("remote-file-service");
                                                   resRef[QStringLiteral("workspace_id")]  = groupCfg.workspaceId;
                                                   resRef[QStringLiteral("resource_id")]   = fileId;
                                                   resRef[QStringLiteral("resource_kind")] = QStringLiteral("shared_file");
                                                   resRef[QStringLiteral("title")]          = cardObj.value(QStringLiteral("file_name")).toString();
                                                   if (SharedFileResourceSync::syncIncomingSharedFileResource(
                                                           resRef, QStringLiteral("remote-file-service"),
                                                           groupCfg, stage2ResourceRepository)) {
                                                       refreshRuntimeArchitectureState();
                                                   }
                                               }
                                           }

                                           scheduleChatUiRefresh();

                                           // 群文件接收通知
                                           if (!windowCanConsumeIncomingConversation()
                                               || currentConversationId != groupId) {
                                               const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(senderId));
                                               const QString senderName = peer.has_value()
                                                   ? displayNameForPeer(*peer) : senderId;
                                               const QString fileName = QString::fromUtf8(
                                                   decoded->body.data(),
                                                   static_cast<int>(decoded->body.size()));
                                               notifyUnreadActivity(
                                                   QStringLiteral("%1 · %2").arg(groupTitle, senderName),
                                                   QStringLiteral("发送了文件：%1").arg(
                                                       fileName.isEmpty() ? QStringLiteral("新文件") : fileName));
                                           }
                                           return;
                                       }

                                       const bool groupMessageStored =
                                           ChatService::storeIncomingGroupEnvelope(
                                               &conversationRepository, *decoded,
                                               groupId, groupTitle);
                                       if (groupMessageStored) {
                                           sendDeliveryReceipt(connection, localClientId, *decoded);
                                       }
                                       const QString storedMessageId =
                                           QString::fromUtf8(decoded->messageId.data(),
                                                             static_cast<int>(decoded->messageId.size()));
                                       // Sync shared file resource reference to stage2 directory
                                       // so the group shared-file panel stays current for receivers.
                                       {
                                           const QJsonObject msgBody = QJsonDocument::fromJson(
                                               QByteArray::fromStdString(decoded->body)).object();
                                           if (msgBody.value(QStringLiteral("message_kind")).toString()
                                                   == QStringLiteral("resource_reference")) {
                                               const QString serviceId =
                                                   msgBody.value(QStringLiteral("service_id")).toString();
                                               const QString wsId =
                                                   msgBody.value(QStringLiteral("workspace_id")).toString();
                                               const QString resKind =
                                                   msgBody.value(QStringLiteral("resource_kind")).toString();
                                               const bool isSharedFile =
                                                   resKind == QStringLiteral("shared_file")
                                                   || resKind == QStringLiteral("group_file");
                                               const GroupFileServiceConfig groupCfg =
                                                   effectiveGroupFileServiceConfigForGroup(groupId);

                                               const QString resourceId =
                                                   msgBody.value(QStringLiteral("resource_id")).toString();

                                               if (serviceId == QStringLiteral("remote-file-service")
                                                       && isSharedFile) {
                                                   if (resourceId.isEmpty()) {
                                                       qWarning() << "LeyoApplication: resource_reference missing "
                                                                     "resource_id for group" << groupId << "— skipping stage2 upsert";
                                                   } else if (SharedFileResourceSync::syncIncomingSharedFileResource(
                                                                  msgBody,
                                                                  QStringLiteral("remote-file-service"),
                                                                  groupCfg,
                                                                  stage2ResourceRepository)) {
                                                       refreshRuntimeArchitectureState();
                                                   } else {
                                                       qWarning() << "LeyoApplication: received shared file "
                                                                     "resource_reference for group" << groupId
                                                                  << "but workspace binding not ready "
                                                                     "(config.workspaceId=" << groupCfg.workspaceId
                                                                  << "msg.workspace_id=" << wsId
                                                                  << ") — skipping stage2 sync";
                                                   }
                                               }
                                           }
                                       }
                                       if (hasInlineAttachment && !storedMessageId.isEmpty()) {
                                           conversationRepository.updateAttachmentMetadata(
                                               storedMessageId,
                                               inlineAttachmentName,
                                               savedInlineAttachmentPath);
                                       }

                                       // @mention 检测：判断当前用户是否被提及
                                       bool isMentionedInThisMessage = false;
                                       if (!decoded->mentionedIds.empty()) {
                                           const std::string localId = localClientId.toStdString();
                                           for (const auto& mid : decoded->mentionedIds) {
                                               if (mid == "__all__" || mid == localId) {
                                                   isMentionedInThisMessage = true;
                                                   break;
                                               }
                                           }
                                           if (isMentionedInThisMessage) {
                                               conversationRepository.setConversationFlag(
                                                   groupId, ConversationFlag::HasMentionMe, true);
                                           }
                                       }

                                      const bool viewingCurrentGroup =
                                          windowCanConsumeIncomingConversation()
                                          && !groupId.isEmpty() && currentConversationId == groupId;
                                      if ((viewingCurrentGroup || isGroupNudge) && !storedMessageId.isEmpty()) {
                                          currentConversationId = groupId;
                                          currentTargetId.clear();
                                          if (isGroupNudge) {
                                              m_mainWindow->showGroupConversation(groupId, groupTitle);
                                          }
                                          if (viewingCurrentGroup) {
                                              // 先发送已读回执（此时消息仍为 Received 状态，eligible for flush）
                                              flushReadReceipts();
                                              ChatService::markMessageRead(&conversationRepository, storedMessageId);
                                              conversationRepository.setConversationFlag(
                                                  groupId, ConversationFlag::ManuallyUnread, false);
                                              if (!isMentionedInThisMessage) {
                                                  conversationRepository.setConversationFlag(
                                                      groupId, ConversationFlag::HasMentionMe, false);
                                              } else {
                                                  m_mainWindow->showChatToast(
                                                      QStringLiteral("\u6709\u4EBA\u5728\u7FA4\u91CC @\u4E86\u4F60"), 3000);
                                              }
                                          }
                                          if (isGroupNudge) {
                                              restoreMainWindow();
                                              shakeMainWindow();
                                              notifyUnreadActivity(QStringLiteral("\u7FA4\u7A97\u53E3\u6296\u52A8\u63D0\u9192"),
                                                                   QStringLiteral("%1 \u7FA4\u6709\u4EBA\u63D0\u9192\u4F60")
                                                                       .arg(groupTitle));
                                          }
                                      } else {
                                           const auto peer =
                                               peerDirectoryService.findPeerByClientId(toUtf8(senderId));
                                           const QString senderName = peer.has_value()
                                               ? displayNameForPeer(*peer)
                                               : senderId;
                                           const QString preview =
                                               isGroupNudge
                                                   ? QStringLiteral("\u5411\u4F60\u53D1\u9001\u4E86\u7FA4\u7A97\u53E3\u6296\u52A8\u63D0\u9192")
                                                   : (hasInlineAttachment
                                                          ? inlinePreviewText
                                                          : groupEnvelopePreviewText(*decoded));
                                           const QString mentionPrefix =
                                               isMentionedInThisMessage
                                                   ? QStringLiteral("[\u6709\u4EBA@\u6211] ")
                                                   : QString();
                                           notifyUnreadActivity(
                                               QStringLiteral("%1 · %2").arg(groupTitle, senderName),
                                               mentionPrefix + (preview.isEmpty() ? QStringLiteral("\u4F60\u6709\u4E00\u6761\u65B0\u7684\u7FA4\u6D88\u606F") : preview));
                                       }
                                  }
                                  scheduleChatUiRefresh(true, true, true, true);
                                 return;
                             }

                             // 先提取并缓存贴纸图片，再 strip gif_base64 后存 DB（防止 WAL 膨胀）
                             if (decoded->messageSubtype == "sticker" && !decoded->payloadJson.empty()) {
                                 // 防护检查：表情消息必须有有效的 pack_id 和 sticker_id
                                 QJsonObject stickerObj = QJsonDocument::fromJson(
                                     QByteArray::fromStdString(decoded->payloadJson)).object();
                                 const QString sPackId = stickerObj.value(QStringLiteral("pack_id")).toString();
                                 const QString sStickerId = stickerObj.value(QStringLiteral("sticker_id")).toString();
                                 if (sPackId.isEmpty() || sStickerId.isEmpty()) {
                                     qWarning().noquote() << "[sticker-guard] REJECTED sticker with invalid IDs msgId="
                                                          << QString::fromUtf8(decoded->messageId.data(),
                                                                               static_cast<int>(decoded->messageId.size()))
                                                          << "pack_id.isEmpty()=" << sPackId.isEmpty()
                                                          << "sticker_id.isEmpty()=" << sStickerId.isEmpty();
                                     scheduleChatUiRefresh(true, true, true, true);
                                     return;
                                 }
                                 // 防护检查：表情消息的 senderId 不能是本地 clientId（防止自己给自己发消息导致数据混乱）
                                 const QString stickerSenderId = QString::fromUtf8(decoded->senderId.data(),
                                                                                     static_cast<int>(decoded->senderId.size())).trimmed();
                                 if (stickerSenderId == localClientId || stickerSenderId.isEmpty()) {
                                     qWarning().noquote() << "[sticker-guard-sender] REJECTED sticker from invalid sender msgId="
                                                          << QString::fromUtf8(decoded->messageId.data(),
                                                                               static_cast<int>(decoded->messageId.size()))
                                                          << "senderId=" << stickerSenderId
                                                          << "localClientId=" << localClientId;
                                     scheduleChatUiRefresh(true, true, true, true);
                                     return;
                                 }
                                 const QByteArray sGifData = QByteArray::fromBase64(
                                     stickerObj.value(QStringLiteral("gif_base64")).toString().toLatin1());
                                 if (!sGifData.isEmpty()) {
                                     StickerManager::instance().cacheReceivedSticker(sPackId, sStickerId, sGifData);
                                 }
                                 // 去掉 gif_base64，用不含大图的版本存 DB
                                 stickerObj.remove(QStringLiteral("gif_base64"));
                                 const QByteArray stripped = QJsonDocument(stickerObj).toJson(QJsonDocument::Compact);
                                 MessageEnvelope strippedEnv = *decoded;
                                 strippedEnv.payloadJson = std::string(stripped.constData(),
                                                                       static_cast<std::size_t>(stripped.size()));
                                 if (!ChatService::storeIncomingEnvelope(localClientId, &conversationRepository, strippedEnv)) {
                                     // 重试一次：瞬态 SQLITE_BUSY 可能导致首次失败
                                     qWarning().noquote() << "[sticker-recv] storeIncomingEnvelope failed, retrying once msgId="
                                                          << QString::fromUtf8(decoded->messageId.data(),
                                                                               static_cast<int>(decoded->messageId.size()));
                                     if (!ChatService::storeIncomingEnvelope(localClientId, &conversationRepository, strippedEnv)) {
                                         qWarning().noquote() << "[sticker-recv] storeIncomingEnvelope retry also failed — message LOST msgId="
                                                              << QString::fromUtf8(decoded->messageId.data(),
                                                                                   static_cast<int>(decoded->messageId.size()));
                                         return;
                                     }
                                 }
                             } else if (!ChatService::storeIncomingEnvelope(localClientId, &conversationRepository, *decoded)) {
                                 qWarning().noquote() << "[msg-recv] storeIncomingEnvelope failed msgId="
                                                      << QString::fromUtf8(decoded->messageId.data(),
                                                                           static_cast<int>(decoded->messageId.size()))
                                                      << "type=" << static_cast<int>(decoded->type)
                                                      << "subtype=" << QString::fromUtf8(decoded->messageSubtype.data(),
                                                                                         static_cast<int>(decoded->messageSubtype.size()));
                                 return;
                             }

                             bool viewingIncomingConversation = false;
                              if (decoded->type == MessageType::ChatText
                                  || decoded->type == MessageType::FileAttachment
                                  || decoded->type == MessageType::ResourceReference
                                  || decoded->type == MessageType::MessageMutation) {
                                  const bool isNudgeMessage =
                                      decoded->type == MessageType::ChatText && decoded->contentType == "nudge";
                                  viewingIncomingConversation =
                                      windowCanConsumeIncomingConversation()
                                      &&
                                      ChatService::shouldAutoActivateIncomingConversation(
                                          currentConversationId,
                                          conversationId,
                                          isNudgeMessage);
                                  if (viewingIncomingConversation) {
                                      currentConversationId = conversationId;
                                      if (!senderId.isEmpty()) {
                                          currentTargetId = senderId;
                                      }
                                      if (isNudgeMessage) {
                                          const auto nudgePeer =
                                              peerDirectoryService.findPeerByClientId(toUtf8(senderId));
                                          const QString nudgeName =
                                              nudgePeer.has_value() ? displayNameForPeer(*nudgePeer)
                                                                    : (senderId.isEmpty()
                                                                           ? QStringLiteral("对方")
                                                                           : senderId);
                                          m_mainWindow->showDirectConversation(conversationId, nudgeName);
                                      }
                                   }
                                   if (isNudgeMessage) {
                                       const auto peer =
                                           peerDirectoryService.findPeerByClientId(toUtf8(senderId));
                                       const QString senderName =
                                           peer.has_value() ? displayNameForPeer(*peer)
                                                            : (senderId.isEmpty()
                                                                   ? QStringLiteral("对方")
                                                                   : senderId);
                                       restoreMainWindow();
                                       shakeMainWindow();
                                       notifyUnreadActivity(QStringLiteral("\u7A97\u53E3\u6296\u52A8\u63D0\u9192"),
                                                            QStringLiteral("%1 正在提醒你").arg(senderName));
                                   }

                                  MessageEnvelope receipt;
                                 receipt.messageId = decoded->messageId;
                                 receipt.type = MessageType::ReceiptReceived;
                                 receipt.senderId = toUtf8(localClientId);
                                 receipt.targetId = decoded->senderId;
                                 receipt.conversationId = decoded->conversationId;
                                 receipt.createdAtMs = QDateTime::currentMSecsSinceEpoch();
                                 qInfo().noquote() << "[receipt-send] msgId="
                                                   << QString::fromUtf8(decoded->messageId.data(),
                                                                        static_cast<int>(decoded->messageId.size())).left(8)
                                                   << "originalSender=" << senderId.left(8)
                                                   << "receiptSender=" << localClientId.left(8);
                                 connection->sendPayload(QByteArray::fromStdString(MessageCodec::encode(receipt)));

                                     if (decoded->type == MessageType::FileAttachment) {
                                         const QString savedPath =
                                             saveIncomingAttachment(*decoded, senderFolderName);
                                         conversationRepository.updateAttachmentMetadata(
                                             QString::fromUtf8(decoded->messageId.data(),
                                                               static_cast<int>(decoded->messageId.size())),
                                             QString::fromUtf8(decoded->attachmentName.data(),
                                                               static_cast<int>(decoded->attachmentName.size())),
                                             savedPath);
                                         if (!savedPath.isEmpty()) {
                                             m_mainWindow->setStatusMessage(
                                                 QStringLiteral("\u5DF2\u6536\u5230\u6587\u4EF6\u5E76\u4FDD\u5B58\u5230 %1")
                                                 .arg(savedPath),
                                             4000);
                                     } else {
                                         m_mainWindow->setStatusMessage(
                                             QStringLiteral("\u5DF2\u6536\u5230\u6587\u4EF6\u6D88\u606F\uFF0C\u4F46\u4FDD\u5B58\u672C\u5730\u5931\u8D25"),
                                             4000);
                                     }
                                 }

                                  if (!viewingIncomingConversation
                                      && decoded->type != MessageType::MessageMutation) {
                                      const auto peer =
                                          peerDirectoryService.findPeerByClientId(toUtf8(senderId));
                                      const QString senderName =
                                          peer.has_value() ? displayNameForPeer(*peer) : senderId;
                                      QString preview;
                                      if (decoded->type == MessageType::FileAttachment) {
                                          preview = QStringLiteral("向你发送了文件：%1")
                                                        .arg(QString::fromUtf8(decoded->attachmentName.data(),
                                                                               static_cast<int>(decoded->attachmentName.size())));
                                      } else if (decoded->type == MessageType::ResourceReference) {
                                          const auto payload = parseResourceReferenceEnvelope(*decoded);
                                          const QString resourceLabel = payload.has_value()
                                                                            ? (!payload->resource.title.trimmed().isEmpty()
                                                                                   ? payload->resource.title.trimmed()
                                                                                   : payload->resource.resourceId.trimmed())
                                                                            : QString();
                                          preview = resourceLabel.isEmpty()
                                                        ? QStringLiteral("向你分享了共享资源")
                                                        : QStringLiteral("向你分享了共享资源：%1").arg(resourceLabel);
                                      } else {
                                          preview = isNudgeMessage
                                                        ? QStringLiteral("\u5411\u4F60\u53D1\u9001\u4E86\u7A97\u53E3\u6296\u52A8\u63D0\u9192")
                                                        : QString::fromUtf8(decoded->body.data(),
                                                                            static_cast<int>(decoded->body.size()))
                                                              .trimmed();
                                      }
                                      notifyUnreadActivity(senderName,
                                                           preview.isEmpty()
                                                               ? QStringLiteral("你收到一条新消息")
                                                               : preview);
                                  }
                             }

                             scheduleChatUiRefresh(true, true, true, true);
                             flushReadReceiptsForIncomingConversations(
                                 QStringList{conversationId});
                         });
    };

    QObject::connect(&peerServer, &PeerServer::connectionAccepted, m_mainWindow.get(),
                     [&](PeerConnection* inbound) { attachConnection(inbound, {}, {}, 0); });
    QObject::connect(&fileTransferServer, &FileTransferServer::connectionAccepted, m_mainWindow.get(),
                     [&](FileTransferConnection* inbound) {
                         QObject::connect(inbound, &FileTransferConnection::chunkReceived, m_mainWindow.get(),
                                          [&](FileTransferChunkHeader header, QByteArray payload) {
                                              handleIncomingChunk(header, payload);
                                          });
                         // 传输连接断开后释放对象，防止内存泄漏
                         QObject::connect(inbound, &FileTransferConnection::disconnected,
                                          inbound, &QObject::deleteLater);
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::conversationSelected, m_mainWindow.get(),
                     [&](const QString& conversationId) {
                        scheduleRecoverySnapshot();
                        // 切换会话时保存旧草稿、清空输入框，然后恢复新会话的草稿
                        if (currentConversationId != conversationId) {
                            m_mainWindow->clearComposerDraft(currentConversationId);
                            m_mainWindow->restoreComposerDraft(conversationId);
                        }
                         currentConversationId = conversationId;
                         const bool isGroupConversation =
                             !conversationId.isEmpty() && groupService.isGroupConversation(conversationId);
                         const QString otherParticipant =
                             DirectConversationAddressing::otherParticipant(localClientId, conversationId);
                         if (isGroupConversation) {
                             currentTargetId.clear();
                         } else {
                             const QString legacyFallbackTarget =
                                 otherParticipant.isEmpty() ? conversationId : otherParticipant;
                             currentTargetId = resolvedTargetIdsByAlias.value(legacyFallbackTarget,
                                                                             legacyFallbackTarget);
                        }
                        syncSelectionState();
                        if (!isGroupConversation) {
                            preflightLegacyP2PForDirectTarget(currentTargetId,
                                                              QStringLiteral("conversation-selected"));
                        }

                        if (isGroupConversation) {
                            // 群聊：一次性构建所有数据，同步设名字→切消息列表→延迟面板/头部
                             const auto groupOpt = groupRepository.findGroupById(conversationId.toStdWString());
                             const QString groupName = groupOpt
                                 ? QString::fromStdWString(groupOpt->groupName)
                                 : conversationId;
                             const QString ownerClientId = groupOpt
                                 ? QString::fromStdWString(groupOpt->ownerClientId)
                                 : QString();
                            const QString announcement = groupOpt
                                ? QString::fromStdWString(groupOpt->announcement) : QString();
                            const auto members = groupRepository.loadMembers(conversationId.toStdWString());

                            // 构建完整成员数据（名字 + 头像 + 面板条目）
                             QHash<QString, QString> memberNames;
                             QHash<QString, QString> memberAvatars;
                             GroupMemberListEntries memberEntries;
                             int activeMemberCount = 0;
                             const QSet<QString> onlineClients = remoteOnlineClientIds();
                             memberNames.reserve(static_cast<int>(members.size()));
                             memberEntries.reserve(static_cast<qsizetype>(members.size()));
                             for (const auto& m : members) {
                                 if (!m.isActive) continue;
                                 ++activeMemberCount;
                                 const QString memberId = QString::fromStdWString(m.memberClientId);
                                 QString displayName;
                                 bool resolvedKnownIdentity = false;
                                 if (memberId == localClientId && !localDisplayName.trimmed().isEmpty()) {
                                     displayName = localDisplayName.trimmed();
                                     resolvedKnownIdentity = true;
                                 }
                                 const auto peerOpt = peerDirectoryService.findPeerByClientId(toUtf8(memberId));
                                 if (displayName.isEmpty() && peerOpt.has_value()) {
                                     displayName = displayNameForPeer(*peerOpt);
                                     resolvedKnownIdentity = !displayName.trimmed().isEmpty();
                                 }
                                 if (displayName.isEmpty() && !m.memberDisplayNameSnapshot.empty()) {
                                     displayName = QString::fromStdWString(m.memberDisplayNameSnapshot);
                                     resolvedKnownIdentity = !displayName.trimmed().isEmpty();
                                 }
                                 if (displayName.isEmpty()) {
                                     displayName = (memberId == localClientId)
                                         ? QStringLiteral("我") : QStringLiteral("未知成员");
                                 }
                                 const QString avatarPath = (resolvedKnownIdentity || memberId == localClientId)
                                     ? cachedAvatarPathForClient(memberId)
                                     : QString();
                                 memberNames.insert(memberId, displayName);
                                 if (!avatarPath.isEmpty()) {
                                     memberAvatars.insert(memberId, avatarPath);
                                 }
                                 const bool isAdmin = (m.role == L"admin");
                                 const bool peerOnline =
                                     peerOpt.has_value()
                                     && PeerPresenceEvaluator::isOnlineOrAway(
                                         *peerOpt,
                                         QDateTime::currentMSecsSinceEpoch());
                                 memberEntries.push_back(GroupMemberListEntry{
                                     memberId, displayName,
                                     memberId == ownerClientId, isAdmin,
                                     memberId == localClientId,
                                     memberId == localClientId || peerOnline || onlineClients.contains(memberId),
                                     avatarPath
                                 });
                             }
                            std::stable_sort(memberEntries.begin(), memberEntries.end(),
                                             [](const GroupMemberListEntry& lhs, const GroupMemberListEntry& rhs) {
                                                 if (lhs.isOwner != rhs.isOwner) return lhs.isOwner;
                                                 if (lhs.isAdmin != rhs.isAdmin) return lhs.isAdmin;
                                                 if (lhs.isSelf != rhs.isSelf) return lhs.isSelf;
                                                 return lhs.displayName.localeAwareCompare(rhs.displayName) < 0;
                                             });

                            // 同步：设置消息模型上下文 + 切换消息列表
                             // 抑制中间 dataChanged 信号，避免对旧消息行的无效重绘
                             messageModel.beginBulkContextUpdate();
                             messageModel.setDisplayContext(localClientId, groupName);
                             messageModel.setAvatarContext(cachedAvatarPathForClient(localClientId), QString());
                            messageModel.setGroupMemberNames(memberNames);
                            messageModel.setGroupMemberAvatars(memberAvatars);
                            messageModel.setGroupActiveMemberCount(activeMemberCount);
                             // 在切换前同步加载传输状态，避免首帧 pureImageBubble 判断不一致（图片先无边框后有边框）
                             {
                                 QHash<QString, MessageListModel::TransferVisualState> ts;
                                 for (const auto& task : fileTransferService.loadTasksForConversation(conversationId)) {
                                     MessageListModel::TransferVisualState vs;
                                     vs.taskId = QString::fromStdWString(task.taskId);
                                     vs.state = task.state;
                                     vs.bytesCompleted = task.bytesCompleted;
                                     vs.fileSize = task.fileSize;
                                     vs.cancelable = task.direction == FileTransferDirection::Outgoing
                                         && (task.state == FileTransferState::PendingOffer
                                             || task.state == FileTransferState::WaitingAccept
                                             || task.state == FileTransferState::ReadyToTransfer
                                             || task.state == FileTransferState::Transferring
                                             || task.state == FileTransferState::Paused
                                             || task.state == FileTransferState::Interrupted);
                                     ts.insert(vs.taskId, vs);
                                 }
                                 messageModel.setTransferStates(ts);
                             }
                            messageModel.endBulkContextUpdate();
                            messageModel.switchToConversation(conversationId);
                            if (!chatDataStore.hasMessages(conversationId)) {
                                QMetaObject::invokeMethod(dbWorker, "loadMessagesForConversation",
                                                          Qt::QueuedConnection,
                                                           Q_ARG(QString, conversationId));
                             }

                             // 延迟：面板 widget 重建 + 头部 + 置顶消息
                             const bool localIsOwnerOrAdmin = ownerClientId == localClientId
                                 || std::any_of(memberEntries.cbegin(), memberEntries.cend(),
                                                [](const GroupMemberListEntry& e) { return e.isSelf && e.isAdmin; });
                             const bool isOwner = (ownerClientId == localClientId);
                             const QString deferConvId = conversationId;
                            QTimer::singleShot(0, m_mainWindow.get(), [&, deferConvId, announcement,
                                                                        memberEntries, localIsOwnerOrAdmin,
                                                                        groupName, activeMemberCount, isOwner]() {
                                if (currentConversationId != deferConvId) return;
                                m_mainWindow->setGroupMembers(memberEntries, localIsOwnerOrAdmin);
                                m_mainWindow->setChatHeaderGroup(
                                    deferConvId, groupName, activeMemberCount);
                                m_mainWindow->setGroupInfoPanel(announcement,
                                                                memberEntries,
                                                                 localIsOwnerOrAdmin);
                                 m_mainWindow->setCurrentUserIsGroupOwner(isOwner);
                                 const auto allPins = conversationRepository.loadPinnedMessages(deferConvId);
                                 if (!allPins.empty()) {
                                     std::vector<PinnedCardInfo> cards;
                                     cards.reserve(allPins.size());
                                     for (const auto& p : allPins) {
                                         cards.push_back({p.messageId, p.pinnedBody, p.authorName, p.pinnerName});
                                     }
                                     m_mainWindow->setPinnedMessageCards(cards);
                                } else {
                                    m_mainWindow->clearPinnedMessageCards();
                                }
                            });
                         } else {
                             // 私聊：同步更新（很快），然后切换消息列表
                             updateChatHeader();
                             // 抑制中间 dataChanged 信号
                             messageModel.beginBulkContextUpdate();
                             // 在切换前同步加载传输状态，避免首帧 pureImageBubble 判断不一致（图片先无边框后有边框）
                             {
                                 QHash<QString, MessageListModel::TransferVisualState> ts;
                                 for (const auto& task : fileTransferService.loadTasksForConversation(conversationId)) {
                                     MessageListModel::TransferVisualState vs;
                                     vs.taskId = QString::fromStdWString(task.taskId);
                                     vs.state = task.state;
                                     vs.bytesCompleted = task.bytesCompleted;
                                     vs.fileSize = task.fileSize;
                                     vs.cancelable = task.direction == FileTransferDirection::Outgoing
                                         && (task.state == FileTransferState::PendingOffer
                                             || task.state == FileTransferState::WaitingAccept
                                             || task.state == FileTransferState::ReadyToTransfer
                                             || task.state == FileTransferState::Transferring
                                             || task.state == FileTransferState::Paused
                                             || task.state == FileTransferState::Interrupted);
                                     ts.insert(vs.taskId, vs);
                                 }
                                 messageModel.setTransferStates(ts);
                             }
                             messageModel.endBulkContextUpdate();
                             messageModel.switchToConversation(conversationId);
                             if (!chatDataStore.hasMessages(conversationId)) {
                                 QMetaObject::invokeMethod(dbWorker, "loadMessagesForConversation",
                                                           Qt::QueuedConnection,
                                                           Q_ARG(QString, conversationId));
                             }
                         }

                         // 会话列表延迟刷新，避免阻塞会话切换的首帧渲染
                         scheduleChatUiRefresh(true, false, false, false, 150);

                         // 延迟执行非关键操作，避免会话切换时主线程连续阻塞
                         QTimer::singleShot(0, m_mainWindow.get(), [&]() {
                             conversationRepository.setConversationFlag(currentConversationId,
                                                                       ConversationFlag::Done,
                                                                       false);
                             conversationRepository.setConversationFlag(currentConversationId,
                                                                       ConversationFlag::HasMentionMe,
                                                                       false);
                             if (windowCanConsumeIncomingConversation()) {
                                 flushReadReceipts();
                             }
                         });
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::contactSelected, m_mainWindow.get(),
                     [&](const QString& clientId) {
                         currentTargetId = clientId;
                         currentConversationId =
                             DirectConversationAddressing::conversationIdForPeers(localClientId, clientId);
                         syncSelectionState();
                         preflightLegacyP2PForDirectTarget(currentTargetId,
                                                           QStringLiteral("contact-selected"));
                         updateChatHeader();

                         // 即时切换消息列表（从 ChatDataStore 缓存）
                         messageModel.switchToConversation(currentConversationId);
                         if (!currentConversationId.isEmpty()
                             && !chatDataStore.hasMessages(currentConversationId)) {
                             QMetaObject::invokeMethod(dbWorker, "loadMessagesForConversation",
                                                       Qt::QueuedConnection,
                                                       Q_ARG(QString, currentConversationId));
                         }

                         scheduleChatUiRefresh(true, false, false, false, 0);
                         // 延迟已读回执，避免联系人切换时阻塞
                         QTimer::singleShot(0, m_mainWindow.get(), [&]() {
                             if (windowCanConsumeIncomingConversation()) {
                                 flushReadReceipts();
                             }
                         });
                         m_mainWindow->focusChatInput();
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::contactProfileRequested, m_mainWindow.get(),
                     [&](const QString& clientId) {
                         const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(clientId));
                         if (!peer.has_value()) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u672A\u627E\u5230\u8BE5\u8054\u7CFB\u4EBA\u8D44\u6599"),
                                                            2500);
                             return;
                         }

                         ElaContentDialog dlg(m_mainWindow.get());
                         dlg.setWindowTitle(QStringLiteral("\u8054\u7CFB\u4EBA\u8D44\u6599"));
                         dlg.setLeftButtonText(QString());
                         dlg.setMiddleButtonText(QString());
                         dlg.setRightButtonText(QStringLiteral("\u5173\u95ED"));

                         // 隐藏左侧和中间空按钮
                         const auto buttons = dlg.findChildren<ElaPushButton*>();
                         if (buttons.size() >= 3) {
                             buttons[0]->hide();
                             buttons[1]->hide();
                         }

                         auto* content = new QWidget(&dlg);
                         auto* mainLayout = new QVBoxLayout(content);
                         mainLayout->setContentsMargins(24, 20, 24, 16);
                         mainLayout->setSpacing(16);

                         // 顶部：昵称大标题 + 在线状态
                         const QString displayName = displayNameForPeer(*peer);
                         const PeerPresenceStatus effectivePresence =
                             PeerPresenceEvaluator::effectivePresence(
                                 *peer,
                                 QDateTime::currentMSecsSinceEpoch());
                         const QString status = effectivePresence == PeerPresenceStatus::Away
                                                    ? QStringLiteral("\u79BB\u5F00")
                                                    : (effectivePresence == PeerPresenceStatus::Online
                                                           ? QStringLiteral("\u5728\u7EBF")
                                                           : QStringLiteral("\u79BB\u7EBF"));
                         const QString statusColor = effectivePresence != PeerPresenceStatus::Offline
                                                        ? QStringLiteral("#4CAF50")
                                                        : QStringLiteral("#9E9E9E");

                         auto* nameLabel = new ElaText(displayName, content);
                         nameLabel->setTextStyle(ElaTextType::Subtitle);
                         mainLayout->addWidget(nameLabel);

                         // 分隔线
                         auto* separator = new QFrame(content);
                         separator->setFrameShape(QFrame::HLine);
                         separator->setStyleSheet(QStringLiteral("color: %1;").arg(AppStyle::border()));
                         separator->setFixedHeight(1);
                         mainLayout->addWidget(separator);

                         // 信息区域
                         auto* infoLayout = new QGridLayout();
                         infoLayout->setSpacing(8);
                         infoLayout->setColumnMinimumWidth(0, 70);

                         auto makeFieldLabel = [&](const QString& text) {
                             auto* lbl = new ElaText(text, content);
                             lbl->setTextStyle(ElaTextType::Caption);
                             lbl->setStyleSheet(QStringLiteral("color: %1;").arg(AppStyle::textMuted()));
                             return lbl;
                         };
                         auto makeValueLabel = [&](const QString& text) {
                             auto* lbl = new ElaText(text, content);
                             lbl->setTextStyle(ElaTextType::Body);
                             lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
                             return lbl;
                         };

                         int row = 0;
                         infoLayout->addWidget(makeFieldLabel(QStringLiteral("\u72B6\u6001")), row, 0);
                         auto* statusVal = makeValueLabel(status);
                         statusVal->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(statusColor));
                         infoLayout->addWidget(statusVal, row, 1);
                         ++row;

                         infoLayout->addWidget(makeFieldLabel(QStringLiteral("Client ID")), row, 0);
                         infoLayout->addWidget(makeValueLabel(clientId), row, 1);
                         ++row;

                         infoLayout->addWidget(makeFieldLabel(QStringLiteral("IP \u5730\u5740")), row, 0);
                         infoLayout->addWidget(makeValueLabel(QString::fromStdString(peer->host)), row, 1);
                         ++row;

                         infoLayout->addWidget(makeFieldLabel(QStringLiteral("\u7AEF\u53E3")), row, 0);
                         infoLayout->addWidget(makeValueLabel(QString::number(peer->port)), row, 1);

                         mainLayout->addLayout(infoLayout);
                         mainLayout->addStretch();

                         dlg.setCentralWidget(content);
                         dlg.exec();
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::avatarProfileRequested, m_mainWindow.get(),
                     [&](const QString& clientId, const QPoint& globalPos) {
                         const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(clientId));
                         ProfileCardPopup::ProfileInfo info;
                         info.clientId = clientId;
                         if (peer.has_value()) {
                             info.displayName = displayNameForPeer(*peer);
                             info.host = QString::fromStdString(peer->host);
                             info.port = peer->port;
                             info.isOnline =
                                 PeerPresenceEvaluator::isOnlineOrAway(
                                     *peer,
                                     QDateTime::currentMSecsSinceEpoch());
                         } else {
                             info.displayName = clientId;
                         }
                         info.signature = peerSignatures.value(clientId);
                         // 填充个人资料字段
                         if (peerProfiles.contains(clientId)) {
                             const auto& pp = peerProfiles.value(clientId);
                             info.department  = pp.department;
                             info.jobTitle    = pp.jobTitle;
                             info.phoneNumber = pp.phoneNumber;
                             info.gender      = pp.gender;
                             info.email       = pp.email;
                         }
                         m_mainWindow->showProfileCard(info, globalPos);
                     });
    // 会话列表头像悬停 → 解析 conversationId → 弹出资料卡
    QObject::connect(m_mainWindow.get(), &MainWindow::conversationAvatarHovered, m_mainWindow.get(),
                     [&](const QString& conversationId, const QPoint& globalPos) {
                         const QString peerId =
                             DirectConversationAddressing::otherParticipant(localClientId, conversationId);
                         if (peerId.isEmpty()) return;
                         const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(peerId));
                         ProfileCardPopup::ProfileInfo info;
                         info.clientId = peerId;
                         if (peer.has_value()) {
                             info.displayName = displayNameForPeer(*peer);
                             info.host = QString::fromStdString(peer->host);
                             info.port = peer->port;
                             info.isOnline =
                                 PeerPresenceEvaluator::isOnlineOrAway(
                                     *peer,
                                     QDateTime::currentMSecsSinceEpoch());
                         } else {
                             info.displayName = peerId;
                         }
                         info.signature = peerSignatures.value(peerId);
                         if (peerProfiles.contains(peerId)) {
                             const auto& pp = peerProfiles.value(peerId);
                             info.department  = pp.department;
                             info.jobTitle    = pp.jobTitle;
                             info.phoneNumber = pp.phoneNumber;
                             info.gender      = pp.gender;
                             info.email       = pp.email;
                         }
                         m_mainWindow->showProfileCard(info, globalPos);
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::contactDeleteRequested, m_mainWindow.get(),
                     [&](const QString& clientId) {
                         peerDirectoryService.removePeerByClientId(toUtf8(clientId));
                         conversationRepository.deleteKnownPeer(clientId);
                         refreshContactList();
                         refreshDirectoryOrg();
                         syncSelectionState();
                         m_mainWindow->setStatusMessage(
                             QStringLiteral("\u5DF2\u5220\u9664\u8054\u7CFB\u4EBA %1").arg(clientId),
                             2500);
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::groupMemberDirectChatRequested, m_mainWindow.get(),
                     [&](const QString& clientId) {
                         const QString normalizedClientId = clientId.trimmed();
                         if (normalizedClientId.isEmpty() || normalizedClientId == localClientId) {
                             return;
                         }
                         currentTargetId = normalizedClientId;
                         currentConversationId =
                             DirectConversationAddressing::conversationIdForPeers(localClientId, normalizedClientId);
                         syncSelectionState();

                         // 即时切换消息列表（从 ChatDataStore 缓存或清空），
                         // 避免仍显示旧的群聊内容
                         messageModel.switchToConversation(currentConversationId);
                         if (!currentConversationId.isEmpty()
                             && !chatDataStore.hasMessages(currentConversationId)) {
                             QMetaObject::invokeMethod(dbWorker, "loadMessagesForConversation",
                                                       Qt::QueuedConnection,
                                                       Q_ARG(QString, currentConversationId));
                         }

                         scheduleChatUiRefresh(true, false, true, false, 0);
                         QTimer::singleShot(0, m_mainWindow.get(), [&]() {
                             if (windowCanConsumeIncomingConversation()) {
                                 flushReadReceipts();
                             }
                         });
                         m_mainWindow->focusChatInput();
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::closeCurrentConversationRequested,
                     m_mainWindow.get(), [&]() {
        currentConversationId.clear();
        currentTargetId.clear();
        m_mainWindow->clearCurrentConversationView();
        scheduleChatUiRefresh(true, true, true, true, 0);
        qInfo() << "[startup-perf] all deferred work done:" << startupTimer.elapsed() << "ms";
    });
    QObject::connect(m_mainWindow.get(), &MainWindow::groupMemberAdminRequested, m_mainWindow.get(),
                     [&](const QString& groupId, const QString& clientId) {
                         const auto groupOpt = groupRepository.findGroupById(groupId.toStdWString());
                         if (!groupOpt.has_value()) {
                             return;
                         }
                         // 检查目标是否已是管理员
                         const auto members = groupRepository.loadMembers(groupOpt->groupId);
                         bool targetIsAdmin = false;
                         for (const auto& m : members) {
                             if (QString::fromStdWString(m.memberClientId) == clientId && m.role == L"admin") {
                                 targetIsAdmin = true;
                                 break;
                             }
                         }

                         GroupEvent adminEvent;
                         if (targetIsAdmin) {
                             // 取消管理员
                             if (!groupService.unsetAdmin(localClientId, groupId, clientId, &adminEvent)) {
                                 m_mainWindow->setStatusMessage(
                                     QStringLiteral("\u53D6\u6D88\u7BA1\u7406\u5458\u5931\u8D25\uFF0C\u4EC5\u7FA4\u4E3B\u53EF\u64CD\u4F5C"),
                                     3500);
                                 return;
                             }
                         } else {
                             // 设置管理员
                             if (!groupService.setAdmin(localClientId, groupId, clientId, &adminEvent)) {
                                 m_mainWindow->setStatusMessage(
                                     QStringLiteral("\u8BBE\u7F6E\u7BA1\u7406\u5458\u5931\u8D25\uFF0C\u53EF\u80FD\u5DF2\u8FBE\u4E0A\u9650\uFF083\u4EBA\uFF09\u6216\u4EC5\u7FA4\u4E3B\u53EF\u64CD\u4F5C"),
                                     3500);
                                 return;
                             }
                         }

                         const auto updatedGroupOpt = groupRepository.findGroupById(groupId.toStdWString());
                         if (updatedGroupOpt.has_value()) {
                             const QStringList activeMembers = activeGroupMemberIds(groupId);
                             broadcastGroupMeta(*updatedGroupOpt,
                                                activeMembers,
                                                activeMembers,
                                                targetIsAdmin ? QStringLiteral("unset_admin") : QStringLiteral("set_admin"),
                                                clientId);
                         }

                         scheduleChatUiRefresh(true, true, true, true, 0);
                         refreshDirectoryGroups();
                         m_mainWindow->setStatusMessage(
                             targetIsAdmin
                                 ? QStringLiteral("\u5DF2\u53D6\u6D88 %1 \u7684\u7BA1\u7406\u5458\u8EAB\u4EFD").arg(clientId)
                                 : QStringLiteral("\u5DF2\u5C06 %1 \u8BBE\u4E3A\u7BA1\u7406\u5458").arg(clientId),
                             3000);
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::groupMemberRemoveRequested, m_mainWindow.get(),
                     [&](const QString& groupId, const QString& clientId) {
                         GroupEvent removeEvent;
                         if (!groupService.removeMember(localClientId, groupId, clientId, &removeEvent)) {
                             m_mainWindow->setStatusMessage(
                                 QStringLiteral("\u79FB\u51FA\u7FA4\u6210\u5458\u5931\u8D25\uFF0C\u8BF7\u786E\u8BA4\u4F60\u662F\u7FA4\u4E3B\u6216\u7BA1\u7406\u5458\u4E14\u76EE\u6807\u4ECD\u5728\u7FA4\u5185"),
                                 3500);
                             return;
                         }

                         const auto updatedGroupOpt = groupRepository.findGroupById(groupId.toStdWString());
                         if (updatedGroupOpt.has_value()) {
                             const QStringList activeMembers = activeGroupMemberIds(groupId);
                             QStringList recipients = activeMembers;
                             recipients.append(clientId);
                             recipients.removeDuplicates();
                             broadcastGroupMeta(*updatedGroupOpt,
                                                activeMembers,
                                                recipients,
                                                QStringLiteral("remove_member"),
                                                clientId);
                         }

                         if (groupId == currentConversationId) {
                             lastScheduledHeaderConversationId.clear();
                         }
                         scheduleChatUiRefresh(true, true, true, true, 0);
                         refreshDirectoryGroups();
                         m_mainWindow->setStatusMessage(
                             QStringLiteral("\u5DF2\u5C06 %1 \u79FB\u51FA\u7FA4\u804A").arg(clientId),
                             3000);
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::groupMemberMuteRequested, m_mainWindow.get(),
                     [&](const QString&, const QString&) {
                         m_mainWindow->setStatusMessage(
                            QStringLiteral("\u6210\u5458\u9759\u97F3\u529F\u80FD\u8FD8\u5728\u8865\u5168\u7FA4\u6743\u9650\u6A21\u578B"),
                            3000);
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::connectRequested, m_mainWindow.get(),
                     [&](const QString& host, quint16 port) {
                         QHostAddress address;
                         if (!address.setAddress(host)) {
                             LeyoDialog::warning(m_mainWindow.get(),
                                                  appDisplayName,
                                                  QStringLiteral("\u65E0\u6548\u7684 IP \u5730\u5740\u3002"));
                             return;
                         }

                         const QString targetId = endpointKey(host, port);
                         if (tryAutoConnectPeer(targetId, host, port)) {
                             m_mainWindow->setStatusMessage(
                                 QStringLiteral("\u6B63\u5728\u8FDE\u63A5 %1:%2 ...").arg(host, QString::number(port)));
                         } else {
                             m_mainWindow->setStatusMessage(
                                 QStringLiteral("%1:%2 \u5DF2\u8FDE\u63A5\u6216\u6B63\u5728\u8FDE\u63A5").arg(host, QString::number(port)), 3000);
                         }
                     });

    // 添加联系人弹窗
    QObject::connect(m_mainWindow.get(), &MainWindow::addContactRequested, m_mainWindow.get(), [&]() {
        auto* dlg = new ConnectIpDialog(m_mainWindow.get());
        dlg->setWindowModality(Qt::WindowModal);

        // 填充已知 Peers 供搜索
        const auto knownPeers = peerDirectoryService.visiblePeers(toUtf8(localClientId));
        QVector<ConnectIpDialog::SearchResult> searchPeers;
        searchPeers.reserve(static_cast<int>(knownPeers.size()));
        for (const auto& p : knownPeers) {
            ConnectIpDialog::SearchResult sr;
            sr.clientId = QString::fromStdString(p.clientId);
            sr.displayName = QString::fromStdString(p.displayName);
            sr.host = QString::fromStdString(p.host);
            sr.port = p.port;
            sr.isOnline = (p.presence == PeerPresenceStatus::Online);
            searchPeers.append(sr);
        }
        dlg->setKnownPeers(searchPeers);

        if (dlg->exec() != QDialog::Accepted) {
            dlg->deleteLater();
            return;
        }

        if (dlg->hasSelectedResults()) {
            const auto selected = dlg->selectedResults();
            for (const auto& r : selected) {
                const QString targetId = endpointKey(r.host, r.port);
                tryAutoConnectPeer(targetId, r.host, r.port);
            }
            m_mainWindow->setStatusMessage(
                QStringLiteral("\u6B63\u5728\u8FDE\u63A5\u5DF2\u9009\u8054\u7CFB\u4EBA\u2026"), 3000);
        } else {
            const QString host = dlg->host();
            const quint16 port = dlg->port();
            if (!host.isEmpty()) {
                QHostAddress address;
                if (!address.setAddress(host)) {
                    LeyoDialog::warning(m_mainWindow.get(), appDisplayName,
                                         QStringLiteral("\u65E0\u6548\u7684 IP \u5730\u5740\u3002"));
                } else {
                    const QString targetId = endpointKey(host, port);
                    if (tryAutoConnectPeer(targetId, host, port)) {
                        m_mainWindow->setStatusMessage(
                            QStringLiteral("\u6B63\u5728\u8FDE\u63A5 %1:%2 ...").arg(host, QString::number(port)));
                    } else {
                        m_mainWindow->setStatusMessage(
                            QStringLiteral("%1:%2 \u5DF2\u8FDE\u63A5\u6216\u6B63\u5728\u8FDE\u63A5").arg(host, QString::number(port)), 3000);
                    }
                }
            }
        }
        dlg->deleteLater();
    });

    // 建群弹窗 鈫?使用 CreateGroupDialog（LoginDialog 椋庢牸锛?
    QObject::connect(m_mainWindow.get(), &MainWindow::createGroupRequested, m_mainWindow.get(), [&]() {
        const auto knownPeers = peerDirectoryService.visiblePeers(toUtf8(localClientId));
        if (knownPeers.empty()) {
            m_mainWindow->setStatusMessage(QStringLiteral("\u5F53\u524D\u6CA1\u6709\u53EF\u62C9\u5165\u7FA4\u7684\u8054\u7CFB\u4EBA"), 3000);
            return;
        }

        QList<PeerEndpoint> peerList;
        peerList.reserve(static_cast<int>(knownPeers.size()));
        for (const auto& p : knownPeers) {
            peerList.append(p);
        }

        auto* dlg = new CreateGroupDialog(peerList, m_mainWindow.get());
        dlg->setWindowModality(Qt::WindowModal);
        if (dlg->exec() != QDialog::Accepted) {
            dlg->deleteLater();
            return;
        }

        const QString groupName    = dlg->groupName();
        const QStringList selected = dlg->selectedMemberIds();
        dlg->deleteLater();

        if (groupName.isEmpty()) {
            m_mainWindow->setStatusMessage(QStringLiteral("\u7FA4\u540D\u79F0\u4E0D\u80FD\u4E3A\u7A7A"), 3000);
            return;
        }

        Group group;
        if (!groupService.createGroup(localClientId, groupName, selected, &group)) {
            m_mainWindow->setStatusMessage(QStringLiteral("\u521B\u5EFA\u7FA4\u5931\u8D25"), 3000);
            return;
        }

        // 通知所有在线成员有新群创建
        {
            const QStringList allMemberIds =
                groupService.activeMemberIds(QString::fromStdWString(group.groupId));
            const auto metaEnvelopes = groupService.buildGroupCreateMetaFanOut(
                localClientId, group, allMemberIds);
            for (const auto& metaEnv : metaEnvelopes) {
                const QString tid = QString::fromStdString(metaEnv.targetId);
                if (PeerConnection* conn = connectionsByTargetId.value(tid, nullptr)) {
                    if (conn->isConnected()) {
                        conn->sendPayload(
                            QByteArray::fromStdString(MessageCodec::encode(metaEnv)));
                    }
                }
            }
        }

        // 先刷新会话列表使新群可见，再通过 conversationSelected 完整切换
        refreshDirectoryGroups();
        scheduleChatUiRefresh(true, false, false, false, 0);
        const QString newGroupId = QString::fromStdWString(group.groupId);
        QTimer::singleShot(50, m_mainWindow.get(), [&, newGroupId]() {
            emit m_mainWindow->conversationSelected(newGroupId);
        });
        m_mainWindow->setStatusMessage(
            QStringLiteral("\u5DF2\u521B\u5EFA\u7FA4\u300C%1\u300D").arg(QString::fromStdWString(group.groupName)),
            3000);
    });

    // 从直聊页面发起建群（预选当前聊天对象）
    QObject::connect(m_mainWindow.get(), &MainWindow::createGroupWithPeerRequested, m_mainWindow.get(), [&](const QString& peerId) {
        const auto knownPeers = peerDirectoryService.visiblePeers(toUtf8(localClientId));
        if (knownPeers.empty()) {
            m_mainWindow->setStatusMessage(QStringLiteral("\u5F53\u524D\u6CA1\u6709\u53EF\u62C9\u5165\u7FA4\u7684\u8054\u7CFB\u4EBA"), 3000);
            return;
        }

        QList<PeerEndpoint> peerList;
        peerList.reserve(static_cast<int>(knownPeers.size()));
        for (const auto& p : knownPeers) {
            peerList.append(p);
        }

        auto* dlg = new CreateGroupDialog(peerList, m_mainWindow.get());
        if (!peerId.isEmpty()) {
            dlg->preSelectMembers(QStringList{peerId});
        }
        dlg->setWindowModality(Qt::WindowModal);
        if (dlg->exec() != QDialog::Accepted) {
            dlg->deleteLater();
            return;
        }

        const QString groupName    = dlg->groupName();
        const QStringList selected = dlg->selectedMemberIds();
        dlg->deleteLater();

        if (groupName.isEmpty()) {
            m_mainWindow->setStatusMessage(QStringLiteral("\u7FA4\u540D\u79F0\u4E0D\u80FD\u4E3A\u7A7A"), 3000);
            return;
        }

        Group group;
        if (!groupService.createGroup(localClientId, groupName, selected, &group)) {
            m_mainWindow->setStatusMessage(QStringLiteral("\u521B\u5EFA\u7FA4\u5931\u8D25"), 3000);
            return;
        }

        // 通知所有在线成员有新群创建
        {
            const QStringList allMemberIds =
                groupService.activeMemberIds(QString::fromStdWString(group.groupId));
            const auto metaEnvelopes = groupService.buildGroupCreateMetaFanOut(
                localClientId, group, allMemberIds);
            for (const auto& metaEnv : metaEnvelopes) {
                const QString tid = QString::fromStdString(metaEnv.targetId);
                if (PeerConnection* conn = connectionsByTargetId.value(tid, nullptr)) {
                    if (conn->isConnected()) {
                        conn->sendPayload(
                            QByteArray::fromStdString(MessageCodec::encode(metaEnv)));
                    }
                }
            }
        }

        // 先刷新会话列表使新群可见，再通过 conversationSelected 完整切换
        refreshDirectoryGroups();
        scheduleChatUiRefresh(true, false, false, false, 0);
        const QString newGroupId = QString::fromStdWString(group.groupId);
        QTimer::singleShot(50, m_mainWindow.get(), [&, newGroupId]() {
            emit m_mainWindow->conversationSelected(newGroupId);
        });
        m_mainWindow->setStatusMessage(
            QStringLiteral("\u5DF2\u521B\u5EFA\u7FA4\u300C%1\u300D").arg(QString::fromStdWString(group.groupName)),
            3000);
    });

    // 缇ゅ叕鍛?
    QObject::connect(m_mainWindow.get(), &MainWindow::groupAnnouncementRequested, m_mainWindow.get(),
                     [&](const QString& groupId) {
        const auto groupOpt = groupRepository.findGroupById(groupId.toStdWString());
        const QString currentAnnouncement = groupOpt
            ? QString::fromStdWString(groupOpt->announcement) : QString();

        auto* dlg = new QDialog(m_mainWindow.get());
        dlg->setWindowTitle(QStringLiteral("\u7fa4\u516c\u544a"));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowModality(Qt::WindowModal);
        dlg->setFixedWidth(440);
        dlg->setStyleSheet(QStringLiteral(
            "QDialog { background:%1; border-radius:12px; }"
            "QLabel#dlgTitle { font-size:18px; font-weight:bold; color:%2; }"
            "QLabel#dlgSub { font-size:13px; color:%3; }"
            "QLabel#fieldLabel { font-size:13px; color:%2; font-weight:500; }"
            "QTextEdit { background:%4; border:1.5px solid %5; border-radius:8px;"
            " padding:8px 14px; font-size:14px; color:%2; }"
            "QTextEdit:focus { border:1.5px solid %6; background:%7; }")
            .arg(AppStyle::surface(), AppStyle::textPrimary(),
                 AppStyle::textSecondary(), AppStyle::surfaceAlt(),
                 AppStyle::border(), AppStyle::accent(), AppStyle::surface()));

        auto* iconLabel = new QLabel(QStringLiteral("\U0001f4e2  \u7fa4\u516c\u544a"), dlg);
        iconLabel->setObjectName(QStringLiteral("dlgTitle"));
        iconLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        auto* subtitleLabel = new QLabel(QStringLiteral("\u8bbe\u7f6e\u6216\u66f4\u65b0\u7fa4\u516c\u544a\u5185\u5bb9"), dlg);
        subtitleLabel->setObjectName(QStringLiteral("dlgSub"));

        auto* fieldLabel = new QLabel(QStringLiteral("\u516c\u544a\u5185\u5bb9"), dlg);
        fieldLabel->setObjectName(QStringLiteral("fieldLabel"));

        auto* edit = new ElaTextEdit(dlg);
        edit->setPlainText(currentAnnouncement);
        edit->setPlaceholderText(QStringLiteral("\u8f93\u5165\u7fa4\u516c\u544a\u5185\u5bb9\u2026"));
        edit->setFixedHeight(120);

        auto* saveBtn = new QPushButton(QStringLiteral("\u4fdd\u5b58"), dlg);
        saveBtn->setFixedSize(100, 36);
        saveBtn->setCursor(Qt::PointingHandCursor);
        saveBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background:#2B5CE6; color:#fff; border:none; border-radius:8px;"
            " font-size:14px; font-weight:bold; }"
            "QPushButton:hover { background:#4070F4; }"
            "QPushButton:pressed { background:#1A44C8; }"));

        auto* cancelBtn = new QPushButton(QStringLiteral("\u53d6\u6d88"), dlg);
        cancelBtn->setFlat(true);
        cancelBtn->setCursor(Qt::PointingHandCursor);
        cancelBtn->setStyleSheet(QStringLiteral(
            "QPushButton { color:#8F959E; font-size:13px; border:none; background:transparent; }"
            "QPushButton:hover { color:#2B5CE6; }"));

        auto* btnRow = new QHBoxLayout;
        btnRow->addWidget(cancelBtn);
        btnRow->addStretch();
        btnRow->addWidget(saveBtn);

        auto* content = new QVBoxLayout(dlg);
        content->setContentsMargins(32, 28, 32, 24);
        content->setSpacing(0);
        content->addWidget(iconLabel);
        content->addSpacing(4);
        content->addWidget(subtitleLabel);
        content->addSpacing(20);
        content->addWidget(fieldLabel);
        content->addSpacing(6);
        content->addWidget(edit);
        content->addSpacing(20);
        content->addLayout(btnRow);

        QObject::connect(saveBtn, &QPushButton::clicked, dlg, [&, dlg, edit, groupId]() mutable {
            const QString text = edit->toPlainText().trimmed();
            if (const auto groupOpt = groupRepository.findGroupById(groupId.toStdWString()); groupOpt.has_value()) {
                Group updatedGroup = *groupOpt;
                updatedGroup.announcement = text.toStdWString();
                updatedGroup.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
                groupRepository.upsertGroup(updatedGroup);
                const QStringList activeMembers = activeGroupMemberIds(groupId);
                broadcastGroupMeta(updatedGroup,
                                   activeMembers,
                                   activeMembers,
                                   QStringLiteral("announcement"));
            } else {
                groupRepository.setAnnouncement(groupId, text);
            }
            scheduleChatUiRefresh(true, true, true, true, 0);
            m_mainWindow->setStatusMessage(QStringLiteral("\u7fa4\u516c\u544a\u5df2\u66f4\u65b0"), 2000);
            dlg->accept();
        });
        QObject::connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);

        dlg->exec();
    });

    // 添加成员
    QObject::connect(m_mainWindow.get(), &MainWindow::groupAddMemberRequested, m_mainWindow.get(),
                     [&](const QString& groupId) {
        const auto knownPeers = peerDirectoryService.visiblePeers(toUtf8(localClientId));
        const auto existingMembers = groupRepository.loadMembers(groupId.toStdWString());
        QSet<QString> existingIds;
        for (const auto& m : existingMembers) {
            existingIds.insert(QString::fromStdWString(m.memberClientId));
        }
        QList<PeerEndpoint> candidates;
        for (const auto& p : knownPeers) {
            const QString pid = QString::fromStdString(p.clientId);
            if (!existingIds.contains(pid)) {
                candidates.append(p);
            }
        }

        // 鈹€鈹€ 缁勫悎瀵硅瘽妗嗭細宸茶繛鎺ユ垚鍛?+ 通过IP添加 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        auto* dlg = new QDialog(m_mainWindow.get());
        dlg->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dlg->setAttribute(Qt::WA_TranslucentBackground);
        dlg->setWindowModality(Qt::WindowModal);
        dlg->setFixedWidth(420);

        auto* outer = new QVBoxLayout(dlg);
        outer->setContentsMargins(0, 0, 0, 0);
        auto* card = new QWidget(dlg);
        card->setObjectName(QStringLiteral("card"));
        card->setStyleSheet(QStringLiteral(
            "#card {"
            "  background: %1;"
            "  border-radius: 16px;"
            "  border: 1px solid %2;"
            "}").arg(AppStyle::surface(), AppStyle::border()));
        outer->addWidget(card);

        auto* vl = new QVBoxLayout(card);
        vl->setContentsMargins(32, 28, 32, 24);
        vl->setSpacing(10);

        // 右上角关闭按钮
        auto* closeBtn = new QPushButton(QStringLiteral("\u2715"), card);
        closeBtn->setFixedSize(28, 28);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background:transparent; border:none; font-size:16px; color:%1; border-radius:14px; }"
            "QPushButton:hover { background:%2; color:%3; }")
            .arg(AppStyle::textMuted(), AppStyle::hoverBg(), AppStyle::textPrimary()));
        QObject::connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::reject);
        auto* topRow = new QHBoxLayout;
        topRow->setContentsMargins(0, 0, 0, 0);
        topRow->addStretch();
        topRow->addWidget(closeBtn);
        vl->addLayout(topRow);

        auto* titleLbl = new QLabel(QStringLiteral("\u6DFB\u52A0\u7FA4\u6210\u5458"), card);
        titleLbl->setAlignment(Qt::AlignCenter);
        titleLbl->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: bold; color: %1;").arg(AppStyle::textPrimary()));
        vl->addWidget(titleLbl);
        vl->addSpacing(4);

        const QString editStyle = QStringLiteral(
            "QLineEdit {"
            "  background: %1;"
            "  border: 1.5px solid %2;"
            "  border-radius: 8px;"
            "  padding: 0 14px;"
            "  font-size: 14px;"
            "  color: %3;"
            "}"
            "QLineEdit:focus {"
            "  border: 1.5px solid %4;"
            "  background: %5;"
            "}").arg(AppStyle::surfaceAlt(), AppStyle::border(),
                     AppStyle::textPrimary(), AppStyle::accent(), AppStyle::surface());

        const QString checkStyle = QStringLiteral(
            "QCheckBox { font-size:14px; color:%1; padding:6px 4px; spacing:8px; }"
            "QCheckBox::indicator { width:18px; height:18px; border:1.5px solid %2;"
            "  border-radius:4px; background:%3; }"
            "QCheckBox::indicator:checked { background:%4; border-color:%4; }")
            .arg(AppStyle::textPrimary(), AppStyle::border(),
                 AppStyle::surfaceAlt(), AppStyle::accent());

        QList<QCheckBox*> memberCheckboxes;
        if (!candidates.isEmpty()) {
            auto* secLbl = new QLabel(QStringLiteral("\u5DF2\u8FDE\u63A5\u7684\u8054\u7CFB\u4EBA\uFF1A"), card);
            secLbl->setStyleSheet(QStringLiteral("font-size: 13px; color: %1; font-weight: 500;").arg(AppStyle::textSecondary()));
            vl->addWidget(secLbl);

            // 搜索框：按名称或 IP 过滤联系人
            auto* searchEdit = new QLineEdit(card);
            searchEdit->setPlaceholderText(QStringLiteral("\u641C\u7D22\u540D\u79F0\u6216 IP \u5730\u5740..."));
            searchEdit->setFixedHeight(36);
            searchEdit->setClearButtonEnabled(true);
            searchEdit->setStyleSheet(editStyle);
            vl->addWidget(searchEdit);

            auto* membersWidget = new QWidget(card);
            auto* membersLayout = new QVBoxLayout(membersWidget);
            membersLayout->setContentsMargins(0, 0, 0, 0);
            membersLayout->setSpacing(0);
            for (const auto& peer : candidates) {
                const QString dn = QString::fromStdString(peer.displayName).trimmed();
                const QString cid = QString::fromStdString(peer.clientId);
                const QString addr = QString::fromStdString(peer.host).trimmed();
                auto* cb = new QCheckBox(dn.isEmpty() ? cid : dn, membersWidget);
                cb->setProperty("clientId", cid);
                cb->setProperty("displayName", dn.toLower());
                cb->setProperty("address", addr.toLower());
                cb->setStyleSheet(checkStyle);
                memberCheckboxes.append(cb);
                membersLayout->addWidget(cb);
            }
            membersLayout->addStretch();

            // 搜索过滤逻辑
            QObject::connect(searchEdit, &QLineEdit::textChanged, dlg,
                             [memberCheckboxes](const QString& text) {
                const QString kw = text.trimmed().toLower();
                for (auto* cb : memberCheckboxes) {
                    if (kw.isEmpty()) {
                        cb->setVisible(true);
                    } else {
                        const bool match =
                            cb->property("displayName").toString().contains(kw)
                            || cb->property("clientId").toString().toLower().contains(kw)
                            || cb->property("address").toString().contains(kw)
                            || cb->text().toLower().contains(kw);
                        cb->setVisible(match);
                    }
                }
            });

            auto* scroll = new QScrollArea(card);
            scroll->setWidget(membersWidget);
            scroll->setWidgetResizable(true);
            scroll->setMaximumHeight(160);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
            vl->addWidget(scroll);

            auto* sep = new ElaFrame(card);
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet(QStringLiteral("color: #E8E9ED;"));
            vl->addWidget(sep);
        }

        // IP 地址连接区域
        auto* ipSecLbl = new QLabel(QStringLiteral("\u901A\u8FC7 IP \u5730\u5740\u6DFB\u52A0\uFF1A"), card);
        ipSecLbl->setStyleSheet(QStringLiteral("font-size: 13px; color: #8F959E; font-weight: 500;"));
        vl->addWidget(ipSecLbl);

        auto* ipRow = new QHBoxLayout();
        ipRow->setSpacing(8);
        auto* ipEdit = new QLineEdit(card);
        ipEdit->setPlaceholderText(QStringLiteral("IP \u5730\u5740"));
        ipEdit->setFixedHeight(44);
        ipEdit->setStyleSheet(editStyle);
        auto* portEdit = new QLineEdit(card);
        portEdit->setPlaceholderText(QStringLiteral("\u7AEF\u53E3"));
        portEdit->setFixedHeight(44);
        portEdit->setFixedWidth(90);
        portEdit->setStyleSheet(editStyle);
        portEdit->setText(QString::number(activeListenPort));
        ipRow->addWidget(ipEdit);
        ipRow->addWidget(portEdit);
        vl->addLayout(ipRow);

        auto* connectBtn = new QPushButton(QStringLiteral("\u7ACB\u5373\u8FDE\u63A5"), card);
        connectBtn->setFixedHeight(44);
        connectBtn->setCursor(Qt::PointingHandCursor);
        connectBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background:#2B5CE6; color:white; border:none; border-radius:8px;"
            "  font-size:14px; font-weight:bold; }"
            "QPushButton:hover { background:#4070F4; }"
            "QPushButton:pressed { background:#1A44C8; }"));
        vl->addWidget(connectBtn);

        vl->addSpacing(8);
        auto* btnRow = new QHBoxLayout();
        auto* cancelBtn = new QPushButton(QStringLiteral("\u53D6\u6D88"), card);
        cancelBtn->setFixedHeight(36);
        cancelBtn->setCursor(Qt::PointingHandCursor);
        cancelBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background:#2B5CE6; color:white; border:none; border-radius:6px;"
            "  font-size:13px; font-weight:bold; padding:0 16px; }"
            "QPushButton:hover { background:#4070F4; }"
            "QPushButton:pressed { background:#1A44C8; }"));
        btnRow->addStretch();
        btnRow->addWidget(cancelBtn);
        if (!candidates.isEmpty()) {
            auto* addBtn = new QPushButton(QStringLiteral("\u6DFB\u52A0\u6240\u9009\u6210\u5458"), card);
            addBtn->setFixedHeight(36);
            addBtn->setCursor(Qt::PointingHandCursor);
            addBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background:#2B5CE6; color:white; border:none; border-radius:6px;"
                "  font-size:13px; font-weight:bold; padding:0 16px; }"
                "QPushButton:hover { background:#4070F4; }"
                "QPushButton:pressed { background:#1A44C8; }"));
            QObject::connect(addBtn, &QPushButton::clicked, dlg, &QDialog::accept);
            btnRow->addWidget(addBtn);
        }
        vl->addLayout(btnRow);

        // 立即连接按钮逻辑
        QObject::connect(connectBtn, &QPushButton::clicked, dlg,
                         [&, dlg, ipEdit, portEdit]() {
            const QString host = ipEdit->text().trimmed();
            const QString portStr = portEdit->text().trimmed();
            bool ok = false;
            const quint16 port = static_cast<quint16>(portStr.toUInt(&ok));
            QHostAddress address;
            if (!ok || port == 0 || !address.setAddress(host)) {
                return;
            }
            const QString targetId = endpointKey(host, port);
            tryAutoConnectPeer(targetId, host, port);
            dlg->reject();
            m_mainWindow->setStatusMessage(
                QStringLiteral("\u6B63\u5728\u8FDE\u63A5 %1:%2\uFF0C\u8FDE\u63A5\u6210\u529F\u540E\u8BF7\u91CD\u65B0\u70B9\u51FB\u300C\u6DFB\u52A0\u6210\u5458\u300D")
                    .arg(host, portStr), 5000);
        });
        QObject::connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);

        if (dlg->exec() != QDialog::Accepted) {
            dlg->deleteLater();
            return;
        }
        QStringList newMemberIds;
        for (auto* cb : memberCheckboxes) {
            if (cb->isChecked()) {
                newMemberIds.append(cb->property("clientId").toString());
            }
        }
        dlg->deleteLater();
        if (newMemberIds.isEmpty()) {
            return;
        }
        GroupEvent addEvent;
        if (!groupService.addMembers(localClientId, groupId, newMemberIds, &addEvent)) {
            m_mainWindow->setStatusMessage(QStringLiteral("\u6DFB\u52A0\u6210\u5458\u5931\u8D25"), 3000);
            return;
        }
        const auto updatedGroupOpt = groupRepository.findGroupById(groupId.toStdWString());
        if (updatedGroupOpt.has_value()) {
            const auto allMembers = groupRepository.loadMembers(groupId.toStdWString());
            QStringList allMemberIds;
            for (const auto& m : allMembers) {
                if (m.isActive) {
                    allMemberIds.append(QString::fromStdWString(m.memberClientId));
                }
            }
            broadcastGroupMeta(*updatedGroupOpt,
                               allMemberIds,
                               allMemberIds,
                               QStringLiteral("members_updated"));

            // Re-push file service config to new members if the group has one configured
            const auto fsCfg = effectiveGroupFileServiceConfigForGroup(groupId);
            if (fsCfg.enabled) {
                const auto cfgEnvelopes = groupService.buildGroupFileServiceConfigFanOut(
                    localClientId, groupId, fsCfg);
                for (const auto& envelope : cfgEnvelopes) {
                    const QString recipId = QString::fromStdString(envelope.targetId);
                    if (!newMemberIds.contains(recipId))
                        continue;
                    PeerConnection* conn = connectionsByTargetId.value(recipId, nullptr);
                    if (conn && conn->isConnected())
                        conn->sendPayload(QByteArray::fromStdString(MessageCodec::encode(envelope)));
                }
            }
        }
        updateChatHeader();
        m_mainWindow->setStatusMessage(
            QStringLiteral("\u5DF2\u6DFB\u52A0 %1 \u4F4D\u6210\u5458").arg(newMemberIds.size()), 3000);
    });

    // 聊天记录
    QObject::connect(m_mainWindow.get(), &MainWindow::groupChatHistoryRequested, m_mainWindow.get(),
                     [&](const QString& groupId) {
        auto messagesPtr = std::make_shared<std::vector<ChatMessage>>(
            ChatService::loadMessages(&conversationRepository, groupId));
        // 构建成员名字映射
        const auto members = groupRepository.loadMembers(groupId.toStdWString());
        auto nameMapPtr = std::make_shared<QHash<QString, QString>>();
        QHash<QString, QString> knownPeerNames;
        for (const PeerEndpoint& knownPeer : conversationRepository.loadKnownPeers()) {
            const QString clientId = QString::fromStdString(knownPeer.clientId).trimmed();
            const QString displayName = QString::fromStdString(knownPeer.displayName).trimmed();
            if (!clientId.isEmpty() && !displayName.isEmpty()) {
                knownPeerNames.insert(clientId, displayName);
            }
        }
        for (const auto& m : members) {
            const QString mid = QString::fromStdWString(m.memberClientId);
            const auto po = peerDirectoryService.findPeerByClientId(toUtf8(mid));
            QString dn = po.has_value() ? displayNameForPeer(*po) : knownPeerNames.value(mid);
            if (dn.isEmpty() && !m.memberDisplayNameSnapshot.empty()) {
                dn = QString::fromStdWString(m.memberDisplayNameSnapshot);
            }
            if (dn.isEmpty()) {
                dn = mid;
            }
            nameMapPtr->insert(mid, dn);
        }
        const auto groupOpt = groupRepository.findGroupById(groupId.toStdWString());
        const QString groupName = groupOpt ? QString::fromStdWString(groupOpt->groupName) : groupId;

        auto* dlg = new QDialog(m_mainWindow.get());
        dlg->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dlg->setAttribute(Qt::WA_TranslucentBackground);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowModality(Qt::WindowModal);
        dlg->resize(580, 500);

        auto* outer = new QVBoxLayout(dlg);
        outer->setContentsMargins(0, 0, 0, 0);
        auto* card = new QWidget(dlg);
        card->setObjectName(QStringLiteral("card"));
        card->setStyleSheet(QStringLiteral(
            "#card { background:#FFFFFF; border-radius:16px; border:1px solid #E0E0E0; }"));
        outer->addWidget(card);

        auto* vl = new QVBoxLayout(card);
        vl->setContentsMargins(28, 24, 28, 20);
        vl->setSpacing(10);

        // 鏍囬
        auto* titleLbl = new QLabel(
            QStringLiteral("\u804a\u5929\u8bb0\u5f55 \u00B7 ") + groupName, card);
        titleLbl->setStyleSheet(
            QStringLiteral("font-size:18px;font-weight:bold;color:#1F2329;"));
        vl->addWidget(titleLbl);

        // 鎼滅储妗?
        auto* searchEdit = new QLineEdit(card);
        searchEdit->setPlaceholderText(QStringLiteral("\u641c\u7d22\u6d88\u606f\u5185\u5bb9..."));
        searchEdit->setFixedHeight(36);
        searchEdit->setStyleSheet(QStringLiteral(
            "QLineEdit { background:#F7F8FA; border:1.5px solid #DEE0E6; border-radius:8px;"
            " padding:0 12px; font-size:13px; color:#1F2329; }"
            "QLineEdit:focus { border-color:#2B5CE6; background:#FFF; }"));
        vl->addWidget(searchEdit);

        // 消息列表区域
        auto* listWidget = new ElaListWidget(card);
        listWidget->setStyleSheet(QStringLiteral(
            "QListWidget { border:none; outline:none; background:transparent; }"
            "QListWidget::item { padding:0; border-bottom:1px solid #F0F0F0; }"
            "QListWidget::item:hover { background:#F5F8FF; }"
            "QListWidget::item:selected { background:#EEF3FF; }"));
        listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

        // 头像调色板（与消息列表一致）
        static const char* kHistAvatarColors[] = {
            "#5273E8", "#2FA484", "#D9963A", "#7B68E6", "#D85A9A", "#3296C4"
        };

        const auto populateList = [messagesPtr, nameMapPtr, listWidget, groupId](const QString& filterText) {
            listWidget->clear();
            for (const auto& msg : *messagesPtr) {
                QTextDocument doc;
                doc.setHtml(QString::fromStdWString(msg.body));
                const QString plain = doc.toPlainText().trimmed();
                if (!filterText.isEmpty()
                    && !plain.contains(filterText, Qt::CaseInsensitive)) {
                    continue;
                }
                const QString senderId = QString::fromStdWString(msg.senderId);
                const QString senderName = nameMapPtr->value(senderId, senderId);
                const QDateTime ts = QDateTime::fromMSecsSinceEpoch(msg.createdAtMs);
                const QString msgId = QString::fromStdWString(msg.messageId);
                const QString attachName = QString::fromStdWString(msg.attachmentName).trimmed();
                const QString localPath = QString::fromStdWString(msg.localFilePath).trimmed();
                const bool isFile = !attachName.isEmpty() || !localPath.isEmpty();

                // 消息预览文本
                QString preview;
                if (isFile) {
                    preview = isChatPreviewImageAttachmentName(attachName)
                                  ? QStringLiteral("\U0001F5BC [图片]")
                                  : QStringLiteral("\U0001F4CE [文件] %1").arg(attachName);
                } else {
                    preview = plain;
                }
                if (preview.length() > 80) preview = preview.left(80) + QStringLiteral("...");

                // 计算头像颜色
                int h = 0;
                for (const QChar ch : senderName)
                    h = (h * 31 + ch.unicode()) & 0x7FFFFFFF;
                const QColor avatarBg(kHistAvatarColors[h % 6]);
                const QString initial = senderName.isEmpty() ? QStringLiteral("?")
                                                             : QString(senderName.at(0));

                // 构建自定义 item widget
                auto* row = new QWidget;
                row->setStyleSheet(QStringLiteral("background:transparent;"));
                auto* rowLayout = new QHBoxLayout(row);
                rowLayout->setContentsMargins(4, 8, 4, 8);
                rowLayout->setSpacing(10);

                // 圆形首字母头像
                auto* avatarLabel = new QLabel(row);
                avatarLabel->setFixedSize(36, 36);
                avatarLabel->setAlignment(Qt::AlignCenter);
                avatarLabel->setStyleSheet(QStringLiteral(
                    "QLabel { background:%1; color:white; border-radius:18px;"
                    " font-size:14px; font-weight:bold; }").arg(avatarBg.name()));
                avatarLabel->setText(initial);
                rowLayout->addWidget(avatarLabel);

                // 右侧信息
                auto* infoWidget = new QWidget(row);
                infoWidget->setStyleSheet(QStringLiteral("background:transparent;"));
                auto* infoLayout = new QVBoxLayout(infoWidget);
                infoLayout->setContentsMargins(0, 0, 0, 0);
                infoLayout->setSpacing(2);

                auto* topRow = new QWidget(infoWidget);
                topRow->setStyleSheet(QStringLiteral("background:transparent;"));
                auto* topLayout = new QHBoxLayout(topRow);
                topLayout->setContentsMargins(0, 0, 0, 0);
                auto* nameLabel = new QLabel(senderName, topRow);
                nameLabel->setStyleSheet(QStringLiteral(
                    "QLabel { font-size:13px; font-weight:500; color:#1F2329; background:transparent; }"));
                auto* timeLabel = new QLabel(ts.toString(QStringLiteral("MM-dd HH:mm")), topRow);
                timeLabel->setStyleSheet(QStringLiteral(
                    "QLabel { font-size:11px; color:#8F959E; background:transparent; }"));
                timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                topLayout->addWidget(nameLabel);
                topLayout->addStretch();
                topLayout->addWidget(timeLabel);
                infoLayout->addWidget(topRow);

                auto* bodyLabel = new QLabel(preview, infoWidget);
                bodyLabel->setStyleSheet(QStringLiteral(
                    "QLabel { font-size:12px; color:#646A73; background:transparent; }"));
                bodyLabel->setWordWrap(false);
                infoLayout->addWidget(bodyLabel);

                rowLayout->addWidget(infoWidget, 1);

                auto* item = new QListWidgetItem(listWidget);
                item->setSizeHint(QSize(0, 56));
                item->setData(Qt::UserRole, msgId);
                item->setData(Qt::UserRole + 1, groupId);
                listWidget->setItemWidget(item, row);
            }
        };
        populateList(QString());

        // 双击跳转到对应消息
        QObject::connect(listWidget, &QListWidget::itemDoubleClicked, dlg,
                [dlg, mainWin = m_mainWindow.get()](QListWidgetItem* item) {
                    const QString msgId = item->data(Qt::UserRole).toString();
                    const QString convId = item->data(Qt::UserRole + 1).toString();
                    if (!msgId.isEmpty()) {
                        dlg->close();
                        emit mainWin->searchResultJumpRequested(convId, msgId);
                    }
                });

        QObject::connect(searchEdit, &QLineEdit::textChanged, dlg,
                [populateList](const QString& text) { populateList(text); });
        vl->addWidget(listWidget, 1);

        auto* closeBtn = new QPushButton(QStringLiteral("\u5173\u95ed"), card);
        closeBtn->setFixedHeight(36);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background:#2B5CE6; color:#fff; border:none; border-radius:8px;"
            " font-size:14px; font-weight:bold; }"
            "QPushButton:hover { background:#4070F4; }"
            "QPushButton:pressed { background:#1A44C8; }"));
        QObject::connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
        vl->addWidget(closeBtn);

        // 非模态显示，避免阻塞事件循环导致截图时假死
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    });

    // 缇よ缃紙鏀圭兢鍚嶏級
    QObject::connect(m_mainWindow.get(), &MainWindow::groupSettingsRequested, m_mainWindow.get(),
                     [&](const QString& groupId) {
        const auto groupOpt = groupRepository.findGroupById(groupId.toStdWString());
        const QString currentName = groupOpt
            ? QString::fromStdWString(groupOpt->groupName) : QString();
        const bool isOwner = groupOpt.has_value()
                             && QString::fromStdWString(groupOpt->ownerClientId) == localClientId;
        bool currentMuted = false;
        for (const auto& summary : conversationRepository.loadConversationSummaries()) {
            if (QString::fromStdWString(summary.conversationId) == groupId) {
                currentMuted = summary.isMuted;
                break;
            }
        }

        auto* dlg = new ElaDialog(m_mainWindow.get());
        dlg->setWindowTitle(QStringLiteral("\u7fa4\u8bbe\u7f6e"));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
        dlg->setIsFixedSize(true);
        dlg->setFixedSize(880, 600);
        dlg->setObjectName(QStringLiteral("groupSettingsDialog"));

        auto applyGroupSettingsTheme = [dlg]() {
            const ElaThemeType::ThemeMode themeMode = eTheme->getThemeMode();
            const QColor pageBg = ElaThemeColor(themeMode, WindowCentralStackBase);
            const QColor cardBg = ElaThemeColor(themeMode, BasicBase);
            const QColor cardAlt = ElaThemeColor(themeMode, BasicBaseDeep);
            const QColor disabledBg = ElaThemeColor(themeMode, BasicDisable);
            const QColor border = ElaThemeColor(themeMode, BasicBorder);
            const QColor borderHover = ElaThemeColor(themeMode, BasicBorderHover);
            const QColor text = ElaThemeColor(themeMode, BasicText);
            const QColor muted = ElaThemeColor(themeMode, BasicDetailsText);
            const QColor primary = ElaThemeColor(themeMode, PrimaryNormal);
            const QColor danger = ElaThemeColor(themeMode, StatusDanger);
            QColor heroEnd = themeMode == ElaThemeType::Light ? primary.lighter(235) : primary.darker(165);
            QColor heroBorder = primary;
            heroBorder.setAlpha(themeMode == ElaThemeType::Light ? 58 : 96);
            dlg->setStyleSheet(QStringLiteral(
                "QDialog#groupSettingsDialog { background:%1; }"
                "QWidget#groupSettingsRightPane, QWidget#groupSettingsBasicPage { background:transparent; }"
                "QFrame#groupSettingsSidebar, QFrame#groupSettingsProfileCard {"
                "  background:%2; border:1px solid %3; border-radius:22px;"
                "}"
                "QFrame#groupSettingsHeroCard {"
                "  background:qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 %2, stop:1 %4);"
                "  border:1px solid %5; border-radius:24px;"
                "}"
                "QLabel { color:%6; }"
                "QLabel#GroupSettingsSecondary, QLabel#GroupSettingsStatus { color:%7; }"
                "QLineEdit {"
                "  background:%8; color:%6; border:1px solid %3; border-radius:9px;"
                "  padding:6px 10px; selection-background-color:%9;"
                "}"
                "QLineEdit:hover { border:1px solid %10; }"
                "QLineEdit:focus { border:1px solid %9; }"
                "QLineEdit:disabled { background:%11; color:%7; }"
                "QCheckBox { color:%6; spacing:8px; }"
                "QPushButton#GroupDangerButton { color:%12; }")
                .arg(pageBg.name(QColor::HexArgb),
                     cardBg.name(QColor::HexArgb),
                     border.name(QColor::HexArgb),
                     heroEnd.name(QColor::HexArgb),
                     heroBorder.name(QColor::HexArgb),
                     text.name(QColor::HexArgb),
                     muted.name(QColor::HexArgb),
                     cardAlt.name(QColor::HexArgb),
                     primary.name(QColor::HexArgb),
                     borderHover.name(QColor::HexArgb),
                     disabledBg.name(QColor::HexArgb),
                     danger.name(QColor::HexArgb)));
        };
        QObject::connect(eTheme, &ElaTheme::themeModeChanged, dlg, [applyGroupSettingsTheme]() {
            applyGroupSettingsTheme();
        });

        auto* root = new QHBoxLayout(dlg);
        root->setContentsMargins(24, 52, 24, 24);
        root->setSpacing(20);

        auto* sidebar = new ElaFrame(dlg);
        sidebar->setObjectName(QStringLiteral("groupSettingsSidebar"));
        sidebar->setFrameShape(QFrame::NoFrame);
        sidebar->setAttribute(Qt::WA_StyledBackground, true);
        sidebar->setFixedWidth(190);
        auto* sidebarLayout = new QVBoxLayout(sidebar);
        sidebarLayout->setContentsMargins(18, 18, 18, 18);
        sidebarLayout->setSpacing(10);

        auto* navTitle = new ElaText(QStringLiteral("\u7FA4\u8BBE\u7F6E"), sidebar);
        navTitle->setTextPixelSize(18);
        QFont navTitleFont = navTitle->font();
        navTitleFont.setWeight(QFont::DemiBold);
        navTitle->setFont(navTitleFont);
        sidebarLayout->addWidget(navTitle);

        auto* navHint = new ElaText(currentName.isEmpty() ? groupId : currentName, sidebar);
        navHint->setObjectName(QStringLiteral("GroupSettingsSecondary"));
        navHint->setTextPixelSize(12);
        navHint->setWordWrap(true);
        sidebarLayout->addWidget(navHint);
        sidebarLayout->addSpacing(12);

        auto* navList = new ElaListView(sidebar);
        navList->setItemHeight(42);
        navList->setIsTransparent(true);
        navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* navModel = new QStringListModel({
            QStringLiteral("\u57FA\u7840\u8BBE\u7F6E")
        }, navList);
        navList->setModel(navModel);
        sidebarLayout->addWidget(navList, 1);

        auto* stack = new ElaStackedWidget(dlg);
        stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        auto* rightPane = new QWidget(dlg);
        rightPane->setObjectName(QStringLiteral("groupSettingsRightPane"));
        auto* rightLayout = new QVBoxLayout(rightPane);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(16);

        auto* heroCard = new ElaFrame(rightPane);
        heroCard->setObjectName(QStringLiteral("groupSettingsHeroCard"));
        heroCard->setFrameShape(QFrame::NoFrame);
        heroCard->setAttribute(Qt::WA_StyledBackground, true);
        auto* heroLayout = new QVBoxLayout(heroCard);
        heroLayout->setContentsMargins(24, 20, 24, 20);
        heroLayout->setSpacing(6);
        auto* heroTitle = new ElaText(QStringLiteral("\u7FA4\u8BBE\u7F6E"), heroCard);
        heroTitle->setTextPixelSize(24);
        QFont heroTitleFont = heroTitle->font();
        heroTitleFont.setWeight(QFont::DemiBold);
        heroTitle->setFont(heroTitleFont);
        auto* heroSubtitle = new ElaText(
            QStringLiteral("\u5E38\u7528\u64CD\u4F5C\u4FDD\u7559\u5728\u9876\u90E8\uFF0C\u8BBE\u7F6E\u7C7B\u5165\u53E3\u96C6\u4E2D\u5230\u8FD9\u91CC\u3002"),
            heroCard);
        heroSubtitle->setObjectName(QStringLiteral("GroupSettingsSecondary"));
        heroSubtitle->setTextPixelSize(13);
        heroSubtitle->setWordWrap(true);
        heroLayout->addWidget(heroTitle);
        heroLayout->addWidget(heroSubtitle);
        rightLayout->addWidget(heroCard);

        auto makeScrollPage = [&](const QString& objectName) {
            auto* scroll = new ElaScrollArea(stack);
            scroll->setObjectName(objectName + QStringLiteral("Scroll"));
            scroll->setWidgetResizable(true);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scroll->setFrameShape(QFrame::NoFrame);
            auto* page = new QWidget(scroll);
            page->setObjectName(objectName);
            page->setAttribute(Qt::WA_StyledBackground, true);
            scroll->setWidget(page);
            return std::pair<ElaScrollArea*, QWidget*>(scroll, page);
        };
        auto makeSectionTitle = [](const QString& text, QWidget* parent) {
            auto* label = new ElaText(text, parent);
            label->setTextPixelSize(18);
            QFont labelFont = label->font();
            labelFont.setWeight(QFont::DemiBold);
            label->setFont(labelFont);
            return label;
        };
        auto makeBodyText = [](const QString& text, QWidget* parent) {
            auto* label = new ElaText(text, parent);
            label->setObjectName(QStringLiteral("GroupSettingsSecondary"));
            label->setTextPixelSize(13);
            label->setWordWrap(true);
            return label;
        };
        auto makeCard = [](QWidget* parent, const QString& objectName) {
            auto* card = new ElaFrame(parent);
            card->setObjectName(objectName);
            card->setFrameShape(QFrame::NoFrame);
            card->setAttribute(Qt::WA_StyledBackground, true);
            return card;
        };

        auto [basicScroll, basicPage] = makeScrollPage(QStringLiteral("groupSettingsBasicPage"));
        auto* basicLayout = new QVBoxLayout(basicPage);
        basicLayout->setContentsMargins(0, 0, 12, 0);
        basicLayout->setSpacing(14);
        basicLayout->addWidget(makeSectionTitle(QStringLiteral("\u57FA\u7840\u8BBE\u7F6E"), basicPage));
        basicLayout->addWidget(makeBodyText(QStringLiteral("\u7BA1\u7406\u7FA4\u540D\u79F0\u3001\u6D88\u606F\u514D\u6253\u6270\u548C\u7FA4\u804A\u72B6\u6001\u3002"), basicPage));

        auto* profileCard = makeCard(basicPage, QStringLiteral("groupSettingsProfileCard"));
        auto* profileLayout = new QVBoxLayout(profileCard);
        profileLayout->setContentsMargins(18, 16, 18, 16);
        profileLayout->setSpacing(10);
        auto* fieldLabel = new ElaText(QStringLiteral("\u7FA4\u540D\u79F0"), profileCard);
        fieldLabel->setTextPixelSize(13);
        QFont fieldFont = fieldLabel->font();
        fieldFont.setWeight(QFont::DemiBold);
        fieldLabel->setFont(fieldFont);
        auto* nameEdit = new ElaLineEdit(profileCard);
        nameEdit->setText(currentName);
        nameEdit->setFixedHeight(38);
        auto* muteCheck = new ElaCheckBox(QStringLiteral("\u7FA4\u6D88\u606F\u514D\u6253\u6270"), profileCard);
        muteCheck->setChecked(currentMuted);
        auto* basicStatusLabel = new ElaText(profileCard);
        basicStatusLabel->setObjectName(QStringLiteral("GroupSettingsStatus"));
        basicStatusLabel->setTextPixelSize(12);
        basicStatusLabel->hide();
        auto* basicButtonRow = new QHBoxLayout;
        auto* destructiveBtn = new ElaPushButton(isOwner ? QStringLiteral("\u89E3\u6563\u7FA4\u804A")
                                                         : QStringLiteral("\u9000\u51FA\u7FA4\u804A"), profileCard);
        auto* saveBtn = new ElaPushButton(QStringLiteral("\u4FDD\u5B58\u57FA\u7840\u8BBE\u7F6E"), profileCard);
        for (auto* btn : {destructiveBtn, saveBtn}) {
            btn->setFixedHeight(38);
            btn->setMinimumWidth(128);
        }
        destructiveBtn->setObjectName(QStringLiteral("GroupDangerButton"));
        basicButtonRow->addWidget(destructiveBtn);
        basicButtonRow->addStretch();
        basicButtonRow->addWidget(saveBtn);
        profileLayout->addWidget(fieldLabel);
        profileLayout->addWidget(nameEdit);
        profileLayout->addWidget(muteCheck);
        profileLayout->addWidget(basicStatusLabel);
        profileLayout->addSpacing(4);
        profileLayout->addLayout(basicButtonRow);
        basicLayout->addWidget(profileCard);
        basicLayout->addStretch();

        stack->addWidget(basicScroll);

        rightLayout->addWidget(stack, 1);
        root->addWidget(sidebar);
        root->addWidget(rightPane, 1);

        auto selectPage = [=](int index) {
            if (index < 0 || index >= stack->count()) {
                return;
            }
            stack->setCurrentIndex(index);
            const QModelIndex modelIndex = navModel->index(index, 0);
            if (modelIndex.isValid() && navList->currentIndex() != modelIndex) {
                navList->setCurrentIndex(modelIndex);
            }
        };
        QObject::connect(navList->selectionModel(), &QItemSelectionModel::currentChanged, dlg,
                         [=](const QModelIndex& current, const QModelIndex&) {
            selectPage(current.row());
        });
        selectPage(0);
        applyGroupSettingsTheme();

        QObject::connect(saveBtn, &QAbstractButton::clicked, dlg, [&, nameEdit, muteCheck, groupId, groupOpt, basicStatusLabel, navHint]() mutable {
            conversationRepository.setConversationFlag(groupId,
                                                       ConversationFlag::Muted,
                                                       muteCheck && muteCheck->isChecked());
            const QString newName = nameEdit->text().trimmed();
            if (!newName.isEmpty() && groupOpt.has_value()) {
                Group updated = *groupOpt;
                updated.groupName = newName.toStdWString();
                updated.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
                groupRepository.upsertGroup(updated);
                QString previewText;
                qint64 lastMessageAtMs = updated.updatedAtMs;
                for (const auto& summary : conversationRepository.loadConversationSummaries()) {
                    if (QString::fromStdWString(summary.conversationId) != groupId) {
                        continue;
                    }
                    previewText = QString::fromStdWString(summary.lastMessagePreview);
                    if (summary.lastMessageAtMs > 0) {
                        lastMessageAtMs = summary.lastMessageAtMs;
                    }
                    break;
                }
                conversationRepository.upsertConversationWithType(
                    ConversationSummary{
                        updated.groupId,
                        updated.groupName,
                        previewText.toStdWString(),
                        lastMessageAtMs
                    },
                    QStringLiteral("group"));
                const QStringList activeMembers = activeGroupMemberIds(groupId);
                broadcastGroupMeta(updated,
                                   activeMembers,
                                   activeMembers,
                                   QStringLiteral("rename"));
                scheduleChatUiRefresh(true, true, true, true, 0);
                m_mainWindow->setStatusMessage(
                    QStringLiteral("\u7fa4\u540d\u79f0\u5df2\u66f4\u65b0\u4e3a\u300c%1\u300d")
                        .arg(newName),
                    2000);
                navHint->setText(newName);
            }
            basicStatusLabel->setText(QStringLiteral("\u5DF2\u4FDD\u5B58"));
            basicStatusLabel->show();
        });
        QObject::connect(destructiveBtn, &QAbstractButton::clicked, dlg, [&, dlg, groupId, isOwner]() {
            if (isOwner) {
                if (!LeyoDialog::question(m_mainWindow.get(),
                                          QStringLiteral("\u89E3\u6563\u7FA4\u804A"),
                                          QStringLiteral("\u786E\u8BA4\u89E3\u6563\u8BE5\u7FA4\u804A\u5417\uFF1F"))) {
                    return;
                }

                GroupEvent disbandEvent;
                if (!groupService.disbandGroup(localClientId, groupId, &disbandEvent)) {
                    if (tryRemoveLegacyGroupLocally(groupId, QStringLiteral("移除"))) {
                        dlg->accept();
                        return;
                    }
                    m_mainWindow->setStatusMessage(QStringLiteral("\u89E3\u6563\u7FA4\u804A\u5931\u8D25"), 3000);
                    return;
                }

                if (const auto updatedGroupOpt = groupRepository.findGroupById(groupId.toStdWString());
                    updatedGroupOpt.has_value()) {
                    const QStringList activeMembers = activeGroupMemberIds(groupId);
                    broadcastGroupMeta(*updatedGroupOpt,
                                       activeMembers,
                                       activeMembers,
                                       QStringLiteral("disband"));
                }
                removeGroupConversationLocally(groupId);
                scheduleChatUiRefresh(true, true, true, true, 0);
                m_mainWindow->setStatusMessage(QStringLiteral("\u7FA4\u804A\u5DF2\u89E3\u6563"), 3000);
                dlg->accept();
                return;
            }

                if (!LeyoDialog::question(m_mainWindow.get(),
                                          QStringLiteral("\u9000\u51FA\u7FA4\u804A"),
                                          QStringLiteral("\u786E\u8BA4\u9000\u51FA\u8BE5\u7FA4\u804A\u5417\uFF1F"))) {
                    return;
                }

                GroupEvent removeEvent;
                if (!groupService.leaveGroup(localClientId, groupId, &removeEvent)) {
                    if (tryRemoveLegacyGroupLocally(groupId, QStringLiteral("退出"))) {
                        dlg->accept();
                        return;
                    }
                    m_mainWindow->setStatusMessage(QStringLiteral("\u9000\u51FA\u7FA4\u804A\u5931\u8D25"), 3000);
                    return;
                }

                if (const auto updatedGroupOpt = groupRepository.findGroupById(groupId.toStdWString());
                    updatedGroupOpt.has_value()) {
                    const QStringList activeMembers = activeGroupMemberIds(groupId);
                    broadcastGroupMeta(*updatedGroupOpt,
                                       activeMembers,
                                       activeMembers,
                                       QStringLiteral("remove_member"),
                                       localClientId);
                }

                removeGroupConversationLocally(groupId);
                scheduleChatUiRefresh(true, true, true, true, 0);
                m_mainWindow->setStatusMessage(QStringLiteral("\u5DF2\u9000\u51FA\u7FA4\u804A"), 3000);
                dlg->accept();
        });

        dlg->moveToCenter();
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    });

    // 单聊历史记录
    QObject::connect(m_mainWindow.get(), &MainWindow::chatHistoryRequested, m_mainWindow.get(),
                     [&](const QString& /*unused*/) {
        if (currentConversationId.isEmpty()) {
            return;
        }
        auto messagesPtr = std::make_shared<std::vector<ChatMessage>>(
            ChatService::loadMessages(&conversationRepository, currentConversationId));
        const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(currentTargetId));
        const QString peerName = peer.has_value() ? displayNameForPeer(*peer) : currentTargetId;

        auto* dlg = new QDialog(m_mainWindow.get());
        dlg->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dlg->setAttribute(Qt::WA_TranslucentBackground);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowModality(Qt::WindowModal);
        dlg->resize(580, 500);

        auto* outer = new QVBoxLayout(dlg);
        outer->setContentsMargins(0, 0, 0, 0);
        auto* card = new QWidget(dlg);
        card->setObjectName(QStringLiteral("card"));
        card->setStyleSheet(QStringLiteral(
            "#card { background:%1; border-radius:16px; border:1px solid %2; }")
            .arg(AppStyle::surface(), AppStyle::border()));
        outer->addWidget(card);

        auto* vl = new QVBoxLayout(card);
        vl->setContentsMargins(28, 24, 28, 20);
        vl->setSpacing(10);

        auto* titleLbl = new QLabel(
            QStringLiteral("\u804a\u5929\u8bb0\u5f55 \u00B7 ") + peerName, card);
        titleLbl->setStyleSheet(QStringLiteral("font-size:18px;font-weight:bold;color:%1;").arg(AppStyle::textPrimary()));
        vl->addWidget(titleLbl);

        auto* searchEdit = new QLineEdit(card);
        searchEdit->setPlaceholderText(QStringLiteral("\u641c\u7d22\u6d88\u606f\u5185\u5bb9..."));
        searchEdit->setFixedHeight(36);
        searchEdit->setStyleSheet(QStringLiteral(
            "QLineEdit { background:%1; border:1.5px solid %2; border-radius:8px;"
            " padding:0 12px; font-size:13px; color:%3; }"
            "QLineEdit:focus { border-color:%4; background:%5; }")
            .arg(AppStyle::surfaceAlt(), AppStyle::border(),
                 AppStyle::textPrimary(), AppStyle::accent(), AppStyle::surface()));
        vl->addWidget(searchEdit);

        auto* listWidget = new ElaListWidget(card);
        listWidget->setStyleSheet(QStringLiteral(
            "QListWidget { border:none; outline:none; background:transparent; }"
            "QListWidget::item { padding:0; border-bottom:1px solid %1; }"
            "QListWidget::item:hover { background:%2; }"
            "QListWidget::item:selected { background:%3; }")
            .arg(AppStyle::border(), AppStyle::hoverBg(), AppStyle::selectedBg()));
        listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

        static const char* kHistAvatarColorsDirect[] = {
            "#5273E8", "#2FA484", "#D9963A", "#7B68E6", "#D85A9A", "#3296C4"
        };

        const auto populateDirect = [messagesPtr, listWidget, localClientId, peerName,
                                     convId = currentConversationId](const QString& filter) {
            listWidget->clear();
            for (const auto& msg : *messagesPtr) {
                QTextDocument doc;
                doc.setHtml(QString::fromStdWString(msg.body));
                const QString plain = doc.toPlainText().trimmed();
                if (!filter.isEmpty() && !plain.contains(filter, Qt::CaseInsensitive)) {
                    continue;
                }
                const QString senderId = QString::fromStdWString(msg.senderId);
                const QString senderName = (senderId == localClientId)
                    ? QStringLiteral("\u6211") : peerName;
                const QDateTime ts = QDateTime::fromMSecsSinceEpoch(msg.createdAtMs);
                const QString msgId = QString::fromStdWString(msg.messageId);
                const QString attachName = QString::fromStdWString(msg.attachmentName).trimmed();
                const QString localPath = QString::fromStdWString(msg.localFilePath).trimmed();
                const bool isFile = !attachName.isEmpty() || !localPath.isEmpty();

                QString preview;
                if (isFile) {
                    preview = isChatPreviewImageAttachmentName(attachName)
                                  ? QStringLiteral("\U0001F5BC [图片]")
                                  : QStringLiteral("\U0001F4CE [文件] %1").arg(attachName);
                } else {
                    preview = plain;
                }
                if (preview.length() > 80) preview = preview.left(80) + QStringLiteral("...");

                int h = 0;
                for (const QChar ch : senderName)
                    h = (h * 31 + ch.unicode()) & 0x7FFFFFFF;
                const QColor avatarBg(kHistAvatarColorsDirect[h % 6]);
                const QString initial = senderName.isEmpty() ? QStringLiteral("?")
                                                             : QString(senderName.at(0));

                auto* row = new QWidget;
                row->setStyleSheet(QStringLiteral("background:transparent;"));
                auto* rowLayout = new QHBoxLayout(row);
                rowLayout->setContentsMargins(4, 8, 4, 8);
                rowLayout->setSpacing(10);

                auto* avatarLabel = new QLabel(row);
                avatarLabel->setFixedSize(36, 36);
                avatarLabel->setAlignment(Qt::AlignCenter);
                avatarLabel->setStyleSheet(QStringLiteral(
                    "QLabel { background:%1; color:white; border-radius:18px;"
                    " font-size:14px; font-weight:bold; }").arg(avatarBg.name()));
                avatarLabel->setText(initial);
                rowLayout->addWidget(avatarLabel);

                auto* infoWidget = new QWidget(row);
                infoWidget->setStyleSheet(QStringLiteral("background:transparent;"));
                auto* infoLayout = new QVBoxLayout(infoWidget);
                infoLayout->setContentsMargins(0, 0, 0, 0);
                infoLayout->setSpacing(2);

                auto* topRow = new QWidget(infoWidget);
                topRow->setStyleSheet(QStringLiteral("background:transparent;"));
                auto* topLayout = new QHBoxLayout(topRow);
                topLayout->setContentsMargins(0, 0, 0, 0);
                auto* nameLabel = new QLabel(senderName, topRow);
                nameLabel->setStyleSheet(QStringLiteral(
                    "QLabel { font-size:13px; font-weight:500; color:#1F2329; background:transparent; }"));
                auto* timeLabel = new QLabel(ts.toString(QStringLiteral("MM-dd HH:mm")), topRow);
                timeLabel->setStyleSheet(QStringLiteral(
                    "QLabel { font-size:11px; color:#8F959E; background:transparent; }"));
                timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                topLayout->addWidget(nameLabel);
                topLayout->addStretch();
                topLayout->addWidget(timeLabel);
                infoLayout->addWidget(topRow);

                auto* bodyLabel = new QLabel(preview, infoWidget);
                bodyLabel->setStyleSheet(QStringLiteral(
                    "QLabel { font-size:12px; color:#646A73; background:transparent; }"));
                bodyLabel->setWordWrap(false);
                infoLayout->addWidget(bodyLabel);

                rowLayout->addWidget(infoWidget, 1);

                auto* item = new QListWidgetItem(listWidget);
                item->setSizeHint(QSize(0, 56));
                item->setData(Qt::UserRole, msgId);
                item->setData(Qt::UserRole + 1, convId);
                listWidget->setItemWidget(item, row);
            }
        };
        populateDirect(QString());

        // 双击跳转到对应消息
        QObject::connect(listWidget, &QListWidget::itemDoubleClicked, dlg,
                [dlg, mainWin = m_mainWindow.get()](QListWidgetItem* item) {
                    const QString msgId = item->data(Qt::UserRole).toString();
                    const QString cId = item->data(Qt::UserRole + 1).toString();
                    if (!msgId.isEmpty()) {
                        dlg->close();
                        emit mainWin->searchResultJumpRequested(cId, msgId);
                    }
                });

        QObject::connect(searchEdit, &QLineEdit::textChanged, dlg,
                [populateDirect](const QString& text) { populateDirect(text); });
        vl->addWidget(listWidget, 1);

        auto* closeBtn = new QPushButton(QStringLiteral("\u5173\u95ed"), card);
        closeBtn->setFixedHeight(36);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background:#2B5CE6; color:#fff; border:none; border-radius:8px;"
            " font-size:14px; font-weight:bold; }"
            "QPushButton:hover { background:#4070F4; }"
            "QPushButton:pressed { background:#1A44C8; }"));
        QObject::connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
        vl->addWidget(closeBtn);

        // 非模态显示，避免阻塞事件循环导致截图时假死
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    });

    // 群聊导航按钮：切换到群组过滤视图
    QObject::connect(m_mainWindow.get(), &MainWindow::groupNavSelected, m_mainWindow.get(), [&]() {
        // filter index 6 对应 "群组" 筛选条目
        emit m_mainWindow->conversationFilterChanged(6);
    });

    // 头像按钮：弹出个人资料浮层
    QObject::connect(m_mainWindow.get(), &MainWindow::avatarClicked, m_mainWindow.get(), [&]() {
        auto* panel = new QDialog(m_mainWindow.get(), Qt::Dialog);
        panel->setAttribute(Qt::WA_DeleteOnClose);
        panel->setModal(false);
        panel->setAttribute(Qt::WA_InputMethodEnabled);
        panel->setWindowTitle(QStringLiteral("个人设置"));
        panel->setWindowFlag(Qt::WindowContextHelpButtonHint, false);
        panel->setMinimumWidth(360);
        panel->setStyleSheet(QStringLiteral(
            "QDialog {"
            "  background: %1;"
            "}"
            "QScrollArea {"
            "  background: %1;"
            "  border: none;"
            "}"
            "QWidget#profilePanel {"
            "  background: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 10px;"
            "}"
            "QLabel#profileName { font-size: 15px; font-weight: bold; color: %3; }"
            "QLabel#profileId   { font-size: 12px; color: %4; }"
            "QLabel#profileFieldLabel { font-size: 12px; color: %5; }"
            "QLineEdit { background:%6; border:1.5px solid %2; border-radius:6px;"
            "  padding:4px 10px; font-size:13px; color:%3; }"
            "QComboBox { background:%6; border:1.5px solid %2; border-radius:6px;"
            "  padding:4px 10px; font-size:13px; color:%3; }"
            "QPushButton#saveBtn { background:%7; color:#fff; border:none; border-radius:6px;"
            "  font-size:13px; padding:6px 16px; }"
            "QPushButton#saveBtn:hover { background:%8; }")
            .arg(AppStyle::surface(),
                 AppStyle::border(),
                 AppStyle::textPrimary(),
                 AppStyle::textMuted(),
                 AppStyle::textSecondary(),
                 AppStyle::surfaceAlt(),
                 AppStyle::accent(),
                 AppStyle::accentHover()));

        auto* rootLayout = new QVBoxLayout(panel);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        // ── Tab Widget ──────────────────────────────────
        auto* tabWidget = new QTabWidget(panel);
        tabWidget->setDocumentMode(true);
        tabWidget->setStyleSheet(QStringLiteral(
            "QTabWidget::pane { border:none; background:%1; }"
            "QTabBar::tab {"
            "  background:%2; color:%3; border:none;"
            "  padding:8px 18px; font-size:13px; font-weight:600;"
            "  border-bottom:2px solid transparent;"
            "}"
            "QTabBar::tab:selected {"
            "  color:%4; border-bottom:2px solid %4;"
            "}"
            "QTabBar::tab:hover {"
            "  background:%5; color:%6;"
            "}")
            .arg(AppStyle::surface(),
                 AppStyle::surface(),
                 AppStyle::textMuted(),
                 AppStyle::accent(),
                 AppStyle::hoverBg(),
                 AppStyle::textPrimary()));
        rootLayout->addWidget(tabWidget, 1);

        // ── Helper: create a scrollable tab page ──
        const auto makeTabPage = [&](const QString& title) -> std::pair<QWidget*, QVBoxLayout*> {
            auto* scroll = new QScrollArea(tabWidget);
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            auto* body = new QWidget(scroll);
            body->setObjectName(QStringLiteral("profilePanel"));
            scroll->setWidget(body);
            auto* lay = new QVBoxLayout(body);
            lay->setContentsMargins(16, 16, 16, 16);
            lay->setSpacing(10);
            tabWidget->addTab(scroll, title);
            return {body, lay};
        };

        auto [tab1Body, tab1Layout] = makeTabPage(QStringLiteral("\u57FA\u672C\u4FE1\u606F"));
        auto [tab2Body, tab2Layout] = makeTabPage(QStringLiteral("\u96C6\u6210\u670D\u52A1"));
        auto [tabKnowledgeBody, tabKnowledgeLayout] = makeTabPage(QStringLiteral("\u77E5\u8BC6\u670D\u52A1"));
        auto [tab3Body, tab3Layout] = makeTabPage(QStringLiteral("\u9AD8\u7EA7\u8BBE\u7F6E"));
        auto [tab4Body, tab4Layout] = makeTabPage(QStringLiteral("\u5173\u4E8E"));

        auto* footerWidget = new QWidget(panel);
        footerWidget->setObjectName(QStringLiteral("profileFooter"));
        footerWidget->setStyleSheet(QStringLiteral(
            "QWidget#profileFooter {"
            "  background:%1;"
            "  border-top:1px solid %2;"
            "}").arg(AppStyle::surface(), AppStyle::border()));
        rootLayout->addWidget(footerWidget);

        // ── Tab 1: 基本信息 ──────────────────────────
        auto* panelBody = tab1Body;
        auto* panelLayout = tab1Layout;

        // 头像圆形 + 姓名
        auto* nameLabel = new QLabel(localDisplayName, panelBody);
        nameLabel->setObjectName(QStringLiteral("profileName"));
        auto* idLabel   = new QLabel(localClientId, panelBody);
        idLabel->setObjectName(QStringLiteral("profileId"));

        panelLayout->addWidget(nameLabel);
        panelLayout->addWidget(idLabel);

        auto* nameFieldLabel = new QLabel(QStringLiteral("姓名"), panelBody);
        nameFieldLabel->setObjectName(QStringLiteral("profileFieldLabel"));
        auto* nameEdit = new QLineEdit(localDisplayName, panelBody);
        nameEdit->setPlaceholderText(QStringLiteral("请输入显示姓名"));
        nameEdit->setMaxLength(48);
        panelLayout->addWidget(nameFieldLabel);
        panelLayout->addWidget(nameEdit);

        auto* sep = new ElaFrame(panelBody);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(QStringLiteral("color: #E0E2E7;"));
        panelLayout->addWidget(sep);


        auto* sigLbl = new QLabel(QStringLiteral("\u4E2A\u6027\u7B7E\u540D"), panelBody); // 涓€х鍚?
        sigLbl->setStyleSheet(QStringLiteral("font-size:12px; color:#8F959E;"));
        auto* sigEdit = new QLineEdit(panelBody);
        sigEdit->setPlaceholderText(QStringLiteral("\u8FD9\u4E2A\u4EBA\u5F88\u61D2\uFF0C\u4EC0\u4E48\u90FD\u6CA1\u7559\u4E0B")); // 杩欎釜浜哄緢鎳掆€?
        sigEdit->setText(QString::fromStdWString(profile->signature));
        panelLayout->addWidget(sigLbl);
        panelLayout->addWidget(sigEdit);

        auto* departmentLabel = new QLabel(QStringLiteral("部门"), panelBody);
        departmentLabel->setObjectName(QStringLiteral("profileFieldLabel"));
        auto* departmentEdit = new QLineEdit(panelBody);
        departmentEdit->setPlaceholderText(QStringLiteral("例如：工业软件中心"));
        departmentEdit->setText(QString::fromStdWString(profile->department));
        panelLayout->addWidget(departmentLabel);
        panelLayout->addWidget(departmentEdit);

        auto* jobTitleLabel = new QLabel(QStringLiteral("职位"), panelBody);
        jobTitleLabel->setObjectName(QStringLiteral("profileFieldLabel"));
        auto* jobTitleEdit = new QLineEdit(panelBody);
        jobTitleEdit->setPlaceholderText(QStringLiteral("例如：产品经理 / 开发工程师"));
        jobTitleEdit->setText(QString::fromStdWString(profile->jobTitle));
        panelLayout->addWidget(jobTitleLabel);
        panelLayout->addWidget(jobTitleEdit);

        auto* phoneLabel = new QLabel(QStringLiteral("联系电话"), panelBody);
        phoneLabel->setObjectName(QStringLiteral("profileFieldLabel"));
        auto* phoneEdit = new QLineEdit(panelBody);
        phoneEdit->setPlaceholderText(QStringLiteral("例如：13800000000"));
        phoneEdit->setText(QString::fromStdWString(profile->phoneNumber));
        panelLayout->addWidget(phoneLabel);
        panelLayout->addWidget(phoneEdit);

        auto* genderLabel = new QLabel(QStringLiteral("性别"), panelBody);
        genderLabel->setObjectName(QStringLiteral("profileFieldLabel"));
        auto* genderCombo = new QComboBox(panelBody);
        genderCombo->addItems({QStringLiteral("保密"), QStringLiteral("男"), QStringLiteral("女")});
        const QString genderValue = QString::fromStdWString(profile->gender).trimmed();
        genderCombo->setCurrentText(genderValue.isEmpty() ? QStringLiteral("保密") : genderValue);
        genderCombo->setStyleSheet(QStringLiteral(
            "QComboBox {"
            "  border:1px solid #D9DEE7;"
            "  border-radius:6px;"
            "  padding:4px 10px;"
            "  min-height:30px;"
            "  color:#1F2329;"
            "  background:#FFFFFF;"
            "}"
            "QComboBox::drop-down {"
            "  subcontrol-origin:padding;"
            "  subcontrol-position:center right;"
            "  border:none;"
            "  width:28px;"
            "}"
            "QComboBox::down-arrow {"
            "  image:none;"
            "  border-left:5px solid transparent;"
            "  border-right:5px solid transparent;"
            "  border-top:6px solid #6B7280;"
            "  width:0; height:0;"
            "  margin-right:6px;"
            "}"
            "QComboBox::down-arrow:hover {"
            "  border-top-color:#2F6FED;"
            "}"
            "QComboBox QAbstractItemView {"
            "  background:#FFFFFF;"
            "  color:#1F2329;"
            "  selection-background-color:#E8F0FF;"
            "  selection-color:#1D4ED8;"
            "  border:1px solid #D9DEE7;"
            "}"));
        panelLayout->addWidget(genderLabel);
        panelLayout->addWidget(genderCombo);

        auto* emailLabel = new QLabel(QStringLiteral("邮箱"), panelBody);
        emailLabel->setObjectName(QStringLiteral("profileFieldLabel"));
        auto* emailEdit = new QLineEdit(panelBody);
        emailEdit->setPlaceholderText(QStringLiteral("例如：name@example.com"));
        emailEdit->setText(QString::fromStdWString(profile->email));
        panelLayout->addWidget(emailLabel);
        panelLayout->addWidget(emailEdit);

        // ── 上传头像按钮（放在 Tab 1 底部）──
        auto* uploadAvatarBtn = new QPushButton(QStringLiteral("上传头像"), tab1Body);
        uploadAvatarBtn->setCursor(Qt::PointingHandCursor);
        uploadAvatarBtn->setStyleSheet(QStringLiteral(
            "QPushButton { color:%1; border:none; background:transparent; font-size:13px; }"
            "QPushButton:hover { text-decoration:underline; }")
            .arg(AppStyle::accent()));
        panelLayout->addSpacing(6);
        panelLayout->addWidget(uploadAvatarBtn);
        panelLayout->addStretch();

        // ── Tab: 知识服务 ──────────────────────────
        const QVector<KnowledgeServiceConfig> knowledgeServiceConfigs = KnowledgeServiceSettingsStore::load();
        auto* knowledgeServiceWidget = new KnowledgeServiceSettingsWidget(tabKnowledgeBody);
        knowledgeServiceWidget->setConfigs(knowledgeServiceConfigs);
        tabKnowledgeLayout->addWidget(knowledgeServiceWidget);
        tabKnowledgeLayout->addStretch();

        // ── Tab 3: 高级设置 ──────────────────────────
        auto* tab3SectionLabel = new QLabel(
            QStringLiteral("<span style='font-size:14px;font-weight:700;color:%1;'>\u5916\u89C2</span>")
                .arg(AppStyle::textPrimary()), tab3Body);
        tab3Layout->addWidget(tab3SectionLabel);

        auto* themeLabel = new QLabel(QStringLiteral("主题"), tab3Body);
        themeLabel->setObjectName(QStringLiteral("profileFieldLabel"));
        auto* themeCombo = new QComboBox(tab3Body);
        themeCombo->setStyleSheet(genderCombo->styleSheet());
        themeCombo->addItem(QStringLiteral("跟随系统"), AppStyle::themeModeToString(AppStyle::ThemeMode::FollowSystem));
        themeCombo->addItem(QStringLiteral("浅色"), AppStyle::themeModeToString(AppStyle::ThemeMode::Light));
        themeCombo->addItem(QStringLiteral("深色"), AppStyle::themeModeToString(AppStyle::ThemeMode::Dark));
        {
            QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
            const QString currentTheme = cfg.value(QStringLiteral("appearance/themeMode"),
                                                   QStringLiteral("system")).toString();
            const int themeIndex = qMax(0, themeCombo->findData(currentTheme));
            themeCombo->setCurrentIndex(themeIndex);
        }
        tab3Layout->addWidget(themeLabel);
        tab3Layout->addWidget(themeCombo);

        // ── 通知 ──
        auto* tab3NotifSectionLabel = new QLabel(
            QStringLiteral("<span style='font-size:14px;font-weight:700;color:%1;'>\u901A\u77E5</span>")
                .arg(AppStyle::textPrimary()), tab3Body);
        tab3Layout->addSpacing(8);
        tab3Layout->addWidget(tab3NotifSectionLabel);

        auto* trayPopupCheckBox = new QCheckBox(QStringLiteral("\u542F\u7528\u7CFB\u7EDF\u6258\u76D8\u5F39\u7A97\u901A\u77E5"), tab3Body);
        {
            QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
            trayPopupCheckBox->setChecked(
                cfg.value(QStringLiteral("notification/trayPopupEnabled"), false).toBool());
        }
        auto* trayPopupHint = new QLabel(
            QStringLiteral("\u5F00\u542F\u540E\uFF0C\u6536\u5230\u65B0\u6D88\u606F\u65F6\u7CFB\u7EDF\u6258\u76D8\u4F1A\u5F39\u51FA\u63D0\u793A\uFF1B\u5173\u95ED\u540E\u4EC5\u4FDD\u7559\u4EFB\u52A1\u680F\u95EA\u70C1\u548C\u63D0\u793A\u97F3\u3002"),
            tab3Body);
        trayPopupHint->setWordWrap(true);
        trayPopupHint->setStyleSheet(
            QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));
        tab3Layout->addWidget(trayPopupCheckBox);
        tab3Layout->addWidget(trayPopupHint);

        // ── 截图 ──
        auto* tab3ScreenshotSectionLabel = new QLabel(
            QStringLiteral("<span style='font-size:14px;font-weight:700;color:%1;'>\u622A\u56FE</span>")
                .arg(AppStyle::textPrimary()), tab3Body);
        tab3Layout->addSpacing(8);
        tab3Layout->addWidget(tab3ScreenshotSectionLabel);

        auto* hotkeyLabel = new QLabel(QStringLiteral("截图快捷键"), tab3Body);
        hotkeyLabel->setObjectName(QStringLiteral("profileFieldLabel"));
        auto* hotkeyEdit = new QKeySequenceEdit(tab3Body);
        {
            QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
            const QString savedHotkey = cfg.value(QStringLiteral("screenshotHotkey"),
                                                  QStringLiteral("Ctrl+Alt+A")).toString();
            hotkeyEdit->setKeySequence(QKeySequence(savedHotkey));
        }
        hotkeyEdit->setToolTip(QStringLiteral("点击此处后按下想要的快捷键组合"));

        auto* hotkeyHint = new QLabel(
            QStringLiteral("在任意界面按此快捷键即可截图，截图结果会进入聊天输入框并复制到剪贴板。"),
            tab3Body);
        hotkeyHint->setWordWrap(true);
        hotkeyHint->setStyleSheet(
            QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));

        auto* hotkeyTestRow = new QHBoxLayout();
        auto* hotkeyTestBtn = new QPushButton(QStringLiteral("检测冲突"), tab3Body);
        auto* hotkeyTestResult = new QLabel(tab3Body);
        hotkeyTestResult->setStyleSheet(
            QStringLiteral("font-size:12px; padding-left:8px;"));
        hotkeyTestRow->addWidget(hotkeyTestBtn);
        hotkeyTestRow->addWidget(hotkeyTestResult);
        hotkeyTestRow->addStretch();

        QObject::connect(hotkeyTestBtn, &QPushButton::clicked, tab3Body,
                         [hotkeyEdit, hotkeyTestResult]() {
            const QKeySequence seq = hotkeyEdit->keySequence();
            if (seq.isEmpty()) {
                hotkeyTestResult->setText(QStringLiteral("\u26A0\uFE0F 请先设置快捷键"));
                hotkeyTestResult->setStyleSheet(
                    QStringLiteral("font-size:12px; padding-left:8px; color:orange;"));
                return;
            }
            if (GlobalHotkeyManager::testHotkeyAvailable(seq)) {
                hotkeyTestResult->setText(
                    QStringLiteral("\u2705 快捷键 %1 可用").arg(seq.toString(QKeySequence::NativeText)));
                hotkeyTestResult->setStyleSheet(
                    QStringLiteral("font-size:12px; padding-left:8px; color:green;"));
            } else {
                hotkeyTestResult->setText(
                    QStringLiteral("\u26A0\uFE0F 快捷键 %1 已被其他程序占用，请换一个")
                        .arg(seq.toString(QKeySequence::NativeText)));
                hotkeyTestResult->setStyleSheet(
                    QStringLiteral("font-size:12px; padding-left:8px; color:red;"));
            }
        });

        tab3Layout->addWidget(hotkeyLabel);
        tab3Layout->addWidget(hotkeyEdit);
        tab3Layout->addWidget(hotkeyHint);
        tab3Layout->addLayout(hotkeyTestRow);

        // ── 软件更新 ──
        auto* tab3UpdateSectionLabel = new QLabel(
            QStringLiteral("<span style='font-size:14px;font-weight:700;color:%1;'>\u8F6F\u4EF6\u66F4\u65B0</span>")
                .arg(AppStyle::textPrimary()), tab3Body);
        tab3Layout->addSpacing(8);
        tab3Layout->addWidget(tab3UpdateSectionLabel);

        auto* updateServerLabel = new QLabel(QStringLiteral("\u5347\u7EA7\u670D\u52A1\u5668\u5730\u5740"), tab3Body);
        updateServerLabel->setStyleSheet(
            QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textSecondary()));
        auto* updateServerEdit = new QLineEdit(tab3Body);
        updateServerEdit->setPlaceholderText(QStringLiteral("例如：\\\\server\\leyochat\\updates"));
        {
            QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
            updateServerEdit->setText(
                cfg.value(QStringLiteral("update/serverPath"), QString()).toString());
        }
        auto* updateServerHint = new QLabel(
            QStringLiteral("\u7BA1\u7406\u5458\u5C06\u65B0\u7248\u5B89\u88C5\u5305\u653E\u5728\u6B64\u5171\u4EAB\u76EE\u5F55\uFF0C\u5BA2\u6237\u7AEF\u4F1A\u81EA\u52A8\u68C0\u67E5\u3002"),
            tab3Body);
        updateServerHint->setWordWrap(true);
        updateServerHint->setStyleSheet(
            QStringLiteral("font-size:11px; color:%1;").arg(AppStyle::textMuted()));

        tab3Layout->addWidget(updateServerLabel);
        tab3Layout->addWidget(updateServerEdit);
        tab3Layout->addWidget(updateServerHint);

        auto* autoCheckBox = new QCheckBox(QStringLiteral("\u542F\u7528\u81EA\u52A8\u66F4\u65B0\u68C0\u6D4B"), tab3Body);
        auto* checkIntervalLabel = new QLabel(QStringLiteral("\u68C0\u6D4B\u95F4\u9694\uFF08\u5206\u949F\uFF09"), tab3Body);
        checkIntervalLabel->setStyleSheet(
            QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textSecondary()));
        auto* checkIntervalSpin = new QSpinBox(tab3Body);
        checkIntervalSpin->setRange(10, 1440);
        checkIntervalSpin->setSuffix(QStringLiteral(" \u5206\u949F"));
        {
            QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
            const bool autoEnabled = cfg.value(QStringLiteral("update/autoCheckEnabled"), true).toBool();
            const int intervalMin = cfg.value(QStringLiteral("update/checkIntervalMinutes"), 60).toInt();
            autoCheckBox->setChecked(autoEnabled);
            checkIntervalSpin->setValue(intervalMin);
            checkIntervalLabel->setEnabled(autoEnabled);
            checkIntervalSpin->setEnabled(autoEnabled);
        }
        QObject::connect(autoCheckBox, &QCheckBox::toggled, tab3Body, [checkIntervalLabel, checkIntervalSpin](bool checked) {
            checkIntervalLabel->setEnabled(checked);
            checkIntervalSpin->setEnabled(checked);
        });

        tab3Layout->addWidget(autoCheckBox);
        auto* intervalRow = new QHBoxLayout();
        intervalRow->addWidget(checkIntervalLabel);
        intervalRow->addWidget(checkIntervalSpin);
        intervalRow->addStretch();
        tab3Layout->addLayout(intervalRow);

        // ── 数据管理 ──
        auto* tab3DataSectionLabel = new QLabel(
            QStringLiteral("<span style='font-size:14px;font-weight:700;color:%1;'>\u6570\u636E\u7BA1\u7406</span>")
                .arg(AppStyle::textPrimary()), tab3Body);
        tab3Layout->addSpacing(8);
        tab3Layout->addWidget(tab3DataSectionLabel);

        auto* dataHint = new QLabel(
            QStringLiteral("\u53EF\u5C06\u804A\u5929\u8BB0\u5F55\u5BFC\u51FA\u4E3A\u6570\u636E\u5E93\u6587\u4EF6\u8FDB\u884C\u5907\u4EFD\uFF0C\u6216\u4ECE\u5907\u4EFD\u6587\u4EF6\u6062\u590D\u3002"),
            tab3Body);
        dataHint->setWordWrap(true);
        dataHint->setStyleSheet(
            QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));
        tab3Layout->addWidget(dataHint);

        auto* dataButtonRow = new QHBoxLayout();
        auto* exportBtn = new QPushButton(QStringLiteral("\U0001F4E4 \u5BFC\u51FA\u804A\u5929\u8BB0\u5F55"), tab3Body);
        auto* importBtn = new QPushButton(QStringLiteral("\U0001F4E5 \u5BFC\u5165\u804A\u5929\u8BB0\u5F55"), tab3Body);
        dataButtonRow->addWidget(exportBtn);
        dataButtonRow->addWidget(importBtn);
        dataButtonRow->addStretch();
        tab3Layout->addLayout(dataButtonRow);

        QObject::connect(exportBtn, &QPushButton::clicked, tab3Body, [&]() {
            QMetaObject::invokeMethod(m_mainWindow.get(), "dataExportRequested", Qt::QueuedConnection);
        });
        QObject::connect(importBtn, &QPushButton::clicked, tab3Body, [&]() {
            QMetaObject::invokeMethod(m_mainWindow.get(), "dataImportRequested", Qt::QueuedConnection);
        });

        tab3Layout->addStretch();

        // ── Tab 2: 集成服务 ──────────────────────────
        const AzureDevOpsConnectionSettings azureDevOpsSettings = AzureDevOpsSettingsStore::load();
        const OutlookConnectionSettings outlookSettings = OutlookSettingsStore::load();

        auto* devOpsLabel = new QLabel(QStringLiteral("集成账号 · Azure DevOps"), tab2Body);
        devOpsLabel->setObjectName(QStringLiteral("profileFieldLabel"));
        auto* devOpsEnabled = new QCheckBox(QStringLiteral("启用 Azure DevOps 集成"), tab2Body);
        devOpsEnabled->setChecked(azureDevOpsSettings.enabled);
        auto* devOpsBaseUrlEdit = new QLineEdit(azureDevOpsSettings.baseUrl, tab2Body);
        devOpsBaseUrlEdit->setPlaceholderText(QStringLiteral("例如：https://dev.azure.com"));
        auto* devOpsOrgCombo = new QComboBox(tab2Body);
        devOpsOrgCombo->setEditable(true);
        devOpsOrgCombo->setInsertPolicy(QComboBox::NoInsert);
        devOpsOrgCombo->setCurrentText(azureDevOpsSettings.organization);
        devOpsOrgCombo->setPlaceholderText(QStringLiteral("认证后可发现或手动输入组织"));
        devOpsOrgCombo->setStyleSheet(genderCombo->styleSheet());
        auto* devOpsProjectCombo = new QComboBox(tab2Body);
        devOpsProjectCombo->setEditable(true);
        devOpsProjectCombo->setInsertPolicy(QComboBox::NoInsert);
        devOpsProjectCombo->setCurrentText(azureDevOpsSettings.project);
        devOpsProjectCombo->setPlaceholderText(QStringLiteral("选好组织后可加载或手动输入项目"));
        devOpsProjectCombo->setStyleSheet(genderCombo->styleSheet());
        auto* devOpsTokenEdit = new QLineEdit(azureDevOpsSettings.personalAccessToken, tab2Body);
        devOpsTokenEdit->setPlaceholderText(QStringLiteral("填写 Azure DevOps PAT"));
        devOpsTokenEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
        auto* devOpsBaseUrlLabel = new QLabel(QStringLiteral("服务地址"), tab2Body);
        auto* devOpsTokenLabel = new QLabel(QStringLiteral("PAT Token"), tab2Body);
        auto* devOpsOrganizationLabel = new QLabel(QStringLiteral("组织"), tab2Body);
        auto* devOpsProjectLabel = new QLabel(QStringLiteral("项目"), tab2Body);
        auto* devOpsPollIntervalLabel = new QLabel(QStringLiteral("轮询间隔"), tab2Body);
        auto* devOpsDiscoverOrganizationsBtn =
            new QPushButton(QStringLiteral("发现组织"), tab2Body);
        auto* devOpsLoadProjectsBtn =
            new QPushButton(QStringLiteral("加载项目"), tab2Body);
        auto* devOpsAuthHintLabel = new QLabel(
            QStringLiteral("这里只保留账号认证与默认项目选择；自动提醒会统一进入左侧“通知”页面。"), tab2Body);
        devOpsAuthHintLabel->setWordWrap(true);
        devOpsAuthHintLabel->setStyleSheet(
            QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));
        auto* devOpsNotifyEnabled = new QCheckBox(QStringLiteral("启用 Azure DevOps 自动通知"), tab2Body);
        devOpsNotifyEnabled->setChecked(azureDevOpsSettings.notificationsEnabled);
        auto* devOpsNotifyHintLabel = new QLabel(
            QStringLiteral("通知默认进入“通知中心”，后续 Outlook 等系统也会共用这里。"),
            tab2Body);
        devOpsNotifyHintLabel->setWordWrap(true);
        devOpsNotifyHintLabel->setStyleSheet(
            QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));
        auto* devOpsTestConnectionBtn =
            new QPushButton(QStringLiteral("测试连接"), tab2Body);
        auto* devOpsPollIntervalSpin = new QSpinBox(tab2Body);
        devOpsPollIntervalSpin->setRange(1, 60);
        devOpsPollIntervalSpin->setSuffix(QStringLiteral(" 分钟"));
        devOpsPollIntervalSpin->setValue(qMax(1, azureDevOpsSettings.notificationPollIntervalMinutes));
        tab2Layout->addWidget(devOpsLabel);
        tab2Layout->addWidget(devOpsEnabled);
        tab2Layout->addWidget(devOpsBaseUrlLabel);
        tab2Layout->addWidget(devOpsBaseUrlEdit);
        tab2Layout->addWidget(devOpsTokenLabel);
        tab2Layout->addWidget(devOpsTokenEdit);
        tab2Layout->addWidget(devOpsAuthHintLabel);
        tab2Layout->addWidget(devOpsTestConnectionBtn);
        tab2Layout->addWidget(devOpsDiscoverOrganizationsBtn);
        tab2Layout->addWidget(devOpsOrganizationLabel);
        tab2Layout->addWidget(devOpsOrgCombo);
        tab2Layout->addWidget(devOpsLoadProjectsBtn);
        tab2Layout->addWidget(devOpsProjectLabel);
        tab2Layout->addWidget(devOpsProjectCombo);

        // ── 通知目标列表 ──────────────────────────────
        auto* devOpsTargetsLabel = new QLabel(QStringLiteral("通知目标（可跟踪多个组织/项目）"), tab2Body);
        devOpsTargetsLabel->setStyleSheet(
            QStringLiteral("font-size:12px; color:%1; margin-top:6px;").arg(AppStyle::textSecondary()));
        auto* devOpsTargetsList = new ElaListWidget(tab2Body);
        devOpsTargetsList->setMaximumHeight(120);
        devOpsTargetsList->setSelectionMode(QAbstractItemView::SingleSelection);
        devOpsTargetsList->setStyleSheet(
            QStringLiteral("QListWidget { border:1px solid %1; border-radius:4px; font-size:13px; }"
                           "QListWidget::item { padding:3px 6px; }"
                           "QListWidget::item:selected { background:%2; color:%3; }")
                .arg(AppStyle::border(), AppStyle::accent(), QStringLiteral("#FFFFFF")));

        // 填充已有的通知目标
        const auto populateTargetsList = [devOpsTargetsList](const QVector<AzureDevOpsNotificationTarget>& targets) {
            devOpsTargetsList->clear();
            for (const AzureDevOpsNotificationTarget& target : targets) {
                if (target.organization.trimmed().isEmpty() || target.project.trimmed().isEmpty()) {
                    continue;
                }
                auto* item = new QListWidgetItem(
                    QStringLiteral("%1 / %2").arg(target.organization.trimmed(), target.project.trimmed()));
                item->setData(Qt::UserRole, target.organization.trimmed());
                item->setData(Qt::UserRole + 1, target.project.trimmed());
                item->setCheckState(target.enabled ? Qt::Checked : Qt::Unchecked);
                devOpsTargetsList->addItem(item);
            }
        };
        populateTargetsList(azureDevOpsSettings.notificationTargets);

        auto* devOpsTargetButtonsLayout = new QHBoxLayout();
        auto* devOpsAddTargetBtn = new QPushButton(QStringLiteral("添加当前项目 ↓"), tab2Body);
        auto* devOpsRemoveTargetBtn = new QPushButton(QStringLiteral("删除选中"), tab2Body);
        devOpsAddTargetBtn->setStyleSheet(
            QStringLiteral("QPushButton { background:%1; color:#FFFFFF; border:none; border-radius:4px;"
                           " font-size:12px; padding:4px 10px; }"
                           "QPushButton:hover { background:%2; }")
                .arg(AppStyle::accent(), AppStyle::accentHover()));
        devOpsRemoveTargetBtn->setStyleSheet(
            QStringLiteral("QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:4px;"
                           " font-size:12px; padding:4px 10px; }"
                           "QPushButton:hover { background:%4; }")
                .arg(AppStyle::surfaceAlt(), AppStyle::textSecondary(), AppStyle::border(), AppStyle::hoverBg()));
        devOpsTargetButtonsLayout->addWidget(devOpsAddTargetBtn);
        devOpsTargetButtonsLayout->addWidget(devOpsRemoveTargetBtn);
        devOpsTargetButtonsLayout->addStretch();

        QObject::connect(devOpsAddTargetBtn, &QPushButton::clicked, tab2Body, [=]() {
            const QString org = devOpsOrgCombo->currentText().trimmed();
            const QString proj = devOpsProjectCombo->currentText().trimmed();
            if (org.isEmpty() || proj.isEmpty()) {
                return;
            }
            // 检查是否已存在
            for (int i = 0; i < devOpsTargetsList->count(); ++i) {
                QListWidgetItem* existing = devOpsTargetsList->item(i);
                if (existing->data(Qt::UserRole).toString().compare(org, Qt::CaseInsensitive) == 0
                    && existing->data(Qt::UserRole + 1).toString().compare(proj, Qt::CaseInsensitive) == 0) {
                    devOpsTargetsList->setCurrentItem(existing);
                    return;
                }
            }
            auto* item = new QListWidgetItem(QStringLiteral("%1 / %2").arg(org, proj));
            item->setData(Qt::UserRole, org);
            item->setData(Qt::UserRole + 1, proj);
            item->setCheckState(Qt::Checked);
            devOpsTargetsList->addItem(item);
            devOpsTargetsList->setCurrentItem(item);
        });

        QObject::connect(devOpsRemoveTargetBtn, &QPushButton::clicked, tab2Body, [=]() {
            QListWidgetItem* current = devOpsTargetsList->currentItem();
            if (current) {
                delete devOpsTargetsList->takeItem(devOpsTargetsList->row(current));
            }
        });

        tab2Layout->addWidget(devOpsTargetsLabel);
        tab2Layout->addWidget(devOpsTargetsList);
        tab2Layout->addLayout(devOpsTargetButtonsLayout);

        tab2Layout->addWidget(devOpsNotifyEnabled);
        tab2Layout->addWidget(devOpsNotifyHintLabel);
        tab2Layout->addWidget(devOpsPollIntervalLabel);
        tab2Layout->addWidget(devOpsPollIntervalSpin);

        auto* outlookSep = new ElaFrame(tab2Body);
        outlookSep->setFrameShape(QFrame::HLine);
        outlookSep->setStyleSheet(QStringLiteral("color: #E0E2E7;"));
        tab2Layout->addSpacing(4);
        tab2Layout->addWidget(outlookSep);

        auto* outlookLabel = new QLabel(QStringLiteral("集成账号 · Outlook"), tab2Body);
        outlookLabel->setObjectName(QStringLiteral("profileFieldLabel"));
        auto* outlookEnabled = new QCheckBox(QStringLiteral("启用 Outlook 集成"), tab2Body);
        outlookEnabled->setChecked(outlookSettings.enabled);
        auto* outlookServerUrlEdit = new QLineEdit(outlookSettings.serverUrl, tab2Body);
        outlookServerUrlEdit->setPlaceholderText(
            QStringLiteral("Exchange 服务器地址，如 https://mail.company.com"));
        auto* outlookUsernameEdit = new QLineEdit(outlookSettings.username, tab2Body);
        outlookUsernameEdit->setPlaceholderText(QStringLiteral("域账号，如 testuser"));
        auto* outlookPasswordEdit = new QLineEdit(outlookSettings.password, tab2Body);
        outlookPasswordEdit->setEchoMode(QLineEdit::Password);
        outlookPasswordEdit->setPlaceholderText(QStringLiteral("密码"));
        auto* outlookServerUrlLabel = new QLabel(QStringLiteral("Exchange 服务器"), tab2Body);
        auto* outlookUsernameLabel = new QLabel(QStringLiteral("用户名"), tab2Body);
        auto* outlookPasswordLabel = new QLabel(QStringLiteral("密码"), tab2Body);
        auto* outlookEmailLabel = new QLabel(QStringLiteral("邮箱账号"), tab2Body);
        auto* outlookDisplayNameLabel = new QLabel(QStringLiteral("显示名称"), tab2Body);
        auto* outlookPollIntervalLabel = new QLabel(QStringLiteral("轮询间隔"), tab2Body);
        auto* outlookTestConnectionBtn = new QPushButton(QStringLiteral("测试连接"), tab2Body);
        auto* outlookAuthStatusLabel = new QLabel(tab2Body);
        outlookAuthStatusLabel->setWordWrap(true);
        outlookAuthStatusLabel->setStyleSheet(
            QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));
        auto* outlookEmailEdit = new QLineEdit(outlookSettings.accountEmail, tab2Body);
        outlookEmailEdit->setPlaceholderText(QStringLiteral("授权成功后自动回填"));
        outlookEmailEdit->setReadOnly(true);
        auto* outlookDisplayNameEdit = new QLineEdit(outlookSettings.displayName, tab2Body);
        outlookDisplayNameEdit->setPlaceholderText(QStringLiteral("授权成功后自动回填"));
        outlookDisplayNameEdit->setReadOnly(true);
        auto* outlookNotifyEnabled = new QCheckBox(QStringLiteral("启用 Outlook 自动通知"), tab2Body);
        outlookNotifyEnabled->setChecked(outlookSettings.notificationsEnabled);
        auto outlookWorkingSettings = std::make_shared<OutlookConnectionSettings>(outlookSettings);
        auto* outlookHintLabel = new QLabel(
            QStringLiteral("使用 Exchange Web Services (EWS) 连接内网 Exchange Server；邮件和会议提醒会统一进入左侧“通知”页面。"),
            tab2Body);
        outlookHintLabel->setWordWrap(true);
        outlookHintLabel->setStyleSheet(
            QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));
        auto* outlookPollIntervalSpin = new QSpinBox(tab2Body);
        outlookPollIntervalSpin->setRange(1, 60);
        outlookPollIntervalSpin->setSuffix(QStringLiteral(" 分钟"));
        outlookPollIntervalSpin->setValue(qMax(1, outlookSettings.notificationPollIntervalMinutes));
        tab2Layout->addWidget(outlookLabel);
        tab2Layout->addWidget(outlookEnabled);
        tab2Layout->addWidget(outlookServerUrlLabel);
        tab2Layout->addWidget(outlookServerUrlEdit);
        tab2Layout->addWidget(outlookUsernameLabel);
        tab2Layout->addWidget(outlookUsernameEdit);
        tab2Layout->addWidget(outlookPasswordLabel);
        tab2Layout->addWidget(outlookPasswordEdit);
        tab2Layout->addWidget(outlookTestConnectionBtn);
        tab2Layout->addWidget(outlookAuthStatusLabel);
        tab2Layout->addWidget(outlookEmailLabel);
        tab2Layout->addWidget(outlookEmailEdit);
        tab2Layout->addWidget(outlookDisplayNameLabel);
        tab2Layout->addWidget(outlookDisplayNameEdit);
        tab2Layout->addWidget(outlookNotifyEnabled);
        tab2Layout->addWidget(outlookHintLabel);
        tab2Layout->addWidget(outlookPollIntervalLabel);
        tab2Layout->addWidget(outlookPollIntervalSpin);
        const auto updateOutlookAuthStatus = [=]() {
            OutlookConnectionSettings settings = *outlookWorkingSettings;
            settings.enabled = outlookEnabled->isChecked();
            settings.serverUrl = outlookServerUrlEdit->text().trimmed();
            settings.username = outlookUsernameEdit->text().trimmed();
            settings.password = outlookPasswordEdit->text();
            if (!settings.hasCredentialConfiguration()) {
                outlookAuthStatusLabel->setText(QStringLiteral("请填写服务器地址和用户名"));
                outlookAuthStatusLabel->setStyleSheet(
                    QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));
            } else {
                outlookAuthStatusLabel->setText(
                    QStringLiteral("已配置：%1 @ %2").arg(settings.username, settings.serverUrl));
                outlookAuthStatusLabel->setStyleSheet(
                    QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textSecondary()));
            }
        };
        updateOutlookAuthStatus();
        tab2Layout->addStretch();

        // ── Tab 4: 关于 ──────────────────────────────
        auto* versionLabel = new QLabel(QStringLiteral("版本与更新"), tab4Body);
        versionLabel->setObjectName(QStringLiteral("profileFieldLabel"));
        auto* versionCard = new ElaFrame(tab4Body);
        versionCard->setObjectName(QStringLiteral("profileVersionCard"));
        versionCard->setStyleSheet(QStringLiteral(
            "QFrame#profileVersionCard {"
            "  background:%1;"
            "  border:1px solid %2;"
            "  border-radius:10px;"
            "}"
            "QLabel#profileVersionBadge {"
            "  background:%3;"
            "  color:%4;"
            "  border-radius:10px;"
            "  padding:2px 10px;"
            "  font-size:11px;"
            "  font-weight:600;"
            "}"
            "QLabel#profileVersionTitle {"
            "  color:%5;"
            "  font-size:13px;"
            "  font-weight:600;"
            "}"
            "QLabel#profileVersionDetail {"
            "  color:%6;"
            "  font-size:12px;"
            "}"
            "QPushButton#profileLinkButton {"
            "  color:%4;"
            "  border:none;"
            "  background:transparent;"
            "  font-size:12px;"
            "  padding:0;"
            "  text-align:left;"
            "}"
            "QPushButton#profileLinkButton:hover { text-decoration:underline; }")
            .arg(AppStyle::surfaceAlt(),
                 AppStyle::border(),
                 AppStyle::hoverBg(),
                 AppStyle::accent(),
                 AppStyle::textPrimary(),
                 AppStyle::textSecondary()));
        auto* versionCardLayout = new QVBoxLayout(versionCard);
        versionCardLayout->setContentsMargins(12, 12, 12, 12);
        versionCardLayout->setSpacing(6);
        auto* versionBadge = new QLabel(QStringLiteral("当前版本"), versionCard);
        versionBadge->setObjectName(QStringLiteral("profileVersionBadge"));
        versionBadge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        auto* versionTitle = new QLabel(QStringLiteral("%1 %2")
                                            .arg(appDisplayName,
                                                 ApplicationInfo::currentVersion()),
                                        versionCard);
        versionTitle->setObjectName(QStringLiteral("profileVersionTitle"));
        auto* versionDetail = new QLabel(
            QStringLiteral("这里可以查看本版更新内容。每次发布新安装包时，版本号和更新说明会一起变化。"),
            versionCard);
        versionDetail->setObjectName(QStringLiteral("profileVersionDetail"));
        versionDetail->setWordWrap(true);
        auto* versionButtonRow = new QHBoxLayout;
        versionButtonRow->setContentsMargins(0, 2, 0, 0);
        versionButtonRow->setSpacing(12);
        auto* aboutButton = new QPushButton(QStringLiteral("关于 LeyoChat"), versionCard);
        aboutButton->setObjectName(QStringLiteral("profileLinkButton"));
        aboutButton->setCursor(Qt::PointingHandCursor);
        auto* releaseNotesButton = new QPushButton(QStringLiteral("本版更新"), versionCard);
        releaseNotesButton->setObjectName(QStringLiteral("profileLinkButton"));
        releaseNotesButton->setCursor(Qt::PointingHandCursor);
        versionButtonRow->addWidget(aboutButton);
        versionButtonRow->addWidget(releaseNotesButton);
        auto* checkUpdateButton = new QPushButton(QStringLiteral("\u68C0\u67E5\u66F4\u65B0"), versionCard);
        checkUpdateButton->setObjectName(QStringLiteral("profileLinkButton"));
        checkUpdateButton->setCursor(Qt::PointingHandCursor);
        auto* checkUpdateStatus = new QLabel(versionCard);
        checkUpdateStatus->setStyleSheet(
            QStringLiteral("font-size:11px; color:%1; padding-left:4px;").arg(AppStyle::textMuted()));
        checkUpdateStatus->hide();
        versionButtonRow->addWidget(checkUpdateButton);
        versionButtonRow->addWidget(checkUpdateStatus);
        versionButtonRow->addStretch();
        versionCardLayout->addWidget(versionBadge, 0, Qt::AlignLeft);
        versionCardLayout->addWidget(versionTitle);
        versionCardLayout->addWidget(versionDetail);
        versionCardLayout->addLayout(versionButtonRow);
        tab4Layout->addWidget(versionLabel);
        tab4Layout->addWidget(versionCard);

        const RuntimeArchitecturePresentation runtimePresentation =
            m_runtimeArchitectureSnapshot
                ? buildRuntimeArchitecturePresentation(*m_runtimeArchitectureSnapshot)
                : buildRuntimeArchitecturePresentation(0, 0, 0, 0, false, QString());
        auto* runtimeSep = new ElaFrame(tab4Body);
        runtimeSep->setFrameShape(QFrame::HLine);
        runtimeSep->setStyleSheet(QStringLiteral("color: #E0E2E7;"));
        tab4Layout->addSpacing(4);
        tab4Layout->addWidget(runtimeSep);

        auto* runtimeLabel = new QLabel(QStringLiteral("混合架构状态"), tab4Body);
        runtimeLabel->setObjectName(QStringLiteral("profileFieldLabel"));
        auto* runtimeCard = new ElaFrame(tab4Body);
        runtimeCard->setObjectName(QStringLiteral("profileRuntimeCard"));
        runtimeCard->setStyleSheet(QStringLiteral(
            "QFrame#profileRuntimeCard {"
            "  background:%1;"
            "  border:1px solid %2;"
            "  border-radius:10px;"
            "}"
            "QLabel#profileRuntimeBadge {"
            "  background:%3;"
            "  color:%4;"
            "  border-radius:10px;"
            "  padding:2px 10px;"
            "  font-size:11px;"
            "  font-weight:600;"
            "}"
            "QLabel#profileRuntimeSummary {"
            "  color:%5;"
            "  font-size:13px;"
            "  font-weight:600;"
            "}"
            "QLabel#profileRuntimeDetail {"
            "  color:%6;"
            "  font-size:12px;"
            "}"
            "QLabel#profileRuntimeFootnote {"
            "  color:%6;"
            "  font-size:11px;"
            "}")
            .arg(AppStyle::surfaceAlt(),
                 AppStyle::border(),
                 AppStyle::hoverBg(),
                 AppStyle::accent(),
                 AppStyle::textPrimary(),
                 AppStyle::textSecondary()));
        auto* runtimeCardLayout = new QVBoxLayout(runtimeCard);
        runtimeCardLayout->setContentsMargins(12, 12, 12, 12);
        runtimeCardLayout->setSpacing(6);
        auto* runtimeBadge = new QLabel(runtimePresentation.panelBadge, runtimeCard);
        runtimeBadge->setObjectName(QStringLiteral("profileRuntimeBadge"));
        runtimeBadge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        auto* runtimeSummaryLabel = new QLabel(runtimePresentation.panelSummary, runtimeCard);
        runtimeSummaryLabel->setObjectName(QStringLiteral("profileRuntimeSummary"));
        runtimeSummaryLabel->setWordWrap(true);
        auto* runtimeDetailLabel = new QLabel(runtimePresentation.panelDetail, runtimeCard);
        runtimeDetailLabel->setObjectName(QStringLiteral("profileRuntimeDetail"));
        runtimeDetailLabel->setWordWrap(true);
        auto* runtimeFootnoteLabel = new QLabel(runtimePresentation.panelFootnote, runtimeCard);
        runtimeFootnoteLabel->setObjectName(QStringLiteral("profileRuntimeFootnote"));
        runtimeFootnoteLabel->setWordWrap(true);
        runtimeCardLayout->addWidget(runtimeBadge, 0, Qt::AlignLeft);
        runtimeCardLayout->addWidget(runtimeSummaryLabel);
        runtimeCardLayout->addWidget(runtimeDetailLabel);
        runtimeCardLayout->addWidget(runtimeFootnoteLabel);
        tab4Layout->addWidget(runtimeLabel);
        tab4Layout->addWidget(runtimeCard);

        sigEdit->setAttribute(Qt::WA_InputMethodEnabled);
        nameEdit->setAttribute(Qt::WA_InputMethodEnabled);
        departmentEdit->setAttribute(Qt::WA_InputMethodEnabled);
        jobTitleEdit->setAttribute(Qt::WA_InputMethodEnabled);
        phoneEdit->setAttribute(Qt::WA_InputMethodEnabled);
        genderCombo->setAttribute(Qt::WA_InputMethodEnabled);
        emailEdit->setAttribute(Qt::WA_InputMethodEnabled);
        themeCombo->setAttribute(Qt::WA_InputMethodEnabled);
        devOpsEnabled->setAttribute(Qt::WA_InputMethodEnabled);
        devOpsBaseUrlEdit->setAttribute(Qt::WA_InputMethodEnabled);
        devOpsOrgCombo->setAttribute(Qt::WA_InputMethodEnabled);
        devOpsProjectCombo->setAttribute(Qt::WA_InputMethodEnabled);
        devOpsTokenEdit->setAttribute(Qt::WA_InputMethodEnabled);
        devOpsNotifyEnabled->setAttribute(Qt::WA_InputMethodEnabled);
        devOpsNotifyHintLabel->setAttribute(Qt::WA_InputMethodEnabled);
        devOpsPollIntervalSpin->setAttribute(Qt::WA_InputMethodEnabled);
        devOpsTestConnectionBtn->setAttribute(Qt::WA_InputMethodEnabled);
        devOpsDiscoverOrganizationsBtn->setAttribute(Qt::WA_InputMethodEnabled);
        devOpsLoadProjectsBtn->setAttribute(Qt::WA_InputMethodEnabled);
        outlookEnabled->setAttribute(Qt::WA_InputMethodEnabled);
        outlookServerUrlEdit->setAttribute(Qt::WA_InputMethodEnabled);
        outlookUsernameEdit->setAttribute(Qt::WA_InputMethodEnabled);
        outlookPasswordEdit->setAttribute(Qt::WA_InputMethodEnabled);
        outlookEmailEdit->setAttribute(Qt::WA_InputMethodEnabled);
        outlookDisplayNameEdit->setAttribute(Qt::WA_InputMethodEnabled);
        outlookNotifyEnabled->setAttribute(Qt::WA_InputMethodEnabled);
        outlookHintLabel->setAttribute(Qt::WA_InputMethodEnabled);
        outlookPollIntervalSpin->setAttribute(Qt::WA_InputMethodEnabled);
        outlookTestConnectionBtn->setAttribute(Qt::WA_InputMethodEnabled);

        auto* cancelBtn = new QPushButton(QStringLiteral("取消"), footerWidget);
        cancelBtn->setObjectName(QStringLiteral("cancelBtn"));
        cancelBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background:%1;"
            "  color:%2;"
            "  border:1px solid %3;"
            "  border-radius:6px;"
            "  font-size:13px;"
            "  padding:6px 16px;"
            "}"
            "QPushButton:hover {"
            "  background:%4;"
            "  color:%5;"
            "}").arg(AppStyle::surfaceAlt(),
                      AppStyle::textSecondary(),
                      AppStyle::border(),
                      AppStyle::hoverBg(),
                      AppStyle::textPrimary()));
        auto* saveBtn = new QPushButton(QStringLiteral("保存"), footerWidget);
        saveBtn->setObjectName(QStringLiteral("saveBtn"));
        QObject::connect(aboutButton, &QPushButton::clicked, panel, [showAboutDialog]() {
            showAboutDialog();
        });
        QObject::connect(releaseNotesButton, &QPushButton::clicked, panel, [showCurrentReleaseNotes, appDisplayName]() {
            showCurrentReleaseNotes(QStringLiteral("%1 本版更新").arg(appDisplayName),
                                    QStringLiteral("当前版本 %1")
                                        .arg(ApplicationInfo::currentVersion()));
        });
        QObject::connect(checkUpdateButton, &QPushButton::clicked, panel,
                         [this, checkUpdateButton, checkUpdateStatus]() {
            if (!m_updateChecker) return;
            checkUpdateButton->setEnabled(false);
            checkUpdateStatus->setText(QStringLiteral("\u23F3 \u6B63\u5728\u68C0\u67E5..."));
            checkUpdateStatus->show();
            auto* conn = new QMetaObject::Connection;
            auto* connFail = new QMetaObject::Connection;
            auto* connNone = new QMetaObject::Connection;
            *conn = QObject::connect(m_updateChecker, &UpdateChecker::updateAvailable,
                           checkUpdateButton, [this, checkUpdateButton, checkUpdateStatus, conn, connFail, connNone](const UpdateChecker::UpdateInfo& info) {
                checkUpdateButton->setEnabled(true);
                checkUpdateStatus->setText(QStringLiteral("\U0001F514 \u53D1\u73B0\u65B0\u7248\u672C v%1\uFF01").arg(info.version));
                checkUpdateStatus->setStyleSheet(QStringLiteral("font-size:11px; color:#165DFF; padding-left:4px;"));
                QObject::disconnect(*conn); QObject::disconnect(*connFail); QObject::disconnect(*connNone);
                delete conn; delete connFail; delete connNone;

                // 弹窗询问是否立即更新
                const bool doUpdate = LeyoDialog::question(
                    checkUpdateButton->window(),
                    QStringLiteral("\u53D1\u73B0\u65B0\u7248\u672C"),
                    QStringLiteral("\u53D1\u73B0\u65B0\u7248\u672C v%1\uFF0C\u662F\u5426\u7ACB\u5373\u4E0B\u8F7D\u5E76\u5B89\u88C5\uFF1F").arg(info.version));
                if (doUpdate) {
                    m_pendingUpdateInfo = info;
                    if (m_updateDownloader) {
                        m_updateDownloader->download(
                            m_updateChecker->updateSourcePath(),
                            info.fileName,
                            info.sha256);
                        if (m_mainWindow && m_mainWindow->updateBar()) {
                            m_mainWindow->updateBar()->showProgress(0);
                            m_mainWindow->repositionUpdateBar();
                        }
                    }
                }
            });
            *connNone = QObject::connect(m_updateChecker, &UpdateChecker::noUpdateAvailable,
                               checkUpdateButton, [checkUpdateButton, checkUpdateStatus, conn, connFail, connNone]() {
                checkUpdateButton->setEnabled(true);
                checkUpdateStatus->setText(QStringLiteral("\u2705 \u5DF2\u662F\u6700\u65B0\u7248\u672C"));
                checkUpdateStatus->setStyleSheet(QStringLiteral("font-size:11px; color:green; padding-left:4px;"));
                QObject::disconnect(*conn); QObject::disconnect(*connFail); QObject::disconnect(*connNone);
                delete conn; delete connFail; delete connNone;
            });
            *connFail = QObject::connect(m_updateChecker, &UpdateChecker::checkFailed,
                               checkUpdateButton, [checkUpdateButton, checkUpdateStatus, conn, connFail, connNone](const QString& error) {
                checkUpdateButton->setEnabled(true);
                checkUpdateStatus->setText(QStringLiteral("\u26A0\uFE0F \u68C0\u67E5\u5931\u8D25"));
                checkUpdateStatus->setToolTip(error);
                checkUpdateStatus->setStyleSheet(QStringLiteral("font-size:11px; color:orange; padding-left:4px;"));
                QObject::disconnect(*conn); QObject::disconnect(*connFail); QObject::disconnect(*connNone);
                delete conn; delete connFail; delete connNone;
            });
            m_updateChecker->checkNow();
        });
        const auto populateEditableCombo =
            [](QComboBox* combo, const QStringList& items, const QString& currentText) {
                if (!combo) {
                    return;
                }
                combo->blockSignals(true);
                combo->clear();
                QStringList uniqueItems;
                for (const QString& item : items) {
                    const QString trimmed = item.trimmed();
                    if (!trimmed.isEmpty() && !uniqueItems.contains(trimmed)) {
                        uniqueItems.push_back(trimmed);
                    }
                }
                for (const QString& item : uniqueItems) {
                    combo->addItem(item);
                }
                if (!currentText.trimmed().isEmpty()
                    && combo->findText(currentText.trimmed()) < 0) {
                    combo->addItem(currentText.trimmed());
                }
                combo->setCurrentText(currentText.trimmed());
                combo->blockSignals(false);
            };
        const auto currentAzureDevOpsSettingsFromUi =
            [=]() {
                AzureDevOpsConnectionSettings settings;
                settings.enabled = devOpsEnabled->isChecked();
                settings.baseUrl = devOpsBaseUrlEdit->text().trimmed();
                settings.organization = devOpsOrgCombo->currentText().trimmed();
                settings.project = devOpsProjectCombo->currentText().trimmed();
                settings.personalAccessToken = devOpsTokenEdit->text().trimmed();
                settings.notificationsEnabled = devOpsNotifyEnabled->isChecked();
                settings.notificationPollIntervalMinutes = devOpsPollIntervalSpin->value();
                settings.lastNotifiedBuildId = azureDevOpsSettings.lastNotifiedBuildId;
                return settings;
            };
        const auto discoverOrganizationsIntoUi =
            [=, this](QWidget* owner) {
                LocalAzureDevOpsAdapter adapter(currentAzureDevOpsSettingsFromUi());
                QString errorMessage;
                const auto organizations = adapter.discoverOrganizations(&errorMessage);
                if (organizations.isEmpty()) {
                    LeyoDialog::warning(owner,
                                         QStringLiteral("Azure DevOps"),
                                         errorMessage.trimmed().isEmpty()
                                             ? QStringLiteral("没有发现可用组织，请确认 PAT 权限是否包含组织读取能力。")
                                             : errorMessage);
                    return false;
                }
                QStringList names;
                for (const auto& organization : organizations) {
                    names.push_back(organization.organizationName);
                }
                const QString currentText =
                    devOpsOrgCombo->currentText().trimmed().isEmpty()
                        ? names.front()
                        : devOpsOrgCombo->currentText().trimmed();
                populateEditableCombo(devOpsOrgCombo, names, currentText);
                if (devOpsProjectCombo->currentText().trimmed().isEmpty()) {
                    devOpsProjectCombo->clear();
                }
                m_mainWindow->setStatusMessage(QStringLiteral("已发现 Azure DevOps 组织"), 2000);
                return true;
            };
        const auto loadProjectsIntoUi =
            [=, this](QWidget* owner) {
                const QString organization = devOpsOrgCombo->currentText().trimmed();
                LocalAzureDevOpsAdapter adapter(currentAzureDevOpsSettingsFromUi());
                QString errorMessage;
                const auto projects = adapter.discoverProjects(organization, &errorMessage);
                if (projects.isEmpty()) {
                    LeyoDialog::warning(owner,
                                         QStringLiteral("Azure DevOps"),
                                         errorMessage.trimmed().isEmpty()
                                             ? QStringLiteral("没有发现可用项目，请确认组织名称和 PAT 权限。")
                                             : errorMessage);
                    return false;
                }
                QStringList names;
                for (const auto& project : projects) {
                    names.push_back(project.projectName);
                }
                const QString currentText =
                    devOpsProjectCombo->currentText().trimmed().isEmpty()
                        ? names.front()
                        : devOpsProjectCombo->currentText().trimmed();
                populateEditableCombo(devOpsProjectCombo, names, currentText);
                m_mainWindow->setStatusMessage(QStringLiteral("已加载 Azure DevOps 项目"), 2000);
                return true;
            };
        QObject::connect(devOpsTestConnectionBtn, &QPushButton::clicked, panel,
                         [=]() {
            devOpsTestConnectionBtn->setEnabled(false);
            devOpsDiscoverOrganizationsBtn->setEnabled(false);
            devOpsLoadProjectsBtn->setEnabled(false);
            QApplication::setOverrideCursor(Qt::WaitCursor);
            AzureDevOpsConnectionSettings settings = currentAzureDevOpsSettingsFromUi();
            LocalAzureDevOpsAdapter adapter(settings);
            QString errorMessage;
            if (!adapter.testConnection(&errorMessage)) {
                QApplication::restoreOverrideCursor();
                devOpsTestConnectionBtn->setEnabled(true);
                devOpsDiscoverOrganizationsBtn->setEnabled(true);
                devOpsLoadProjectsBtn->setEnabled(true);
                LeyoDialog::warning(panel,
                                     QStringLiteral("Azure DevOps"),
                                     errorMessage.trimmed().isEmpty()
                                         ? QStringLiteral("Azure DevOps 连接测试失败")
                                         : errorMessage);
                return;
            }

            QApplication::restoreOverrideCursor();
            devOpsTestConnectionBtn->setEnabled(true);
            devOpsDiscoverOrganizationsBtn->setEnabled(true);
            devOpsLoadProjectsBtn->setEnabled(true);
            LeyoDialog::information(panel,
                                     QStringLiteral("Azure DevOps"),
                                     settings.hasProjectSelection()
                                         ? QStringLiteral("Azure DevOps 连接测试成功，可以启用通知或继续完善集成配置。")
                                         : QStringLiteral("Azure DevOps 认证成功，接下来可以发现组织并选择项目。"));
            if (!settings.hasProjectSelection()) {
                discoverOrganizationsIntoUi(panel);
            }
        });
        QObject::connect(devOpsDiscoverOrganizationsBtn, &QPushButton::clicked, panel,
                         [=]() {
            devOpsTestConnectionBtn->setEnabled(false);
            devOpsDiscoverOrganizationsBtn->setEnabled(false);
            devOpsLoadProjectsBtn->setEnabled(false);
            devOpsDiscoverOrganizationsBtn->setText(QStringLiteral("正在发现…"));
            const auto settings = currentAzureDevOpsSettingsFromUi();
            auto* future = new QFutureWatcher<QPair<QStringList, QString>>(panel);
            QObject::connect(future, &QFutureWatcher<QPair<QStringList, QString>>::finished, panel,
                             [=]() {
                const auto result = future->result();
                future->deleteLater();
                devOpsDiscoverOrganizationsBtn->setText(QStringLiteral("发现组织"));
                devOpsTestConnectionBtn->setEnabled(true);
                devOpsDiscoverOrganizationsBtn->setEnabled(true);
                devOpsLoadProjectsBtn->setEnabled(true);
                if (result.first.isEmpty()) {
                    LeyoDialog::warning(panel,
                                         QStringLiteral("Azure DevOps"),
                                         result.second.trimmed().isEmpty()
                                             ? QStringLiteral("没有发现可用组织，请确认 PAT 权限是否包含组织读取能力。")
                                             : result.second);
                    return;
                }
                const QString currentText =
                    devOpsOrgCombo->currentText().trimmed().isEmpty()
                        ? result.first.front()
                        : devOpsOrgCombo->currentText().trimmed();
                populateEditableCombo(devOpsOrgCombo, result.first, currentText);
                if (devOpsProjectCombo->currentText().trimmed().isEmpty()) {
                    devOpsProjectCombo->clear();
                }
                m_mainWindow->setStatusMessage(QStringLiteral("已发现 Azure DevOps 组织"), 2000);
            });
            future->setFuture(QtConcurrent::run([settings]() -> QPair<QStringList, QString> {
                LocalAzureDevOpsAdapter adapter(settings);
                QString errorMessage;
                const auto organizations = adapter.discoverOrganizations(&errorMessage);
                QStringList names;
                for (const auto& org : organizations) {
                    names.push_back(org.organizationName);
                }
                return {names, errorMessage};
            }));
        });
        QObject::connect(devOpsLoadProjectsBtn, &QPushButton::clicked, panel,
                         [=]() {
            devOpsTestConnectionBtn->setEnabled(false);
            devOpsDiscoverOrganizationsBtn->setEnabled(false);
            devOpsLoadProjectsBtn->setEnabled(false);
            devOpsLoadProjectsBtn->setText(QStringLiteral("正在加载…"));
            const auto settings = currentAzureDevOpsSettingsFromUi();
            const QString organization = devOpsOrgCombo->currentText().trimmed();
            auto* future = new QFutureWatcher<QPair<QStringList, QString>>(panel);
            QObject::connect(future, &QFutureWatcher<QPair<QStringList, QString>>::finished, panel,
                             [=]() {
                const auto result = future->result();
                future->deleteLater();
                devOpsLoadProjectsBtn->setText(QStringLiteral("加载项目"));
                devOpsTestConnectionBtn->setEnabled(true);
                devOpsDiscoverOrganizationsBtn->setEnabled(true);
                devOpsLoadProjectsBtn->setEnabled(true);
                if (result.first.isEmpty()) {
                    LeyoDialog::warning(panel,
                                         QStringLiteral("Azure DevOps"),
                                         result.second.trimmed().isEmpty()
                                             ? QStringLiteral("没有发现可用项目，请确认组织名称和 PAT 权限。")
                                             : result.second);
                    return;
                }
                const QString currentText =
                    devOpsProjectCombo->currentText().trimmed().isEmpty()
                        ? result.first.front()
                        : devOpsProjectCombo->currentText().trimmed();
                populateEditableCombo(devOpsProjectCombo, result.first, currentText);
                m_mainWindow->setStatusMessage(QStringLiteral("已加载 Azure DevOps 项目"), 2000);
            });
            future->setFuture(QtConcurrent::run([settings, organization]() -> QPair<QStringList, QString> {
                LocalAzureDevOpsAdapter adapter(settings);
                QString errorMessage;
                const auto projects = adapter.discoverProjects(organization, &errorMessage);
                QStringList names;
                for (const auto& proj : projects) {
                    names.push_back(proj.projectName);
                }
                return {names, errorMessage};
            }));
        });
        QObject::connect(devOpsOrgCombo, &QComboBox::editTextChanged, panel, [=](const QString&) {
            devOpsProjectCombo->clear();
        });
        QObject::connect(devOpsOrgCombo, &QComboBox::currentTextChanged, panel, [=](const QString&) {
            devOpsProjectCombo->clear();
        });
        const auto currentOutlookSettingsFromUi =
            [=]() {
                OutlookConnectionSettings settings = *outlookWorkingSettings;
                settings.enabled = outlookEnabled->isChecked();
                settings.serverUrl = outlookServerUrlEdit->text().trimmed();
                settings.username = outlookUsernameEdit->text().trimmed();
                settings.password = outlookPasswordEdit->text();
                settings.accountEmail = outlookEmailEdit->text().trimmed();
                settings.displayName = outlookDisplayNameEdit->text().trimmed();
                settings.notificationsEnabled = outlookNotifyEnabled->isChecked();
                settings.notificationPollIntervalMinutes = outlookPollIntervalSpin->value();
                return settings;
            };
        const auto persistOutlookWorkingSettings =
            [=]() {
                *outlookWorkingSettings = currentOutlookSettingsFromUi();
                OutlookSettingsStore::save(*outlookWorkingSettings);
                updateOutlookAuthStatus();
            };
        QObject::connect(outlookTestConnectionBtn, &QPushButton::clicked, panel, [=]() {
            OutlookConnectionSettings settings = currentOutlookSettingsFromUi();
            LocalOutlookAdapter adapter(settings);
            QString errorMessage;
            if (!adapter.testConnection(&errorMessage)) {
                const QString summary = OutlookSettingsStore::summarizeErrorMessage(
                    errorMessage,
                    settings.serverUrl.trimmed().isEmpty() || settings.username.trimmed().isEmpty()
                        ? QString()
                        : QStringLiteral("network"));
                LeyoDialog::warning(panel,
                                     QStringLiteral("Outlook"),
                                     summary.trimmed().isEmpty()
                                         ? QStringLiteral("Outlook 连接测试失败")
                                         : summary);
                return;
            }

            *outlookWorkingSettings = adapter.settings();
            outlookEmailEdit->setText(outlookWorkingSettings->accountEmail);
            outlookDisplayNameEdit->setText(outlookWorkingSettings->displayName);
            persistOutlookWorkingSettings();
            LeyoDialog::information(panel,
                                     QStringLiteral("Outlook"),
                                     QStringLiteral("Outlook 连接测试成功，可以开启自动提醒。"));
        });
        QObject::connect(outlookEnabled, &QCheckBox::toggled, panel, [=](bool) {
            updateOutlookAuthStatus();
        });
        QObject::connect(outlookServerUrlEdit, &QLineEdit::textChanged, panel, [=](const QString&) {
            updateOutlookAuthStatus();
        });
        QObject::connect(outlookUsernameEdit, &QLineEdit::textChanged, panel, [=](const QString&) {
            updateOutlookAuthStatus();
        });
        QObject::connect(outlookPasswordEdit, &QLineEdit::textChanged, panel, [=](const QString&) {
            updateOutlookAuthStatus();
        });
        QObject::connect(cancelBtn, &QPushButton::clicked, panel, &QDialog::close);
        QObject::connect(saveBtn, &QPushButton::clicked, panel,
                         [&, nameLabel, nameEdit, sigEdit, departmentEdit, jobTitleEdit, phoneEdit, genderCombo, emailEdit, themeCombo, trayPopupCheckBox, hotkeyEdit, globalHotkeyManager, devOpsEnabled, devOpsBaseUrlEdit, devOpsOrgCombo, devOpsProjectCombo, devOpsTokenEdit, devOpsNotifyEnabled, devOpsPollIntervalSpin, devOpsTargetsList, knowledgeServiceWidget, tabWidget, outlookEnabled, outlookServerUrlEdit, outlookUsernameEdit, outlookPasswordEdit, outlookEmailEdit, outlookDisplayNameEdit, outlookNotifyEnabled, outlookPollIntervalSpin, panel, outlookWorkingSettings, azureDevOpsSettings, updateServerEdit, autoCheckBox, checkIntervalSpin]() {
            const QString newDisplayName = nameEdit->text().trimmed();
            if (newDisplayName.isEmpty()) {
                LeyoDialog::warning(panel,
                                     QStringLiteral("保存失败"),
                                     QStringLiteral("姓名不能为空"));
                nameEdit->setFocus();
                return;
            }
            QString knowledgeServiceError;
            if (knowledgeServiceWidget && !knowledgeServiceWidget->validate(&knowledgeServiceError)) {
                tabWidget->setCurrentIndex(2);
                LeyoDialog::warning(panel,
                                     QStringLiteral("保存失败"),
                                     knowledgeServiceError.trimmed().isEmpty()
                                         ? QStringLiteral("知识服务配置无效，请检查后再保存。")
                                         : knowledgeServiceError);
                return;
            }
            localDisplayName = newDisplayName;
            profile->displayName = localDisplayName.toStdWString();
            profile->signature = sigEdit->text().trimmed().toStdWString();
            profile->department = departmentEdit->text().trimmed().toStdWString();
            profile->jobTitle = jobTitleEdit->text().trimmed().toStdWString();
            profile->phoneNumber = phoneEdit->text().trimmed().toStdWString();
            profile->gender = genderCombo->currentText().trimmed().toStdWString();
            profile->email = emailEdit->text().trimmed().toStdWString();
            identityService.saveProfile(*profile);
            nameLabel->setText(localDisplayName);
            QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
            const QString selectedThemeMode = themeCombo->currentData().toString();
            cfg.setValue(QStringLiteral("appearance/themeMode"), selectedThemeMode);
            cfg.setValue(QStringLiteral("notification/trayPopupEnabled"),
                         trayPopupCheckBox->isChecked());
            // ── 截图快捷键保存 ──
            {
                const QKeySequence newHotkey = hotkeyEdit->keySequence();
                cfg.setValue(QStringLiteral("screenshotHotkey"),
                             newHotkey.toString(QKeySequence::PortableText));
                if (globalHotkeyManager) {
                    if (newHotkey.isEmpty()) {
                        globalHotkeyManager->unregisterHotkey();
                    } else if (!globalHotkeyManager->registerHotkey(newHotkey)) {
                        LeyoDialog::warning(panel,
                                             QStringLiteral("截图快捷键"),
                                             QStringLiteral("快捷键 %1 注册失败，可能已被其他程序占用。\n"
                                                            "设置已保存，下次启动时会重新尝试注册。")
                                                 .arg(newHotkey.toString(QKeySequence::NativeText)));
                    }
                }
            }
            // ── 升级服务器地址保存 ──
            cfg.setValue(QStringLiteral("update/serverPath"),
                         updateServerEdit->text().trimmed());
            if (m_updateChecker) {
                m_updateChecker->setUpdateSourcePath(updateServerEdit->text().trimmed());
            }

            // ── 自动更新检测配置保存 ──
            const bool autoEnabled = autoCheckBox->isChecked();
            const int intervalMin = checkIntervalSpin->value();
            cfg.setValue(QStringLiteral("update/autoCheckEnabled"), autoEnabled);
            cfg.setValue(QStringLiteral("update/checkIntervalMinutes"), intervalMin);
            if (m_updateChecker) {
                if (autoEnabled) {
                    m_updateChecker->start(intervalMin * 60000);
                } else {
                    m_updateChecker->stop();
                    if (m_mainWindow && m_mainWindow->updateBar()) {
                        m_mainWindow->updateBar()->hideBar();
                        m_pendingUpdateInfo = {};
                    }
                }
            }
            cfg.sync();

            // ── 知识服务配置保存 ──────────────────────────
            if (knowledgeServiceWidget) {
                const QVector<KnowledgeServiceConfig> savedKnowledgeServices = knowledgeServiceWidget->configs();
                KnowledgeServiceSettingsStore::save(savedKnowledgeServices);
                if (m_mainWindow) {
                    QString preferredServiceId;
                    for (const KnowledgeServiceConfig& config : savedKnowledgeServices) {
                        if (config.isDefault) {
                            preferredServiceId = config.localId;
                            break;
                        }
                    }
                    m_mainWindow->setAiKnowledgeServices(savedKnowledgeServices, preferredServiceId);
                }
            }

            AzureDevOpsConnectionSettings devOpsSettings = azureDevOpsSettings;
            devOpsSettings.enabled = devOpsEnabled->isChecked();
            devOpsSettings.baseUrl = devOpsBaseUrlEdit->text().trimmed();
            devOpsSettings.organization = devOpsOrgCombo->currentText().trimmed();
            devOpsSettings.project = devOpsProjectCombo->currentText().trimmed();
            devOpsSettings.personalAccessToken = devOpsTokenEdit->text().trimmed();
            devOpsSettings.notificationsEnabled = devOpsNotifyEnabled->isChecked();
            devOpsSettings.notificationPollIntervalMinutes = devOpsPollIntervalSpin->value();
            // 从通知目标列表 widget 收集 notificationTargets
            {
                // 构建新 target 列表，保留现有 target 的轮询状态
                QVector<AzureDevOpsNotificationTarget> newTargets;
                for (int i = 0; i < devOpsTargetsList->count(); ++i) {
                    const QListWidgetItem* item = devOpsTargetsList->item(i);
                    const QString org = item->data(Qt::UserRole).toString().trimmed();
                    const QString proj = item->data(Qt::UserRole + 1).toString().trimmed();
                    if (org.isEmpty() || proj.isEmpty()) {
                        continue;
                    }
                    // 查找现有 target 以保留轮询状态
                    AzureDevOpsNotificationTarget target;
                    target.organization = org;
                    target.project = proj;
                    target.enabled = (item->checkState() == Qt::Checked);
                    for (const auto& existing : azureDevOpsSettings.notificationTargets) {
                        if (existing.organization.trimmed().compare(org, Qt::CaseInsensitive) == 0
                            && existing.project.trimmed().compare(proj, Qt::CaseInsensitive) == 0) {
                            target.lastNotifiedBuildId = existing.lastNotifiedBuildId;
                            target.lastNotifiedPullRequestUpdatedAtMs = existing.lastNotifiedPullRequestUpdatedAtMs;
                            target.lastNotifiedAssignedWorkItemUpdatedAtMs = existing.lastNotifiedAssignedWorkItemUpdatedAtMs;
                            target.lastNotifiedBuildResult = existing.lastNotifiedBuildResult;
                            target.lastPollSuccessAtMs = existing.lastPollSuccessAtMs;
                            break;
                        }
                    }
                    newTargets.push_back(target);
                }
                devOpsSettings.notificationTargets = newTargets;
            }
            devOpsSettings.normalizeSelection();
            // 用户重新保存配置时清除历史退避状态，让下次轮询立即以基准间隔执行
            devOpsSettings.consecutivePollFailures = 0;
            devOpsSettings.lastPollErrorMessage.clear();
            devOpsSettings.lastPollErrorCategory.clear();
            for (auto& target : devOpsSettings.notificationTargets) {
                target.consecutivePollFailures = 0;
                target.lastPollErrorMessage.clear();
                target.lastPollErrorCategory.clear();
            }
            AzureDevOpsSettingsStore::save(devOpsSettings);
            {
                OutlookConnectionSettings settings = *outlookWorkingSettings;
                settings.enabled = outlookEnabled->isChecked();
                settings.serverUrl = outlookServerUrlEdit->text().trimmed();
                settings.username = outlookUsernameEdit->text().trimmed();
                settings.password = outlookPasswordEdit->text();
                settings.accountEmail = outlookEmailEdit->text().trimmed();
                settings.displayName = outlookDisplayNameEdit->text().trimmed();
                settings.notificationsEnabled = outlookNotifyEnabled->isChecked();
                settings.notificationPollIntervalMinutes = outlookPollIntervalSpin->value();
                // 同样清除 Outlook 的历史退避状态
                settings.consecutivePollFailures = 0;
                settings.lastPollErrorMessage.clear();
                settings.lastPollErrorCategory.clear();
                OutlookSettingsStore::save(settings);
            }
            const QString avatarPath =
                cfg.value(QStringLiteral("avatar/") + localClientId,
                          cfg.value(QStringLiteral("avatar/self"))).toString();
            if (avatarPath.trimmed().isEmpty()) {
                m_mainWindow->setAvatarText(localDisplayName.left(1).toUpper());
            } else {
                m_mainWindow->setAvatarImagePath(avatarPath);
            }
            m_mainWindow->reloadAppearanceSettings();
            scheduleChatUiRefresh(true, true, true, true, 0);
            panel->close();
            m_mainWindow->setStatusMessage(QStringLiteral("个人资料已更新"), 2000);
            // 延迟执行可能涉及网络的操作，避免阻塞 UI
            QTimer::singleShot(300, m_mainWindow.get(), [&]() {
                if (refreshAzureDevOpsBuildNotificationTimer) {
                    refreshAzureDevOpsBuildNotificationTimer();
                }
                if (refreshOutlookNotificationTimer) {
                    refreshOutlookNotificationTimer();
                }
                const OutlookConnectionSettings outlookCfg = OutlookSettingsStore::load();
                if (outlookCfg.hasNotificationConfiguration() && restartOutlookStreaming) {
                    restartOutlookStreaming(outlookCfg);
                } else if (stopOutlookStreaming) {
                    stopOutlookStreaming();
                }
                refreshLocalPresence(true);
            });
        });

        auto* runtimeDetailsBtn = new QPushButton(QStringLiteral("查看并维护架构"), tab4Body);
        runtimeDetailsBtn->setCursor(Qt::PointingHandCursor);
        runtimeDetailsBtn->setStyleSheet(QStringLiteral(
            "QPushButton { color:%1; border:none; background:transparent; font-size:13px; }"
            "QPushButton:hover { text-decoration:underline; color:%2; }")
            .arg(AppStyle::textSecondary(), AppStyle::accent()));
        auto* exportDiagnosticsBtn = new QPushButton(QStringLiteral("导出诊断包"), tab4Body);
        exportDiagnosticsBtn->setCursor(Qt::PointingHandCursor);
        exportDiagnosticsBtn->setStyleSheet(QStringLiteral(
            "QPushButton { color:%1; border:none; background:transparent; font-size:13px; }"
            "QPushButton:hover { text-decoration:underline; color:%2; }")
            .arg(AppStyle::textSecondary(), AppStyle::accent()));
        auto* tab4LinkRow = new QHBoxLayout;
        tab4LinkRow->setContentsMargins(0, 4, 0, 0);
        tab4LinkRow->setSpacing(12);
        tab4LinkRow->addWidget(runtimeDetailsBtn);
        tab4LinkRow->addWidget(exportDiagnosticsBtn);
        tab4LinkRow->addStretch();
        tab4Layout->addLayout(tab4LinkRow);
        tab4Layout->addStretch();

        QObject::connect(uploadAvatarBtn, &QPushButton::clicked, panel,
                         [this, safePanel = QPointer<QDialog>(panel), localClientId, &refreshLocalPresence]() {
            const QString filePath = QFileDialog::getOpenFileName(
                m_mainWindow.get(),
                QStringLiteral("选择头像图片"),
                QString(),
                QStringLiteral("图片 (*.png *.jpg *.jpeg *.webp *.bmp)"));
            if (filePath.isEmpty()) return;
            const QString avatarRoot = avatarDirectoryPath();
            QDir().mkpath(avatarRoot);
            const QString suffix = QFileInfo(filePath).suffix().trimmed().toLower();
            const QString storedAvatarPath =
                QDir(avatarRoot).filePath(QStringLiteral("self-avatar.%1")
                                              .arg(suffix.isEmpty() ? QStringLiteral("png") : suffix));
            QFile::remove(storedAvatarPath);
            if (!QFile::copy(filePath, storedAvatarPath)) {
                m_mainWindow->setStatusMessage(QStringLiteral("头像图片保存失败"), 2500);
                return;
            }
            QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
            cfg.setValue(QStringLiteral("avatar/self"), storedAvatarPath);
            cfg.setValue(QStringLiteral("avatar/") + localClientId, storedAvatarPath);
            cfg.sync();
            invalidateAvatarCache(localClientId);
            invalidateAvatarImageCache();
            m_mainWindow->setAvatarImagePath(storedAvatarPath);
            refreshLocalPresence(true);
            if (safePanel) {
                safePanel->close();
            }
            m_mainWindow->setStatusMessage(QStringLiteral("头像已更新"), 2000);
        });
        QObject::connect(exportDiagnosticsBtn, &QPushButton::clicked, panel, [this, exportDiagnosticsBtn]() {
            const QString targetDir = QFileDialog::getExistingDirectory(
                m_mainWindow.get(),
                QStringLiteral("选择诊断包导出目录"));
            if (targetDir.trimmed().isEmpty()) {
                return;
            }

            // 导出操作（VACUUM INTO + 递归文件拷贝）可能耗时数秒，放到后台线程避免 UI 卡死
            exportDiagnosticsBtn->setEnabled(false);
            exportDiagnosticsBtn->setText(QStringLiteral("导出中…"));
            m_mainWindow->setStatusMessage(QStringLiteral("正在导出诊断包，请稍候…"), 0);

            const Diagnostics::BundleSourcePaths sourcePaths = Diagnostics::defaultSourcePaths();
            (void)QtConcurrent::run([this, targetDir, sourcePaths, exportDiagnosticsBtn]() {
                QString exportedDir;
                QString errorMessage;
                const bool ok = Diagnostics::exportBundle(sourcePaths,
                                                          targetDir,
                                                          &exportedDir,
                                                          &errorMessage);
                QMetaObject::invokeMethod(m_mainWindow.get(), [this, ok, exportedDir, errorMessage, exportDiagnosticsBtn]() {
                    exportDiagnosticsBtn->setEnabled(true);
                    exportDiagnosticsBtn->setText(QStringLiteral("导出诊断包"));
                    if (!ok) {
                        m_mainWindow->setStatusMessage(QString(), 0);
                        LeyoDialog::warning(m_mainWindow.get(),
                                             QStringLiteral("导出诊断包"),
                                             errorMessage.isEmpty()
                                                 ? QStringLiteral("诊断包导出失败")
                                                 : errorMessage);
                        return;
                    }
                    m_mainWindow->setStatusMessage(QStringLiteral("诊断包已导出"), 2500);
                    QDesktopServices::openUrl(QUrl::fromLocalFile(exportedDir));
                });
            });
        });
        QObject::connect(runtimeDetailsBtn, &QPushButton::clicked, panel,
                         [this,
                          panel,
                          &stage2RegistryRepository,
                          &stage2BindingRepository,
                         &stage2ResourceRepository,
                         &refreshRuntimeArchitectureState]() {
            RuntimeArchitectureDialog dlg(panel);
            dlg.setWindowModality(Qt::WindowModal);
            if (m_runtimeArchitectureSnapshot) {
                dlg.setSnapshot(*m_runtimeArchitectureSnapshot);
            }

            if (dlg.exec() != QDialog::Accepted) {
                return;
            }

            const ServiceSelectionSnapshot selection = dlg.editedSelection();
            const qint64 observedAtMs = QDateTime::currentMSecsSinceEpoch();
            if (!stage2RegistryRepository.replaceRegistry(dlg.editedServiceRegistry(),
                                                          selection.serviceId,
                                                          observedAtMs)
                || !stage2BindingRepository.replaceWorkspaceBindings(dlg.editedWorkspaceBindings())
                || !stage2BindingRepository.replaceGroupBindings(dlg.editedGroupBindings())
                || !stage2ResourceRepository.replaceResources(dlg.editedResources())
                || !stage2BindingRepository.saveCurrentSelection(selection)) {
                LeyoDialog::warning(panel,
                                     QStringLiteral("混合架构状态"),
                                     QStringLiteral("保存本地混合架构状态失败"));
                return;
            }

            refreshRuntimeArchitectureState();
            m_mainWindow->setStatusMessage(QStringLiteral("混合架构状态已保存"), 2500);
        });
        auto* footerLayout = new QHBoxLayout(footerWidget);
        footerLayout->setContentsMargins(16, 10, 16, 10);
        footerLayout->setSpacing(10);
        footerLayout->addStretch();
        footerLayout->addWidget(cancelBtn);
        footerLayout->addWidget(saveBtn);

        // 瀹氫綅鍒板ご鍍忔寜閽梺杈?
        const QScreen* screen = m_mainWindow->screen() ? m_mainWindow->screen()
                                                       : QGuiApplication::primaryScreen();
        const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
        const int panelWidth = qMin(780, qMax(400, available.width() - 40));
        const int panelHeight = qMin(640, qMax(420, available.height() - 40));
        panel->resize(panelWidth, panelHeight);
        QPoint targetPos(available.center().x() - panel->width() / 2,
                         available.center().y() - panel->height() / 2);
        targetPos.setX(qBound(available.left(), targetPos.x(), available.right() - panel->width()));
        targetPos.setY(qBound(available.top(), targetPos.y(), available.bottom() - panel->height()));
        panel->move(targetPos);
        panel->show();
        panel->activateWindow();
    });

    // 数据导出：将数据库文件复制到用户选择的目录
    QObject::connect(m_mainWindow.get(), &MainWindow::dataExportRequested, m_mainWindow.get(), [&]() {
        const QString dbPath = databasePath();
        if (!QFile::exists(dbPath)) {
            m_mainWindow->setStatusMessage(QStringLiteral("\u6570\u636E\u5E93\u6587\u4EF6\u4E0D\u5B58\u5728"), 3000);
            return;
        }
        const QString savePath = QFileDialog::getSaveFileName(
            m_mainWindow.get(),
            QStringLiteral("\u5BFC\u51FA\u804A\u5929\u8BB0\u5F55"),
            QDir::homePath() + QStringLiteral("/leyochat-backup.db"),
            QStringLiteral("\u6570\u636E\u5E93\u6587\u4EF6 (*.db)"));
        if (savePath.isEmpty()) return;
        if (QFile::exists(savePath)) QFile::remove(savePath);
        if (QFile::copy(dbPath, savePath)) {
            m_mainWindow->setStatusMessage(
                QStringLiteral("\u804A\u5929\u8BB0\u5F55\u5DF2\u5BFC\u51FA\u5230 %1").arg(savePath), 5000);
        } else {
            m_mainWindow->setStatusMessage(QStringLiteral("\u5BFC\u51FA\u5931\u8D25\uFF0C\u8BF7\u68C0\u67E5\u78C1\u76D8\u7A7A\u95F4"), 3000);
        }
    });

    // 数据导入：从用户选择的文件恢复数据库（需重启）
    QObject::connect(m_mainWindow.get(), &MainWindow::dataImportRequested, m_mainWindow.get(), [&]() {
        const QString importPath = QFileDialog::getOpenFileName(
            m_mainWindow.get(),
            QStringLiteral("\u5BFC\u5165\u804A\u5929\u8BB0\u5F55"),
            QDir::homePath(),
            QStringLiteral("\u6570\u636E\u5E93\u6587\u4EF6 (*.db)"));
        if (importPath.isEmpty()) return;
        if (!LeyoDialog::question(
            m_mainWindow.get(),
            QStringLiteral("\u5BFC\u5165\u786E\u8BA4"),
            QStringLiteral("\u5BFC\u5165\u5C06\u8986\u76D6\u5F53\u524D\u804A\u5929\u8BB0\u5F55\uFF0C\u5EFA\u8BAE\u5148\u5BFC\u51FA\u5907\u4EFD\u3002\n\u5BFC\u5165\u5B8C\u6210\u540E\u7A0B\u5E8F\u5C06\u81EA\u52A8\u91CD\u542F\u3002\n\n\u786E\u8BA4\u5BFC\u5165\uFF1F"))) return;
        const QString dbPath = databasePath();
        const QString backupPath = dbPath + QStringLiteral(".bak");
        if (QFile::exists(backupPath)) QFile::remove(backupPath);
        QFile::rename(dbPath, backupPath);
        if (QFile::copy(importPath, dbPath)) {
            m_mainWindow->setStatusMessage(QStringLiteral("\u5BFC\u5165\u6210\u529F\uFF0C\u6B63\u5728\u91CD\u542F..."), 2000);
            QTimer::singleShot(1500, []() {
                QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments());
                QCoreApplication::quit();
            });
        } else {
            // 恢复备份
            if (QFile::exists(backupPath)) {
                QFile::rename(backupPath, dbPath);
            }
            m_mainWindow->setStatusMessage(QStringLiteral("\u5BFC\u5165\u5931\u8D25"), 3000);
        }
    });

    // ── SettingsPage 信号连接 ──────────────────────────────────────────
    if (auto* sp = m_mainWindow->settingsPage()) {
        // 保存设置：更新应用状态并刷新 UI
        QObject::connect(sp, &SettingsPage::settingsSaved, m_mainWindow.get(),
                         [&](const Profile& savedProfile, const ClientPreferences& savedPrefs, bool wasInitialSetup) {
            Q_UNUSED(savedPrefs);
            localDisplayName = QString::fromStdWString(savedProfile.displayName);
            *profile = savedProfile;
            identityService.saveProfile(*profile);
            m_mainWindow->setLocalDisplayName(localDisplayName);

            // 更新升级服务器和自动更新
            {
                QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
                const QString updateServer = cfg.value(QStringLiteral("update/serverPath")).toString();
                if (m_updateChecker && !updateServer.isEmpty()) {
                    m_updateChecker->setUpdateSourcePath(updateServer);
                }
                const bool autoEnabled = cfg.value(QStringLiteral("update/autoCheckEnabled"), true).toBool();
                const int intervalMin = cfg.value(QStringLiteral("update/checkIntervalMinutes"), 60).toInt();
                if (m_updateChecker) {
                    if (autoEnabled) {
                        m_updateChecker->start(intervalMin * 60000);
                    } else {
                        m_updateChecker->stop();
                        if (m_mainWindow && m_mainWindow->updateBar()) {
                            m_mainWindow->updateBar()->hideBar();
                            m_pendingUpdateInfo = {};
                        }
                    }
                }
            }

            // 更新知识服务
            if (sp->knowledgeServiceWidget()) {
                const QVector<KnowledgeServiceConfig> savedKnowledgeServices = sp->knowledgeServiceWidget()->configs();
                QString preferredServiceId;
                for (const KnowledgeServiceConfig& config : savedKnowledgeServices) {
                    if (config.isDefault) {
                        preferredServiceId = config.localId;
                        break;
                    }
                }
                m_mainWindow->setAiKnowledgeServices(savedKnowledgeServices, preferredServiceId);
            }

            // 更新头像
            {
                QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
                const QString avatarPath =
                    cfg.value(QStringLiteral("avatar/") + localClientId,
                              cfg.value(QStringLiteral("avatar/self"))).toString();
                if (avatarPath.trimmed().isEmpty()) {
                    m_mainWindow->setAvatarText(localDisplayName.left(1).toUpper());
                    sp->setAvatarPixmap(QPixmap());
                } else {
                    m_mainWindow->setAvatarImagePath(avatarPath);
                    QPixmap px(avatarPath);
                    if (!px.isNull()) sp->setAvatarPixmap(px);
                }
            }

            // 更新截图快捷键
            {
                QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
                const QString hotkeyStr = cfg.value(QStringLiteral("screenshotHotkey"),
                                                    QStringLiteral("Ctrl+Alt+A")).toString();
                const QKeySequence newHotkey(hotkeyStr);
                if (globalHotkeyManager) {
                    if (newHotkey.isEmpty()) {
                        globalHotkeyManager->unregisterHotkey();
                    } else if (!globalHotkeyManager->registerHotkey(newHotkey)) {
                        m_mainWindow->showChatToast(
                            QStringLiteral("截图快捷键 %1 注册失败，可能已被其他程序占用。").arg(hotkeyStr), 3000);
                    }
                }
            }

            m_mainWindow->reloadAppearanceSettings();
            scheduleChatUiRefresh(true, true, true, true, 0);
            m_mainWindow->setStatusMessage(
                wasInitialSetup ? QStringLiteral("\u9996\u6B21\u8BBE\u7F6E\u5DF2\u5B8C\u6210")
                                : QStringLiteral("\u8BBE\u7F6E\u5DF2\u4FDD\u5B58"), 2000);

            // 延迟刷新通知服务和在线状态
            QTimer::singleShot(300, m_mainWindow.get(), [&]() {
                if (refreshAzureDevOpsBuildNotificationTimer) {
                    refreshAzureDevOpsBuildNotificationTimer();
                }
                if (refreshOutlookNotificationTimer) {
                    refreshOutlookNotificationTimer();
                }
                const OutlookConnectionSettings outlookCfg = OutlookSettingsStore::load();
                if (outlookCfg.hasNotificationConfiguration() && restartOutlookStreaming) {
                    restartOutlookStreaming(outlookCfg);
                } else if (stopOutlookStreaming) {
                    stopOutlookStreaming();
                }
                refreshLocalPresence(true);
            });
        });

        // 检查更新
        QObject::connect(sp, &SettingsPage::checkUpdateRequested, m_mainWindow.get(), [&, sp]() {
            if (!m_updateChecker) return;
            sp->setCheckUpdateButtonEnabled(false);
            sp->setCheckUpdateStatus(QStringLiteral("\u23F3 \u6B63\u5728\u68C0\u67E5..."), QString());

            auto* conn = new QMetaObject::Connection();
            auto* connFail = new QMetaObject::Connection();
            auto* connNone = new QMetaObject::Connection();

            *conn = QObject::connect(m_updateChecker, &UpdateChecker::updateAvailable,
                sp, [this, sp, conn, connFail, connNone](const UpdateChecker::UpdateInfo& info) {
                    sp->setCheckUpdateButtonEnabled(true);
                    sp->setCheckUpdateStatus(
                        QStringLiteral("\U0001F514 \u53D1\u73B0\u65B0\u7248\u672C v%1\uFF01").arg(info.version),
                        QStringLiteral("#165DFF"));
                    QObject::disconnect(*conn); QObject::disconnect(*connFail); QObject::disconnect(*connNone);
                    delete conn; delete connFail; delete connNone;
                });
            *connNone = QObject::connect(m_updateChecker, &UpdateChecker::noUpdateAvailable,
                sp, [sp, conn, connFail, connNone]() {
                    sp->setCheckUpdateButtonEnabled(true);
                    sp->setCheckUpdateStatus(QStringLiteral("\u2705 \u5DF2\u662F\u6700\u65B0\u7248\u672C"), QStringLiteral("green"));
                    QObject::disconnect(*conn); QObject::disconnect(*connFail); QObject::disconnect(*connNone);
                    delete conn; delete connFail; delete connNone;
                });
            *connFail = QObject::connect(m_updateChecker, &UpdateChecker::checkFailed,
                sp, [sp, conn, connFail, connNone](const QString&) {
                    sp->setCheckUpdateButtonEnabled(true);
                    sp->setCheckUpdateStatus(QStringLiteral("\u26A0\uFE0F \u68C0\u67E5\u5931\u8D25"), QStringLiteral("orange"));
                    QObject::disconnect(*conn); QObject::disconnect(*connFail); QObject::disconnect(*connNone);
                    delete conn; delete connFail; delete connNone;
                });
            m_updateChecker->checkNow();
        });

        // 关于对话框
        QObject::connect(sp, &SettingsPage::showAboutDialogRequested, m_mainWindow.get(), [&]() {
            showAboutDialog();
        });

        // 本版更新
        QObject::connect(sp, &SettingsPage::showReleaseNotesRequested, m_mainWindow.get(), [&, appDisplayName]() {
            showCurrentReleaseNotes(
                QStringLiteral("%1 \u672C\u7248\u66F4\u65B0").arg(appDisplayName),
                QStringLiteral("\u5F53\u524D\u7248\u672C %1").arg(ApplicationInfo::currentVersion()));
        });

        // 查看混合架构
        QObject::connect(sp, &SettingsPage::showRuntimeArchitectureRequested, m_mainWindow.get(),
                         [this, &stage2RegistryRepository, &stage2BindingRepository,
                          &stage2ResourceRepository, &refreshRuntimeArchitectureState]() {
            RuntimeArchitectureDialog dlg(m_mainWindow.get());
            dlg.setWindowModality(Qt::WindowModal);
            if (m_runtimeArchitectureSnapshot) {
                dlg.setSnapshot(*m_runtimeArchitectureSnapshot);
            }
            if (dlg.exec() != QDialog::Accepted) return;
            const ServiceSelectionSnapshot selection = dlg.editedSelection();
            const qint64 observedAtMs = QDateTime::currentMSecsSinceEpoch();
            if (!stage2RegistryRepository.replaceRegistry(dlg.editedServiceRegistry(),
                                                          selection.serviceId, observedAtMs)
                || !stage2BindingRepository.replaceWorkspaceBindings(dlg.editedWorkspaceBindings())
                || !stage2BindingRepository.replaceGroupBindings(dlg.editedGroupBindings())
                || !stage2ResourceRepository.replaceResources(dlg.editedResources())
                || !stage2BindingRepository.saveCurrentSelection(selection)) {
                LeyoDialog::warning(m_mainWindow.get(),
                                     QStringLiteral("\u6DF7\u5408\u67B6\u6784\u72B6\u6001"),
                                     QStringLiteral("\u4FDD\u5B58\u672C\u5730\u6DF7\u5408\u67B6\u6784\u72B6\u6001\u5931\u8D25"));
                return;
            }
            refreshRuntimeArchitectureState();
        });

        // 导出诊断包
        QObject::connect(sp, &SettingsPage::exportDiagnosticsRequested, m_mainWindow.get(), [this]() {
            const QString targetDir = QFileDialog::getExistingDirectory(
                m_mainWindow.get(), QStringLiteral("\u9009\u62E9\u8BCA\u65AD\u5305\u5BFC\u51FA\u76EE\u5F55"));
            if (targetDir.trimmed().isEmpty()) return;
            m_mainWindow->setStatusMessage(QStringLiteral("\u6B63\u5728\u5BFC\u51FA\u8BCA\u65AD\u5305\uFF0C\u8BF7\u7A0D\u5019\u2026"), 0);
            const Diagnostics::BundleSourcePaths sourcePaths = Diagnostics::defaultSourcePaths();
            (void)QtConcurrent::run([this, targetDir, sourcePaths]() {
                QString exportedDir, errorMessage;
                const bool ok = Diagnostics::exportBundle(sourcePaths, targetDir, &exportedDir, &errorMessage);
                QMetaObject::invokeMethod(m_mainWindow.get(), [this, ok, exportedDir, errorMessage]() {
                    if (!ok) {
                        m_mainWindow->setStatusMessage(
                            errorMessage.isEmpty() ? QStringLiteral("\u8BCA\u65AD\u5305\u5BFC\u51FA\u5931\u8D25") : errorMessage, 3000);
                        return;
                    }
                    m_mainWindow->setStatusMessage(QStringLiteral("\u8BCA\u65AD\u5305\u5DF2\u5BFC\u51FA"), 2500);
                    QDesktopServices::openUrl(QUrl::fromLocalFile(exportedDir));
                });
            });
        });

        // Outlook 集成信号
        QObject::connect(sp, &SettingsPage::outlookTestConnectionRequested, m_mainWindow.get(), [&, sp]() {
            const OutlookConnectionSettings settings = sp->collectOutlookSettings();
            if (settings.serverUrl.isEmpty() || settings.username.isEmpty()) {
                sp->setOutlookTestStatus(QStringLiteral("\u8BF7\u5148\u586B\u5199\u670D\u52A1\u5668\u5730\u5740\u548C\u7528\u6237\u540D"));
                return;
            }
            sp->setOutlookTestStatus(QStringLiteral("\u6B63\u5728\u8FDE\u63A5..."));
            LocalOutlookAdapter adapter(settings);
            QString errorMessage;
            if (!adapter.testConnection(&errorMessage)) {
                const QString summary = OutlookSettingsStore::summarizeErrorMessage(
                    errorMessage,
                    settings.serverUrl.trimmed().isEmpty() || settings.username.trimmed().isEmpty()
                        ? QString() : QStringLiteral("network"));
                sp->setOutlookTestStatus(
                    summary.trimmed().isEmpty() ? QStringLiteral("Outlook \u8FDE\u63A5\u6D4B\u8BD5\u5931\u8D25") : summary);
                return;
            }
            const OutlookConnectionSettings updatedSettings = adapter.settings();
            sp->setOutlookAuthResult(updatedSettings.accountEmail, updatedSettings.displayName);
            sp->setOutlookTestStatus(
                QStringLiteral("\u2705 \u8FDE\u63A5\u6210\u529F\uFF1A%1").arg(updatedSettings.accountEmail));
        });

        // 存储分类管理（带联系人选择）
        QObject::connect(sp, &SettingsPage::storageCategoryManageRequested, m_mainWindow.get(),
            [this, sp](StorageCategory category, int ageIndex) {
            const int ageDays = (ageIndex == 0) ? 7 : (ageIndex == 1) ? 30 : (ageIndex == 2) ? 90 : 0;
            const QDateTime cutoff = ageDays > 0
                ? QDateTime::currentDateTime().addDays(-ageDays)
                : QDateTime();

            // ── 日志文件：无联系人概念，直接进入文件管理 ──
            if (category == StorageCategory::Logs) {
                const QString scanPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                                             .filePath(QStringLiteral("logs"));
                const QStringList filters = {QStringLiteral("*.log"), QStringLiteral("*.txt")};
                sp->setCleanupStatus(QStringLiteral("正在扫描…"));
                auto* watcher = new QFutureWatcher<QList<CleanupItem>>(m_mainWindow.get());
                QObject::connect(watcher, &QFutureWatcher<QList<CleanupItem>>::finished, m_mainWindow.get(),
                    [sp, category, watcher]() {
                    watcher->deleteLater();
                    const QList<CleanupItem> items = watcher->result();
                    if (items.isEmpty()) {
                        sp->setCleanupStatus(QStringLiteral("该分类下没有可清理的内容"));
                        return;
                    }
                    sp->setCleanupStatus(QString());
                    auto* dlg = new StorageCleanupDialog(category, items, sp->window());
                    QObject::connect(dlg, &StorageCleanupDialog::deleteRequested, sp,
                        [sp, items](StorageCategory, QList<int> indices) {
                        int deleted = 0;
                        for (int idx : indices) {
                            if (idx >= 0 && idx < items.size() && !items[idx].path.isEmpty()) {
                                if (QFile::remove(items[idx].path))
                                    ++deleted;
                            }
                        }
                        sp->setCleanupStatus(QStringLiteral("已删除 %1 项").arg(deleted));
                    });
                    dlg->setAttribute(Qt::WA_DeleteOnClose);
                    dlg->open();
                });
                watcher->setFuture(QtConcurrent::run([scanPath, filters, cutoff]() -> QList<CleanupItem> {
                    QList<CleanupItem> result;
                    QDir dir(scanPath);
                    if (!dir.exists()) return result;
                    const QFileInfoList entries = dir.entryInfoList(filters, QDir::Files, QDir::Time);
                    for (const QFileInfo& fi : entries) {
                        if (!cutoff.isNull() && fi.lastModified() > cutoff)
                            continue;
                        result.append({fi.fileName(), fi.absoluteFilePath(),
                                       fi.size(), fi.lastModified(), false});
                    }
                    return result;
                }));
                return;
            }

            // ── 聊天消息 / 接收文件 / 图片截图：先枚举联系人再管理 ──
            sp->setCleanupStatus(QStringLiteral("正在扫描联系人…"));

            auto* contactWatcher = new QFutureWatcher<QList<ContactEntry>>(m_mainWindow.get());
            QObject::connect(contactWatcher, &QFutureWatcher<QList<ContactEntry>>::finished, m_mainWindow.get(),
                [this, sp, category, ageDays, cutoff, contactWatcher]() {
                contactWatcher->deleteLater();
                QList<ContactEntry> contacts = contactWatcher->result();
                if (contacts.isEmpty()) {
                    sp->setCleanupStatus(QStringLiteral("该分类下没有可清理的内容"));
                    return;
                }
                sp->setCleanupStatus(QString());

                // 显示联系人选择对话框
                QString dlgTitle;
                switch (category) {
                case StorageCategory::Messages: dlgTitle = QStringLiteral("选择联系人 - 聊天消息"); break;
                case StorageCategory::Files:    dlgTitle = QStringLiteral("选择联系人 - 接收的文件"); break;
                case StorageCategory::Images:   dlgTitle = QStringLiteral("选择联系人 - 图片/截图"); break;
                default: return;
                }

                auto* contactDlg = new ContactSelectionDialog(dlgTitle, contacts, sp->window());
                contactDlg->setAttribute(Qt::WA_DeleteOnClose);
                QObject::connect(contactDlg, &ContactSelectionDialog::selectionConfirmed, sp,
                    [this, sp, category, ageDays, cutoff, contacts](QList<int> indices) {
                    // 收集选中的联系人 id 列表
                    QStringList selectedIds;
                    QStringList selectedNames;
                    for (int idx : indices) {
                        if (idx >= 0 && idx < contacts.size()) {
                            selectedIds.append(contacts[idx].id);
                            selectedNames.append(contacts[idx].name);
                        }
                    }
                    if (selectedIds.isEmpty()) return;

                    // ── 聊天消息：确认后从 DB 删除（异步，避免阻塞主线程） ──
                    if (category == StorageCategory::Messages) {
                        const QString rangeText = ageDays > 0
                            ? QStringLiteral("%1 天前的").arg(ageDays)
                            : QStringLiteral("所有");
                        const QString contactsText = selectedNames.size() <= 3
                            ? selectedNames.join(QStringLiteral("、"))
                            : QStringLiteral("%1 等 %2 位联系人").arg(selectedNames.first()).arg(selectedNames.size());
                        QMessageBox confirmBox(sp->window());
                        confirmBox.setWindowTitle(QStringLiteral("清理聊天消息"));
                        confirmBox.setText(QStringLiteral("请确认聊天记录已备份。\n\n将删除与「%1」的%2聊天消息，该操作不可恢复。")
                                               .arg(contactsText, rangeText));
                        confirmBox.setIcon(QMessageBox::Warning);
                        confirmBox.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
                        auto* okBtn = confirmBox.addButton(QStringLiteral("确认删除"), QMessageBox::AcceptRole);
                        confirmBox.exec();
                        if (confirmBox.clickedButton() != okBtn) return;

                        sp->setCleanupStatus(QStringLiteral("正在删除…"));
                        auto* delWatcher = new QFutureWatcher<int>(sp->window());
                        QObject::connect(delWatcher, &QFutureWatcher<int>::finished, sp,
                            [sp, delWatcher]() {
                            delWatcher->deleteLater();
                            const int totalAffected = delWatcher->result();
                            sp->setCleanupStatus(QStringLiteral("已删除 %1 条聊天消息").arg(totalAffected));
                        });
                        const QString dbPath = databasePath();
                        delWatcher->setFuture(QtConcurrent::run(
                            [dbPath, selectedIds, ageDays]() -> int {
                            const QString connName = QStringLiteral("leyochat-cleanup2-%1")
                                .arg(quintptr(QThread::currentThread()));
                            int totalAffected = 0;
                            {
                                QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
                                db.setDatabaseName(dbPath);
                                if (db.open()) {
                                    QSqlQuery pragma(db);
                                    pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
                                    for (const QString& convId : selectedIds) {
                                        QSqlQuery query(db);
                                        if (ageDays > 0) {
                                            const qint64 cutoffMs = QDateTime::currentDateTime().addDays(-ageDays).toMSecsSinceEpoch();
                                            query.prepare(QStringLiteral("DELETE FROM messages WHERE conversation_id = ? AND created_at_ms < ?"));
                                            query.addBindValue(convId);
                                            query.addBindValue(cutoffMs);
                                        } else {
                                            query.prepare(QStringLiteral("DELETE FROM messages WHERE conversation_id = ?"));
                                            query.addBindValue(convId);
                                        }
                                        if (query.exec()) {
                                            totalAffected += query.numRowsAffected();
                                        }
                                    }
                                    db.close();
                                }
                            }
                            QSqlDatabase::removeDatabase(connName);
                            return totalAffected;
                        }));
                        return;
                    }

                    // ── 接收文件 / 图片截图：扫描选中联系人的文件后进入管理对话框 ──
                    sp->setCleanupStatus(QStringLiteral("正在扫描文件…"));
                    const QString basePath = (category == StorageCategory::Files)
                        ? ensureIncomingFilesDirectory()
                        : ensureIncomingFilesDirectory();
                    const QString screenshotsPath = QDir(QStandardPaths::writableLocation(
                        QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("screenshots"));
                    const QStringList imgFilters = {QStringLiteral("*.png"), QStringLiteral("*.jpg"),
                        QStringLiteral("*.jpeg"), QStringLiteral("*.gif"), QStringLiteral("*.bmp"),
                        QStringLiteral("*.webp")};

                    auto* fileWatcher = new QFutureWatcher<QList<CleanupItem>>(sp->window());
                    QObject::connect(fileWatcher, &QFutureWatcher<QList<CleanupItem>>::finished, sp,
                        [sp, category, fileWatcher]() {
                        fileWatcher->deleteLater();
                        const QList<CleanupItem> items = fileWatcher->result();
                        if (items.isEmpty()) {
                            sp->setCleanupStatus(QStringLiteral("选中的联系人下没有可清理的文件"));
                            return;
                        }
                        sp->setCleanupStatus(QString());
                        auto* dlg = new StorageCleanupDialog(category, items, sp->window());
                        QObject::connect(dlg, &StorageCleanupDialog::deleteRequested, sp,
                            [sp, items](StorageCategory, QList<int> indices) {
                            int deleted = 0;
                            for (int idx : indices) {
                                if (idx >= 0 && idx < items.size() && !items[idx].path.isEmpty()) {
                                    if (QFile::remove(items[idx].path))
                                        ++deleted;
                                }
                            }
                            sp->setCleanupStatus(QStringLiteral("已删除 %1 项").arg(deleted));
                        });
                        dlg->setAttribute(Qt::WA_DeleteOnClose);
                        dlg->open();
                    });

                    fileWatcher->setFuture(QtConcurrent::run(
                        [selectedIds, basePath, screenshotsPath, imgFilters, cutoff, category]() -> QList<CleanupItem> {
                        QList<CleanupItem> result;
                        for (const QString& contactId : selectedIds) {
                            QString dirPath;
                            QStringList filters;
                            if (contactId == QStringLiteral("__screenshots__")) {
                                // 特殊项："我的截图"
                                dirPath = screenshotsPath;
                                filters = imgFilters;
                            } else if (category == StorageCategory::Images) {
                                // 图片模式：扫描联系人目录下的图片文件
                                dirPath = QDir(basePath).filePath(contactId);
                                filters = imgFilters;
                            } else {
                                // 文件模式：扫描联系人目录下的所有文件
                                dirPath = QDir(basePath).filePath(contactId);
                            }
                            QDir dir(dirPath);
                            if (!dir.exists()) continue;
                            const QFileInfoList entries = filters.isEmpty()
                                ? dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time)
                                : dir.entryInfoList(filters, QDir::Files, QDir::Time);
                            for (const QFileInfo& fi : entries) {
                                if (!cutoff.isNull() && fi.lastModified() > cutoff)
                                    continue;
                                result.append({fi.fileName(), fi.absoluteFilePath(),
                                               fi.size(), fi.lastModified(), false});
                            }
                        }
                        std::sort(result.begin(), result.end(), [](const CleanupItem& a, const CleanupItem& b) {
                            return a.lastModified < b.lastModified;
                        });
                        return result;
                    }));
                });
                contactDlg->open();
            });

            // 异步枚举联系人列表
            const QString receivedPath = ensureIncomingFilesDirectory();
            const QString screenshotsDir = QDir(QStandardPaths::writableLocation(
                QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("screenshots"));
            const QStringList imageFilters = {QStringLiteral("*.png"), QStringLiteral("*.jpg"),
                QStringLiteral("*.jpeg"), QStringLiteral("*.gif"), QStringLiteral("*.bmp"),
                QStringLiteral("*.webp")};

            // Messages 分类：异步查询 DB（避免主线程阻塞）
            if (category == StorageCategory::Messages) {
                auto* msgWatcher = new QFutureWatcher<QList<ContactEntry>>(m_mainWindow.get());
                QObject::connect(msgWatcher, &QFutureWatcher<QList<ContactEntry>>::finished, m_mainWindow.get(),
                    [this, sp, category, ageDays, msgWatcher, contactWatcher]() {
                    msgWatcher->deleteLater();
                    contactWatcher->deleteLater(); // 未使用的 contactWatcher
                    QList<ContactEntry> contacts = msgWatcher->result();
                    if (contacts.isEmpty()) {
                        sp->setCleanupStatus(QStringLiteral("该分类下没有可清理的内容"));
                        return;
                    }
                    sp->setCleanupStatus(QString());
                    QString dlgTitle = QStringLiteral("选择联系人 - 聊天消息");
                    auto* contactDlg = new ContactSelectionDialog(dlgTitle, contacts, sp->window());
                    contactDlg->setAttribute(Qt::WA_DeleteOnClose);
                    QObject::connect(contactDlg, &ContactSelectionDialog::selectionConfirmed, sp,
                        [this, sp, ageDays, contacts](QList<int> indices) {
                        QStringList selectedIds;
                        QStringList selectedNames;
                        for (int idx : indices) {
                            if (idx >= 0 && idx < contacts.size()) {
                                selectedIds.append(contacts[idx].id);
                                selectedNames.append(contacts[idx].name);
                            }
                        }
                        if (selectedIds.isEmpty()) return;

                        const QString rangeText = ageDays > 0
                            ? QStringLiteral("%1 天前的").arg(ageDays)
                            : QStringLiteral("所有");
                        const QString contactsText = selectedNames.size() <= 3
                            ? selectedNames.join(QStringLiteral("、"))
                            : QStringLiteral("%1 等 %2 位联系人").arg(selectedNames.first()).arg(selectedNames.size());
                        QMessageBox confirmBox(sp->window());
                        confirmBox.setWindowTitle(QStringLiteral("清理聊天消息"));
                        confirmBox.setText(QStringLiteral("请确认聊天记录已备份。\n\n将删除与「%1」的%2聊天消息，该操作不可恢复。")
                                               .arg(contactsText, rangeText));
                        confirmBox.setIcon(QMessageBox::Warning);
                        confirmBox.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
                        auto* okBtn = confirmBox.addButton(QStringLiteral("确认删除"), QMessageBox::AcceptRole);
                        confirmBox.exec();
                        if (confirmBox.clickedButton() != okBtn) return;

                        // 异步执行 DELETE 操作，避免阻塞主线程
                        sp->setCleanupStatus(QStringLiteral("正在删除…"));
                        auto* delWatcher = new QFutureWatcher<int>(sp->window());
                        QObject::connect(delWatcher, &QFutureWatcher<int>::finished, sp,
                            [sp, delWatcher]() {
                            delWatcher->deleteLater();
                            const int totalAffected = delWatcher->result();
                            sp->setCleanupStatus(QStringLiteral("已删除 %1 条聊天消息").arg(totalAffected));
                        });
                        const QString dbPath = databasePath();
                        delWatcher->setFuture(QtConcurrent::run(
                            [dbPath, selectedIds, ageDays]() -> int {
                            const QString connName = QStringLiteral("leyochat-cleanup-%1")
                                .arg(quintptr(QThread::currentThread()));
                            int totalAffected = 0;
                            {
                                QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
                                db.setDatabaseName(dbPath);
                                if (db.open()) {
                                    QSqlQuery pragma(db);
                                    pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
                                    for (const QString& convId : selectedIds) {
                                        QSqlQuery query(db);
                                        if (ageDays > 0) {
                                            const qint64 cutoffMs = QDateTime::currentDateTime().addDays(-ageDays).toMSecsSinceEpoch();
                                            query.prepare(QStringLiteral("DELETE FROM messages WHERE conversation_id = ? AND created_at_ms < ?"));
                                            query.addBindValue(convId);
                                            query.addBindValue(cutoffMs);
                                        } else {
                                            query.prepare(QStringLiteral("DELETE FROM messages WHERE conversation_id = ?"));
                                            query.addBindValue(convId);
                                        }
                                        if (query.exec()) {
                                            totalAffected += query.numRowsAffected();
                                        }
                                    }
                                    db.close();
                                }
                            }
                            QSqlDatabase::removeDatabase(connName);
                            return totalAffected;
                        }));
                    });
                    contactDlg->open();
                });
                const QString dbPath = databasePath();
                msgWatcher->setFuture(QtConcurrent::run([dbPath]() -> QList<ContactEntry> {
                    QList<ContactEntry> contacts;
                    const QString connName = QStringLiteral("leyochat-enum-%1")
                        .arg(quintptr(QThread::currentThread()));
                    {
                        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
                        db.setDatabaseName(dbPath);
                        if (db.open()) {
                            QSqlQuery pragma(db);
                            pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
                            QSqlQuery query(db);
                            query.exec(QStringLiteral(
                                "SELECT c.conversation_id, "
                                "  CASE WHEN kp.display_name IS NOT NULL AND kp.display_name != '' "
                                "       THEN kp.display_name "
                                "       ELSE c.title END as display_name, "
                                "  COUNT(m.message_id) as cnt, "
                                "  SUM(LENGTH(m.body)) as total_bytes "
                                "FROM conversations c "
                                "INNER JOIN messages m ON c.conversation_id = m.conversation_id "
                                "LEFT JOIN known_peers kp ON c.title = kp.client_id "
                                "GROUP BY c.conversation_id "
                                "ORDER BY cnt DESC"));
                            while (query.next()) {
                                ContactEntry entry;
                                entry.id = query.value(0).toString();
                                entry.name = query.value(1).toString();
                                entry.itemCount = query.value(2).toInt();
                                entry.sizeBytes = query.value(3).toLongLong();
                                contacts.append(entry);
                            }
                            db.close();
                        }
                    }
                    QSqlDatabase::removeDatabase(connName);
                    return contacts;
                }));
                return;
            }

            // Files / Images：异步枚举文件系统（安全跨线程）
            contactWatcher->setFuture(QtConcurrent::run(
                [category, cutoff, receivedPath, screenshotsDir, imageFilters]() -> QList<ContactEntry> {
                QList<ContactEntry> contacts;
                // Files / Images：枚举接收目录下的子文件夹
                QDir recvDir(receivedPath);
                if (recvDir.exists()) {
                    const QStringList subdirs = recvDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                    for (const QString& subdir : subdirs) {
                        if (subdir.startsWith(QLatin1Char('.'))) continue;
                        const QString subdirPath = recvDir.filePath(subdir);
                        QDir senderDir(subdirPath);
                        QFileInfoList files;
                        if (category == StorageCategory::Images) {
                            files = senderDir.entryInfoList(imageFilters, QDir::Files);
                        } else {
                            files = senderDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
                        }
                        // 按日期过滤计数
                        int count = 0;
                        qint64 totalSize = 0;
                        for (const QFileInfo& fi : files) {
                            if (!cutoff.isNull() && fi.lastModified() > cutoff)
                                continue;
                            ++count;
                            totalSize += fi.size();
                        }
                        if (count > 0) {
                            contacts.append({subdir, subdir, totalSize, count, false});
                        }
                    }
                }
                // 图片分类额外添加"我的截图"
                if (category == StorageCategory::Images) {
                    QDir ssDir(screenshotsDir);
                    if (ssDir.exists()) {
                        const QFileInfoList ssFiles = ssDir.entryInfoList(imageFilters, QDir::Files);
                        int count = 0;
                        qint64 totalSize = 0;
                        for (const QFileInfo& fi : ssFiles) {
                            if (!cutoff.isNull() && fi.lastModified() > cutoff)
                                continue;
                            ++count;
                            totalSize += fi.size();
                        }
                        if (count > 0) {
                            contacts.append({QStringLiteral("__screenshots__"),
                                             QStringLiteral("我的截图"), totalSize, count, false});
                        }
                    }
                }
                return contacts;
            }));
        });
    }
    // ── SettingsPage 信号连接完毕 ──────────────────────────────────────

    // 将真实 profile 传给 SettingsPage（构造时传入的是空 Profile）
    if (auto* sp = m_mainWindow->settingsPage()) {
        sp->setProfile(*profile);
        // 同步头像到设置页
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        const QString avatarPath =
            cfg.value(QStringLiteral("avatar/") + localClientId,
                      cfg.value(QStringLiteral("avatar/self"))).toString().trimmed();
        if (!avatarPath.isEmpty()) {
            QPixmap px(avatarPath);
            if (!px.isNull()) sp->setAvatarPixmap(px);
        }
    }

    constexpr int kMessageJumpRetryIntervalMs = 100;
    constexpr int kMessageJumpMaxAttempts = 25;
    const auto scrollToMessageWhenAvailable =
        [&](const QString& conversationId, const QString& messageId) {
            const QString targetConversationId = conversationId.trimmed();
            const QString targetMessageId = messageId.trimmed();
            if (targetConversationId.isEmpty() || targetMessageId.isEmpty()) {
                return;
            }

            auto attempts = std::make_shared<int>(0);
            auto retry = std::make_shared<std::function<void()>>();
            *retry = [&, targetConversationId, targetMessageId, attempts, retry]() {
                if (currentConversationId != targetConversationId) {
                    return;
                }

                const int row = messageModel.findRowByMessageId(targetMessageId);
                if (row >= 0) {
                    const QModelIndex idx = messageModel.index(row);
                    if (auto* msgList = m_mainWindow->findChild<QListView*>(QStringLiteral("messageListView"))) {
                        msgList->scrollTo(idx, QAbstractItemView::PositionAtCenter);
                        msgList->setCurrentIndex(idx);
                    }
                    return;
                }

                ++(*attempts);
                if (*attempts < kMessageJumpMaxAttempts) {
                    QTimer::singleShot(kMessageJumpRetryIntervalMs,
                                       m_mainWindow.get(),
                                       [retry]() { (*retry)(); });
                    return;
                }

                m_mainWindow->setStatusMessage(QStringLiteral("已打开会话，但未定位到原消息"), 3000);
            };

            QTimer::singleShot(kMessageJumpRetryIntervalMs,
                               m_mainWindow.get(),
                               [retry]() { (*retry)(); });
        };

    // 搜索结果点击跳转（来自 GlobalSearchPanel）
    QObject::connect(m_mainWindow.get(), &MainWindow::searchResultJumpRequested, m_mainWindow.get(),
                     [&](const QString& conversationId, const QString& messageId) {
                          emit m_mainWindow->conversationSelected(conversationId);
                          scrollToMessageWhenAvailable(conversationId, messageId);
                      });

    // 灞€鍩熺綉鑷姩鍙戠幇 鈫?鑷姩杩炴帴
    QObject::connect(m_mainWindow.get(), &MainWindow::globalSearchRequested, m_mainWindow.get(),
                     [&](const QString& keyword, int /*tab*/) {
                         if (keyword.trimmed().isEmpty()) return;
                         const QString needle = keyword.trimmed();

                         QVector<GlobalSearchPanel::ContactResult> contacts;
                         QVector<GlobalSearchPanel::GroupResult> groups;
                         QVector<GlobalSearchPanel::MessageResult> messages;
                         QVector<GlobalSearchPanel::FileResult> files;
                         QVector<GlobalSearchPanel::DepartmentResult> departments;

                         // ── 搜索联系人（复用 peers 快照供部门搜索）──
                         const auto allPeers = peerDirectoryService.visiblePeers(toUtf8(localClientId));
                         for (const auto& peer : allPeers) {
                             const QString dn = QString::fromStdString(peer.displayName);
                             const QString cid = QString::fromStdString(peer.clientId);
                             if (dn.contains(needle, Qt::CaseInsensitive)
                                 || cid.contains(needle, Qt::CaseInsensitive)
                                 || PinyinHelper::matchesPinyin(dn, needle)) {
                                 GlobalSearchPanel::ContactResult cr;
                                 cr.clientId = cid;
                                 cr.displayName = dn.isEmpty() ? cid : dn;
                                 cr.isOnline =
                                     PeerPresenceEvaluator::isOnlineOrAway(
                                         peer,
                                         QDateTime::currentMSecsSinceEpoch());
                                 contacts.append(cr);
                             }
                             if (contacts.size() >= 10) break;
                         }

                         // ── 搜索群组（用 countMembers 代替 loadMembers 全量加载）──
                         {
                             const auto allGroups = groupRepository.loadGroupsForMember(
                                 localClientId.toStdWString());
                             for (const auto& g : allGroups) {
                                 const QString gn = QString::fromStdWString(g.groupName);
                                 const QString gid = QString::fromStdWString(g.groupId);
                                 if (gn.contains(needle, Qt::CaseInsensitive)
                                     || gid.contains(needle, Qt::CaseInsensitive)
                                     || PinyinHelper::matchesPinyin(gn, needle)) {
                                     GlobalSearchPanel::GroupResult gr;
                                     gr.groupId = gid;
                                     gr.groupName = gn;
                                     gr.memberCount = groupRepository.countMembers(g.groupId);
                                     groups.append(gr);
                                 }
                                 if (groups.size() >= 10) break;
                             }
                         }

                         // ── 搜索聊天记录 ──
                         {
                             const auto hits = conversationRepository.searchMessagesByContent(needle, 30);
                             for (const auto& msg : hits) {
                                 GlobalSearchPanel::MessageResult mr;
                                 mr.conversationId = QString::fromStdWString(msg.conversationId);
                                 mr.messageId = QString::fromStdWString(msg.messageId);
                                 mr.createdAtMs = msg.createdAtMs;
                                 const QString sid = QString::fromStdWString(msg.senderId);
                                 if (sid == localClientId) {
                                     mr.senderName = localDisplayName.isEmpty()
                                         ? QStringLiteral("\u6211") : localDisplayName;
                                 } else {
                                     const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(sid));
                                     mr.senderName = peer.has_value()
                                         ? displayNameForPeer(*peer) : sid;
                                 }
                                 const bool isGroup = groupService.isGroupConversation(mr.conversationId);
                                 if (isGroup) {
                                     const auto gOpt = groupRepository.findGroupById(
                                         mr.conversationId.toStdWString());
                                     mr.conversationTitle = gOpt
                                         ? QString::fromStdWString(gOpt->groupName)
                                         : QStringLiteral("\u7FA4\u804A");
                                 } else {
                                     const QString otherP = DirectConversationAddressing::otherParticipant(
                                         localClientId, mr.conversationId);
                                     if (!otherP.isEmpty()) {
                                         const auto peer2 = peerDirectoryService.findPeerByClientId(toUtf8(otherP));
                                         mr.conversationTitle = peer2.has_value()
                                             ? displayNameForPeer(*peer2) : otherP;
                                     } else {
                                         mr.conversationTitle = mr.conversationId;
                                     }
                                 }
                                 // 轻量 HTML 剥离（避免 QTextDocument 重量级构造）
                                 QString plain = QString::fromStdWString(msg.body);
                                 static const QRegularExpression htmlTagRe(
                                     QStringLiteral("<[^>]*>"));
                                 plain.remove(htmlTagRe);
                                 plain.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
                                 plain.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
                                 plain.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
                                 plain.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
                                 plain = plain.trimmed();
                                 if (plain.length() > 80) plain = plain.left(80) + QStringLiteral("...");
                                 mr.bodyPreview = plain;
                                 messages.append(mr);
                             }
                         }

                         // ── 搜索文件传输记录 ──
                         {
                             const auto allTasks = fileTransferRepository.loadRecentTasks(200);
                             for (const auto& task : allTasks) {
                                 if (task.state != FileTransferState::Completed) continue;
                                 const QString fn = QString::fromStdWString(task.fileName);
                                 if (fn.contains(needle, Qt::CaseInsensitive)) {
                                     GlobalSearchPanel::FileResult fr;
                                     fr.taskId = QString::fromStdWString(task.taskId);
                                     fr.fileName = fn;
                                     fr.createdAtMs = task.createdAtMs;
                                     const QString peerId = QString::fromStdWString(task.peerClientId);
                                     const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(peerId));
                                     fr.peerName = peer.has_value() ? displayNameForPeer(*peer) : peerId;
                                     files.append(fr);
                                 }
                                 if (files.size() >= 20) break;
                             }
                         }

                         // ── 搜索部门（复用已有 allPeers，不二次查询）──
                         {
                             QHash<QString, int> deptCounts;
                             for (const auto& peer : allPeers) {
                                 const QString cid = QString::fromStdString(peer.clientId);
                                 QString dept;
                                 if (peerProfiles.contains(cid)) {
                                     dept = peerProfiles.value(cid).department.trimmed();
                                 }
                                 if (dept.isEmpty()) dept = QStringLiteral("\u672A\u5206\u7EC4");
                                 deptCounts[dept]++;
                             }
                             for (auto it = deptCounts.constBegin(); it != deptCounts.constEnd(); ++it) {
                                 if (it.key().contains(needle, Qt::CaseInsensitive)
                                     || PinyinHelper::matchesPinyin(it.key(), needle)) {
                                     GlobalSearchPanel::DepartmentResult dr;
                                     dr.department = it.key();
                                     dr.memberCount = it.value();
                                     departments.append(dr);
                                 }
                                 if (departments.size() >= 20) break;
                             }
                         }

                         m_mainWindow->setGlobalSearchResults(contacts, groups, messages, files, departments);
                     });

    QObject::connect(&lanDiscovery, &LanDiscoveryService::peerDiscovered, m_mainWindow.get(),
                     [&](const QString& clientId,
                         const QString& displayName,
                         quint16 tcpPort,
                         const QString& host,
                         const QString& appVersion,
                         const QStringList& capabilities) {
                         Q_UNUSED(appVersion);
                         recipientCapabilityResolver.rememberLocalObservation(
                             clientId,
                             capabilities,
                             QDateTime::currentMSecsSinceEpoch());
                         const bool peerUiChanged = rememberPeer(clientId,
                                                                 displayName,
                                                                 host,
                                                                 tcpPort,
                                                                 false,
                                                                 PeerPresenceStatus::Online);
                         if (peerUiChanged) {
                             if (!lanPeerUiRefreshTimer.isActive()) {
                                 lanPeerUiRefreshTimer.start(250);
                             }
                         }
                         qInfo().noquote() << "[peer-diag] LAN-discovered:"
                                           << host << ":" << tcpPort
                                           << "clientId=" << clientId.left(8);
                         if (P2PConnectionPolicy::shouldStartPeerConnection(
                                 RemoteChatServiceSettingsStore::load(),
                                 P2PConnectionTrigger::LanDiscovery,
                                 capabilities)) {
                             tryAutoConnectPeer(clientId, host, tcpPort);
                         } else {
                             qInfo().noquote()
                                 << "[peer-diag] LAN-discovered stored directory-only:"
                                 << host << ":" << tcpPort
                                 << "clientId=" << clientId.left(8);
                         }
                     });

    const auto recipientIdsFromEnvelopes =
        [](const std::vector<MessageEnvelope>& envelopes) {
            QStringList recipientIds;
            QSet<QString> seen;
            for (const MessageEnvelope& envelope : envelopes) {
                const QString recipientId =
                    QString::fromUtf8(envelope.targetId.data(),
                                      static_cast<int>(envelope.targetId.size()))
                        .trimmed();
                if (recipientId.isEmpty() || seen.contains(recipientId)) {
                    continue;
                }
                seen.insert(recipientId);
                recipientIds.push_back(recipientId);
            }
            return recipientIds;
        };

    const auto partitionRecipientsForMessageService =
        [&](const QStringList& recipientIds,
            const RemoteChatServiceSettings& settings,
            ServerMessageClient& serverMessageClient,
            bool serviceReachable) {
            RecipientRoutePartition partition;
            QStringList normalizedIds;
            QSet<QString> seen;
            for (const QString& rawId : recipientIds) {
                const QString recipientId = rawId.trimmed();
                if (recipientId.isEmpty() || seen.contains(recipientId)) {
                    continue;
                }
                seen.insert(recipientId);
                normalizedIds.push_back(recipientId);
            }
            if (normalizedIds.isEmpty()) {
                return partition;
            }

            if (!serviceReachable || !settings.canUseMessageService()) {
                partition.p2pRecipientIds =
                    QVector<QString>(normalizedIds.cbegin(), normalizedIds.cend());
                return partition;
            }

            QStringList unknownIds;
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            constexpr qint64 kServerCapabilityRefreshMs = 30000;
            for (const QString& recipientId : normalizedIds) {
                const RecipientCapabilityDecision decision =
                    recipientCapabilityResolver.resolveFromCacheOnly(
                        recipientId,
                        nowMs);
                if (decision.status
                    == RecipientCapabilityStatus::ServerReceiveCapable) {
                    partition.serverRecipientIds.push_back(recipientId);
                } else if (recipientCapabilityResolver.shouldRefreshServerProfile(
                               decision,
                               nowMs,
                               kServerCapabilityRefreshMs)) {
                    unknownIds.push_back(recipientId);
                } else {
                    partition.p2pRecipientIds.push_back(recipientId);
                }
            }

            constexpr qsizetype kCapabilityQueryBatchSize = 200;
            for (qsizetype offset = 0; offset < unknownIds.size();
                 offset += kCapabilityQueryBatchSize) {
                const QStringList batch =
                    unknownIds.mid(offset, kCapabilityQueryBatchSize);
                QString capabilityError;
                const std::optional<ServerClientCapabilityQueryResult> query =
                    serverMessageClient.queryClientCapabilities(
                        settings.workspaceId,
                        batch,
                        MessageRoutingCapabilities::serverReceiveV1(),
                        &capabilityError);
                if (!query.has_value()) {
                    if (!capabilityError.trimmed().isEmpty()) {
                        qInfo().noquote()
                            << "[group-send] capability query skipped/failure count="
                            << batch.size()
                            << "error=" << capabilityError;
                    }
                    continue;
                }

                QHash<QString, QStringList> returnedCapabilities;
                for (const ServerClientCapabilityProfile& profile :
                     query->profiles) {
                    returnedCapabilities.insert(profile.clientId,
                                                profile.capabilities);
                }
                recipientCapabilityResolver.rememberServerQueryResult(
                    batch,
                    returnedCapabilities,
                    nowMs);
            }

            for (const QString& recipientId : unknownIds) {
                const RecipientCapabilityDecision decision =
                    recipientCapabilityResolver.resolveFromCacheOnly(
                        recipientId,
                        nowMs);
                if (decision.status
                    == RecipientCapabilityStatus::ServerReceiveCapable) {
                    partition.serverRecipientIds.push_back(recipientId);
                } else {
                    partition.p2pRecipientIds.push_back(recipientId);
                }
            }

            return partition;
        };

    const auto resolveDirectRecipientServerCapability =
        [&](const QString& recipientId,
            const RemoteChatServiceSettings& settings,
            ServerMessageClient& serverMessageClient,
            bool serviceReachable) -> bool {
            const QString normalizedRecipientId = recipientId.trimmed();
            if (normalizedRecipientId.isEmpty()) {
                return false;
            }

            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            constexpr qint64 kServerCapabilityRefreshMs = 30000;
            RecipientCapabilityDecision capability =
                recipientCapabilityResolver.resolveFromCacheOnly(
                    normalizedRecipientId,
                    nowMs);
            if (recipientCapabilityResolver.shouldRefreshServerProfile(
                    capability,
                    nowMs,
                    kServerCapabilityRefreshMs)
                && serviceReachable
                && settings.canUseMessageService()) {
                QString capabilityError;
                const std::optional<ServerClientCapabilityQueryResult> query =
                    serverMessageClient.queryClientCapabilities(
                        settings.workspaceId,
                        QStringList{normalizedRecipientId},
                        MessageRoutingCapabilities::serverReceiveV1(),
                        &capabilityError);
                if (query.has_value()) {
                    QHash<QString, QStringList> returnedCapabilities;
                    for (const ServerClientCapabilityProfile& profile :
                         query->profiles) {
                        returnedCapabilities.insert(profile.clientId,
                                                    profile.capabilities);
                    }
                    recipientCapabilityResolver.rememberServerQueryResult(
                        QStringList{normalizedRecipientId},
                        returnedCapabilities,
                        nowMs);
                    capability =
                        recipientCapabilityResolver.resolveFromCacheOnly(
                            normalizedRecipientId,
                            nowMs);
                } else if (!capabilityError.trimmed().isEmpty()) {
                    qInfo().noquote()
                        << "[direct-envelope] capability query skipped/failure target="
                        << normalizedRecipientId.left(8)
                        << "error=" << capabilityError;
                }
            }
            return capability.status
                == RecipientCapabilityStatus::ServerReceiveCapable;
        };

    const auto retryPendingDirectMessagesForAvailableRoute =
        [&](const QString& rawTargetId) {
            const QString targetId =
                resolvedTargetIdsByAlias.value(rawTargetId, rawTargetId).trimmed();
            const QString conversationId =
                DirectConversationAddressing::conversationIdForPeers(
                    localClientId, targetId);
            if (targetId.isEmpty() || conversationId.isEmpty()) {
                return 0;
            }

            const auto pendingMessages =
                conversationRepository.loadPendingOutgoingMessages(
                    conversationId.toStdWString(),
                    localClientId.toStdWString());
            if (pendingMessages.empty()) {
                return 0;
            }

            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            bool hasRetryableMessage = false;
            for (const ChatMessage& pending : pendingMessages) {
                const QString messageId =
                    QString::fromStdWString(pending.messageId).trimmed();
                if (pending.messageType == L"text"
                    && pending.attachmentName.empty()
                    && !messageId.isEmpty()
                    && pendingDirectRetryNotBeforeMs.value(messageId, 0) <= nowMs) {
                    hasRetryableMessage = true;
                    break;
                }
            }
            if (!hasRetryableMessage) {
                return 0;
            }

            const RemoteChatServiceSettings settings =
                RemoteChatServiceSettingsStore::load();
            ServerMessageClient serverMessageClient(settings, localClientId);
            const bool serviceReachable =
                remoteChatServiceRecentlyReachable(settings);
            const bool receiverServerCapable =
                resolveDirectRecipientServerCapability(
                    targetId,
                    settings,
                    serverMessageClient,
                    serviceReachable);
            PeerConnection* connection =
                ConnectionRegistryUtils::connectedConnectionForTarget(
                    connectionsByTargetId,
                    peerIdsByConnection,
                    targetId);
            QPointer<PeerConnection> p2pConnection(connection);
            const bool p2pAvailable =
                p2pConnection && p2pConnection->isConnected();

            if (!receiverServerCapable && !p2pAvailable) {
                ensureLegacyP2PConnectionForTarget(
                    targetId,
                    settings,
                    receiverServerCapable,
                    false,
                    QStringLiteral("pending-direct-retry"));
            }

            ReliableDirectMessageSender directSender(
                localClientId,
                &conversationRepository,
                &serverMessageClient,
                [&](const ReliableDirectMessageP2PRequest& p2pRequest,
                    QString* errorMessage) -> bool {
                    MessageEnvelope envelope;
                    QString buildError;
                    if (!ChatService::buildEnvelope(
                            localClientId,
                            &conversationRepository,
                            p2pRequest.messageId,
                            p2pRequest.targetId,
                            &envelope,
                            &buildError)) {
                        if (errorMessage) {
                            *errorMessage = buildError.trimmed().isEmpty()
                                ? QStringLiteral("direct message envelope build failed")
                                : buildError;
                        }
                        return false;
                    }
                    envelope.contentType = "html";
                    if (!p2pConnection || !p2pConnection->isConnected()) {
                        if (errorMessage) {
                            *errorMessage = QStringLiteral(
                                "p2p peer is not connected");
                        }
                        return false;
                    }
                    return p2pConnection->sendPayload(
                        QByteArray::fromStdString(MessageCodec::encode(envelope)));
                });

            int retriedCount = 0;
            int attemptedCount = 0;
            constexpr int kMaxDirectRetriesPerTargetPass = 20;
            for (const ChatMessage& pending : pendingMessages) {
                if (attemptedCount >= kMaxDirectRetriesPerTargetPass) {
                    break;
                }
                if (pending.messageType != L"text"
                    || !pending.attachmentName.empty()) {
                    continue;
                }

                ReliableDirectMessageSendRequest request;
                request.conversationId = conversationId;
                request.targetId = targetId;
                request.settings = settings;
                request.serviceReachable = serviceReachable;
                request.receiverServerCapable = receiverServerCapable;
                request.p2pAvailable = p2pAvailable;
                request.requireP2PDeliveryReceipt = true;
                const QString messageId =
                    QString::fromStdWString(pending.messageId).trimmed();
                if (messageId.isEmpty()
                    || pendingDirectRetryNotBeforeMs.value(messageId, 0) > nowMs) {
                    continue;
                }
                ++attemptedCount;
                const ReliableDirectMessageSendResult result =
                    directSender.retryText(messageId, request);
                if (result.success) {
                    if (result.channelUsed == TransportChannel::P2P) {
                        deferPendingDirectRetry(messageId);
                    } else {
                        clearPendingDirectRetryBackoff(messageId);
                    }
                    ++retriedCount;
                } else if (!result.errorMessage.trimmed().isEmpty()) {
                    const qint64 retryDelayMs = deferPendingDirectRetry(messageId);
                    qInfo().noquote()
                        << "[pending-direct-retry] deferred msgId="
                        << messageId.left(8)
                        << "target=" << targetId.left(8)
                        << "retryMs=" << retryDelayMs
                        << "error=" << result.errorMessage;
                    // A stale health success can outlive the service by a few
                    // seconds. Do not synchronously timeout every queued row on
                    // the UI thread; the next sweep will resume after health is
                    // refreshed.
                    break;
                }
            }
            return retriedCount;
        };

    const auto sendDirectEnvelope =
        [&](const QString& logPrefix,
            const QString& resolvedTargetId,
            const MessageEnvelope& envelope) {
            const RemoteChatServiceSettings remoteChatSettings =
                RemoteChatServiceSettingsStore::load();
            ServerMessageClient serverMessageClient(remoteChatSettings, localClientId);
            const bool serviceReachable =
                remoteChatServiceRecentlyReachable(remoteChatSettings);
            const bool receiverServerCapable =
                resolveDirectRecipientServerCapability(resolvedTargetId,
                                                       remoteChatSettings,
                                                       serverMessageClient,
                                                       serviceReachable);
            PeerConnection* connection =
                ConnectionRegistryUtils::connectedConnectionForTarget(connectionsByTargetId,
                                                                      peerIdsByConnection,
                                                                      resolvedTargetId);
            QPointer<PeerConnection> p2pConnection(connection);

            ReliableDirectEnvelopeSender directEnvelopeSender(
                localClientId,
                &serverMessageClient,
                [&](const ReliableDirectEnvelopeP2PRequest& p2pRequest,
                    QString* errorMessage) -> bool {
                    if (!p2pConnection || !p2pConnection->isConnected()) {
                        if (errorMessage) {
                            *errorMessage =
                                QStringLiteral("p2p peer is not connected");
                        }
                        return false;
                    }
                    return p2pConnection->sendPayload(
                        QByteArray::fromStdString(
                            MessageCodec::encode(p2pRequest.envelope)));
                });

            ReliableDirectEnvelopeSendRequest sendRequest;
            sendRequest.envelope = envelope;
            sendRequest.settings = remoteChatSettings;
            sendRequest.serviceReachable = serviceReachable;
            sendRequest.receiverServerCapable = receiverServerCapable;
            sendRequest.p2pAvailable =
                p2pConnection && p2pConnection->isConnected();
            ensureLegacyP2PConnectionForTarget(
                resolvedTargetId,
                remoteChatSettings,
                receiverServerCapable,
                sendRequest.p2pAvailable,
                logPrefix.trimmed().isEmpty()
                    ? QStringLiteral("direct-envelope")
                    : logPrefix.trimmed());

            ReliableDirectEnvelopeSendResult sendResult =
                directEnvelopeSender.send(sendRequest);
            if (!sendResult.success
                && !receiverServerCapable
                && !sendRequest.p2pAvailable
                && P2PConnectionPolicy::shouldStartPeerConnection(
                    remoteChatSettings,
                    P2PConnectionTrigger::ServiceFallback)) {
                ensureLegacyP2PConnectionForTarget(
                    resolvedTargetId,
                    remoteChatSettings,
                    receiverServerCapable,
                    sendRequest.p2pAvailable,
                    QStringLiteral("direct-envelope-fallback"));
            }

            qInfo().noquote()
                << "[" << (logPrefix.trimmed().isEmpty()
                            ? QStringLiteral("direct-envelope")
                            : logPrefix.trimmed()) << "] msgId="
                << sendResult.messageId.left(8)
                << "target=" << resolvedTargetId.left(8)
                << "serverCapable=" << receiverServerCapable
                << "channel=" << static_cast<int>(sendResult.channelUsed)
                << "sent=" << sendResult.success;
            if (!sendResult.success
                && !sendResult.errorMessage.trimmed().isEmpty()) {
                qWarning().noquote()
                    << "[" << (logPrefix.trimmed().isEmpty()
                                ? QStringLiteral("direct-envelope")
                                : logPrefix.trimmed()) << "] failed:"
                    << sendResult.errorMessage;
            }
            return sendResult;
        };

    const auto clearDeliveredGroupFanOutPending =
        [&](const QString& logPrefix, const GroupFanOutPayload& payload) {
            const QString targetId = payload.targetId.trimmed();
            const QString messageId = payload.messageId.trimmed();
            if (targetId.isEmpty() || messageId.isEmpty()) {
                return;
            }
            if (peerDeliveryReceiptCapabilities.value(targetId, false)) {
                qInfo().noquote()
                    << "[" << (logPrefix.trimmed().isEmpty()
                                ? QStringLiteral("group-send")
                                : logPrefix.trimmed())
                    << "] retaining pending envelope until delivery receipt"
                    << "msgId=" << messageId.left(8)
                    << "target=" << targetId.left(8);
                return;
            }
            if (!conversationRepository.deletePendingGroupEnvelopeForTargetMessage(
                    targetId, messageId)) {
                qWarning().noquote()
                    << "[" << (logPrefix.trimmed().isEmpty()
                                ? QStringLiteral("group-send")
                                : logPrefix.trimmed()) << "] failed to clear delivered pending envelope"
                    << "msgId=" << messageId.left(8)
                    << "target=" << targetId.left(8);
            }
        };

    const auto enablePendingCleanupForDelivery =
        [&](GroupFanOutDeliveryOptions* options) {
            if (!options) {
                return;
            }
            const QString cleanupLogPrefix = options->logPrefix;
            options->onDelivered =
                [&, cleanupLogPrefix](const GroupFanOutPayload& payload) {
                    clearDeliveredGroupFanOutPending(cleanupLogPrefix, payload);
                };
        };
    const auto sendPersistedGroupEnvelopes =
        [&](const QString& logPrefix,
            const QString& groupId,
            const QString& groupTitle,
            const std::vector<MessageEnvelope>& envelopes) {
            const RemoteChatServiceSettings remoteChatSettings =
                RemoteChatServiceSettingsStore::load();
            ServerMessageClient serverMessageClient(remoteChatSettings, localClientId);

            const auto sendGroupEnvelopesViaP2P =
                [&](const ReliableGroupEnvelopeP2PRequest& p2pRequest,
                    QString* errorMessage) -> bool {
                    if (p2pRequest.groupId.trimmed().isEmpty()
                        || p2pRequest.envelopes.empty()) {
                        if (errorMessage) {
                            *errorMessage = QStringLiteral(
                                "group envelope fan-out payload is empty");
                        }
                        return false;
                    }

                    std::vector<GroupFanOutPayload> payloads;
                    (void)buildPendingGroupFanOut(p2pRequest.envelopes, &payloads);
                    if (payloads.empty()) {
                        if (errorMessage) {
                            *errorMessage = QStringLiteral(
                                "group envelope payload encoding failed");
                        }
                        return false;
                    }

                    GroupFanOutDeliveryOptions deliveryOptions;
                    deliveryOptions.batchSize = 20;
                    deliveryOptions.logPrefix = logPrefix.trimmed().isEmpty()
                        ? QStringLiteral("group-envelope")
                        : logPrefix.trimmed();
                    enablePendingCleanupForDelivery(&deliveryOptions);
                    const GroupFanOutDeliveryResult deliveryResult =
                        deliverGroupFanOutPayloads(
                        payloads,
                        nullptr,
                        [&](const GroupFanOutPayload& payloadToSend) {
                            PeerConnection* conn =
                                connectionsByTargetId.value(
                                    payloadToSend.targetId, nullptr);
                            if (!conn || !conn->isConnected()) {
                                return false;
                            }
                            return conn->sendPayload(payloadToSend.blob);
                        },
                        deliveryOptions);
                    if (!isGroupFanOutDeliveryAccepted(
                            p2pRequest.acceptQueuedOnlyDelivery,
                            deliveryResult)) {
                        qWarning().noquote()
                            << "[" + deliveryOptions.logPrefix + "] p2p fan-out was not fully accepted"
                            << "group=" << p2pRequest.groupId.left(8)
                            << "attempted=" << deliveryResult.attemptedCount
                            << "delivered=" << deliveryResult.deliveredCount
                            << "failed=" << deliveryResult.failedCount;
                        if (errorMessage) {
                            *errorMessage =
                                QStringLiteral("p2p group envelope send was not fully accepted");
                        }
                        return false;
                    }
                    return true;
                };

            ReliableGroupEnvelopeSender groupEnvelopeSender(
                localClientId,
                &conversationRepository,
                &serverMessageClient,
                sendGroupEnvelopesViaP2P);

            ReliableGroupEnvelopeSendRequest sendRequest;
            sendRequest.groupId = groupId.trimmed();
            sendRequest.groupTitle = groupTitle.trimmed();
            sendRequest.envelopes = envelopes;
            sendRequest.settings = remoteChatSettings;
            sendRequest.serviceReachable =
                remoteChatServiceRecentlyReachable(remoteChatSettings);
            sendRequest.p2pAvailable = !sendRequest.envelopes.empty();

            const RecipientRoutePartition recipientPartition =
                partitionRecipientsForMessageService(
                    recipientIdsFromEnvelopes(sendRequest.envelopes),
                    remoteChatSettings,
                    serverMessageClient,
                    sendRequest.serviceReachable);
            sendRequest.serverRecipientIds = recipientPartition.serverRecipientIds;
            sendRequest.p2pRecipientIds = recipientPartition.p2pRecipientIds;

            const ReliableGroupEnvelopeSendResult sendResult =
                groupEnvelopeSender.send(sendRequest);
            qInfo().noquote()
                << "[" << (logPrefix.trimmed().isEmpty()
                            ? QStringLiteral("group-envelope")
                            : logPrefix.trimmed()) << "] msgId="
                << sendResult.messageId.left(8)
                << "group=" << sendRequest.groupId.left(8)
                << "channel=" << static_cast<int>(sendResult.channelUsed)
                << "sent=" << sendResult.success;
            if (!sendResult.success
                && !sendResult.errorMessage.trimmed().isEmpty()) {
                qWarning().noquote()
                    << "[" << (logPrefix.trimmed().isEmpty()
                                ? QStringLiteral("group-envelope")
                                : logPrefix.trimmed()) << "] failed:"
                    << sendResult.errorMessage;
            }
            return sendResult;
        };

    const auto retryPendingGroupEnvelopesViaMessageService = [&]() {
        const RemoteChatServiceSettings settings =
            RemoteChatServiceSettingsStore::load();
        if (!settings.canUseMessageService()
            || !remoteChatServiceRecentlyReachable(settings)) {
            return 0;
        }

        const auto pendingRows =
            conversationRepository.loadAllPendingGroupEnvelopes();
        if (pendingRows.empty()) {
            return 0;
        }

        QHash<QString, std::vector<MessageEnvelope>> envelopesByMessage;
        QHash<QString, QString> groupIdByMessage;
        for (const auto& pending : pendingRows) {
            const auto decoded = MessageCodec::decode(std::string_view(
                pending.envelopeBlob.constData(),
                static_cast<std::size_t>(pending.envelopeBlob.size())));
            if (!decoded.has_value()) {
                continue;
            }
            const bool supported =
                decoded->type == MessageType::GroupMessage
                || decoded->type == MessageType::ResourceReference
                || decoded->type == MessageType::MessageMutation
                || decoded->type == MessageType::MessageReaction
                || decoded->type == MessageType::PinMessage;
            const QString messageId = QString::fromUtf8(
                decoded->messageId.data(),
                static_cast<int>(decoded->messageId.size())).trimmed();
            const QString groupId = QString::fromUtf8(
                decoded->conversationId.data(),
                static_cast<int>(decoded->conversationId.size())).trimmed();
            const QString senderId = QString::fromUtf8(
                decoded->senderId.data(),
                static_cast<int>(decoded->senderId.size())).trimmed();
            const QString targetId = QString::fromUtf8(
                decoded->targetId.data(),
                static_cast<int>(decoded->targetId.size())).trimmed();
            if (!supported
                || messageId.isEmpty()
                || groupId.isEmpty()
                || senderId != localClientId
                || targetId.isEmpty()) {
                continue;
            }

            const QString key = messageId + QChar(0x1F) + groupId;
            envelopesByMessage[key].push_back(*decoded);
            groupIdByMessage.insert(key, groupId);
        }

        if (envelopesByMessage.isEmpty()) {
            return 0;
        }

        ServerMessageClient serverMessageClient(settings, localClientId);
        ReliableGroupEnvelopeSender sender(
            localClientId,
            &conversationRepository,
            &serverMessageClient,
            [](const ReliableGroupEnvelopeP2PRequest&, QString* errorMessage) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral(
                        "periodic service retry does not use p2p");
                }
                return false;
            });

        int sentBatchCount = 0;
        constexpr int kMaxGroupServiceBatchesPerPass = 50;
        for (auto it = envelopesByMessage.cbegin();
             it != envelopesByMessage.cend()
             && sentBatchCount < kMaxGroupServiceBatchesPerPass;
             ++it) {
            const RecipientRoutePartition partition =
                partitionRecipientsForMessageService(
                    recipientIdsFromEnvelopes(it.value()),
                    settings,
                    serverMessageClient,
                    true);
            if (partition.serverRecipientIds.isEmpty()) {
                continue;
            }

            QSet<QString> serverRecipientSet;
            for (const QString& recipientId : partition.serverRecipientIds) {
                serverRecipientSet.insert(recipientId.trimmed());
            }
            std::vector<MessageEnvelope> serverEnvelopes;
            for (const MessageEnvelope& envelope : it.value()) {
                const QString targetId = QString::fromUtf8(
                    envelope.targetId.data(),
                    static_cast<int>(envelope.targetId.size())).trimmed();
                if (serverRecipientSet.contains(targetId)) {
                    serverEnvelopes.push_back(envelope);
                }
            }
            if (serverEnvelopes.empty()) {
                continue;
            }

            const QString groupId = groupIdByMessage.value(it.key());
            QString groupTitle = groupId;
            if (const auto group = groupRepository.findGroupById(
                    groupId.toStdWString())) {
                const QString storedTitle =
                    QString::fromStdWString(group->groupName).trimmed();
                if (!storedTitle.isEmpty()) {
                    groupTitle = storedTitle;
                }
            }

            ReliableGroupEnvelopeSendRequest request;
            request.groupId = groupId;
            request.groupTitle = groupTitle;
            request.envelopes = std::move(serverEnvelopes);
            request.serverRecipientIds = partition.serverRecipientIds;
            request.settings = settings;
            request.serviceReachable = true;
            request.p2pAvailable = false;
            const ReliableGroupEnvelopeSendResult result = sender.send(request);
            if (result.success) {
                ++sentBatchCount;
            } else if (!result.errorMessage.trimmed().isEmpty()) {
                qInfo().noquote()
                    << "[pending-group-service-retry] deferred"
                    << "group=" << groupId.left(8)
                    << "msgId=" << result.messageId.left(8)
                    << "error=" << result.errorMessage;
                break;
            }
        }
        return sentBatchCount;
    };

    const auto createDirectResourceReferenceMessage =
        [&](const QString& targetAlias,
            const ResourceRefPayload& payload,
            QString* outResolvedTargetId,
            QString* outConversationId,
            QString* outMessageId,
            MessageEnvelope* outEnvelope) -> bool {
            if (targetAlias.trimmed().isEmpty() || !outResolvedTargetId || !outConversationId
                || !outMessageId || !outEnvelope) {
                return false;
            }

            const QString resolvedTargetId =
                resolvedTargetIdsByAlias.value(targetAlias, targetAlias).trimmed();
            const QString canonicalConversationId =
                DirectConversationAddressing::conversationIdForPeers(localClientId, resolvedTargetId);
            if (resolvedTargetId.isEmpty() || canonicalConversationId.isEmpty()) {
                return false;
            }

            const QString messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            const qint64 createdAtMs = QDateTime::currentMSecsSinceEpoch();
            MessageEnvelope envelope = buildResourceReferenceEnvelope(messageId,
                                                                     localClientId,
                                                                     resolvedTargetId,
                                                                     canonicalConversationId,
                                                                     payload,
                                                                     createdAtMs);
            const QString preview = ResourceRefRouter::previewLabel(envelope).trimmed();
            const ChatMessage message{
                messageId.toStdWString(),
                canonicalConversationId.toStdWString(),
                localClientId.toStdWString(),
                preview.toStdWString(),
                createdAtMs,
                MessageDeliveryState::Pending,
                {},
                {},
                L"resource_ref",
                QString::fromUtf8(envelope.payloadJson.data(),
                                  static_cast<int>(envelope.payloadJson.size())).toStdWString(),
            };
            if (!conversationRepository.appendMessage(message)
                || !conversationRepository.upsertConversationWithType(
                    ConversationSummary{
                        canonicalConversationId.toStdWString(),
                        targetAlias.toStdWString(),
                        preview.toStdWString(),
                        createdAtMs,
                    },
                    QStringLiteral("direct"))) {
                return false;
            }

            *outResolvedTargetId = resolvedTargetId;
            *outConversationId = canonicalConversationId;
            *outMessageId = messageId;
            *outEnvelope = envelope;
            return true;
        };
    const auto showStatusIfPresent = [&](const QString& message, int timeoutMs) {
        if (!message.trimmed().isEmpty()) {
            m_mainWindow->setStatusMessage(message.trimmed(), timeoutMs);
        }
    };
    const auto refreshResourceReferenceUiForConversation =
        [&](const QString& conversationId, bool activateConversation) {
            const QString normalizedConversationId = conversationId.trimmed();
            if (activateConversation
                || (!normalizedConversationId.isEmpty()
                    && currentConversationId == normalizedConversationId)) {
                scheduleChatUiRefresh(true, true, true, true, 0);
            } else {
                scheduleChatUiRefresh(true, false, false, true, 0);
            }
        };
    const auto sendResourceReferencePayloadToConversation =
        [&](const QString& conversationId,
            const QString& targetAlias,
            const QString& conversationTitle,
            const ResourceRefPayload& payload,
            const QString& successMessage,
            const QString& queuedMessage,
            const QString& failureMessage,
            bool activateConversation,
            bool refreshUi = true) -> bool {
            const QString normalizedConversationId = conversationId.trimmed();
            if (normalizedConversationId.isEmpty()) {
                m_mainWindow->setStatusMessage(
                    failureMessage.isEmpty() ? QStringLiteral("请先选择一个会话") : failureMessage,
                    2500);
                return false;
            }

            if (groupService.isGroupConversation(normalizedConversationId)) {
                const auto groupOpt = groupRepository.findGroupById(normalizedConversationId.toStdWString());
                const QString groupTitle = conversationTitle.trimmed().isEmpty()
                    ? (groupOpt ? QString::fromStdWString(groupOpt->groupName) : normalizedConversationId)
                    : conversationTitle.trimmed();
                const auto envelopes = groupService.buildGroupResourceReferenceFanOut(
                    localClientId,
                    normalizedConversationId,
                    payload);
                if (envelopes.empty()) {
                    m_mainWindow->setStatusMessage(
                        failureMessage.isEmpty()
                            ? QStringLiteral("通知卡片路由失败，请检查会话和资源引用配置")
                            : failureMessage,
                        2500);
                    return false;
                }

                std::vector<GroupFanOutPayload> payloads;
                const auto pending = buildPendingGroupFanOut(envelopes, &payloads);
                if (!ChatService::persistOutgoingGroupFanOut(&conversationRepository,
                                                             normalizedConversationId,
                                                             groupTitle,
                                                             pending,
                                                             envelopes.front())) {
                    qWarning().noquote() << "[group-resource-ref] persist fan-out failed group="
                                         << normalizedConversationId.left(8);
                    showStatusIfPresent(failureMessage.isEmpty()
                                            ? QStringLiteral("通知卡片保存失败，未发送")
                                            : failureMessage,
                                        2500);
                    return false;
                }

                const ReliableGroupEnvelopeSendResult sendResult =
                    sendPersistedGroupEnvelopes(QStringLiteral("group-resource-ref"),
                                                normalizedConversationId,
                                                groupTitle,
                                                envelopes);
                if (!sendResult.success
                    && !sendResult.errorMessage.trimmed().isEmpty()) {
                    showStatusIfPresent(sendResult.errorMessage, 3000);
                }
                if (activateConversation) {
                    currentConversationId = normalizedConversationId;
                    currentTargetId.clear();
                }
                if (refreshUi) {
                    refreshResourceReferenceUiForConversation(normalizedConversationId,
                                                              activateConversation);
                }
                if (sendResult.success) {
                    showStatusIfPresent(successMessage, 2500);
                }
                return true;
            }

            const QString targetIdForConversation = targetAlias.trimmed().isEmpty()
                ? DirectConversationAddressing::otherParticipant(localClientId, normalizedConversationId)
                : targetAlias.trimmed();
            if (targetIdForConversation.isEmpty()) {
                m_mainWindow->setStatusMessage(
                    failureMessage.isEmpty() ? QStringLiteral("请先选择一个私聊会话") : failureMessage,
                    2500);
                return false;
            }

            QString resolvedTargetId;
            QString canonicalConversationId;
            QString messageId;
            MessageEnvelope envelope;
            if (!createDirectResourceReferenceMessage(targetIdForConversation,
                                                      payload,
                                                      &resolvedTargetId,
                                                      &canonicalConversationId,
                                                      &messageId,
                                                      &envelope)) {
                m_mainWindow->setStatusMessage(
                    failureMessage.isEmpty()
                        ? QStringLiteral("通知卡片路由失败，请检查私聊会话和资源引用配置")
                        : failureMessage,
                    2500);
                return false;
            }

            const ReliableDirectEnvelopeSendResult sendResult =
                sendDirectEnvelope(QStringLiteral("direct-resource-ref"),
                                   resolvedTargetId,
                                   envelope);
            if (sendResult.success
                && sendResult.channelUsed == TransportChannel::MessageService) {
                if (ChatService::markMessageServerAcked(&conversationRepository, messageId)) {
                    const QString serverMessageId = sendResult.serverMessageId.trimmed();
                    if (!serverMessageId.isEmpty()
                        && !conversationRepository.saveRemoteMessageIdMapping(serverMessageId,
                                                                              messageId)) {
                        qWarning().noquote()
                            << "[direct-resource-ref] failed to save server id mapping msgId="
                            << messageId.left(8)
                            << "serverId=" << serverMessageId.left(8);
                    }
                }
            } else if (sendResult.success
                       && sendResult.channelUsed == TransportChannel::P2P) {
                ChatService::markMessageSent(&conversationRepository, messageId);
            } else if (sendResult.errorMessage.trimmed().isEmpty()) {
                showStatusIfPresent(queuedMessage, 3500);
            } else {
                showStatusIfPresent(sendResult.errorMessage, 3500);
            }

            if (activateConversation) {
                currentTargetId = resolvedTargetId;
                currentConversationId = canonicalConversationId;
            }
            if (refreshUi) {
                refreshResourceReferenceUiForConversation(canonicalConversationId,
                                                          activateConversation);
            }
            return true;
        };
    const auto sendResourceReferencePayloadBatchToConversation =
        [&](const QString& conversationId,
            const QString& targetAlias,
            const QString& conversationTitle,
            const QVector<ResourceRefPayload>& payloads,
            const QString& failureMessage) -> bool {
            const QString normalizedConversationId = conversationId.trimmed();
            bool routedAny = false;
            for (const ResourceRefPayload& payload : payloads) {
                routedAny = sendResourceReferencePayloadToConversation(normalizedConversationId,
                                                                      targetAlias,
                                                                      conversationTitle,
                                                                      payload,
                                                                      QString(),
                                                                      QString(),
                                                                      QString(),
                                                                      false,
                                                                      false)
                    || routedAny;
            }
            if (!routedAny) {
                if (!normalizedConversationId.isEmpty()) {
                    showStatusIfPresent(failureMessage, 2500);
                }
                return false;
            }

            refreshResourceReferenceUiForConversation(normalizedConversationId, false);
            return true;
        };
    const auto sendResourceReferencePayloadToCurrentConversation =
        [&](const ResourceRefPayload& payload,
            const QString& successMessage,
            const QString& queuedMessage,
            const QString& failureMessage) -> bool {
            return sendResourceReferencePayloadToConversation(currentConversationId,
                                                             currentTargetId,
                                                             QString(),
                                                             payload,
                                                             successMessage,
                                                             queuedMessage,
                                                             failureMessage,
                                                             true,
                                                             true);
        };
    auto* azureDevOpsNotificationTimer = new QTimer(m_mainWindow.get());
    azureDevOpsNotificationTimer->setSingleShot(true);
    // Fix E: 启动时清除历史退避状态，防止上次运行累积的失败计数拖慢首次轮询
    {
        AzureDevOpsConnectionSettings startupDevOps = AzureDevOpsSettingsStore::load();
        if (startupDevOps.consecutivePollFailures > 0) {
            startupDevOps.consecutivePollFailures = 0;
            startupDevOps.lastPollErrorMessage.clear();
            startupDevOps.lastPollErrorCategory.clear();
            for (auto& target : startupDevOps.notificationTargets) {
                target.consecutivePollFailures = 0;
                target.lastPollErrorMessage.clear();
                target.lastPollErrorCategory.clear();
            }
            AzureDevOpsSettingsStore::save(startupDevOps);
        }
        OutlookConnectionSettings startupOutlook = OutlookSettingsStore::load();
        if (startupOutlook.consecutivePollFailures > 0) {
            startupOutlook.consecutivePollFailures = 0;
            startupOutlook.lastPollErrorMessage.clear();
            startupOutlook.lastPollErrorCategory.clear();
            OutlookSettingsStore::save(startupOutlook);
        }
    }
    const auto runAzureDevOpsBuildNotificationPoll = [&]() {
        if (devOpsFlushInFlight) {
            return;
        }

        AzureDevOpsConnectionSettings initialSettings = AzureDevOpsSettingsStore::load();
        if (!initialSettings.hasNotificationConfiguration()) {
            azureDevOpsNotificationTimer->stop();
            (void)QtConcurrent::run([initialSettings]() {
                Diagnostics::writeIntegrationSnapshot(
                    Diagnostics::defaultSourcePaths(),
                    initialSettings,
                    OutlookSettingsStore::load());
            });
            return;
        }
        initialSettings.lastPollAttemptAtMs = QDateTime::currentMSecsSinceEpoch();

        devOpsFlushInFlight = true;

        auto* devOpsWatcher = new QFutureWatcher<DevOpsPollResult>(m_mainWindow.get());
        QObject::connect(devOpsWatcher, &QFutureWatcher<DevOpsPollResult>::finished,
                         m_mainWindow.get(), [&, devOpsWatcher]() {
            auto result = devOpsWatcher->result();
            devOpsWatcher->deleteLater();
            qInfo() << "[devOpsPoll] watcher finished, mainWindow=" << (m_mainWindow ? "valid" : "NULL");
            logUserObjects("devOpsPoll-then-entered");
            devOpsFlushInFlight = false;
            if (!result.hadConfig) {
                return;
            }
            auto& polledSettings        = result.updatedSettings;
            const QString& errorMessage = result.errorMessage;

            if (!errorMessage.trimmed().isEmpty()) {
                polledSettings.lastPollErrorMessage   = errorMessage.trimmed();
                polledSettings.lastPollErrorCategory  = classifyIntegrationErrorCategory(errorMessage);
                polledSettings.consecutivePollFailures += 1;
            } else {
                polledSettings.lastPollSuccessAtMs    = QDateTime::currentMSecsSinceEpoch();
                polledSettings.lastPollErrorMessage.clear();
                polledSettings.lastPollErrorCategory.clear();
                polledSettings.consecutivePollFailures = 0;
            }
            if (!result.events.isEmpty()) {
                for (const AzureDevOpsNotificationEvent& event : result.events) {
                    enqueueAzureDevOpsNotification(event);
                }
                if (!polledSettings.notificationTargets.isEmpty()) {
                    polledSettings.lastNotifiedBuildId = 0;
                }
            }
            AzureDevOpsConnectionSettings settings =
                AzureDevOpsSettingsStore::mergePollState(AzureDevOpsSettingsStore::load(),
                                                         polledSettings);
            if (!settings.notificationConversationId.isEmpty() && !result.events.isEmpty()) {
                QVector<ResourceRefPayload> payloads;
                payloads.reserve(result.events.size());
                for (const auto& event : result.events) {
                    payloads.push_back(AzureDevOpsNotificationDispatcher::payloadForEvent(event));
                }
                sendResourceReferencePayloadBatchToConversation(
                    settings.notificationConversationId,
                    QString{},
                    settings.notificationConversationTitle,
                    payloads,
                    QStringLiteral("通知卡片路由失败，请检查 DevOps 通知会话 ID 设置"));
            }
            const int nextAzurePollMinutes =
                qMax(1,
                     computeIntegrationPollIntervalMs(settings.notificationPollIntervalMinutes,
                                                      settings.consecutivePollFailures,
                                                      settings.lastPollErrorCategory)
                         / (60 * 1000));
            if (!errorMessage.trimmed().isEmpty()) {
                publishIntegrationFailureNotification(QStringLiteral("azure-devops"),
                                                      QStringLiteral("Azure DevOps"),
                                                      settings.lastPollErrorCategory,
                                                      errorMessage,
                                                      QStringLiteral("自动通知轮询没有成功完成。"),
                                                      settings.consecutivePollFailures,
                                                      nextAzurePollMinutes,
                                                      devOpsFailureAnnouncementKey);
            } else {
                publishIntegrationRecoveryNotification(QStringLiteral("azure-devops"),
                                                       QStringLiteral("Azure DevOps"),
                                                       devOpsFailureAnnouncementKey);
            }
            const AzureDevOpsConnectionSettings persistedAzureSettings = settings;
            (void)QtConcurrent::run([persistedAzureSettings]() {
                AzureDevOpsSettingsStore::save(persistedAzureSettings);
                Diagnostics::writeIntegrationSnapshot(
                    Diagnostics::defaultSourcePaths(),
                    persistedAzureSettings,
                    OutlookSettingsStore::load());
            });
            if (!errorMessage.trimmed().isEmpty()) {
                qWarning().noquote()
                    << QStringLiteral("[azure-devops] build notification poll failed: %1")
                           .arg(errorMessage.trimmed());
            }
            if (refreshAzureDevOpsBuildNotificationTimer) {
                refreshAzureDevOpsBuildNotificationTimer();
            }
            logUserObjects("devOpsPoll-then-EXIT");
        });
        devOpsWatcher->setFuture(QtConcurrent::run(
            [settingsCopy = initialSettings]() mutable -> DevOpsPollResult {
                logUserObjects("devOpsPoll-bg-START");
                DevOpsPollResult result;
                result.hadConfig       = true;
                result.updatedSettings = settingsCopy;
                AzureDevOpsBuildNotificationPoller poller(settingsCopy);
                result.events = poller.pollTrackedNotifications(&result.updatedSettings,
                                                                &result.errorMessage);
                logUserObjects("devOpsPoll-bg-END");
                return result;
            }
        ));
    };
    refreshAzureDevOpsBuildNotificationTimer = [&]() {
        const AzureDevOpsConnectionSettings settings = AzureDevOpsSettingsStore::load();
        if (!settings.hasNotificationConfiguration()) {
            azureDevOpsNotificationTimer->stop();
            return;
        }
        azureDevOpsNotificationTimer->start(computeIntegrationPollIntervalMs(
            settings.notificationPollIntervalMinutes,
            settings.consecutivePollFailures,
            settings.lastPollErrorCategory));
    };
    QObject::connect(azureDevOpsNotificationTimer, &QTimer::timeout, m_mainWindow.get(),
                     runAzureDevOpsBuildNotificationPoll);
    refreshAzureDevOpsBuildNotificationTimer();
    QTimer::singleShot(1500, m_mainWindow.get(), [runAzureDevOpsBuildNotificationPoll]() {
        runAzureDevOpsBuildNotificationPoll();
    });
    auto* outlookNotificationTimer = new QTimer(m_mainWindow.get());
    outlookNotificationTimer->setSingleShot(true);
    const auto runOutlookNotificationPoll = [&]() {
        if (outlookFlushInFlight) {
            return;
        }

        OutlookConnectionSettings initialSettings = OutlookSettingsStore::load();
        if (!initialSettings.hasNotificationConfiguration()) {
            outlookNotificationTimer->stop();
            (void)QtConcurrent::run([initialSettings]() {
                Diagnostics::writeIntegrationSnapshot(
                    Diagnostics::defaultSourcePaths(),
                    AzureDevOpsSettingsStore::load(),
                    initialSettings);
            });
            return;
        }

        outlookFlushInFlight = true;
        const QDateTime nowSnapshot = QDateTime::currentDateTime();

        auto* outlookWatcher = new QFutureWatcher<OutlookPollResult>(m_mainWindow.get());
        QObject::connect(outlookWatcher, &QFutureWatcher<OutlookPollResult>::finished,
                         m_mainWindow.get(), [&, outlookWatcher]() {
            const auto result = outlookWatcher->result();
            outlookWatcher->deleteLater();
            qInfo() << "[outlookPoll] watcher finished, mainWindow=" << (m_mainWindow ? "valid" : "NULL");
            logUserObjects("outlookPoll-then-entered");
            outlookFlushInFlight = false;
            if (!result.hadConfig) {
                return;
            }
            const QString& errorMessage = result.errorMessage;
            OutlookConnectionSettings settings =
                OutlookSettingsStore::mergePollState(OutlookSettingsStore::load(),
                                                     result.pollResult.updatedSettings);

            for (const auto& event : result.pollResult.events) {
                enqueueOutlookNotificationEvent(event);
            }
            if (!settings.notificationConversationId.isEmpty() && !result.pollResult.events.isEmpty()) {
                QVector<ResourceRefPayload> payloads;
                payloads.reserve(result.pollResult.events.size());
                for (const auto& event : result.pollResult.events) {
                    payloads.push_back(OutlookNotificationDispatcher::payloadForEvent(event));
                }
                sendResourceReferencePayloadBatchToConversation(
                    settings.notificationConversationId,
                    QString{},
                    settings.notificationConversationTitle,
                    payloads,
                    QStringLiteral("通知卡片路由失败，请检查 Outlook 通知会话 ID 设置"));
            }
            const int nextOutlookPollMinutes =
                qMax(1,
                     computeIntegrationPollIntervalMs(
                         settings.notificationPollIntervalMinutes,
                         settings.consecutivePollFailures,
                         settings.lastPollErrorCategory)
                         / (60 * 1000));
            if (!errorMessage.trimmed().isEmpty()) {
                publishIntegrationFailureNotification(
                    QStringLiteral("outlook"),
                    QStringLiteral("Outlook"),
                    settings.lastPollErrorCategory,
                    errorMessage,
                    QStringLiteral("邮箱与日程轮询没有成功完成。"),
                    settings.consecutivePollFailures,
                    nextOutlookPollMinutes,
                    outlookFailureAnnouncementKey);
            } else {
                publishIntegrationRecoveryNotification(QStringLiteral("outlook"),
                                                       QStringLiteral("Outlook"),
                                                       outlookFailureAnnouncementKey);
            }
            const OutlookConnectionSettings persistedOutlookSettings = settings;
            (void)QtConcurrent::run([persistedOutlookSettings]() {
                OutlookSettingsStore::save(persistedOutlookSettings);
                Diagnostics::writeIntegrationSnapshot(
                    Diagnostics::defaultSourcePaths(),
                    AzureDevOpsSettingsStore::load(),
                    persistedOutlookSettings);
            });
            if (!errorMessage.trimmed().isEmpty()) {
                qWarning().noquote()
                    << QStringLiteral("[outlook] notification poll failed: %1")
                           .arg(errorMessage.trimmed());
            }
            if (refreshOutlookNotificationTimer) {
                refreshOutlookNotificationTimer();
            }
            logUserObjects("outlookPoll-then-EXIT");
        });
        outlookWatcher->setFuture(QtConcurrent::run(
            [settingsCopy = initialSettings, nowSnapshot]() mutable -> OutlookPollResult {
                logUserObjects("outlookPoll-bg-START");
                OutlookPollResult result;
                result.hadConfig  = true;
                OutlookNotificationPoller poller(settingsCopy);
                result.pollResult = poller.poll(nowSnapshot, &result.errorMessage);
                logUserObjects("outlookPoll-bg-END");
                return result;
            }
        ));
    };
    refreshOutlookNotificationTimer = [&]() {
        const OutlookConnectionSettings settings = OutlookSettingsStore::load();
        if (!settings.hasNotificationConfiguration()) {
            outlookNotificationTimer->stop();
            return;
        }
        outlookNotificationTimer->start(computeIntegrationPollIntervalMs(
            settings.notificationPollIntervalMinutes,
            settings.consecutivePollFailures,
            settings.lastPollErrorCategory));
    };
    QObject::connect(outlookNotificationTimer, &QTimer::timeout, m_mainWindow.get(),
                     runOutlookNotificationPoll);
    refreshOutlookNotificationTimer();
    QTimer::singleShot(2500, m_mainWindow.get(), [runOutlookNotificationPoll]() {
        runOutlookNotificationPoll();
    });

    // ── Outlook EWS Streaming Subscription ──────────────────────────────
    // Maintains a long-lived connection to Exchange so new mail / calendar
    // events arrive in seconds instead of waiting for the next poll cycle.
    // When a streaming event fires, we trigger an immediate poll to fetch
    // the actual item details and dispatch through the normal pipeline.
    auto* outlookStreaming = new OutlookStreamingConnection(m_mainWindow.get());
    QObject::connect(outlookStreaming, &OutlookStreamingConnection::streamingEventReceived,
                     m_mainWindow.get(), [&]() {
        qInfo().noquote() << QStringLiteral("[outlook-streaming] event → triggering immediate poll");
        runOutlookNotificationPoll();
    });
    QObject::connect(outlookStreaming, &OutlookStreamingConnection::streamingError,
                     m_mainWindow.get(), [](const QString& error) {
        qWarning().noquote() << QStringLiteral("[outlook-streaming] error: %1").arg(error);
    });
    restartOutlookStreaming = [outlookStreaming](const OutlookConnectionSettings& cfg) {
        outlookStreaming->start(cfg);
    };
    stopOutlookStreaming = [outlookStreaming]() {
        outlookStreaming->stop();
    };
    const auto refreshOutlookStreamingConnection = [outlookStreaming]() {
        const OutlookConnectionSettings settings = OutlookSettingsStore::load();
        if (settings.hasNotificationConfiguration()) {
            if (!outlookStreaming->isRunning()) {
                outlookStreaming->start(settings);
            }
        } else {
            outlookStreaming->stop();
        }
    };
    // Start streaming after a short delay to let the UI settle.
    QTimer::singleShot(4000, m_mainWindow.get(), [refreshOutlookStreamingConnection]() {
        refreshOutlookStreamingConnection();
    });

    QObject::connect(m_mainWindow.get(), &MainWindow::fileServiceDownloadRequested, m_mainWindow.get(),
        [&](const QString& messageId) {
            const auto messages = conversationRepository.loadMessages(currentConversationId.toStdWString());
            const ChatMessage* found = nullptr;
            for (const auto& msg : messages) {
                if (QString::fromStdWString(msg.messageId) == messageId) {
                    found = &msg;
                    break;
                }
            }
            if (!found) return;
            const auto payloadOpt = ResourceRefRouter::parsePayload(
                QString::fromStdWString(found->payloadJson).toUtf8());
            if (!payloadOpt || payloadOpt->kind != QStringLiteral("shared_file")) return;

            const QString fileId   = payloadOpt->resourceId;
            const QString fileName = payloadOpt->title.isEmpty()
                ? QStringLiteral("downloaded_file")
                : payloadOpt->title;

            const RemoteFileServiceConnectionSettings fsSettings =
                groupService.isGroupConversation(currentConversationId)
                    ? fileServiceConnectionSettingsForGroup(currentConversationId)
                    : RemoteFileServiceConnectionSettings{};
            const QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);

            m_mainWindow->setStatusMessage(QStringLiteral("正在下载文件…"), 0);

            // Extract download URL from card action target — correct per P0-2 fix
            QString downloadUrl;
            for (const auto& action : payloadOpt->actions) {
                if (action.actionId == QStringLiteral("download")) {
                    downloadUrl = action.target;
                    break;
                }
            }
            if (downloadUrl.isEmpty()) {
                m_mainWindow->setStatusMessage(QStringLiteral("共享文件卡片缺少下载地址"), 3000);
                return;
            }

            const QString fileNameCopy  = fileName;
            const QString downloadDirCopy = downloadDir;

            auto* fsWatcher = new QFutureWatcher<std::pair<QString, QString>>(m_mainWindow.get());
            QObject::connect(fsWatcher, &QFutureWatcher<std::pair<QString, QString>>::finished,
                             m_mainWindow.get(), [this, fsWatcher]() {
                const auto result = fsWatcher->result();
                fsWatcher->deleteLater();
                qInfo() << "[fsDownload] watcher finished, mainWindow=" << (m_mainWindow ? "valid" : "NULL");
                if (!result.first.isEmpty()) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(result.first));
                    m_mainWindow->setStatusMessage(
                        QStringLiteral("文件已下载：") + result.first, 3000);
                } else {
                    m_mainWindow->setStatusMessage(
                        QStringLiteral("下载失败：") + result.second, 4000);
                }
            });
            fsWatcher->setFuture(QtConcurrent::run([fsSettings, downloadUrl, fileNameCopy, downloadDirCopy]() -> std::pair<QString, QString> {
                RemoteFileServiceAdapter adapter(fsSettings);
                QString err;
                const auto path = adapter.downloadByUrl(downloadUrl, fileNameCopy, downloadDirCopy, &err);
                return {path.value_or(QString{}), err};
            }));
        });
    QObject::connect(m_mainWindow.get(), &MainWindow::fileServiceVersionHistoryRequested,
                     m_mainWindow.get(), [&](const QString& messageId) {
        const auto messages = conversationRepository.loadMessages(currentConversationId.toStdWString());
        const ChatMessage* found = nullptr;
        for (const auto& msg : messages) {
            if (QString::fromStdWString(msg.messageId) == messageId) {
                found = &msg;
                break;
            }
        }
        if (!found) return;
        const auto payloadOpt = ResourceRefRouter::parsePayload(
            QString::fromStdWString(found->payloadJson).toUtf8());
        if (!payloadOpt || payloadOpt->kind != QStringLiteral("shared_file")) return;

        const RemoteFileServiceConnectionSettings fsSettings =
            groupService.isGroupConversation(currentConversationId)
                ? fileServiceConnectionSettingsForGroup(currentConversationId)
                : RemoteFileServiceConnectionSettings{};
        const QString fileName = payloadOpt->title.isEmpty()
            ? QStringLiteral("shared_file")
            : payloadOpt->title;

        auto* dialog = new FileVersionHistoryDialog(
            payloadOpt->resourceId, fileName, fsSettings, m_mainWindow.get());
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->open();
    });
    QObject::connect(m_mainWindow.get(), &MainWindow::notificationMarkedReadRequested, m_mainWindow.get(),
                     [&, markSystemNotificationRead](const QString& notificationId) {
        markSystemNotificationRead(notificationId);
    });
    QObject::connect(m_mainWindow.get(), &MainWindow::notificationArchivedRequested, m_mainWindow.get(),
                     [&, archiveSystemNotification](const QString& notificationId) {
        archiveSystemNotification(notificationId);
    });
    QObject::connect(m_mainWindow.get(), &MainWindow::notificationsMarkAllReadRequested, m_mainWindow.get(),
                     [&, markAllSystemNotificationsRead]() {
        markAllSystemNotificationsRead();
    });

    // 正在输入指示器：发送端
    if (m_mainWindow->chatComposerWidget()) {
        QObject::connect(m_mainWindow->chatComposerWidget(), &ChatComposerWidget::typingActivity,
                         m_mainWindow.get(), [&]() {
            if (currentConversationId.isEmpty() || currentTargetId.isEmpty()) return;
            if (groupService.isGroupConversation(currentConversationId)) return;
            const QString resolvedTarget = resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
            PeerConnection* conn = connectionsByTargetId.value(resolvedTarget, nullptr);
            if (!conn || !conn->isConnected()) return;
            MessageEnvelope typingEnv;
            typingEnv.type = MessageType::TypingIndicator;
            typingEnv.senderId = localClientId.toStdString();
            typingEnv.targetId = resolvedTarget.toStdString();
            typingEnv.createdAtMs = QDateTime::currentMSecsSinceEpoch();
            conn->sendPayload(QByteArray::fromStdString(MessageCodec::encode(typingEnv)));
        });
    }

    QObject::connect(m_mainWindow.get(), &MainWindow::sendRequested, m_mainWindow.get(),
                     [&](const QString& body) {
                         try {
                           if (currentConversationId.isEmpty()) {
                               return;
                           }

                         // Capture reply context from composer before clearing
                         const QString replyToMsgId = m_mainWindow->chatComposerWidget()
                             ? m_mainWindow->chatComposerWidget()->replyToMessageId()
                             : QString();
                         const QString replyToSenderId = m_mainWindow->chatComposerWidget()
                             ? m_mainWindow->chatComposerWidget()->replyToSenderId()
                             : QString();
                         const QString replyToBody = m_mainWindow->chatComposerWidget()
                             ? m_mainWindow->chatComposerWidget()->replyToBody()
                             : QString();

                         // --- 群聊 fan-out ---
                         if (groupService.isGroupConversation(currentConversationId)) {
                             const auto groupOpt = groupRepository.findGroupById(
                                 currentConversationId.toStdWString());
                             const QString groupTitle = groupOpt
                                 ? QString::fromStdWString(groupOpt->groupName)
                                 : currentConversationId;
                             auto envelopes = groupService.buildGroupTextFanOut(
                                 localClientId, currentConversationId, body);
                             if (envelopes.empty()) {
                                 m_mainWindow->setStatusMessage(QStringLiteral("当前群暂无可发送成员"), 2500);
                                 scheduleChatUiRefresh(true, true, false, false, 0);
                                 return;
                             }

                             // 提取 @mention：从 body 中匹配 @DisplayName，
                             // 映射为 clientId 后写入所有 fan-out 信封
                             {
                                 const auto members = groupRepository.loadMembers(
                                     currentConversationId.toStdWString());
                                 // 构建 displayName → clientId 映射
                                 // 使用与 @弹出列表相同的名称解析链：PeerDirectory → 快照 → 回退，
                                 // 避免弹出列表显示实时名而此处仅用快照名导致匹配失败
                                 QHash<QString, QString> nameToId;
                                 for (const auto& m : members) {
                                     const QString cid = QString::fromStdWString(m.memberClientId).trimmed();
                                     if (cid.isEmpty()) continue;
                                     QString dn;
                                     const auto peerOpt = peerDirectoryService.findPeerByClientId(toUtf8(cid));
                                     if (peerOpt.has_value()) {
                                         dn = displayNameForPeer(*peerOpt);
                                     }
                                     if (dn.trimmed().isEmpty() && !m.memberDisplayNameSnapshot.empty()) {
                                         dn = QString::fromStdWString(m.memberDisplayNameSnapshot);
                                     }
                                     dn = dn.trimmed();
                                     if (!dn.isEmpty()) {
                                         nameToId.insert(dn, cid);
                                     }
                                 }

                                 // body 是 HTML，先转为纯文本再做正则匹配
                                 const QString plainBody = QTextDocumentFragment::fromHtml(body).toPlainText();
                                 // 正则匹配 @DisplayName（半角和全角 @）
                                 static const QRegularExpression mentionRx(
                                     QStringLiteral("[@\uFF20]([^@\uFF20\\s]+)"));
                                 std::vector<std::string> mentionedIds;
                                 QSet<QString> seen;
                                 auto it = mentionRx.globalMatch(plainBody);
                                 while (it.hasNext()) {
                                     const auto match = it.next();
                                     const QString name = match.captured(1).trimmed();
                                     if (name == QStringLiteral("\u6240\u6709\u4EBA")) {
                                         // "所有人" → __all__
                                         if (!seen.contains(QStringLiteral("__all__"))) {
                                             seen.insert(QStringLiteral("__all__"));
                                             mentionedIds.push_back("__all__");
                                         }
                                     } else if (nameToId.contains(name)) {
                                         const QString cid = nameToId.value(name);
                                         if (!seen.contains(cid)) {
                                             seen.insert(cid);
                                             mentionedIds.push_back(cid.toStdString());
                                         }
                                     }
                                 }
                                 if (!mentionedIds.empty()) {
                                     for (auto& env : envelopes) {
                                         env.mentionedIds = mentionedIds;
                                     }
                                 }
                             }

                             // Attach replyTo fields to all fan-out envelopes
                             if (!replyToMsgId.isEmpty()) {
                                 for (auto& env : envelopes) {
                                     env.replyToMessageId = replyToMsgId.toUtf8().toStdString();
                                     env.replyToSenderId = replyToSenderId.toUtf8().toStdString();
                                     env.replyToBody = replyToBody.toUtf8().toStdString();
                                 }
                             }
                             const RemoteChatServiceSettings remoteChatSettings =
                                 RemoteChatServiceSettingsStore::load();
                             if (remoteChatSettings.mode
                                 != RemoteChatTransportMode::P2POnly) {
                                 const ReliableGroupMessageP2PStatusCallback sendGroupTextFanOutViaP2P =
                                     [&](const ReliableGroupMessageP2PRequest& p2pRequest,
                                         QString* errorMessage) {
                                         GroupFanOutDeliveryResult emptyResult;
                                         const QString groupId =
                                             p2pRequest.groupId.trimmed();
                                         if (groupId.isEmpty()
                                             || p2pRequest.envelopes.empty()) {
                                             if (errorMessage) {
                                                 *errorMessage = QStringLiteral(
                                                     "group fan-out payload is empty");
                                             }
                                             emptyResult.failedCount = 1;
                                             return emptyResult;
                                         }

                                         std::vector<GroupFanOutPayload> payloads;
                                         const auto pending = buildPendingGroupFanOut(
                                             p2pRequest.envelopes, &payloads);
                                         if (!ChatService::persistOutgoingGroupFanOut(
                                                 &conversationRepository,
                                                 groupId,
                                                 p2pRequest.groupTitle,
                                                 pending,
                                                 p2pRequest.envelopes.front())) {
                                             qWarning().noquote()
                                                 << "[group-send] persist fan-out failed group="
                                                 << groupId.left(8);
                                             if (errorMessage) {
                                                 *errorMessage = QStringLiteral(
                                                     "group fan-out persistence failed");
                                             }
                                             emptyResult.failedCount =
                                                 static_cast<int>(p2pRequest.envelopes.size());
                                             return emptyResult;
                                         }

                                         GroupFanOutDeliveryOptions deliveryOptions;
                                         deliveryOptions.batchSize = 20;
                                         deliveryOptions.logPrefix =
                                             QStringLiteral("group-send");
                                         enablePendingCleanupForDelivery(&deliveryOptions);
                                        const GroupFanOutDeliveryResult deliveryResult =
                                            deliverGroupFanOutPayloadsWithStatus(
                                             payloads,
                                             nullptr,
                                             [&](const GroupFanOutPayload& payloadToSend) {
                                                 PeerConnection* conn =
                                                     connectionsByTargetId.value(
                                                         payloadToSend.targetId,
                                                         nullptr);
                                                 if (!conn || !conn->isConnected()) {
                                                     return GroupFanOutPayloadDisposition::Queued;
                                                 }
                                                 return conn->sendPayload(payloadToSend.blob)
                                                     ? GroupFanOutPayloadDisposition::Written
                                                     : GroupFanOutPayloadDisposition::Failed;
                                             },
                                             deliveryOptions);
                                        if (!isGroupFanOutDeliveryAccepted(
                                                p2pRequest.acceptQueuedOnlyDelivery,
                                                deliveryResult)) {
                                            qWarning().noquote()
                                                << "[group-send] p2p fan-out was not fully accepted"
                                                << "group=" << groupId.left(8)
                                                << "attempted=" << deliveryResult.attemptedCount
                                                << "delivered=" << deliveryResult.deliveredCount
                                                << "failed=" << deliveryResult.failedCount;
                                            if (errorMessage) {
                                                *errorMessage = QStringLiteral(
                                                    "p2p group message send was not fully accepted");
                                            }
                                            return deliveryResult;
                                        }
                                        return deliveryResult;
                                     };

                                 ServerMessageClient serverMessageClient(remoteChatSettings,
                                                                        localClientId);
                                 ReliableGroupMessageSender groupSender(
                                     localClientId,
                                     &conversationRepository,
                                     &serverMessageClient,
                                     sendGroupTextFanOutViaP2P);

                                 ReliableGroupMessageSendRequest sendRequest;
                                 sendRequest.groupId = currentConversationId;
                                 sendRequest.groupTitle = groupTitle;
                                 sendRequest.body = body;
                                 sendRequest.envelopes = envelopes;
                                 sendRequest.settings = remoteChatSettings;
                                 sendRequest.serviceReachable =
                                     remoteChatServiceRecentlyReachable(remoteChatSettings);
                                 sendRequest.p2pAvailable =
                                     !sendRequest.envelopes.empty();
                                 const RecipientRoutePartition recipientPartition =
                                     partitionRecipientsForMessageService(
                                         recipientIdsFromEnvelopes(sendRequest.envelopes),
                                         remoteChatSettings,
                                         serverMessageClient,
                                         sendRequest.serviceReachable);
                                 sendRequest.serverRecipientIds =
                                     recipientPartition.serverRecipientIds;
                                 sendRequest.p2pRecipientIds =
                                     recipientPartition.p2pRecipientIds;

                                 const ReliableGroupMessageSendResult sendResult =
                                     groupSender.sendText(sendRequest);
                                 if (sendResult.messageId.isEmpty()) {
                                     if (!sendResult.errorMessage.trimmed().isEmpty()) {
                                         m_mainWindow->setStatusMessage(
                                             sendResult.errorMessage, 3000);
                                     }
                                     return;
                                 }

                                 qInfo().noquote()
                                     << "[group-msg-send] msgId="
                                     << sendResult.messageId.left(8)
                                     << "group=" << currentConversationId.left(8)
                                     << "channel="
                                     << static_cast<int>(sendResult.channelUsed)
                                     << "sent=" << sendResult.success;
                                 if (!sendResult.success
                                     && !sendResult.errorMessage.trimmed().isEmpty()) {
                                     qWarning().noquote()
                                         << "[group-msg-send] failed:"
                                         << sendResult.errorMessage;
                                     m_mainWindow->setStatusMessage(
                                         sendResult.errorMessage, 3000);
                                 }
                                 if (m_mainWindow->chatComposerWidget()) {
                                     m_mainWindow->chatComposerWidget()
                                         ->clearReplyContext();
                                 }
                                 scheduleChatUiRefresh(true,
                                                       true,
                                                       sendResult.success,
                                                       sendResult.success,
                                                       0);
                                 return;
                             }
                             // 将所有离线入队 + 消息入库包在一个事务中，
                             // 避免 N 个离线成员 = N 次独立 DB 写入
                             std::vector<GroupFanOutPayload> payloads;
                             const auto pending = buildPendingGroupFanOut(envelopes, &payloads);
                             if (!ChatService::persistOutgoingGroupFanOut(&conversationRepository,
                                                                          currentConversationId,
                                                                          groupTitle,
                                                                          pending,
                                                                          envelopes.front())) {
                                 qWarning().noquote() << "[group-send] persist fan-out failed group="
                                                      << currentConversationId.left(8);
                                 m_mainWindow->setStatusMessage(QStringLiteral("群消息保存失败，未发送"), 3000);
                                 scheduleChatUiRefresh(true, true, false, false, 0);
                                 return;
                             }
                             // 收集在线发送列表，事务内仅处理离线入队
                             GroupFanOutDeliveryOptions deliveryOptions;
                             deliveryOptions.batchSize = 20;
                             deliveryOptions.logPrefix = QStringLiteral("group-send");
                             enablePendingCleanupForDelivery(&deliveryOptions);
                             deliverGroupFanOutPayloads(
                                 payloads,
                                 m_mainWindow.get(),
                                 [&](const GroupFanOutPayload& payloadToSend) {
                                     PeerConnection* conn = connectionsByTargetId.value(
                                         payloadToSend.targetId, nullptr);
                                     if (!conn || !conn->isConnected()) {
                                         return false;
                                     }
                                     return conn->sendPayload(payloadToSend.blob);
                                 },
                                 deliveryOptions);
                             // 群消息发送成功后清除回复上下文
                             if (m_mainWindow->chatComposerWidget()) {
                                 m_mainWindow->chatComposerWidget()->clearReplyContext();
                             }
                             scheduleChatUiRefresh(true, true, true, true, 0);
                             return;
                         }

                         if (currentTargetId.isEmpty()) {
                             return;
                         }

                         const QString resolvedTargetId =
                             resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
                         const QString canonicalConversationId =
                             DirectConversationAddressing::conversationIdForPeers(localClientId, resolvedTargetId);
                         if (canonicalConversationId.isEmpty()) {
                             return;
                         }

                         PeerConnection* connection =
                             ConnectionRegistryUtils::connectedConnectionForTarget(connectionsByTargetId,
                                                                                   peerIdsByConnection,
                                                                                   resolvedTargetId);
                         QPointer<PeerConnection> p2pConnection(connection);
                         const RemoteChatServiceSettings remoteChatSettings =
                             RemoteChatServiceSettingsStore::load();
                         ServerMessageClient serverMessageClient(remoteChatSettings,
                                                                localClientId);
                         const bool serviceReachable =
                             remoteChatServiceRecentlyReachable(remoteChatSettings);
                         bool receiverServerCapable = false;
                         {
                             const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                             constexpr qint64 kServerCapabilityRefreshMs = 30000;
                             RecipientCapabilityDecision capability =
                                 recipientCapabilityResolver.resolveFromCacheOnly(
                                     resolvedTargetId,
                                     nowMs);
                             if (recipientCapabilityResolver.shouldRefreshServerProfile(
                                     capability,
                                     nowMs,
                                     kServerCapabilityRefreshMs)
                                 && serviceReachable
                                 && remoteChatSettings.canUseMessageService()) {
                                 QString capabilityError;
                                 const std::optional<ServerClientCapabilityQueryResult> query =
                                     serverMessageClient.queryClientCapabilities(
                                         remoteChatSettings.workspaceId,
                                 QStringList{resolvedTargetId},
                                 MessageRoutingCapabilities::serverReceiveV1(),
                                 &capabilityError);
                             if (query.has_value()) {
                                 QHash<QString, QStringList> returnedCapabilities;
                                 for (const ServerClientCapabilityProfile& profile :
                                      query->profiles) {
                                     returnedCapabilities.insert(profile.clientId,
                                                                 profile.capabilities);
                                 }
                                 recipientCapabilityResolver.rememberServerQueryResult(
                                     QStringList{resolvedTargetId},
                                     returnedCapabilities,
                                     nowMs);
                                 capability =
                                     recipientCapabilityResolver.resolveFromCacheOnly(
                                         resolvedTargetId,
                                             nowMs);
                                 } else if (!capabilityError.trimmed().isEmpty()) {
                                     qInfo().noquote()
                                         << "[msg-send] capability query skipped/failure target="
                                         << resolvedTargetId.left(8)
                                         << "error=" << capabilityError;
                                 }
                             }
                             receiverServerCapable =
                                 capability.status
                                 == RecipientCapabilityStatus::ServerReceiveCapable;
                         }
                         ReliableDirectMessageSender directSender(
                             localClientId,
                             &conversationRepository,
                             &serverMessageClient,
                             [&](const ReliableDirectMessageP2PRequest& p2pRequest,
                                 QString* errorMessage) -> bool {
                                 MessageEnvelope envelope;
                                 QString buildError;
                                 const bool canBuildEnvelope =
                                     ChatService::buildEnvelope(localClientId,
                                                                &conversationRepository,
                                                                p2pRequest.messageId,
                                                                p2pRequest.targetId,
                                                                &envelope,
                                                                &buildError);
                                 if (!canBuildEnvelope) {
                                     if (errorMessage) {
                                         *errorMessage = buildError.trimmed().isEmpty()
                                             ? QStringLiteral("direct message envelope build failed")
                                             : buildError;
                                     }
                                     return false;
                                 }
                                 // MainWindow 始终以 HTML 发送富文本
                                 envelope.contentType = "html";
                                 // Attach reply context to envelope
                                 if (!p2pRequest.replyToMessageId.isEmpty()) {
                                     envelope.replyToMessageId =
                                         p2pRequest.replyToMessageId.toUtf8().toStdString();
                                     envelope.replyToSenderId =
                                         p2pRequest.replyToSenderId.toUtf8().toStdString();
                                     envelope.replyToBody =
                                         p2pRequest.replyToBody.toUtf8().toStdString();
                                 }
                                 if (!p2pConnection || !p2pConnection->isConnected()) {
                                     if (errorMessage) {
                                         *errorMessage =
                                             QStringLiteral("p2p peer is not connected");
                                     }
                                     return false;
                                 }
                                 return p2pConnection->sendPayload(
                                     QByteArray::fromStdString(MessageCodec::encode(envelope)));
                             });

                         ReliableDirectMessageSendRequest sendRequest;
                         sendRequest.conversationId = canonicalConversationId;
                         sendRequest.targetId = resolvedTargetId;
                         sendRequest.body = body;
                         sendRequest.replyToMessageId = replyToMsgId;
                         sendRequest.replyToSenderId = replyToSenderId;
                         sendRequest.replyToBody = replyToBody;
                         sendRequest.settings = remoteChatSettings;
                         sendRequest.serviceReachable = serviceReachable;
                         sendRequest.receiverServerCapable = receiverServerCapable;
                         sendRequest.p2pAvailable =
                             p2pConnection && p2pConnection->isConnected();
                         sendRequest.requireP2PDeliveryReceipt = true;
                         ensureLegacyP2PConnectionForTarget(
                             resolvedTargetId,
                             remoteChatSettings,
                             receiverServerCapable,
                             sendRequest.p2pAvailable,
                             QStringLiteral("direct-text"));

                         const ReliableDirectMessageSendResult sendResult =
                             directSender.sendText(sendRequest);
                         if (!sendResult.success
                             && !receiverServerCapable
                             && !sendRequest.p2pAvailable
                             && P2PConnectionPolicy::shouldStartPeerConnection(
                                 remoteChatSettings,
                                 P2PConnectionTrigger::ServiceFallback)) {
                             ensureLegacyP2PConnectionForTarget(
                                 resolvedTargetId,
                                 remoteChatSettings,
                                 receiverServerCapable,
                                 sendRequest.p2pAvailable,
                                 QStringLiteral("direct-text-fallback"));
                         }
                         if (sendResult.messageId.isEmpty()) {
                             return;
                         }

                         currentTargetId = resolvedTargetId;
                         currentConversationId = canonicalConversationId;
                         qInfo().noquote() << "[msg-send] msgId="
                                           << sendResult.messageId.left(8)
                                           << "sender=" << localClientId.left(8)
                                           << "target=" << resolvedTargetId.left(8)
                                           << "channel="
                                           << static_cast<int>(sendResult.channelUsed)
                                           << "sent=" << sendResult.success;
                         if (!sendResult.success
                             && !sendResult.errorMessage.trimmed().isEmpty()) {
                             qWarning().noquote() << "[msg-send] failed:"
                                                  << sendResult.errorMessage;
                         }
                         if (m_mainWindow->chatComposerWidget()) {
                             m_mainWindow->chatComposerWidget()->clearReplyContext();
                         }
                         syncSelectionState();
                          scheduleChatUiRefresh(true,
                                                true,
                                                sendResult.success,
                                                sendResult.success,
                                                0);
                         } catch (const std::exception& e) {
                             qCritical().noquote()
                                 << "[msg-send] exception in sendRequested handler:"
                                 << e.what();
                             m_mainWindow->setStatusMessage(
                                 QStringLiteral("\u53D1\u9001\u5931\u8D25\uFF0C\u8BF7\u7A0D\u540E\u91CD\u8BD5"),
                                 3000);
                         } catch (...) {
                             qCritical().noquote()
                                 << "[msg-send] unknown exception in sendRequested handler";
                             m_mainWindow->setStatusMessage(
                                 QStringLiteral("\u53D1\u9001\u5931\u8D25\uFF0C\u8BF7\u7A0D\u540E\u91CD\u8BD5"),
                                 3000);
                         }
                      });

    // ─── 贴纸消息发送 ───
    QObject::connect(m_mainWindow.get(), &MainWindow::stickerSendRequested, m_mainWindow.get(),
                     [&](const QString& packId, const QString& stickerId, const QByteArray& gifData) {
                         if (currentConversationId.isEmpty())
                             return;
                         constexpr qsizetype kMaxStickerBinaryBytes = 8 * 1024 * 1024;
                         if (gifData.size() > kMaxStickerBinaryBytes) {
                             m_mainWindow->setStatusMessage(
                                 QStringLiteral("贴纸文件过大，最大支持 8 MB"), 3000);
                             return;
                         }

                         // 构建 payloadJson：DB 版本不含 gif_base64，防止 WAL 无限膨胀
                         QJsonObject payloadObj;
                         payloadObj.insert(QStringLiteral("pack_id"), packId);
                         payloadObj.insert(QStringLiteral("sticker_id"), stickerId);
                         // DB 存储：只保留 pack_id / sticker_id
                         const QByteArray dbPayloadBytes = QJsonDocument(payloadObj).toJson(QJsonDocument::Compact);
                         const std::string dbPayloadStr(dbPayloadBytes.constData(),
                                                        static_cast<std::size_t>(dbPayloadBytes.size()));
                         // 网络传输：带 gif_base64 让接收方缓存图片
                         payloadObj.insert(QStringLiteral("gif_base64"),
                                          QString::fromLatin1(gifData.toBase64()));
                         const QByteArray payloadBytes = QJsonDocument(payloadObj).toJson(QJsonDocument::Compact);
                         const std::string payloadStr(payloadBytes.constData(),
                                                      static_cast<std::size_t>(payloadBytes.size()));
                         const QString bodyText = QStringLiteral("[\u8D34\u7EB8]");

                         // --- 群聊 fan-out ---
                         if (groupService.isGroupConversation(currentConversationId)) {
                             auto envelopes = groupService.buildGroupTextFanOut(
                                 localClientId, currentConversationId, bodyText);
                             if (envelopes.empty()) {
                                 m_mainWindow->setStatusMessage(QStringLiteral("当前群暂无可发送成员"), 2500);
                                 return;
                             }
                             for (auto& env : envelopes) {
                                 env.messageSubtype = "sticker";
                                 env.payloadJson = payloadStr;
                             }
                            auto selfEnv = envelopes.front();
                            selfEnv.messageSubtype = "sticker";
                            selfEnv.payloadJson = dbPayloadStr;
                            const auto groupOpt = groupRepository.findGroupById(
                                currentConversationId.toStdWString());
                            const QString groupTitle = groupOpt
                                ? QString::fromStdWString(groupOpt->groupName)
                                : currentConversationId;
                            std::vector<GroupFanOutPayload> payloads;
                            const auto pending = buildPendingGroupFanOut(envelopes, &payloads);
                            if (!ChatService::persistOutgoingGroupFanOut(&conversationRepository,
                                                                         currentConversationId,
                                                                         groupTitle,
                                                                         pending,
                                                                         selfEnv)) {
                                qWarning().noquote() << "[group-sticker] persist fan-out failed group="
                                                     << currentConversationId.left(8);
                                m_mainWindow->setStatusMessage(QStringLiteral("群贴纸保存失败，未发送"), 3000);
                                scheduleChatUiRefresh(true, true, false, false, 0);
                                return;
                            }
                            const ReliableGroupEnvelopeSendResult sendResult =
                                sendPersistedGroupEnvelopes(QStringLiteral("group-sticker"),
                                                            currentConversationId,
                                                            groupTitle,
                                                            envelopes);
                            if (!sendResult.success
                                && !sendResult.errorMessage.trimmed().isEmpty()) {
                                m_mainWindow->setStatusMessage(sendResult.errorMessage, 3000);
                            }
                             scheduleChatUiRefresh(true, true, true, true, 0);
                             return;
                         }

                         // --- 直接消息 ---
                         if (currentTargetId.isEmpty())
                             return;
                         const QString resolvedTargetId =
                             resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
                         const QString canonicalConversationId =
                             DirectConversationAddressing::conversationIdForPeers(localClientId, resolvedTargetId);
                         if (canonicalConversationId.isEmpty())
                             return;

                         const QString messageId = ChatService::createOutgoingMessage(
                             localClientId, &conversationRepository,
                             canonicalConversationId, resolvedTargetId, bodyText);
                         if (messageId.isEmpty())
                             return;
                         // 更新 DB 中的 messageType 和 payloadJson（不含 gif_base64）
                         conversationRepository.updateMessageFields(
                             messageId, QStringLiteral("sticker"),
                             QString::fromUtf8(dbPayloadBytes));

                         MessageEnvelope envelope;
                         const bool canBuild = ChatService::buildEnvelope(
                             localClientId, &conversationRepository, messageId,
                             resolvedTargetId, &envelope);
                         envelope.contentType = "plain";
                         envelope.messageSubtype = "sticker";
                         envelope.payloadJson = payloadStr;

                         if (!canBuild) {
                             scheduleChatUiRefresh(true, true, false, false, 0);
                             return;
                         }

                        const ReliableDirectEnvelopeSendResult sendResult =
                            sendDirectEnvelope(QStringLiteral("direct-sticker"),
                                               resolvedTargetId,
                                               envelope);
                        if (sendResult.success
                            && sendResult.channelUsed == TransportChannel::MessageService) {
                            if (ChatService::markMessageServerAcked(&conversationRepository, messageId)) {
                                const QString serverMessageId =
                                    sendResult.serverMessageId.trimmed();
                                if (!serverMessageId.isEmpty()
                                    && !conversationRepository.saveRemoteMessageIdMapping(
                                        serverMessageId,
                                        messageId)) {
                                    qWarning().noquote()
                                        << "[direct-sticker] failed to save server id mapping msgId="
                                        << messageId.left(8)
                                        << "serverId=" << serverMessageId.left(8);
                                }
                            }
                        } else if (sendResult.success
                                   && sendResult.channelUsed == TransportChannel::P2P) {
                            ChatService::markMessageSent(&conversationRepository, messageId);
                        } else if (!sendResult.errorMessage.trimmed().isEmpty()) {
                            m_mainWindow->setStatusMessage(sendResult.errorMessage, 3000);
                        }
                        scheduleChatUiRefresh(true,
                                              true,
                                              sendResult.success,
                                              sendResult.success,
                                              0);
                     });

    const auto eligibilityErrorText = [](MutationEligibility e) -> QString {
        switch (e) {
        case MutationEligibility::NotSender:          return QStringLiteral("只能撤回/编辑自己的消息");
        case MutationEligibility::WindowExpired:      return QStringLiteral("超过2分钟，无法撤回/编辑");
        case MutationEligibility::AlreadyRecalled:    return QStringLiteral("消息已撤回");
        case MutationEligibility::EditNotAllowedForType: return QStringLiteral("该类型消息不支持编辑");
        default:                                       return QStringLiteral("操作失败");
        }
    };

    [[maybe_unused]] const auto requestDirectRecallMessage = [&](const QString& messageId) {
        if (messageId.isEmpty() || currentTargetId.isEmpty()) {
            return;
        }
        const QString resolvedTargetId = resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const auto eligibility = MessageMutationService::evaluateLocalEligibility(
            &conversationRepository, messageId, localClientId, MessageMutationKind::Recall, nowMs);
        if (eligibility == MutationEligibility::MessageNotFound) {
            return;
        }
        if (eligibility != MutationEligibility::Eligible) {
            m_mainWindow->setStatusMessage(eligibilityErrorText(eligibility), 3000);
            return;
        }
        ChatMessage original;
        if (!conversationRepository.findMessageMutationStateById(messageId, &original)) {
            return;
        }
        MessageMutation mutation;
        mutation.targetMessageId = messageId;
        mutation.conversationId = QString::fromStdWString(original.conversationId);
        mutation.actorId = localClientId;
        mutation.kind = MessageMutationKind::Recall;
        mutation.mutatedAtMs = nowMs;
        const auto envelopeOpt = MessageMutationService::buildDirectMutationEnvelope(
            localClientId, resolvedTargetId, mutation);
        if (!envelopeOpt) {
            return;
        }
        const ReliableDirectEnvelopeSendResult sendResult =
            sendDirectEnvelope(QStringLiteral("direct-recall"),
                               resolvedTargetId,
                               *envelopeOpt);
        if (!sendResult.success) {
            m_mainWindow->setStatusMessage(
                sendResult.errorMessage.trimmed().isEmpty()
                    ? QStringLiteral("撤回失败：对方当前不可达")
                    : QStringLiteral("撤回失败：%1").arg(sendResult.errorMessage),
                3000);
            return;
        }
        if (!conversationRepository.applyMessageRecall(messageId, localClientId, nowMs)) {
            qWarning() << "[requestDirectRecallMessage] local apply failed for messageId=" << messageId;
            m_mainWindow->setStatusMessage(QStringLiteral("撤回失败：本地状态同步错误"), 3000);
            scheduleChatUiRefresh(true, true, false, false, 0);
            return;
        }
        conversationRepository.refreshConversationPreviewFromLatestVisibleMessage(
            mutation.conversationId);
        scheduleChatUiRefresh(true, true, false, false, 0);
    };

    [[maybe_unused]] const auto requestDirectEditMessage = [&](const QString& messageId, const QString& newBody) {
        if (messageId.isEmpty() || newBody.isEmpty() || currentTargetId.isEmpty()) {
            return;
        }
        const QString resolvedTargetId = resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const auto eligibility = MessageMutationService::evaluateLocalEligibility(
            &conversationRepository, messageId, localClientId, MessageMutationKind::Edit, nowMs);
        if (eligibility == MutationEligibility::MessageNotFound) {
            return;
        }
        if (eligibility != MutationEligibility::Eligible) {
            m_mainWindow->setStatusMessage(eligibilityErrorText(eligibility), 3000);
            return;
        }
        ChatMessage original;
        if (!conversationRepository.findMessageMutationStateById(messageId, &original)) {
            return;
        }
        MessageMutation mutation;
        mutation.targetMessageId = messageId;
        mutation.conversationId = QString::fromStdWString(original.conversationId);
        mutation.actorId = localClientId;
        mutation.kind = MessageMutationKind::Edit;
        mutation.newBody = newBody;
        mutation.newContentType = QStringLiteral("html");
        mutation.mutatedAtMs = nowMs;
        const auto envelopeOpt = MessageMutationService::buildDirectMutationEnvelope(
            localClientId, resolvedTargetId, mutation);
        if (!envelopeOpt) {
            return;
        }
        const ReliableDirectEnvelopeSendResult sendResult =
            sendDirectEnvelope(QStringLiteral("direct-edit"),
                               resolvedTargetId,
                               *envelopeOpt);
        if (!sendResult.success) {
            m_mainWindow->setStatusMessage(
                sendResult.errorMessage.trimmed().isEmpty()
                    ? QStringLiteral("编辑失败：对方当前不可达")
                    : QStringLiteral("编辑失败：%1").arg(sendResult.errorMessage),
                3000);
            return;
        }
        if (!conversationRepository.applyMessageEdit(messageId, localClientId, nowMs, newBody)) {
            qWarning() << "[requestDirectEditMessage] local apply failed for messageId=" << messageId;
            m_mainWindow->setStatusMessage(QStringLiteral("编辑失败：本地状态同步错误"), 3000);
            scheduleChatUiRefresh(true, true, false, false, 0);
            return;
        }
        conversationRepository.refreshConversationPreviewFromLatestVisibleMessage(
            mutation.conversationId);
        scheduleChatUiRefresh(true, true, false, false, 0);
    };

    [[maybe_unused]] const auto requestGroupRecallMessage = [&](const QString& messageId) {
        if (messageId.isEmpty() || currentConversationId.isEmpty()) return;

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const auto eligibility = MessageMutationService::evaluateLocalEligibility(
            &conversationRepository, messageId, localClientId, MessageMutationKind::Recall, nowMs);
        if (eligibility == MutationEligibility::MessageNotFound) return;
        if (eligibility != MutationEligibility::Eligible) {
            m_mainWindow->setStatusMessage(eligibilityErrorText(eligibility), 3000);
            return;
        }

        ChatMessage original;
        if (!conversationRepository.findMessageMutationStateById(messageId, &original)) return;

        MessageMutation mutation;
        mutation.targetMessageId = messageId;
        mutation.conversationId = currentConversationId;  // for group, conversationId IS groupId
        mutation.actorId = localClientId;
        mutation.kind = MessageMutationKind::Recall;
        mutation.mutatedAtMs = nowMs;

        const auto envelopes = groupService.buildGroupMutationFanOut(localClientId, currentConversationId, mutation);
        if (envelopes.empty()) {
            // No active recipients — V1 follows live delivery model; no offline mutation replay.
            m_mainWindow->setStatusMessage(QStringLiteral("撤回失败：当前没有在线成员"), 3000);
            return;
        }

        const auto groupOpt =
            groupRepository.findGroupById(currentConversationId.toStdWString());
        const QString groupTitle = groupOpt
            ? QString::fromStdWString(groupOpt->groupName)
            : currentConversationId;
        std::vector<GroupFanOutPayload> payloads;
        const auto pending = buildPendingGroupFanOut(envelopes, &payloads);
        if (!ChatService::persistPendingGroupFanOutOnly(&conversationRepository,
                                                        currentConversationId,
                                                        pending)) {
            qWarning().noquote() << "[group-recall] persist mutation fan-out failed group="
                                 << currentConversationId.left(8);
            m_mainWindow->setStatusMessage(QStringLiteral("撤回失败：通知保存失败"), 3000);
            scheduleChatUiRefresh(true, true, false, false, 0);
            return;
        }

        // 先保证群成员通知已进入可靠投递队列，再应用本地撤回。
        if (!conversationRepository.applyMessageRecall(messageId, localClientId, nowMs)) {
            const QString envelopeMessageId =
                QString::fromUtf8(envelopes.front().messageId.data(),
                                  static_cast<int>(envelopes.front().messageId.size()));
            for (const auto& pendingEnvelope : pending) {
                conversationRepository.deletePendingGroupEnvelopeForTargetMessage(
                    pendingEnvelope.targetId,
                    envelopeMessageId);
            }
            qWarning() << "[requestGroupRecallMessage] local apply failed for" << messageId;
            m_mainWindow->setStatusMessage(QStringLiteral("撤回失败：本地状态同步错误"), 3000);
            scheduleChatUiRefresh(true, true, false, false, 0);
            return;
        }

        const ReliableGroupEnvelopeSendResult sendResult =
            sendPersistedGroupEnvelopes(QStringLiteral("group-recall"),
                                        currentConversationId,
                                        groupTitle,
                                        envelopes);

        conversationRepository.refreshConversationPreviewFromLatestVisibleMessage(currentConversationId);
        scheduleChatUiRefresh(true, true, false, false, 0);

        if (!sendResult.success) {
            m_mainWindow->setStatusMessage(
                sendResult.errorMessage.trimmed().isEmpty()
                    ? QStringLiteral("已撤回，但通知发送失败")
                    : QStringLiteral("已撤回，但通知发送失败：%1")
                          .arg(sendResult.errorMessage),
                3000);
        }
    };

    [[maybe_unused]] const auto requestGroupEditMessage = [&](const QString& messageId, const QString& newBody) {
        if (messageId.isEmpty() || newBody.isEmpty() || currentConversationId.isEmpty()) return;

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const auto eligibility = MessageMutationService::evaluateLocalEligibility(
            &conversationRepository, messageId, localClientId, MessageMutationKind::Edit, nowMs);
        if (eligibility == MutationEligibility::MessageNotFound) return;
        if (eligibility != MutationEligibility::Eligible) {
            m_mainWindow->setStatusMessage(eligibilityErrorText(eligibility), 3000);
            return;
        }

        ChatMessage original;
        if (!conversationRepository.findMessageMutationStateById(messageId, &original)) return;

        MessageMutation mutation;
        mutation.targetMessageId = messageId;
        mutation.conversationId = currentConversationId;
        mutation.actorId = localClientId;
        mutation.kind = MessageMutationKind::Edit;
        mutation.newBody = newBody;
        mutation.newContentType = QStringLiteral("html");
        mutation.mutatedAtMs = nowMs;

        const auto envelopes = groupService.buildGroupMutationFanOut(localClientId, currentConversationId, mutation);
        if (envelopes.empty()) {
            // No active recipients — V1 follows live delivery model; no offline mutation replay.
            m_mainWindow->setStatusMessage(QStringLiteral("编辑失败：当前没有在线成员"), 3000);
            return;
        }

        const auto groupOpt =
            groupRepository.findGroupById(currentConversationId.toStdWString());
        const QString groupTitle = groupOpt
            ? QString::fromStdWString(groupOpt->groupName)
            : currentConversationId;
        std::vector<GroupFanOutPayload> payloads;
        const auto pending = buildPendingGroupFanOut(envelopes, &payloads);
        if (!ChatService::persistPendingGroupFanOutOnly(&conversationRepository,
                                                        currentConversationId,
                                                        pending)) {
            qWarning().noquote() << "[group-edit] persist mutation fan-out failed group="
                                 << currentConversationId.left(8);
            m_mainWindow->setStatusMessage(QStringLiteral("编辑失败：通知保存失败"), 3000);
            scheduleChatUiRefresh(true, true, false, false, 0);
            return;
        }

        // 先保证群成员通知已进入可靠投递队列，再应用本地编辑。
        if (!conversationRepository.applyMessageEdit(messageId, localClientId, nowMs, newBody)) {
            const QString envelopeMessageId =
                QString::fromUtf8(envelopes.front().messageId.data(),
                                  static_cast<int>(envelopes.front().messageId.size()));
            for (const auto& pendingEnvelope : pending) {
                conversationRepository.deletePendingGroupEnvelopeForTargetMessage(
                    pendingEnvelope.targetId,
                    envelopeMessageId);
            }
            qWarning() << "[requestGroupEditMessage] local apply failed for" << messageId;
            m_mainWindow->setStatusMessage(QStringLiteral("编辑失败：本地状态同步错误"), 3000);
            scheduleChatUiRefresh(true, true, false, false, 0);
            return;
        }

        const ReliableGroupEnvelopeSendResult sendResult =
            sendPersistedGroupEnvelopes(QStringLiteral("group-edit"),
                                        currentConversationId,
                                        groupTitle,
                                        envelopes);

        conversationRepository.refreshConversationPreviewFromLatestVisibleMessage(currentConversationId);
        scheduleChatUiRefresh(true, true, false, false, 0);

        if (!sendResult.success) {
            m_mainWindow->setStatusMessage(
                sendResult.errorMessage.trimmed().isEmpty()
                    ? QStringLiteral("已编辑，但通知发送失败")
                    : QStringLiteral("已编辑，但通知发送失败：%1")
                          .arg(sendResult.errorMessage),
                3000);
        }
    };

    QObject::connect(m_mainWindow.get(), &MainWindow::recallMessageRequested, m_mainWindow.get(),
        [&](const QString& messageId) {
            if (groupService.isGroupConversation(currentConversationId)) {
                requestGroupRecallMessage(messageId);
            } else {
                requestDirectRecallMessage(messageId);
            }
        });

    // 表情回应
    QObject::connect(m_mainWindow.get(), &MainWindow::reactionRequested, m_mainWindow.get(),
        [&](const QString& messageId, const QString& emoji) {
            if (messageId.isEmpty()) return;

            // 判断当前是否已选中相同 emoji → toggle 取消
            QString effectiveEmoji = emoji;
            {
                ChatMessage existingMsg{};
                if (conversationRepository.findMessageById(messageId, &existingMsg)) {
                    const QString reactionsStr = QString::fromStdWString(existingMsg.reactionsJson);
                    if (!reactionsStr.isEmpty()) {
                        const QJsonObject reactions = QJsonDocument::fromJson(reactionsStr.toUtf8()).object();
                        const QJsonArray reactors = reactions.value(emoji).toArray();
                        for (const auto& r : reactors) {
                            if (r.toString() == localClientId) {
                                effectiveEmoji = QString(); // toggle 取消
                                break;
                            }
                        }
                    }
                }
            }

            // 构造并发送 reaction envelope 给对方/群组
            MessageEnvelope env{};
            env.type = MessageType::MessageReaction;
            env.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            env.senderId = localClientId.toStdString();
            env.createdAtMs = QDateTime::currentMSecsSinceEpoch();
            // payloadJson: targetMessageId + emoji
            QJsonObject payloadObj;
            payloadObj.insert(QStringLiteral("targetMessageId"), messageId);
            payloadObj.insert(QStringLiteral("emoji"), effectiveEmoji);
            env.payloadJson = QString::fromUtf8(
                QJsonDocument(payloadObj).toJson(QJsonDocument::Compact)).toStdString();

            if (groupService.isGroupConversation(currentConversationId)) {
                env.conversationId = currentConversationId.toStdString();
                const auto recipients = groupService.activeRecipients(localClientId, currentConversationId);
                std::vector<MessageEnvelope> envelopes;
                envelopes.reserve(recipients.size());
                for (const QString& recipientId : recipients) {
                    env.targetId = recipientId.toStdString();
                    envelopes.push_back(env);
                }
                if (!envelopes.empty()) {
                    const auto groupOpt =
                        groupRepository.findGroupById(currentConversationId.toStdWString());
                    const QString groupTitle = groupOpt
                        ? QString::fromStdWString(groupOpt->groupName)
                        : currentConversationId;
                    std::vector<GroupFanOutPayload> payloads;
                    const auto pending = buildPendingGroupFanOut(envelopes, &payloads);
                    if (!ChatService::persistPendingGroupFanOutOnly(&conversationRepository,
                                                                    currentConversationId,
                                                                    pending)) {
                        qWarning().noquote() << "[group-reaction] persist fan-out failed group="
                                             << currentConversationId.left(8);
                        m_mainWindow->setStatusMessage(QStringLiteral("回应保存失败，未发送"), 3000);
                        scheduleChatUiRefresh(true, true, false, false, 0);
                        return;
                    }

                    ChatService::applyReaction(&conversationRepository,
                                               messageId,
                                               localClientId,
                                               effectiveEmoji);
                    const ReliableGroupEnvelopeSendResult sendResult =
                        sendPersistedGroupEnvelopes(QStringLiteral("group-reaction"),
                                                    currentConversationId,
                                                    groupTitle,
                                                    envelopes);
                    if (!sendResult.success
                        && !sendResult.errorMessage.trimmed().isEmpty()) {
                        m_mainWindow->setStatusMessage(sendResult.errorMessage, 3000);
                    }
                } else {
                    ChatService::applyReaction(&conversationRepository,
                                               messageId,
                                               localClientId,
                                               effectiveEmoji);
                }
            } else {
                QString targetId = DirectConversationAddressing::otherParticipant(
                    localClientId, currentConversationId);
                if (targetId.isEmpty()) {
                    targetId = resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
                }
                if (!targetId.isEmpty()) {
                    const QString canonicalConversationId =
                        DirectConversationAddressing::conversationIdForPeers(localClientId, targetId);
                    env.targetId = targetId.toStdString();
                    env.conversationId = canonicalConversationId.toStdString();
                    const ReliableDirectEnvelopeSendResult sendResult =
                        sendDirectEnvelope(QStringLiteral("direct-reaction"),
                                           targetId,
                                           env);
                    if (!sendResult.success) {
                        m_mainWindow->setStatusMessage(
                            sendResult.errorMessage.trimmed().isEmpty()
                                ? QStringLiteral("回应发送失败：对方当前不可达")
                                : QStringLiteral("回应发送失败：%1").arg(sendResult.errorMessage),
                            3000);
                        return;
                    }
                    ChatService::applyReaction(&conversationRepository,
                                               messageId,
                                               localClientId,
                                               effectiveEmoji);
                }
            }

            // 刷新 UI（需要 refreshMessages=true 以立即更新 reaction pill 显示）
            scheduleChatUiRefresh(true, true, false, false, 0);
        });

    // 群消息置顶
    QObject::connect(m_mainWindow.get(), &MainWindow::pinMessageRequested, m_mainWindow.get(),
        [&](const QString& messageId, const QString& bodyPreview, const QString& authorName) {
            if (!groupService.isGroupConversation(currentConversationId)) {
                return;
            }
            const auto groupOpt = groupRepository.findGroupById(currentConversationId.toStdWString());
            if (!groupOpt.has_value()) { return; }

            // 检查是否已达上限
            const int count = conversationRepository.pinnedMessageCount(currentConversationId);
            if (count >= 3) {
                m_mainWindow->setStatusMessage(QStringLiteral("\u7F6E\u9876\u5DF2\u8FBE\u4E0A\u9650\uFF083\u6761\uFF09"), 2000);
                return;
            }

            const QString pinnerName = localDisplayName.isEmpty() ? localClientId : localDisplayName;
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const auto envelopes = groupService.buildGroupPinFanOut(
                localClientId, currentConversationId, messageId, bodyPreview, authorName, pinnerName, false);
            if (!envelopes.empty()) {
                std::vector<GroupFanOutPayload> payloads;
                const auto pending = buildPendingGroupFanOut(envelopes, &payloads);
                if (!ChatService::persistPendingGroupFanOutOnly(&conversationRepository,
                                                                currentConversationId,
                                                                pending)) {
                    qWarning().noquote() << "[group-pin] persist fan-out failed group="
                                         << currentConversationId.left(8);
                    m_mainWindow->setStatusMessage(QStringLiteral("置顶通知保存失败，未置顶"), 3000);
                    return;
                }
            }

            if (!conversationRepository.pinMessageForConversation(
                    currentConversationId, messageId, localClientId, pinnerName,
                    authorName, bodyPreview, nowMs)) {
                if (!envelopes.empty()) {
                    const QString envelopeMessageId =
                        QString::fromUtf8(envelopes.front().messageId.data(),
                                          static_cast<int>(envelopes.front().messageId.size()));
                    for (const MessageEnvelope& envelope : envelopes) {
                        const QString targetId =
                            QString::fromUtf8(envelope.targetId.data(),
                                              static_cast<int>(envelope.targetId.size()));
                        conversationRepository.deletePendingGroupEnvelopeForTargetMessage(
                            targetId,
                            envelopeMessageId);
                    }
                }
                m_mainWindow->setStatusMessage(QStringLiteral("置顶失败：本地状态同步错误"), 3000);
                return;
            }

            // 刷新卡片
            const auto allPins = conversationRepository.loadPinnedMessages(currentConversationId);
            std::vector<PinnedCardInfo> cards;
            cards.reserve(allPins.size());
            for (const auto& p : allPins) {
                cards.push_back({p.messageId, p.pinnedBody, p.authorName, p.pinnerName});
            }
            m_mainWindow->setPinnedMessageCards(cards);

            // 插入系统消息：「XXX 置顶了一条消息」
            {
                const QString preview = bodyPreview.left(30);
                const QString sysBody = QStringLiteral("%1 \U0001F4CC \u7F6E\u9876\u4E86\u4E00\u6761\u6D88\u606F\uFF1A%2")
                    .arg(pinnerName, preview);
                const QString sysMsgSource = envelopes.empty()
                    ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                    : QString::fromUtf8(envelopes.front().messageId.data(),
                                        static_cast<int>(envelopes.front().messageId.size()));
                const QString sysMsgId = QStringLiteral("system-pin-%1").arg(sysMsgSource);
                ChatMessage sysMsg{
                    sysMsgId.toStdWString(),
                    currentConversationId.toStdWString(),
                    localClientId.toStdWString(),
                    sysBody.toStdWString(),
                    nowMs,
                    MessageDeliveryState::Read,
                    {}, {},
                    L"system",
                    {}
                };
                conversationRepository.appendMessage(sysMsg);
                scheduleChatUiRefresh(true, true, true, true);
            }

            if (!envelopes.empty()) {
                const QString groupTitle = QString::fromStdWString(groupOpt->groupName);
                const ReliableGroupEnvelopeSendResult sendResult =
                    sendPersistedGroupEnvelopes(QStringLiteral("group-pin"),
                                                currentConversationId,
                                                groupTitle,
                                                envelopes);
                if (!sendResult.success
                    && !sendResult.errorMessage.trimmed().isEmpty()) {
                    m_mainWindow->setStatusMessage(sendResult.errorMessage, 3000);
                }
            }
            m_mainWindow->setStatusMessage(QStringLiteral("\u5DF2\u7F6E\u9876"), 1500);
        });

    QObject::connect(m_mainWindow.get(), &MainWindow::unpinMessageRequested, m_mainWindow.get(),
        [&](const QString& messageId) {
            if (!groupService.isGroupConversation(currentConversationId)) {
                return;
            }

            // 取消前先查询被取消的置顶信息（用于系统消息和 fanout）
            QString unpinnedBody;
            {
                const auto pins = conversationRepository.loadPinnedMessages(currentConversationId);
                for (const auto& p : pins) {
                    if (p.messageId == messageId) {
                        unpinnedBody = p.pinnedBody;
                        break;
                    }
                }
            }

            const QString unpinnerName = localDisplayName.isEmpty() ? localClientId : localDisplayName;
            const auto envelopes = groupService.buildGroupPinFanOut(
                localClientId, currentConversationId, messageId, unpinnedBody, QString(), unpinnerName, true);
            if (!envelopes.empty()) {
                std::vector<GroupFanOutPayload> payloads;
                const auto pending = buildPendingGroupFanOut(envelopes, &payloads);
                if (!ChatService::persistPendingGroupFanOutOnly(&conversationRepository,
                                                                currentConversationId,
                                                                pending)) {
                    qWarning().noquote() << "[group-unpin] persist fan-out failed group="
                                         << currentConversationId.left(8);
                    m_mainWindow->setStatusMessage(QStringLiteral("取消置顶通知保存失败，未取消"), 3000);
                    return;
                }
            }

            if (!conversationRepository.unpinMessageForConversation(currentConversationId, messageId)) {
                if (!envelopes.empty()) {
                    const QString envelopeMessageId =
                        QString::fromUtf8(envelopes.front().messageId.data(),
                                          static_cast<int>(envelopes.front().messageId.size()));
                    for (const MessageEnvelope& envelope : envelopes) {
                        const QString targetId =
                            QString::fromUtf8(envelope.targetId.data(),
                                              static_cast<int>(envelope.targetId.size()));
                        conversationRepository.deletePendingGroupEnvelopeForTargetMessage(
                            targetId,
                            envelopeMessageId);
                    }
                }
                m_mainWindow->setStatusMessage(QStringLiteral("取消置顶失败：本地状态同步错误"), 3000);
                return;
            }

            // 刷新卡片
            const auto allPins = conversationRepository.loadPinnedMessages(currentConversationId);
            std::vector<PinnedCardInfo> cards;
            cards.reserve(allPins.size());
            for (const auto& p : allPins) {
                cards.push_back({p.messageId, p.pinnedBody, p.authorName, p.pinnerName});
            }
            m_mainWindow->setPinnedMessageCards(cards);

            // 插入系统消息：「XXX 取消了一条置顶消息」
            {
                const QString preview = unpinnedBody.left(30);
                const QString sysBody = preview.isEmpty()
                    ? QStringLiteral("%1 \u53D6\u6D88\u4E86\u4E00\u6761\u7F6E\u9876\u6D88\u606F")
                          .arg(unpinnerName)
                    : QStringLiteral("%1 \u53D6\u6D88\u4E86\u4E00\u6761\u7F6E\u9876\u6D88\u606F\uFF1A%2")
                          .arg(unpinnerName, preview);
                const QString sysMsgSource = envelopes.empty()
                    ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                    : QString::fromUtf8(envelopes.front().messageId.data(),
                                        static_cast<int>(envelopes.front().messageId.size()));
                const QString sysMsgId = QStringLiteral("system-pin-%1").arg(sysMsgSource);
                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                ChatMessage sysMsg{
                    sysMsgId.toStdWString(),
                    currentConversationId.toStdWString(),
                    localClientId.toStdWString(),
                    sysBody.toStdWString(),
                    nowMs,
                    MessageDeliveryState::Read,
                    {}, {},
                    L"system",
                    {}
                };
                conversationRepository.appendMessage(sysMsg);
                scheduleChatUiRefresh(true, true, true, true);
            }

            if (!envelopes.empty()) {
                const auto groupOpt =
                    groupRepository.findGroupById(currentConversationId.toStdWString());
                const QString groupTitle = groupOpt
                    ? QString::fromStdWString(groupOpt->groupName)
                    : currentConversationId;
                const ReliableGroupEnvelopeSendResult sendResult =
                    sendPersistedGroupEnvelopes(QStringLiteral("group-unpin"),
                                                currentConversationId,
                                                groupTitle,
                                                envelopes);
                if (!sendResult.success
                    && !sendResult.errorMessage.trimmed().isEmpty()) {
                    m_mainWindow->setStatusMessage(sendResult.errorMessage, 3000);
                }
            }
            m_mainWindow->setStatusMessage(QStringLiteral("\u5DF2\u53D6\u6D88\u7F6E\u9876"), 1500);
        });

    // ── 转发会话选择对话框（逐条+合并共用） ──
    const auto showForwardDialog = [&]() -> QString {
        const auto summaries = conversationRepository.loadConversationSummaries();
        if (summaries.empty()) {
            m_mainWindow->setStatusMessage(QStringLiteral("没有可选的会话"), 1500);
            return {};
        }
        QDialog dlg(m_mainWindow.get());
        dlg.setWindowTitle(QStringLiteral("转发到..."));
        dlg.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
        dlg.setWindowModality(Qt::WindowModal);
        dlg.resize(360, 480);
        dlg.setStyleSheet(QStringLiteral(
            "QDialog { background: %1; }"
            "QLineEdit { border: 1px solid %2; border-radius: 6px; padding: 8px 12px;"
            "  font-size: 13px; background: %1; color: %3; }"
            "QLineEdit:focus { border-color: %4; }"
            "QListWidget { border: none; background: transparent; outline: none; }"
            "QListWidget::item { padding: 10px 14px; border-radius: 8px; margin: 2px 4px;"
            "  font-size: 13px; color: %3; }"
            "QListWidget::item:hover { background: %5; }"
            "QListWidget::item:selected { background: %6; color: %4; }"
            "QPushButton { border: 1px solid %2; border-radius: 6px; padding: 8px 20px;"
            "  font-size: 13px; background: %1; color: %3; }"
            "QPushButton:hover { background: %5; }"
            "QPushButton#forwardConfirmBtn { border: none; background: %4; color: white; }"
            "QPushButton#forwardConfirmBtn:hover { background: %7; }"
        ).arg(AppStyle::windowBg(), AppStyle::border(), AppStyle::textPrimary(),
              AppStyle::accent(), AppStyle::accentSoft(), AppStyle::accentSoft(),
              AppStyle::accentHover()));

        auto* dlgLayout = new QVBoxLayout(&dlg);
        dlgLayout->setContentsMargins(16, 16, 16, 16);
        dlgLayout->setSpacing(12);

        auto* searchEdit = new QLineEdit(&dlg);
        searchEdit->setPlaceholderText(QStringLiteral("\U0001F50D 搜索会话..."));
        searchEdit->setClearButtonEnabled(true);
        dlgLayout->addWidget(searchEdit);

        auto* listWidget = new ElaListWidget(&dlg);
        listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        for (const auto& s : summaries) {
            const QString convId = QString::fromStdWString(s.conversationId);
            if (convId == currentConversationId) continue;
            QString displayTitle;
            const bool isGroup = groupService.isGroupConversation(convId);
            if (isGroup) {
                const auto groupOpt = groupRepository.findGroupById(convId.toStdWString());
                displayTitle = groupOpt
                    ? QString::fromStdWString(groupOpt->groupName).trimmed()
                    : QString::fromStdWString(s.title).trimmed();
            } else {
                const QString otherPeer =
                    DirectConversationAddressing::otherParticipant(localClientId, convId);
                if (!otherPeer.isEmpty()) {
                    const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(otherPeer));
                    if (peer.has_value()) {
                        displayTitle = displayNameForPeer(*peer);
                    }
                }
                if (displayTitle.isEmpty()) {
                    displayTitle = QString::fromStdWString(s.title).trimmed();
                }
            }
            if (displayTitle.isEmpty()) continue;
            static const QRegularExpression uuidPattern(
                QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
            if (uuidPattern.match(displayTitle).hasMatch()) continue;
            const QString prefix = isGroup ? QStringLiteral("\U0001F465 ") : QStringLiteral("\U0001F464 ");
            auto* item = new QListWidgetItem(prefix + displayTitle, listWidget);
            item->setData(Qt::UserRole, convId);
        }
        dlgLayout->addWidget(listWidget, 1);

        QObject::connect(searchEdit, &QLineEdit::textChanged, &dlg, [listWidget](const QString& text) {
            for (int i = 0; i < listWidget->count(); ++i) {
                auto* item = listWidget->item(i);
                item->setHidden(!text.isEmpty() && !item->text().contains(text, Qt::CaseInsensitive));
            }
        });
        QObject::connect(listWidget, &QListWidget::itemDoubleClicked, &dlg, [&dlg](QListWidgetItem*) {
            dlg.accept();
        });

        auto* btnLayout = new QHBoxLayout();
        btnLayout->setSpacing(10);
        btnLayout->addStretch();
        auto* cancelBtn = new QPushButton(QStringLiteral("取消"), &dlg);
        auto* confirmBtn = new QPushButton(QStringLiteral("转发"), &dlg);
        confirmBtn->setObjectName(QStringLiteral("forwardConfirmBtn"));
        confirmBtn->setDefault(true);
        btnLayout->addWidget(cancelBtn);
        btnLayout->addWidget(confirmBtn);
        dlgLayout->addLayout(btnLayout);
        QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
        QObject::connect(confirmBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

        if (dlg.exec() != QDialog::Accepted) return {};
        auto* selected = listWidget->currentItem();
        if (!selected) return {};
        return selected->data(Qt::UserRole).toString();
    };

    // 消息转发（逐条）
    QObject::connect(m_mainWindow.get(), &MainWindow::forwardMessageRequested, m_mainWindow.get(),
        [&](const QString& body, bool isFile, const QString& localFilePath, const QString& attachmentName, const QString& senderName) {
            Q_UNUSED(attachmentName)
            const QString targetConvId = showForwardDialog();
            if (targetConvId.isEmpty()) return;

            emit m_mainWindow->conversationSelected(targetConvId);

            QTimer::singleShot(50, m_mainWindow.get(), [&, body, isFile, localFilePath, senderName]() {
                if (isFile && !localFilePath.isEmpty() && QFile::exists(localFilePath)) {
                    // 文件转发：先发转发头，再发文件
                    if (!senderName.trimmed().isEmpty()) {
                        const QString forwardHeader = QStringLiteral(
                            "<div style='font-size:12px;color:%1;margin-bottom:4px;'>\u2197\uFE0F 转发自 <b>%2</b> 的文件</div>")
                            .arg(AppStyle::textMuted(), senderName.toHtmlEscaped());
                        emit m_mainWindow->sendRequested(forwardHeader);
                    }
                    QTimer::singleShot(50, m_mainWindow.get(), [&, localFilePath]() {
                        emit m_mainWindow->fileSendRequested(localFilePath);
                    });
                    m_mainWindow->setStatusMessage(QStringLiteral("已转发文件"), 1500);
                } else {
                    // 文本转发：包裹在转发引用块中
                    QString forwardedBody;
                    if (!senderName.trimmed().isEmpty()) {
                        forwardedBody = QStringLiteral(
                            "<div style='font-size:12px;color:%1;margin-bottom:4px;'>\u2197\uFE0F 转发自 <b>%2</b></div>"
                            "<blockquote style='border-left:3px solid %3;padding-left:8px;margin:4px 0;color:%4;'>%5</blockquote>")
                            .arg(AppStyle::textMuted(),
                                 senderName.toHtmlEscaped(),
                                 AppStyle::border(),
                                 AppStyle::textSecondary(),
                                 body.toHtmlEscaped());
                    } else {
                        forwardedBody = body;
                    }
                    emit m_mainWindow->sendRequested(forwardedBody);
                    m_mainWindow->setStatusMessage(QStringLiteral("已转发"), 1500);
                }
            });
        });

    // 消息转发（合并）
    QObject::connect(m_mainWindow.get(), &MainWindow::mergedForwardRequested, m_mainWindow.get(),
        [&](const QString& mergedHtml) {
            const QString targetConvId = showForwardDialog();
            if (targetConvId.isEmpty()) return;

            emit m_mainWindow->conversationSelected(targetConvId);

            QTimer::singleShot(50, m_mainWindow.get(), [&, mergedHtml]() {
                emit m_mainWindow->sendRequested(mergedHtml);
                m_mainWindow->setStatusMessage(QStringLiteral("已合并转发"), 1500);
            });
        });

    // 合并转发（forward_package 卡片）
    QObject::connect(m_mainWindow.get(), &MainWindow::mergedForwardPackageRequested, m_mainWindow.get(),
        [&](const QJsonObject& package) {
            const QString targetConvId = showForwardDialog();
            if (targetConvId.isEmpty()) return;

            emit m_mainWindow->conversationSelected(targetConvId);

            QTimer::singleShot(50, m_mainWindow.get(), [&, package, targetConvId]() {
                const QByteArray payloadBytes = QJsonDocument(package).toJson(QJsonDocument::Compact);
                const std::string payloadStr(payloadBytes.constData(),
                                             static_cast<std::size_t>(payloadBytes.size()));
                const QString bodyText = QStringLiteral("[\u804a\u5929\u8bb0\u5f55]");

                if (groupService.isGroupConversation(targetConvId)) {
                    auto envelopes = groupService.buildGroupTextFanOut(
                        localClientId, targetConvId, bodyText);
                    if (envelopes.empty()) {
                        m_mainWindow->setStatusMessage(QStringLiteral("当前群暂无可发送成员"), 2500);
                        return;
                    }
                    for (auto& env : envelopes) {
                        env.messageSubtype = "forward_package";
                        env.payloadJson = payloadStr;
                    }
                    auto selfEnv = envelopes.front();
                    selfEnv.messageSubtype = "forward_package";
                    selfEnv.payloadJson = payloadStr;
                    const auto groupOpt = groupRepository.findGroupById(
                        targetConvId.toStdWString());
                    const QString groupTitle = groupOpt
                        ? QString::fromStdWString(groupOpt->groupName)
                        : targetConvId;
                    const auto pending = buildPendingGroupFanOut(envelopes, nullptr);
                    if (!ChatService::persistOutgoingGroupFanOut(&conversationRepository,
                                                                 targetConvId,
                                                                 groupTitle,
                                                                 pending,
                                                                 selfEnv)) {
                        qWarning().noquote() << "[group-forward] persist fan-out failed group="
                                             << targetConvId.left(8);
                        m_mainWindow->setStatusMessage(QStringLiteral("群转发保存失败，未发送"), 3000);
                        scheduleChatUiRefresh(true, true, false, false, 0);
                        return;
                    }
                    const ReliableGroupEnvelopeSendResult sendResult =
                        sendPersistedGroupEnvelopes(QStringLiteral("group-forward"),
                                                    targetConvId,
                                                    groupTitle,
                                                    envelopes);
                    if (!sendResult.success) {
                        const QString statusText = sendResult.errorMessage.trimmed().isEmpty()
                            ? QStringLiteral("群合并转发已保存，等待连接后重试")
                            : sendResult.errorMessage.trimmed();
                        m_mainWindow->setStatusMessage(statusText, 3000);
                        scheduleChatUiRefresh(true, true, true, true, 0);
                        return;
                    }
                    scheduleChatUiRefresh(true, true, true, true, 0);
                } else {
                    // 直接消息
                    const QString resolvedTargetId =
                        resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
                    const QString canonicalConversationId =
                        DirectConversationAddressing::conversationIdForPeers(localClientId, resolvedTargetId);
                    if (canonicalConversationId.isEmpty()) return;

                    const QString messageId = ChatService::createOutgoingMessage(
                        localClientId, &conversationRepository,
                        canonicalConversationId, resolvedTargetId, bodyText);
                    if (messageId.isEmpty()) return;

                    conversationRepository.updateMessageFields(
                        messageId, QStringLiteral("forward_package"),
                        QString::fromUtf8(payloadBytes));

                    MessageEnvelope envelope;
                    const bool canBuild = ChatService::buildEnvelope(
                        localClientId, &conversationRepository, messageId,
                        resolvedTargetId, &envelope);
                    envelope.contentType = "plain";
                    envelope.messageSubtype = "forward_package";
                    envelope.payloadJson = payloadStr;

                    if (!canBuild) {
                        scheduleChatUiRefresh(true, true, false, false, 0);
                        return;
                    }

                    const ReliableDirectEnvelopeSendResult sendResult =
                        sendDirectEnvelope(QStringLiteral("direct-forward"),
                                           resolvedTargetId,
                                           envelope);
                    if (sendResult.success
                        && sendResult.channelUsed == TransportChannel::MessageService) {
                        if (ChatService::markMessageServerAcked(&conversationRepository, messageId)) {
                            const QString serverMessageId = sendResult.serverMessageId.trimmed();
                            if (!serverMessageId.isEmpty()
                                && !conversationRepository.saveRemoteMessageIdMapping(serverMessageId,
                                                                                      messageId)) {
                                qWarning().noquote()
                                    << "[direct-forward] failed to save server id mapping msgId="
                                    << messageId.left(8)
                                    << "serverId=" << serverMessageId.left(8);
                            }
                        }
                    } else if (sendResult.success
                               && sendResult.channelUsed == TransportChannel::P2P) {
                        ChatService::markMessageSent(&conversationRepository, messageId);
                    } else if (!sendResult.success) {
                        const QString statusText = sendResult.errorMessage.trimmed().isEmpty()
                            ? QStringLiteral("合并转发已保存，等待连接后重试")
                            : sendResult.errorMessage.trimmed();
                        m_mainWindow->setStatusMessage(statusText, 3000);
                        scheduleChatUiRefresh(true, true, false, false, 0);
                        return;
                    }
                    scheduleChatUiRefresh(true, true, false, false, 0);
                }
                m_mainWindow->setStatusMessage(QStringLiteral("\u5df2\u5408\u5e76\u8f6c\u53d1"), 1500);
            });
        });

    QObject::connect(m_mainWindow.get(), &MainWindow::editSaveRequested, m_mainWindow.get(),
        [&](const QString& messageId, const QString& newBody) {
            if (groupService.isGroupConversation(currentConversationId)) {
                requestGroupEditMessage(messageId, newBody);
            } else {
                requestDirectEditMessage(messageId, newBody);
            }
        });

    QObject::connect(m_mainWindow.get(), &MainWindow::devOpsInsertRequested, m_mainWindow.get(), [&]() {
        if (currentConversationId.isEmpty()) {
            m_mainWindow->setStatusMessage(QStringLiteral("请先选择一个会话"), 2500);
            return;
        }

        const AzureDevOpsConnectionSettings settings = AzureDevOpsSettingsStore::load();
        if (!settings.hasCredentialConfiguration()) {
            LeyoDialog::information(m_mainWindow.get(),
                                     QStringLiteral("Azure DevOps"),
                                     QStringLiteral("请先在通知页打开 DevOps 设置，补全 Azure DevOps 地址和 PAT；如果是手动输入编号，再选择组织和项目。"));
            return;
        }

        AzureDevOpsInsertDialog dialog(settings, m_mainWindow.get());
        dialog.setWindowModality(Qt::WindowModal);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        const auto locator = dialog.parsedLocator();
        if (!locator.has_value()) {
            return;
        }

        using DevOpsInsertResult = std::pair<std::optional<ResourceRefPayload>, QString>;
        auto* insertWatcher = new QFutureWatcher<DevOpsInsertResult>(m_mainWindow.get());
        QObject::connect(insertWatcher, &QFutureWatcher<DevOpsInsertResult>::finished,
                         m_mainWindow.get(), [&, insertWatcher]() {
            const auto result = insertWatcher->result();
            insertWatcher->deleteLater();
            qInfo() << "[devOpsInsert] watcher finished, mainWindow=" << (m_mainWindow ? "valid" : "NULL");
            if (!result.first.has_value()) {
                LeyoDialog::warning(m_mainWindow.get(),
                                     QStringLiteral("Azure DevOps"),
                                     result.second.trimmed().isEmpty()
                                         ? QStringLiteral("无法识别或加载当前 Azure DevOps 资源。")
                                         : result.second);
                return;
            }
            sendResourceReferencePayloadToCurrentConversation(
                *result.first,
                QString(),
                QString(),
                QStringLiteral("DevOps 卡片发送失败"));
        });
        insertWatcher->setFuture(QtConcurrent::run(
            [settings, locatorCopy = *locator]() mutable
            -> DevOpsInsertResult {
                LocalAzureDevOpsAdapter adapter(settings);
                QString errorMessage;
                auto payload = adapter.payloadForLocator(locatorCopy, &errorMessage);
                return {std::move(payload), errorMessage};
            }
        ));
    });
    QObject::connect(m_mainWindow.get(), &MainWindow::nudgeRequested, m_mainWindow.get(), [&]() {
        if (currentConversationId.isEmpty()) {
            m_mainWindow->setStatusMessage(QStringLiteral("\u8BF7\u5148\u9009\u62E9\u4E00\u4E2A\u4F1A\u8BDD"), 2500);
            return;
        }
        if (groupService.isGroupConversation(currentConversationId)) {
            const auto groupOpt = groupRepository.findGroupById(currentConversationId.toStdWString());
            const QString groupTitle = groupOpt ? QString::fromStdWString(groupOpt->groupName)
                                                : currentConversationId;
            const auto envelopes = groupService.buildGroupNudgeFanOut(localClientId, currentConversationId);
            if (envelopes.empty()) {
                m_mainWindow->setStatusMessage(QStringLiteral("\u5F53\u524D\u7FA4\u6682\u65F6\u6CA1\u6709\u53EF\u63D0\u9192\u7684\u6210\u5458"), 2500);
                return;
            }
            std::vector<GroupFanOutPayload> payloads;
            const auto pending = buildPendingGroupFanOut(envelopes, &payloads);
            if (!ChatService::persistOutgoingGroupFanOut(&conversationRepository,
                                                         currentConversationId,
                                                         groupTitle,
                                                         pending,
                                                         envelopes.front())) {
                qWarning().noquote() << "[group-nudge] persist fan-out failed group="
                                     << currentConversationId.left(8);
                m_mainWindow->setStatusMessage(QStringLiteral("群提醒保存失败，未发送"), 3000);
                scheduleChatUiRefresh(true, true, false, false, 0);
                return;
            }
            const ReliableGroupEnvelopeSendResult sendResult =
                sendPersistedGroupEnvelopes(QStringLiteral("group-nudge"),
                                            currentConversationId,
                                            groupTitle,
                                            envelopes);
            if (!sendResult.success
                && !sendResult.errorMessage.trimmed().isEmpty()) {
                m_mainWindow->setStatusMessage(sendResult.errorMessage, 3000);
            }
            scheduleChatUiRefresh(true, true, true, true, 0);
            return;
        }

        if (currentTargetId.isEmpty()) {
            m_mainWindow->setStatusMessage(QStringLiteral("\u8BF7\u5148\u9009\u62E9\u4E00\u4E2A\u79C1\u804A\u4F1A\u8BDD"), 2500);
            return;
        }

        const QString resolvedTargetId =
            resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
        const QString canonicalConversationId =
            DirectConversationAddressing::conversationIdForPeers(localClientId, resolvedTargetId);
        if (canonicalConversationId.isEmpty()) {
            return;
        }

        const QString reminderBody = QStringLiteral("\u3010\u7A97\u53E3\u6296\u52A8\u63D0\u9192\u3011");
        const QString messageId =
            ChatService::createOutgoingMessage(localClientId,
                                               &conversationRepository,
                                               canonicalConversationId,
                                               resolvedTargetId,
                                               reminderBody);
        if (messageId.isEmpty()) {
            m_mainWindow->setStatusMessage(QStringLiteral("\u63D0\u9192\u6D88\u606F\u521B\u5EFA\u5931\u8D25"), 2500);
            return;
        }
        conversationRepository.updateMessageFields(messageId,
                                                   QStringLiteral("nudge"),
                                                   QString());

        MessageEnvelope envelope;
        const bool canBuildEnvelope = ChatService::buildEnvelope(localClientId,
                                                                 &conversationRepository,
                                                                 messageId,
                                                                 resolvedTargetId,
                                                                 &envelope);
        envelope.contentType = "nudge";
        if (!canBuildEnvelope) {
            scheduleChatUiRefresh(true, true, false, false, 0);
            return;
        }

        const ReliableDirectEnvelopeSendResult sendResult =
            sendDirectEnvelope(QStringLiteral("direct-nudge"),
                               resolvedTargetId,
                               envelope);
        if (sendResult.success
            && sendResult.channelUsed == TransportChannel::MessageService) {
            if (ChatService::markMessageServerAcked(&conversationRepository, messageId)) {
                const QString serverMessageId = sendResult.serverMessageId.trimmed();
                if (!serverMessageId.isEmpty()
                    && !conversationRepository.saveRemoteMessageIdMapping(serverMessageId,
                                                                          messageId)) {
                    qWarning().noquote()
                        << "[direct-nudge] failed to save server id mapping msgId="
                        << messageId.left(8)
                        << "serverId=" << serverMessageId.left(8);
                }
            }
        } else if (sendResult.success
                   && sendResult.channelUsed == TransportChannel::P2P) {
            ChatService::markMessageSent(&conversationRepository, messageId);
        } else if (!sendResult.errorMessage.trimmed().isEmpty()) {
            m_mainWindow->setStatusMessage(sendResult.errorMessage, 3000);
        }
        scheduleChatUiRefresh(true, true, sendResult.success, sendResult.success, 0);
    });
    QObject::connect(m_mainWindow.get(), &MainWindow::fileSendRequested, m_mainWindow.get(), [&](const QString& filePath) {
        qInfo() << "[file-send] triggered filePath=" << filePath
                << "conversationId=" << currentConversationId
                << "targetId=" << currentTargetId;
        if (currentConversationId.isEmpty()) {
            qInfo() << "[file-send] REJECTED: conversationId is empty";
            m_mainWindow->setStatusMessage(QStringLiteral("\u8BF7\u5148\u9009\u62E9\u4E00\u4E2A\u4F1A\u8BDD\u518D\u53D1\u6587\u4EF6"),
                                           2500);
            return;
        }

        // --- 群聊文件 fan-out ---
        if (groupService.isGroupConversation(currentConversationId)) {
            HybridRoutingDecision routingDecision;
            if (m_runtimeArchitectureSnapshot) {
                routingDecision = HybridRoutingPolicy::decideGroupFileRouting(
                    *m_runtimeArchitectureSnapshot, currentConversationId);
            }
            const GroupFileServiceConfig groupFsCfg =
                effectiveGroupFileServiceConfigForGroup(currentConversationId);
            const auto sendRoute = decideGroupFileSendRoute(
                filePath, routingDecision, groupFsCfg);

            const QString routeModeText =
                sendRoute.mode == GroupFileSendMode::InlineAttachmentImage
                    ? QStringLiteral("INLINE_IMAGE")
                    : (sendRoute.mode == GroupFileSendMode::FileServiceUpload
                           ? QStringLiteral("SERVICE_UPLOAD")
                           : QStringLiteral("P2P_FILE"));
            qInfo() << "[file-send] routing decision:"
                    << "mode=" << routeModeText
                    << "isImage=" << sendRoute.isImage
                    << "hasBoundService=" << sendRoute.hasBoundService
                    << "sharedFilesEnabled=" << sendRoute.sharedFilesEnabled
                    << "hasUsableFileServiceConfig=" << sendRoute.hasUsableFileServiceConfig
                    << "serviceId=" << routingDecision.serviceId;

            if (sendRoute.mode == GroupFileSendMode::FileServiceUpload) {
                GroupFileServiceConfig uploadConfig = groupFsCfg;
                if (!routingDecision.workspaceId.trimmed().isEmpty()) {
                    uploadConfig.workspaceId = routingDecision.workspaceId;
                }
                const QString uploaderName = localDisplayName.isEmpty() ? localClientId : localDisplayName;
                const QString convId = currentConversationId;

                qInfo() << "[file-send] using GroupFileTransferService /api/v1/chat-files"
                        << "groupId=" << convId
                        << "workspaceId=" << uploadConfig.workspaceId;
                m_mainWindow->setStatusMessage(QStringLiteral("正在上传群文件…"), 0);
                groupFileTransferService.sendGroupFile(
                    convId, filePath, uploadConfig, uploaderName, localClientId);
                return;
            }

            // P2P 降级：图片直接推送（内联显示），非图片走 Offer-only 文件卡片
            {
                const bool isImageSend =
                    sendRoute.mode == GroupFileSendMode::InlineAttachmentImage;

                if (isImageSend) {
                    // ── 图片：为每个群成员创建 P2P 文件传输任务（与一对一相同），内联渲染 ──
                    m_mainWindow->setStatusMessage(QStringLiteral("正在发送图片…"), 0);
                    const auto taskIds = groupService.createOutgoingGroupFileTasks(
                        localClientId, currentConversationId, filePath, &fileTransferService);
                    if (taskIds.empty()) {
                        m_mainWindow->setStatusMessage(QStringLiteral("图片发送失败（无活跃成员或任务创建失败）"), 3500);
                        return;
                    }
                    const qint64 imgCreatedAtMs = QDateTime::currentMSecsSinceEpoch();
                    int imgSentCount = 0;
                    bool imgMessageInserted = false; // 群图片只插入一条消息，否则每个接收者都会变成一条
                    for (const QString& taskId : taskIds) {
                        FileTransferTask task;
                        if (!fileTransferService.loadTask(taskId, &task)) continue;
                        const QString recipientId = QString::fromStdWString(task.peerClientId);
                        if (!imgMessageInserted) {
                            ensureFileTransferMessage(task, localClientId,
                                                      MessageDeliveryState::Pending, imgCreatedAtMs);
                            imgMessageInserted = true;
                        }
                        PeerConnection* conn = connectionsByTargetId.value(recipientId, nullptr);
                        if (conn && conn->isConnected()) {
                            MessageEnvelope offerEnv;
                            if (fileTransferService.buildOfferEnvelope(task, localClientId,
                                    recipientId, QString(),
                                    activeFileTransferPort, &offerEnv)) {
                                conn->sendPayload(QByteArray::fromStdString(MessageCodec::encode(offerEnv)));
                                if (scheduleOptimisticDataPump) {
                                    scheduleOptimisticDataPump(taskId, recipientId);
                                }
                                scheduleOutgoingOfferRetry(taskId, recipientId);
                                ++imgSentCount;
                            }
                        }
                    }
                    scheduleChatUiRefresh(true, true, false, false, 0);
                    m_mainWindow->setStatusMessage(
                        QStringLiteral("图片已发送（%1/%2 成员在线）")
                            .arg(imgSentCount).arg(taskIds.size()), 3500);
                } else {
                    m_mainWindow->setStatusMessage(QStringLiteral("正在发送群文件通知..."), 0);
                    const QString uploaderName =
                        localDisplayName.isEmpty() ? localClientId : localDisplayName;
                    const auto fileCardEnvelopes = groupService.buildGroupFileOfferFanOut(
                        localClientId, currentConversationId, filePath, uploaderName);
                    if (fileCardEnvelopes.empty()) {
                        m_mainWindow->setStatusMessage(
                            QStringLiteral("群文件通知发送失败（无活跃成员）"), 3500);
                        return;
                    }

                    const auto& firstEnv = fileCardEnvelopes.front();
                    const QString msgId = QString::fromStdString(firstEnv.messageId);
                    const QString fileCardJsonStr =
                        QString::fromStdString(firstEnv.payloadJson);
                    const QJsonObject p2pFileCard =
                        QJsonDocument::fromJson(fileCardJsonStr.toUtf8()).object();
                    const QString body = QString::fromStdString(firstEnv.body);
                    QString groupTitle = currentConversationId;
                    if (const auto groupOpt =
                            groupRepository.findGroupById(currentConversationId.toStdWString())) {
                        const QString storedTitle =
                            QString::fromStdWString(groupOpt->groupName).trimmed();
                        if (!storedTitle.isEmpty()) {
                            groupTitle = storedTitle;
                        }
                    }

                    const auto sendGroupFileCardViaP2P =
                        [&](const ReliableGroupFileMessageP2PRequest& p2pRequest,
                            QString* errorMessage) -> bool {
                            const QString callbackGroupId = p2pRequest.groupId.trimmed();
                            if (callbackGroupId.isEmpty() || p2pRequest.envelopes.empty()) {
                                if (errorMessage) {
                                    *errorMessage = QStringLiteral(
                                        "group file fan-out payload is empty");
                                }
                                return false;
                            }

                            std::vector<GroupFanOutPayload> payloads;
                            const auto pending =
                                buildPendingGroupFanOut(p2pRequest.envelopes, &payloads);
                            QSqlDatabase fanOutDb = QSqlDatabase::database(
                                conversationRepository.connectionName(), false);
                            const bool fanOutTxStarted = fanOutDb.transaction();
                            bool persisted = fanOutTxStarted;
                            for (const auto& pendingEnvelope : pending) {
                                if (!persisted) {
                                    break;
                                }
                                persisted =
                                    conversationRepository.enqueuePendingGroupEnvelope(
                                        pendingEnvelope.targetId,
                                        callbackGroupId,
                                        pendingEnvelope.envelopeBlob,
                                        pendingEnvelope.createdAtMs);
                            }
                            if (!persisted) {
                                if (fanOutTxStarted) {
                                    fanOutDb.rollback();
                                }
                                qWarning().noquote()
                                    << "[group-file-card-direct] persist fan-out failed group="
                                    << callbackGroupId.left(8);
                                if (errorMessage) {
                                    *errorMessage = QStringLiteral(
                                        "group file fan-out persistence failed");
                                }
                                return false;
                            }
                            if (!fanOutDb.commit()) {
                                fanOutDb.rollback();
                                qWarning().noquote()
                                    << "[group-file-card-direct] commit failed group="
                                    << callbackGroupId.left(8);
                                if (errorMessage) {
                                    *errorMessage = QStringLiteral(
                                        "group file fan-out commit failed");
                                }
                                return false;
                            }

                            GroupFanOutDeliveryOptions deliveryOptions;
                            deliveryOptions.batchSize = 20;
                            deliveryOptions.logPrefix =
                                QStringLiteral("group-file-card-direct");
                            enablePendingCleanupForDelivery(&deliveryOptions);
                            const GroupFanOutDeliveryResult deliveryResult =
                                deliverGroupFanOutPayloads(
                                payloads,
                                nullptr,
                                [&](const GroupFanOutPayload& payloadToSend) {
                                    PeerConnection* conn =
                                        ConnectionRegistryUtils::connectedConnectionForTarget(
                                            connectionsByTargetId,
                                            peerIdsByConnection,
                                            payloadToSend.targetId);
                                    if (!conn || !conn->isConnected()) {
                                        return false;
                                    }
                                    return conn->sendPayload(payloadToSend.blob);
                                },
                                deliveryOptions);
                            if (!isGroupFanOutDeliveryAccepted(
                                    p2pRequest.acceptQueuedOnlyDelivery,
                                    deliveryResult)) {
                                qWarning().noquote()
                                    << "[group-file-card-direct] p2p fan-out was not fully accepted"
                                    << "group=" << callbackGroupId.left(8)
                                    << "attempted=" << deliveryResult.attemptedCount
                                    << "delivered=" << deliveryResult.deliveredCount
                                    << "failed=" << deliveryResult.failedCount;
                                if (errorMessage) {
                                    *errorMessage = QStringLiteral(
                                        "p2p group file fan-out was not fully accepted");
                                }
                                return false;
                            }
                            return true;
                        };

                    const RemoteChatServiceSettings remoteChatSettings =
                        RemoteChatServiceSettingsStore::load();
                    ServerMessageClient serverMessageClient(remoteChatSettings,
                                                            localClientId);
                    ReliableGroupFileMessageSender fileMessageSender(
                        localClientId,
                        &conversationRepository,
                        &serverMessageClient,
                        sendGroupFileCardViaP2P);

                    ReliableGroupFileMessageSendRequest sendRequest;
                    sendRequest.messageId = msgId;
                    sendRequest.createdAtMs = firstEnv.createdAtMs;
                    sendRequest.groupId = currentConversationId;
                    sendRequest.groupTitle = groupTitle;
                    sendRequest.body = body;
                    sendRequest.localFilePath = filePath;
                    sendRequest.localFileCard = p2pFileCard;
                    sendRequest.broadcastFileCard = p2pFileCard;
                    sendRequest.envelopes = fileCardEnvelopes;
                    sendRequest.settings = remoteChatSettings;
                    sendRequest.serviceReachable =
                        remoteChatServiceRecentlyReachable(remoteChatSettings);
                    sendRequest.p2pAvailable = !sendRequest.envelopes.empty();
                    const RecipientRoutePartition recipientPartition =
                        partitionRecipientsForMessageService(
                            recipientIdsFromEnvelopes(sendRequest.envelopes),
                            remoteChatSettings,
                            serverMessageClient,
                            sendRequest.serviceReachable);
                    sendRequest.serverRecipientIds =
                        recipientPartition.serverRecipientIds;
                    sendRequest.p2pRecipientIds =
                        recipientPartition.p2pRecipientIds;

                    const ReliableGroupFileMessageSendResult sendResult =
                        fileMessageSender.sendFileCard(sendRequest);
                    qInfo().noquote()
                        << "[group-file-card-direct] msgId=" << sendResult.messageId.left(8)
                        << "group=" << currentConversationId.left(8)
                        << "channel=" << static_cast<int>(sendResult.channelUsed)
                        << "sent=" << sendResult.success;

                    if (sendResult.success) {
                        if (sendResult.channelUsed == TransportChannel::MessageService) {
                            m_mainWindow->setStatusMessage(
                                QStringLiteral("群文件已保存到消息服务"), 3000);
                        } else if (sendResult.channelUsed == TransportChannel::Mixed) {
                            m_mainWindow->setStatusMessage(
                                QStringLiteral("群文件已通过消息服务和 P2P 发送"), 3000);
                        } else if (sendResult.channelUsed == TransportChannel::P2P) {
                            m_mainWindow->setStatusMessage(
                                QStringLiteral("群文件已通过 P2P 通知"), 3000);
                        } else {
                            m_mainWindow->setStatusMessage(
                                QStringLiteral("群文件通知已保存"), 3000);
                        }
                    } else {
                        qWarning().noquote()
                            << "[group-file-card-direct] send failed:"
                            << sendResult.errorMessage;
                        m_mainWindow->setStatusMessage(
                            sendResult.errorMessage.trimmed().isEmpty()
                                ? QStringLiteral("群文件通知待重试")
                                : sendResult.errorMessage,
                            3000);
                    }
                    scheduleChatUiRefresh();
                }
            }
            return;
        }

        if (currentTargetId.isEmpty()) {
            qInfo() << "[file-send] REJECTED: targetId is empty";
            m_mainWindow->setStatusMessage(QStringLiteral("\u8BF7\u5148\u9009\u62E9\u4E00\u4E2A\u4F1A\u8BDD\u518D\u53D1\u6587\u4EF6"),
                                           2500);
            return;
        }

        const QFileInfo sendFileInfo(filePath);
        if (!sendFileInfo.exists() || !sendFileInfo.isFile()) {
            qInfo() << "[file-send] REJECTED: file does not exist or is not a file:" << filePath;
            m_mainWindow->setStatusMessage(QStringLiteral("\u6587\u4EF6\u4E0D\u5B58\u5728\u6216\u4E0D\u662F\u666E\u901A\u6587\u4EF6"), 3000);
            return;
        }
        if (sendFileInfo.size() <= 0) {
            qInfo() << "[file-send] REJECTED: file is empty (0 bytes):" << filePath;
            m_mainWindow->setStatusMessage(QStringLiteral("\u7A7A\u6587\u4EF6\u65E0\u6CD5\u53D1\u9001"), 3000);
            return;
        }

        const QString resolvedTargetId =
            resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
        PeerConnection* connection =
            ConnectionRegistryUtils::connectedConnectionForTarget(
                connectionsByTargetId,
                peerIdsByConnection,
                resolvedTargetId);
        const bool peerOnline = connection && connection->isConnected();
        if (!peerOnline) {
            preflightDirectFileP2PForTarget(resolvedTargetId,
                                            QStringLiteral("direct-file-send"));
            qInfo() << "[file-send] peer route unavailable; starting on-demand P2P, resolvedTargetId="
                    << resolvedTargetId;
        }

        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(localClientId, resolvedTargetId);
        m_mainWindow->setStatusMessage(QStringLiteral("\u6B63\u5728\u5206\u6790\u6587\u4EF6\uFF0C\u8BF7\u7A0D\u5019..."), 0);
        qInfo() << "[file-send] starting async hash for" << filePath
                << "fileSize=" << sendFileInfo.size();
        auto* watcher = new QFutureWatcher<
            std::optional<FileTransferService::PreparedOutgoingFile>>(m_mainWindow.get());
        QObject::connect(
            watcher,
            &QFutureWatcher<std::optional<FileTransferService::PreparedOutgoingFile>>::finished,
            m_mainWindow.get(),
            [watcher, &fileTransferService, &ensureFileTransferMessage,
             &scheduleOptimisticDataPump, &scheduleOutgoingOfferRetry,
             &scheduleChatUiRefresh, &connectionsByTargetId, &peerIdsByConnection,
             &preflightDirectFileP2PForTarget,
             localClientId, resolvedTargetId, conversationId, filePath,
             activeFileTransferPort, this]() {
                watcher->deleteLater();
                const auto preparedFile = watcher->result();
                const QString fileHash = preparedFile.has_value()
                    ? preparedFile->contentHash
                    : QString();
                qInfo() << "[file-send] hash completed, hash=" << (fileHash.isEmpty() ? "EMPTY" : fileHash.left(16))
                        << "filePath=" << filePath;
                if (!preparedFile.has_value() || fileHash.isEmpty()) {
                    qInfo() << "[file-send] REJECTED: hash is empty";
                    m_mainWindow->setStatusMessage(
                        QStringLiteral("\u6587\u4EF6\u54C8\u5E0C\u8BA1\u7B97\u5931\u8D25"), 3000);
                    return;
                }

                const qint64 taskCreatedAtMs = QDateTime::currentMSecsSinceEpoch();
                FileTransferTask transferTask;
                if (!fileTransferService.createOutgoingTask(conversationId,
                                                            resolvedTargetId,
                                                            QString(),
                                                            filePath,
                                                            taskCreatedAtMs,
                                                            &transferTask,
                                                            fileHash,
                                                            preparedFile->fileSize)) {
                    m_mainWindow->setStatusMessage(
                        QStringLiteral("\u6587\u4EF6\u4EFB\u52A1\u521B\u5EFA\u5931\u8D25"),
                        3500);
                    return;
                }

                if (!ensureFileTransferMessage(transferTask,
                                               localClientId,
                                               MessageDeliveryState::Pending,
                                               taskCreatedAtMs)) {
                    m_mainWindow->setStatusMessage(
                        QStringLiteral("\u6587\u4EF6\u5360\u4F4D\u6D88\u606F\u521B\u5EFA\u5931\u8D25"),
                        3500);
                    return;
                }

                PeerConnection* activeConnection =
                    ConnectionRegistryUtils::connectedConnectionForTarget(
                        connectionsByTargetId,
                        peerIdsByConnection,
                        resolvedTargetId);
                if (!activeConnection || !activeConnection->isConnected()) {
                    // 对方离线：将任务状态设为 PendingOffer，对方上线时自动重发
                    const QString taskId = QString::fromStdWString(transferTask.taskId);
                    fileTransferService.markTaskState(taskId,
                                                     FileTransferState::PendingOffer,
                                                     0, -1, QString(), QString(),
                                                     QDateTime::currentMSecsSinceEpoch());
                    preflightDirectFileP2PForTarget(resolvedTargetId, QStringLiteral("direct-file-post-hash"));
                    scheduleChatUiRefresh(true, true, false, false, 0);
                    m_mainWindow->setStatusMessage(
                        QStringLiteral("\u5BF9\u65B9\u5F53\u524D\u4E0D\u5728\u7EBF\uFF0C\u6587\u4EF6\u5C06\u5728\u5BF9\u65B9\u4E0A\u7EBF\u540E\u81EA\u52A8\u53D1\u9001"),
                        3500);
                    return;
                }

                MessageEnvelope offerEnvelope;
                if (!fileTransferService.buildOfferEnvelope(transferTask,
                                                            localClientId,
                                                            resolvedTargetId,
                                                            QString(),
                                                            activeFileTransferPort,
                                                            &offerEnvelope)) {
                    m_mainWindow->setStatusMessage(
                        QStringLiteral("\u6587\u4EF6\u4F20\u8F93\u8BF7\u6C42\u521B\u5EFA\u5931\u8D25"),
                        3500);
                    return;
                }

                activeConnection->sendPayload(
                    QByteArray::fromStdString(MessageCodec::encode(offerEnvelope)));
                if (scheduleOptimisticDataPump) {
                    scheduleOptimisticDataPump(QString::fromStdWString(transferTask.taskId), resolvedTargetId);
                }
                scheduleOutgoingOfferRetry(QString::fromStdWString(transferTask.taskId), resolvedTargetId);
                scheduleChatUiRefresh(true, true, false, false, 0);
            });
        watcher->setFuture(QtConcurrent::run(FileTransferService::prepareOutgoingFile, filePath));
        return;
    });
    QObject::connect(m_mainWindow.get(), &MainWindow::retryPendingRequested, m_mainWindow.get(), [&]() {
        if (currentConversationId.isEmpty() || currentTargetId.isEmpty()) {
            m_mainWindow->setStatusMessage(
                QStringLiteral("\u8BF7\u5148\u9009\u62E9\u4E00\u4E2A\u4F1A\u8BDD\u518D\u91CD\u8BD5"), 2500);
            return;
        }

        const QString resolvedTargetId =
            resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
        const int pendingCount = pendingMessageCountForTarget(resolvedTargetId);
        const int resumableFileTaskCount = resumableOutgoingFileTaskCountForTarget(resolvedTargetId);
        if (pendingCount <= 0 && resumableFileTaskCount <= 0) {
            m_mainWindow->setStatusMessage(
                QStringLiteral("\u5F53\u524D\u4F1A\u8BDD\u6CA1\u6709\u5F85\u53D1\u6D88\u606F\u6216\u6587\u4EF6\u4EFB\u52A1"),
                                           2500);
            return;
        }

        PeerConnection* connection = connectionsByTargetId.value(resolvedTargetId, nullptr);
        if (!connection || !connection->isConnected()) {
            m_mainWindow->setStatusMessage(
                QStringLiteral("\u5F53\u524D\u672A\u8FDE\u63A5\u5230\u5BF9\u65B9\uFF0C\u6682\u65F6\u65E0\u6CD5\u91CD\u8BD5"),
                3000);
            return;
        }

        const int resentCount = flushPendingMessagesForTarget(resolvedTargetId, connection);
        const int reofferedFileCount = resendPendingFileOffersForTarget(resolvedTargetId, connection);
        const int resumeRequestedCount = requestFileTransferResumeForTarget(resolvedTargetId, connection);
        if (resentCount > 0 || reofferedFileCount > 0 || resumeRequestedCount > 0) {
            QStringList statusParts;
            if (resentCount > 0) {
                statusParts.push_back(
                    QStringLiteral("\u8865\u53D1 %1 \u6761\u5F85\u53D1\u6D88\u606F").arg(QString::number(resentCount)));
            }
            if (reofferedFileCount > 0) {
                statusParts.push_back(
                    QStringLiteral("\u91CD\u53D1 %1 \u4E2A\u6587\u4EF6\u9080\u8BF7").arg(QString::number(reofferedFileCount)));
            }
            if (resumeRequestedCount > 0) {
                statusParts.push_back(
                    QStringLiteral("\u7EED\u4F20 %1 \u4E2A\u6587\u4EF6\u4EFB\u52A1").arg(QString::number(resumeRequestedCount)));
            }
            m_mainWindow->setStatusMessage(QStringLiteral("\u5DF2\u624B\u52A8%1").arg(statusParts.join(QStringLiteral("\uFF0C"))),
                                           3500);
            return;
        }

        const int unsupportedCount = unsupportedPendingMessageCountForTarget(resolvedTargetId);
        if (unsupportedCount > 0) {
            m_mainWindow->setStatusMessage(
                QStringLiteral("\u5F53\u524D\u7248\u672C\u6682\u4E0D\u652F\u6301\u91CD\u8BD5 %1 \u6761\u6587\u4EF6\u6D88\u606F")
                    .arg(QString::number(unsupportedCount)),
                3500);
            return;
        }

        m_mainWindow->setStatusMessage(QStringLiteral("\u91CD\u8BD5\u672A\u6210\u529F\uFF0C\u8BF7\u7A0D\u540E\u518D\u8BD5"),
                                       3000);
    });
    QObject::connect(m_mainWindow.get(), &MainWindow::retryMessageRequested, m_mainWindow.get(),
                     [&](const QString& messageId) {
                         if (messageId.trimmed().isEmpty() || currentConversationId.isEmpty()
                             || currentTargetId.isEmpty()) {
                             return;
                         }

                         const QString resolvedTargetId =
                             resolvedTargetIdsByAlias.value(currentTargetId, currentTargetId);
                         const RemoteChatServiceSettings remoteChatSettings =
                             RemoteChatServiceSettingsStore::load();
                         ServerMessageClient serverMessageClient(remoteChatSettings,
                                                                localClientId);
                         const bool serviceReachable =
                             remoteChatServiceRecentlyReachable(remoteChatSettings);
                         const bool receiverServerCapable =
                             resolveDirectRecipientServerCapability(
                                 resolvedTargetId,
                                 remoteChatSettings,
                                 serverMessageClient,
                                 serviceReachable);
                         PeerConnection* connection =
                             ConnectionRegistryUtils::connectedConnectionForTarget(
                                 connectionsByTargetId,
                                 peerIdsByConnection,
                                 resolvedTargetId);
                         QPointer<PeerConnection> p2pConnection(connection);

                         ReliableDirectMessageSender directSender(
                             localClientId,
                             &conversationRepository,
                             &serverMessageClient,
                             [&](const ReliableDirectMessageP2PRequest& p2pRequest,
                                 QString* errorMessage) -> bool {
                                 MessageEnvelope envelope;
                                 QString buildError;
                                 if (!ChatService::buildEnvelope(
                                         localClientId,
                                         &conversationRepository,
                                         p2pRequest.messageId,
                                         p2pRequest.targetId,
                                         &envelope,
                                         &buildError)) {
                                     if (errorMessage) {
                                         *errorMessage = buildError.trimmed().isEmpty()
                                             ? QStringLiteral("direct message envelope build failed")
                                             : buildError;
                                     }
                                     return false;
                                 }
                                 envelope.contentType = "html";
                                 if (!p2pConnection
                                     || !p2pConnection->isConnected()) {
                                     if (errorMessage) {
                                         *errorMessage = QStringLiteral(
                                             "p2p peer is not connected");
                                     }
                                     return false;
                                 }
                                 return p2pConnection->sendPayload(
                                     QByteArray::fromStdString(
                                         MessageCodec::encode(envelope)));
                             });

                         ReliableDirectMessageSendRequest sendRequest;
                         sendRequest.conversationId = currentConversationId;
                         sendRequest.targetId = resolvedTargetId;
                         sendRequest.settings = remoteChatSettings;
                         sendRequest.serviceReachable = serviceReachable;
                         sendRequest.receiverServerCapable =
                             receiverServerCapable;
                         sendRequest.p2pAvailable =
                             p2pConnection && p2pConnection->isConnected();
                         sendRequest.requireP2PDeliveryReceipt = true;
                         ensureLegacyP2PConnectionForTarget(
                             resolvedTargetId,
                             remoteChatSettings,
                             receiverServerCapable,
                             sendRequest.p2pAvailable,
                             QStringLiteral("direct-text-retry"));

                         const ReliableDirectMessageSendResult sendResult =
                             directSender.retryText(messageId, sendRequest);
                         if (!sendResult.success
                             && !receiverServerCapable
                             && !sendRequest.p2pAvailable) {
                             ensureLegacyP2PConnectionForTarget(
                                 resolvedTargetId,
                                 remoteChatSettings,
                                 receiverServerCapable,
                                 sendRequest.p2pAvailable,
                                 QStringLiteral("direct-text-retry-fallback"));
                         }

                         scheduleChatUiRefresh(true,
                                               true,
                                               sendResult.success,
                                               sendResult.success,
                                               0);
                         if (sendResult.success) {
                             m_mainWindow->setStatusMessage(
                                 QStringLiteral("\u5DF2\u91CD\u8BD5\u53D1\u9001\u6D88\u606F"),
                                 2500);
                         } else if (!sendRequest.p2pAvailable
                                    && !receiverServerCapable) {
                             m_mainWindow->setStatusMessage(
                                 QStringLiteral("\u6B63\u5728\u8FDE\u63A5\u5BF9\u65B9\uFF0C\u8FDE\u63A5\u540E\u5C06\u81EA\u52A8\u8865\u53D1"),
                                 3000);
                         } else {
                             m_mainWindow->setStatusMessage(
                                 QStringLiteral("\u53D1\u9001\u5931\u8D25\uFF0C\u8BF7\u7A0D\u540E\u91CD\u8BD5"),
                                 3000);
                         }
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::retryTransferRequested, m_mainWindow.get(),
                     [&](const QString& taskId) {
                         FileTransferTask task;
                         if (taskId.trimmed().isEmpty() || !fileTransferService.loadTask(taskId, &task)) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u672A\u627E\u5230\u6587\u4EF6\u4EFB\u52A1"),
                                                            2500);
                             return;
                         }

                         const QString targetClientId = QString::fromStdWString(task.peerClientId);
                         PeerConnection* connection = connectionsByTargetId.value(targetClientId, nullptr);
                         if (!connection || !connection->isConnected()) {
                             m_mainWindow->setStatusMessage(
                                 task.direction == FileTransferDirection::Incoming
                                     ? QStringLiteral("\u5F53\u524D\u672A\u8FDE\u63A5\u5230\u53D1\u9001\u65B9\uFF0C\u6682\u65F6\u65E0\u6CD5\u7EE7\u7EED\u63A5\u6536")
                                     : QStringLiteral("\u5F53\u524D\u672A\u8FDE\u63A5\u5230\u5BF9\u65B9\uFF0C\u6682\u65F6\u65E0\u6CD5\u91CD\u65B0\u53D1\u9001"),
                                 3000);
                             return;
                         }

                         MessageEnvelope envelope;
                         bool built = false;
                         QString statusMessage;
                         if (task.direction == FileTransferDirection::Outgoing) {
                             if (task.state == FileTransferState::WaitingAccept
                                 || task.state == FileTransferState::PendingOffer
                                 || task.state == FileTransferState::Failed
                                 || task.state == FileTransferState::Canceled) {
                                 built = fileTransferService.buildOfferEnvelope(
                                     task, localClientId, targetClientId, QString(), activeFileTransferPort, &envelope);
                                 statusMessage = QStringLiteral("\u5DF2\u91CD\u65B0\u53D1\u8D77\u6587\u4EF6\u4F20\u8F93");
                             } else {
                                 built = fileTransferService.buildResumeRequestEnvelope(
                                     taskId, localClientId, targetClientId, &envelope);
                                 statusMessage = QStringLiteral("\u5DF2\u53D1\u9001\u7EED\u4F20\u8BF7\u6C42");
                             }
                         } else {
                             if (task.state == FileTransferState::ReadyToTransfer
                                 || task.state == FileTransferState::WaitingAccept
                                 || task.state == FileTransferState::Failed
                                 || task.state == FileTransferState::Canceled) {
                                 built = fileTransferService.buildReadyEnvelope(
                                     FileControlType::Accept,
                                     taskId,
                                     localClientId,
                                     targetClientId,
                                     QString(),
                                     activeFileTransferPort,
                                     &envelope);
                                 statusMessage = QStringLiteral("\u5DF2\u91CD\u65B0\u786E\u8BA4\u63A5\u6536\u6587\u4EF6");
                             } else {
                                 built = fileTransferService.buildReadyEnvelope(
                                     FileControlType::ResumeResponse,
                                     taskId,
                                     localClientId,
                                     targetClientId,
                                     QString(),
                                     activeFileTransferPort,
                                     &envelope);
                                 statusMessage = QStringLiteral("\u5DF2\u7EE7\u7EED\u63A5\u6536\u6587\u4EF6");
                             }
                         }

                         if (!built) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u5F53\u524D\u4EFB\u52A1\u6682\u4E0D\u652F\u6301\u91CD\u8BD5"),
                                                            3000);
                             return;
                         }

                         connection->sendPayload(QByteArray::fromStdString(MessageCodec::encode(envelope)));
                         refreshTransferList();
                         m_mainWindow->setStatusMessage(statusMessage, 2500);
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::openMessageFileRequested, m_mainWindow.get(),
                     [&](const QString& messageId) {
                         QString conversationId;
                         QString body;
                         qint64 createdAtMs = 0;
                         QString attachmentName;
                         QString localFilePath;
                         if (!conversationRepository.findMessageStorageRecordById(messageId,
                                                                                 &conversationId,
                                                                                 &body,
                                                                                 &createdAtMs,
                                                                                 &attachmentName,
                                                                                 &localFilePath)) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u672A\u627E\u5230\u8FD9\u6761\u6587\u4EF6\u6D88\u606F"),
                                                            2500);
                             return;
                         }

                         // fallback: 群文件卡片的 local_path 字段
                         if (localFilePath.isEmpty()) {
                             ChatMessage foundMsg;
                             if (conversationRepository.findMessageById(messageId, &foundMsg)
                                 && !foundMsg.fileCardJson.empty()) {
                                 const QJsonObject card = QJsonDocument::fromJson(
                                     QString::fromStdWString(foundMsg.fileCardJson).toUtf8()).object();
                                 localFilePath = card.value(QStringLiteral("local_path")).toString().trimmed();
                             }
                         }
                         if (localFilePath.isEmpty()) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u8FD9\u6761\u6587\u4EF6\u6D88\u606F\u8FD8\u6CA1\u6709\u672C\u5730\u6587\u4EF6"),
                                                            3000);
                             return;
                         }
                         // 图片文件使用自定义查看器
                         if (isImageViewerSupportedPath(localFilePath)) {
                             auto* viewer = new ImageViewerWidget(localFilePath, attachmentName, m_mainWindow.get());
                             viewer->show();
                             return;
                         }
                         if (!openFilePath(localFilePath)) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u6253\u5F00\u6587\u4EF6\u5931\u8D25"),
                                                            3000);
                         }
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::revealMessageFileRequested, m_mainWindow.get(),
                     [&](const QString& messageId) {
                         QString conversationId;
                         QString body;
                         qint64 createdAtMs = 0;
                         QString attachmentName;
                         QString localFilePath;
                         if (!conversationRepository.findMessageStorageRecordById(messageId,
                                                                                 &conversationId,
                                                                                 &body,
                                                                                 &createdAtMs,
                                                                                 &attachmentName,
                                                                                 &localFilePath)) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u672A\u627E\u5230\u8FD9\u6761\u6587\u4EF6\u6D88\u606F"),
                                                            2500);
                             return;
                         }

                         // fallback: 群文件卡片的 local_path 字段
                         if (localFilePath.isEmpty()) {
                             ChatMessage foundMsg;
                             if (conversationRepository.findMessageById(messageId, &foundMsg)
                                 && !foundMsg.fileCardJson.empty()) {
                                 const QJsonObject card = QJsonDocument::fromJson(
                                     QString::fromStdWString(foundMsg.fileCardJson).toUtf8()).object();
                                 localFilePath = card.value(QStringLiteral("local_path")).toString().trimmed();
                             }
                         }
                         if (localFilePath.isEmpty()) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u8FD9\u6761\u6587\u4EF6\u6D88\u606F\u8FD8\u6CA1\u6709\u672C\u5730\u6587\u4EF6"),
                                                            3000);
                             return;
                         }
                         if (!openParentDirectory(localFilePath)) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u6253\u5F00\u6240\u5728\u76EE\u5F55\u5931\u8D25"),
                                                            3000);
                         }
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::messageUrlOpenRequested, m_mainWindow.get(),
                     [&](const QString& urlString) {
                         if (const auto reminderAction = parseReminderActionUrl(urlString);
                             reminderAction.has_value()) {
                             const QString reminderId = reminderAction->reminderId;
                             if (reminderAction->verb == QStringLiteral("done")) {
                                 m_mainWindow->setStatusMessage(
                                     reminderService.markDone(reminderId)
                                         ? QStringLiteral("提醒已完成")
                                         : QStringLiteral("提醒完成失败"),
                                     2500);
                                 return;
                             }
                             if (reminderAction->verb == QStringLiteral("dismiss")) {
                                 m_mainWindow->setStatusMessage(
                                     reminderService.dismiss(reminderId)
                                         ? QStringLiteral("提醒已删除")
                                         : QStringLiteral("提醒删除失败"),
                                     2500);
                                 return;
                             }
                             if (reminderAction->verb == QStringLiteral("snooze")) {
                                 const qint64 dueAtMs =
                                     QDateTime::currentDateTime().addSecs(30 * 60).toMSecsSinceEpoch();
                                 m_mainWindow->setStatusMessage(
                                     reminderService.snooze(reminderId, dueAtMs)
                                         ? QStringLiteral("已延后 30 分钟提醒")
                                         : QStringLiteral("提醒延后失败"),
                                     2500);
                                 return;
                             }

                             const auto reminder = reminderRepository.findReminderById(reminderId);
                             if (!reminder.has_value()) {
                                 m_mainWindow->setStatusMessage(QStringLiteral("未找到这条提醒"), 3000);
                                 return;
                             }

                             if (reminder->targetType == QStringLiteral("group_announcement")) {
                                 const QString reminderGroupId =
                                     reminder->groupId.trimmed().isEmpty()
                                         ? reminder->targetId.trimmed()
                                         : reminder->groupId.trimmed();
                                 if (reminderGroupId.isEmpty()) {
                                     m_mainWindow->setStatusMessage(QStringLiteral("未找到提醒群聊"), 3000);
                                     return;
                                 }

                                  restoreMainWindow();
                                  m_mainWindow->showMessagesPage();
                                  emit m_mainWindow->conversationSelected(reminderGroupId);
                                  m_mainWindow->setStatusMessage(QStringLiteral("提醒来自群公告"), 2500);
                                  return;
                             }

                             if (reminder->targetType == QStringLiteral("group_file")) {
                                 const QString reminderGroupId = reminder->groupId.trimmed();
                                 if (reminderGroupId.isEmpty()) {
                                     m_mainWindow->setStatusMessage(QStringLiteral("未找到提醒群聊"), 3000);
                                     return;
                                 }

                                  restoreMainWindow();
                                  m_mainWindow->showMessagesPage();
                                  emit m_mainWindow->conversationSelected(reminderGroupId);
                                  const GroupFileServiceConfig config =
                                      effectiveGroupFileServiceConfigForGroup(reminderGroupId);
                                 if (config.enabled && !config.baseUrl.trimmed().isEmpty()) {
                                     emit m_mainWindow->groupFileManagerRequested(reminderGroupId, config);
                                     m_mainWindow->setStatusMessage(QStringLiteral("提醒来自群文件"), 2500);
                                 } else {
                                     m_mainWindow->setStatusMessage(
                                         QStringLiteral("提醒来自群文件，群文件服务未启用"),
                                         3500);
                                 }
                                 return;
                             }

                             if (reminder->targetType == QStringLiteral("contact")) {
                                 const QString reminderContactId =
                                     reminder->contactId.trimmed().isEmpty()
                                         ? reminder->targetId.trimmed()
                                         : reminder->contactId.trimmed();
                                 if (reminderContactId.isEmpty()) {
                                     m_mainWindow->setStatusMessage(QStringLiteral("未找到提醒联系人"), 3000);
                                     return;
                                 }

                                  restoreMainWindow();
                                  m_mainWindow->showMessagesPage();
                                  m_mainWindow->setSelectedContactId(reminderContactId);
                                  emit m_mainWindow->contactSelected(reminderContactId);
                                  m_mainWindow->setStatusMessage(QStringLiteral("已打开提醒联系人"), 2500);
                                 return;
                             }

                             const QString targetConversationId = reminder->conversationId.trimmed();
                             if (targetConversationId.isEmpty()) {
                                 const QString fallbackTitle = reminder->titleSnapshot.trimmed().isEmpty()
                                                                   ? QStringLiteral("本机提醒")
                                                                   : reminder->titleSnapshot.trimmed();
                                 const QString fallbackPreview = reminder->previewSnapshot.trimmed().isEmpty()
                                                                     ? QStringLiteral("提醒时间到了")
                                                                     : reminder->previewSnapshot.trimmed();
                                 m_mainWindow->setStatusMessage(
                                     QStringLiteral("%1：%2").arg(fallbackTitle, fallbackPreview),
                                     5000);
                                 return;
                             }

                              restoreMainWindow();
                              m_mainWindow->showMessagesPage();
                              if (!reminder->sourceMessageId.trimmed().isEmpty()) {
                                  emit m_mainWindow->searchResultJumpRequested(
                                      targetConversationId, reminder->sourceMessageId.trimmed());
                             } else {
                                 emit m_mainWindow->conversationSelected(targetConversationId);
                             }
                             m_mainWindow->setStatusMessage(QStringLiteral("已打开提醒所在会话"), 2500);
                             return;
                         }

                         const QUrl url = QUrl::fromUserInput(urlString);
                         const QString scheme = url.scheme().toLower();
                         if (url.isValid()
                             && (scheme == QStringLiteral("http")
                                 || scheme == QStringLiteral("https"))) {
                             // 保存窗口位置，防止 ShellExecuteW 导致无框窗口跳到主屏
                             const QRect savedGeometry = m_mainWindow->geometry();
                             QDesktopServices::openUrl(url);
                             QTimer::singleShot(150, m_mainWindow.get(), [this, savedGeometry]() {
                                 if (m_mainWindow && m_mainWindow->geometry() != savedGeometry
                                     && !m_mainWindow->isMaximized() && !m_mainWindow->isFullScreen()) {
                                     m_mainWindow->setGeometry(savedGeometry);
                                 }
                             });
                         }
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::openTransferFileRequested, m_mainWindow.get(),
                     [&](const QString& taskId) {
                         FileTransferTask task;
                         if (!fileTransferService.loadTask(taskId, &task)) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u672A\u627E\u5230\u6587\u4EF6\u4EFB\u52A1"),
                                                            2500);
                             return;
                         }

                         const QString filePath = localFilePathForTransferTask(task);
                         if (!openFilePath(filePath)) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u6253\u5F00\u6587\u4EF6\u5931\u8D25"),
                                                            3000);
                         }
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::revealTransferFileRequested, m_mainWindow.get(),
                     [&](const QString& taskId) {
                         FileTransferTask task;
                         if (!fileTransferService.loadTask(taskId, &task)) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u672A\u627E\u5230\u6587\u4EF6\u4EFB\u52A1"),
                                                            2500);
                             return;
                         }

                         const QString filePath = localFilePathForTransferTask(task);
                         if (!openParentDirectory(filePath)) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u6253\u5F00\u6240\u5728\u76EE\u5F55\u5931\u8D25"),
                                                            3000);
                         }
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::cancelTransferRequested, m_mainWindow.get(),
                     [&](const QString& taskId) {
                         FileTransferTask task;
                         if (!fileTransferService.loadTask(taskId, &task)) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u672A\u627E\u5230\u6587\u4EF6\u4EFB\u52A1"),
                                                            2500);
                             return;
                         }
                         if (task.state == FileTransferState::Completed
                             || task.state == FileTransferState::Failed
                             || task.state == FileTransferState::Canceled) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u8FD9\u4E2A\u6587\u4EF6\u4EFB\u52A1\u5DF2\u7ED3\u675F"),
                                                            2500);
                             return;
                         }

                         if (task.direction == FileTransferDirection::Outgoing) {
                             activeOutgoingDataPumpTasks.remove(taskId);
                         }
                         const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                         fileTransferService.markTaskState(taskId,
                                                           FileTransferState::Canceled,
                                                           task.bytesCompleted,
                                                           task.lastChunkIndex,
                                                           QStringLiteral("canceled"),
                                                           QStringLiteral("\u7528\u6237\u53D6\u6D88\u4E86\u6587\u4EF6\u4F20\u8F93"),
                                                           nowMs);
                         const QString localPath = (task.direction == FileTransferDirection::Outgoing)
                                                       ? QString::fromStdWString(task.sourcePath)
                                                       : QString::fromStdWString(task.targetPath);
                         syncFileTransferMessageState(task,
                                                      FileTransferState::Canceled,
                                                      MessageDeliveryState::Sent,
                                                      localPath);

                         const QString peerClientId = QString::fromStdWString(task.peerClientId).trimmed();
                         PeerConnection* connection = connectionsByTargetId.value(peerClientId, nullptr);
                         if (connection && connection->isConnected()) {
                             FileControlPayload payload;
                             payload.type = FileControlType::Cancel;
                             payload.taskId = toUtf8(taskId);
                             payload.conversationId = toUtf8(QString::fromStdWString(task.conversationId));
                             payload.groupId = toUtf8(QString::fromStdWString(task.groupId));
                             payload.senderId = toUtf8(localClientId);
                             payload.targetId = toUtf8(peerClientId);
                             payload.fileName = toUtf8(QString::fromStdWString(task.fileName));
                             payload.fileHash = toUtf8(QString::fromStdWString(task.fileHash));
                             payload.fileSize = task.fileSize;
                             payload.chunkSize = task.chunkSize;
                             payload.chunkCount = task.chunkCount;
                             payload.reason = toUtf8(QStringLiteral("\u7528\u6237\u53D6\u6D88\u4E86\u4F20\u8F93"));
                             MessageEnvelope envelope;
                             if (FileTransferService::envelopeFromPayload(payload, &envelope)) {
                                 connection->sendPayload(
                                     QByteArray::fromStdString(MessageCodec::encode(envelope)));
                             }
                         }

                         scheduleChatUiRefresh(true, true, false, false, 0);
                         scheduleDeferredTransferRefresh();
                         m_mainWindow->setStatusMessage(QStringLiteral("\u5DF2\u53D6\u6D88\u6587\u4EF6\u4F20\u8F93"),
                                                        2500);
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::readReceiptDetailRequested, m_mainWindow.get(),
                     [&](const QString& messageId) {
                         if (messageId.trimmed().isEmpty() || currentConversationId.isEmpty()) {
                             return;
                         }
                         const auto receipts = conversationRepository.loadReadReceiptsForMessage(messageId);
                         QSet<QString> readReaderIds;
                         for (const auto& receipt : receipts) {
                             readReaderIds.insert(receipt.first.trimmed());
                         }
                         const auto members = groupRepository.loadMembers(currentConversationId.toStdWString());
                         QStringList readNames;
                         QStringList unreadNames;
                         for (const auto& member : members) {
                             if (!member.isActive) {
                                 continue;
                             }
                             const QString memberId = QString::fromStdWString(member.memberClientId).trimmed();
                             if (memberId == localClientId) {
                                 continue;
                             }
                             const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(memberId));
                             const QString name = peer.has_value() ? displayNameForPeer(*peer) : memberId;
                             if (readReaderIds.contains(memberId)) {
                                 readNames << name;
                             } else {
                                 unreadNames << name;
                             }
                         }
                         QDialog dialog(m_mainWindow.get());
                         dialog.setWindowTitle(QStringLiteral("\u6D88\u606F\u63A5\u6536\u4EBA\u5217\u8868"));
                         dialog.setMinimumSize(520, 400);
                         dialog.resize(560, 460);
                         dialog.setWindowModality(Qt::WindowModal);
                         dialog.setStyleSheet(
                             QStringLiteral(
                                 "QDialog { background:%1; border-radius:12px; }"
                                 "QListWidget { background:transparent; border:none; outline:0; }"
                                 "QListWidget::item { padding:8px 12px; border-radius:8px; margin:2px 0; }"
                                 "QListWidget::item:hover { background:%2; }")
                                 .arg(AppStyle::surface(), AppStyle::hoverBg()));

                         auto* layout = new QVBoxLayout(&dialog);
                         layout->setContentsMargins(20, 20, 20, 16);
                         layout->setSpacing(14);

                         // ── 顶部统计区：已读进度 ──
                         const int totalCount = readNames.size() + unreadNames.size();
                         const int readPct = totalCount > 0 ? readNames.size() * 100 / totalCount : 0;

                         auto* statsWidget = new QWidget(&dialog);
                         statsWidget->setStyleSheet(
                             QStringLiteral("QWidget { background:%1; border-radius:10px; }")
                                 .arg(AppStyle::surfaceAlt()));
                         auto* statsLayout = new QHBoxLayout(statsWidget);
                         statsLayout->setContentsMargins(16, 12, 16, 12);
                         statsLayout->setSpacing(16);

                         // 圆形进度指示
                         auto* progressLabel = new QLabel(statsWidget);
                         progressLabel->setFixedSize(48, 48);
                         {
                             QPixmap ring(48, 48);
                             ring.fill(Qt::transparent);
                             QPainter rp(&ring);
                             rp.setRenderHint(QPainter::Antialiasing);
                             // 背景圆环
                             QPen bgPen(QColor(AppStyle::border()), 5);
                             bgPen.setCapStyle(Qt::RoundCap);
                             rp.setPen(bgPen);
                             rp.drawArc(QRect(5, 5, 38, 38), 0, 360 * 16);
                             // 进度圆环
                             if (readPct > 0) {
                                 QPen fgPen(QColor(AppStyle::success()), 5);
                                 fgPen.setCapStyle(Qt::RoundCap);
                                 rp.setPen(fgPen);
                                 rp.drawArc(QRect(5, 5, 38, 38), 90 * 16, -readPct * 360 * 16 / 100);
                             }
                             // 中心百分比
                             QFont pf = rp.font();
                             pf.setPixelSize(11);
                             pf.setBold(true);
                             rp.setFont(pf);
                             rp.setPen(QColor(AppStyle::textPrimary()));
                             rp.drawText(QRect(0, 0, 48, 48), Qt::AlignCenter,
                                         QStringLiteral("%1%").arg(readPct));
                             rp.end();
                             progressLabel->setPixmap(ring);
                         }
                         statsLayout->addWidget(progressLabel);

                         auto* statsText = new QVBoxLayout;
                         statsText->setSpacing(2);
                         auto* statsTitle = new ElaText(
                             QStringLiteral("\u6D88\u606F\u63A5\u6536\u8BE6\u60C5"), statsWidget);
                         statsTitle->setTextStyle(ElaTextType::Subtitle);
                         statsText->addWidget(statsTitle);
                         auto* statsSub = new ElaText(
                             QStringLiteral("%1/%2 \u4EBA\u5DF2\u8BFB")
                                 .arg(readNames.size()).arg(totalCount), statsWidget);
                         statsSub->setTextStyle(ElaTextType::Body);
                         statsSub->setStyleSheet(QStringLiteral("color:%1;").arg(AppStyle::textMuted()));
                         statsText->addWidget(statsSub);
                         statsLayout->addLayout(statsText, 1);
                         layout->addWidget(statsWidget);

                         // ── 辅助函数：为列表项生成带头像的 widget ──
                         auto makeAvatarItem = [](const QString& name, bool isRead) -> QWidget* {
                             auto* row = new QWidget;
                             auto* rl = new QHBoxLayout(row);
                             rl->setContentsMargins(0, 0, 0, 0);
                             rl->setSpacing(10);

                             // 字母头像
                             auto* avatarLabel = new QLabel(row);
                             avatarLabel->setFixedSize(32, 32);
                             static const QColor palette[] = {
                                 QColor(0x52, 0x73, 0xE8), QColor(0x2F, 0xA4, 0x84),
                                 QColor(0xD9, 0x96, 0x3A), QColor(0x7B, 0x68, 0xE6),
                                 QColor(0xD8, 0x5A, 0x9A), QColor(0x32, 0x96, 0xC4),
                             };
                             const QColor bg = palette[qHash(name) % 6];
                             const QString letter = name.trimmed().isEmpty()
                                 ? QStringLiteral("?") : name.trimmed().left(1).toUpper();
                             QPixmap pm(32, 32);
                             pm.fill(Qt::transparent);
                             QPainter pp(&pm);
                             pp.setRenderHint(QPainter::Antialiasing);
                             pp.setPen(Qt::NoPen);
                             pp.setBrush(bg);
                             pp.drawEllipse(0, 0, 32, 32);
                             QFont af = pp.font();
                             af.setBold(true);
                             af.setPixelSize(13);
                             pp.setFont(af);
                             pp.setPen(Qt::white);
                             pp.drawText(QRect(0, 0, 32, 32), Qt::AlignCenter, letter);
                             pp.end();
                             avatarLabel->setPixmap(pm);
                             rl->addWidget(avatarLabel);

                             // 名字
                             auto* nameLabel = new QLabel(name, row);
                             nameLabel->setStyleSheet(
                                 QStringLiteral("font-size:13px; color:%1;")
                                     .arg(AppStyle::textPrimary()));
                             rl->addWidget(nameLabel, 1);

                             // 状态标记
                             auto* tag = new QLabel(row);
                             tag->setFixedSize(8, 8);
                             tag->setStyleSheet(
                                 QStringLiteral("background:%1; border-radius:4px;")
                                     .arg(isRead ? AppStyle::success() : AppStyle::danger()));
                             rl->addWidget(tag);

                             return row;
                         };

                         // ── 左右分栏 ──
                         auto* columnsLayout = new QHBoxLayout;
                         columnsLayout->setSpacing(12);

                         // Unread column
                         auto* unreadColumn = new QVBoxLayout;
                         unreadColumn->setSpacing(4);
                         auto* unreadHeader = new ElaText(
                             QStringLiteral("\u672A\u8BFB (%1)").arg(unreadNames.size()), &dialog);
                         unreadHeader->setTextStyle(ElaTextType::Body);
                         unreadHeader->setStyleSheet(
                             QStringLiteral("color:%1; font-weight:700;").arg(AppStyle::danger()));
                         unreadColumn->addWidget(unreadHeader);

                         auto* unreadList = new ElaListWidget;
                         unreadList->setFrameShape(QFrame::NoFrame);
                         if (unreadNames.isEmpty()) {
                             auto* emptyItem = new QListWidgetItem(
                                 QStringLiteral("\u5168\u90E8\u6210\u5458\u5DF2\u8BFB"), unreadList);
                             emptyItem->setForeground(QColor(AppStyle::textMuted()));
                             emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
                         } else {
                             for (const QString& n : unreadNames) {
                                 auto* item = new QListWidgetItem(unreadList);
                                 item->setSizeHint(QSize(0, 44));
                                 unreadList->setItemWidget(item, makeAvatarItem(n, false));
                             }
                         }
                         unreadColumn->addWidget(unreadList, 1);
                         columnsLayout->addLayout(unreadColumn, 1);

                         // Separator
                         auto* separator = new QFrame(&dialog);
                         separator->setFrameShape(QFrame::VLine);
                         separator->setStyleSheet(
                             QStringLiteral("color:%1;").arg(AppStyle::border()));
                         columnsLayout->addWidget(separator);

                         // Read column
                         auto* readColumn = new QVBoxLayout;
                         readColumn->setSpacing(4);
                         auto* readHeader = new ElaText(
                             QStringLiteral("\u5DF2\u8BFB (%1)").arg(readNames.size()), &dialog);
                         readHeader->setTextStyle(ElaTextType::Body);
                         readHeader->setStyleSheet(
                             QStringLiteral("color:%1; font-weight:700;").arg(AppStyle::success()));
                         readColumn->addWidget(readHeader);

                         auto* readList = new ElaListWidget;
                         readList->setFrameShape(QFrame::NoFrame);
                         if (readNames.isEmpty()) {
                             auto* emptyItem = new QListWidgetItem(
                                 QStringLiteral("\u6682\u65E0\u6210\u5458\u5DF2\u8BFB"), readList);
                             emptyItem->setForeground(QColor(AppStyle::textMuted()));
                             emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
                         } else {
                             for (const QString& n : readNames) {
                                 auto* item = new QListWidgetItem(readList);
                                 item->setSizeHint(QSize(0, 44));
                                 readList->setItemWidget(item, makeAvatarItem(n, true));
                             }
                         }
                         readColumn->addWidget(readList, 1);
                         columnsLayout->addLayout(readColumn, 1);

                         layout->addLayout(columnsLayout, 1);

                         // Close button
                         auto* closeBtn = new ElaPushButton(QStringLiteral("\u5173\u95ED"), &dialog);
                         closeBtn->setFixedHeight(36);
                         closeBtn->setStyleSheet(
                             QStringLiteral("ElaPushButton { border-radius:8px; font-size:13px; }"));
                         QObject::connect(closeBtn, &ElaPushButton::clicked, &dialog, &QDialog::accept);
                         layout->addWidget(closeBtn);

                         dialog.exec();
                     });

    // 会话列表过滤切换
    QObject::connect(m_mainWindow.get(), &MainWindow::deleteTransferRequested, m_mainWindow.get(),
                     [&](const QString& taskId) {
                         if (taskId.trimmed().isEmpty() || !fileTransferService.deleteTask(taskId)) {
                             m_mainWindow->setStatusMessage(QStringLiteral("\u5220\u9664\u4F20\u8F93\u8BB0\u5F55\u5931\u8D25"),
                                                            2500);
                             return;
                         }
                         refreshTransferList();
                         m_mainWindow->setStatusMessage(QStringLiteral("\u5DF2\u5220\u9664\u4F20\u8F93\u8BB0\u5F55"),
                                                        2000);
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::clearPendingTransfersRequested, m_mainWindow.get(),
                     [&]() {
                         const int removedCount = fileTransferService.deleteTasksByStates(
                             {FileTransferState::PendingOffer,
                              FileTransferState::WaitingAccept,
                              FileTransferState::ReadyToTransfer});
                         refreshTransferList();
                         m_mainWindow->setStatusMessage(
                             removedCount > 0
                                 ? QStringLiteral("已清理 %1 条准备接收/待发送传输").arg(removedCount)
                                 : QStringLiteral("没有可清理的准备接收/待发送传输"),
                             2500);
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::clearCompletedTransfersRequested, m_mainWindow.get(),
                     [&]() {
                         const int removedCount = fileTransferService.deleteTasksByStates(
                             {FileTransferState::Completed});
                         refreshTransferList();
                         m_mainWindow->setStatusMessage(
                             removedCount > 0
                                 ? QStringLiteral("\u5DF2\u6E05\u7406 %1 \u6761\u5DF2\u5B8C\u6210\u4F20\u8F93")
                                       .arg(removedCount)
                                 : QStringLiteral("\u6CA1\u6709\u53EF\u6E05\u7406\u7684\u5DF2\u5B8C\u6210\u4F20\u8F93"),
                             2500);
                     });
    QObject::connect(m_mainWindow.get(), &MainWindow::clearFailedTransfersRequested, m_mainWindow.get(),
                     [&]() {
                         const int removedCount = fileTransferService.deleteTasksByStates(
                             {FileTransferState::Failed,
                              FileTransferState::Interrupted,
                              FileTransferState::Canceled});
                         refreshTransferList();
                         m_mainWindow->setStatusMessage(
                             removedCount > 0
                                 ? QStringLiteral("\u5DF2\u6E05\u7406 %1 \u6761\u5931\u8D25/\u4E2D\u65AD\u4F20\u8F93")
                                       .arg(removedCount)
                                 : QStringLiteral("\u6CA1\u6709\u53EF\u6E05\u7406\u7684\u5931\u8D25/\u4E2D\u65AD\u4F20\u8F93"),
                             2500);
                     });

    // 批量取消同名文件的所有传输
    QObject::connect(m_mainWindow.get(), &MainWindow::cancelSameNameTransfersRequested, m_mainWindow.get(),
                     [&](const QString& fileName) {
                         if (fileName.trimmed().isEmpty()) return;
                         const auto allTasks = fileTransferService.loadAllTasks();
                         int canceledCount = 0;
                         for (const auto& task : allTasks) {
                             if (QString::fromStdWString(task.fileName) != fileName) continue;
                             if (task.state == FileTransferState::Completed
                                 || task.state == FileTransferState::Failed
                                 || task.state == FileTransferState::Canceled) {
                                 continue;
                             }
                             const QString tid = QString::fromStdWString(task.taskId);
                             if (task.direction == FileTransferDirection::Outgoing) {
                                 activeOutgoingDataPumpTasks.remove(tid);
                             }
                             const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                             fileTransferService.markTaskState(tid,
                                                               FileTransferState::Canceled,
                                                               task.bytesCompleted,
                                                               task.lastChunkIndex,
                                                               QStringLiteral("canceled"),
                                                               QStringLiteral("\u7528\u6237\u6279\u91CF\u53D6\u6D88"),
                                                               nowMs);
                             ++canceledCount;
                         }
                         scheduleChatUiRefresh(true, true, false, false, 0);
                         scheduleDeferredTransferRefresh();
                         m_mainWindow->setStatusMessage(
                             canceledCount > 0
                                 ? QStringLiteral("\u5DF2\u53D6\u6D88 %1 \u6761\u540C\u540D\u6587\u4EF6\u4F20\u8F93").arg(canceledCount)
                                 : QStringLiteral("\u6CA1\u6709\u53EF\u53D6\u6D88\u7684\u540C\u540D\u6587\u4EF6\u4F20\u8F93"),
                             2500);
                     });

    QObject::connect(m_mainWindow.get(), &MainWindow::conversationFilterChanged,
                     m_mainWindow.get(), [&conversationModel](int filterIndex) {
        conversationModel.setFilter(filterIndex);
    });

    // 缃《
    QObject::connect(m_mainWindow.get(), &MainWindow::conversationPinToggled,
                     m_mainWindow.get(), [&](const QString& convId, bool pinned) {
        conversationRepository.setConversationFlag(convId, ConversationFlag::Pinned, pinned);
        refreshConversationList();
    });

    // 星标
    QObject::connect(m_mainWindow.get(), &MainWindow::conversationStarToggled,
                     m_mainWindow.get(), [&](const QString& convId, bool starred) {
        conversationRepository.setConversationFlag(convId, ConversationFlag::Starred, starred);
        refreshConversationList();
    });

    // 鍏嶆墦鎵?
    QObject::connect(m_mainWindow.get(), &MainWindow::conversationMuteToggled,
                     m_mainWindow.get(), [&](const QString& convId, bool muted) {
        conversationRepository.setConversationFlag(convId, ConversationFlag::Muted, muted);
        refreshConversationList();
    });

    // 标记已读/未读
    QObject::connect(m_mainWindow.get(), &MainWindow::conversationMarkUnread,
                     m_mainWindow.get(), [&](const QString& convId, bool unread) {
        conversationRepository.setConversationFlag(convId, ConversationFlag::ManuallyUnread, unread);
        refreshConversationList();
    });

    // “关闭会话/退出群聊”按当前工作区语义处理：
    // - 消息工作区：关闭会话（归档）
    // - 群聊工作区：退出群聊（群主为解散）
    QObject::connect(m_mainWindow.get(), &MainWindow::conversationMarkDone,
                     m_mainWindow.get(), [&](const QString& convId) {
        if (groupService.isGroupConversation(convId) && m_mainWindow->isGroupWorkspaceActive()) {
            const auto groupOpt = groupRepository.findGroupById(convId.toStdWString());
            const bool isOwner = groupOpt.has_value()
                                 && QString::fromStdWString(groupOpt->ownerClientId).trimmed() == localClientId;

            if (isOwner) {
                GroupEvent disbandEvent;
                if (groupService.disbandGroup(localClientId, convId, &disbandEvent)) {
                    if (const auto updatedGroupOpt = groupRepository.findGroupById(convId.toStdWString());
                        updatedGroupOpt.has_value()) {
                        const QStringList activeMembers = activeGroupMemberIds(convId);
                        broadcastGroupMeta(*updatedGroupOpt,
                                           activeMembers,
                                           activeMembers,
                                           QStringLiteral("disband"));
                    }
                    removeGroupConversationLocally(convId);
                    m_mainWindow->setStatusMessage(QStringLiteral("群聊已解散"), 3000);
                    return;
                }
                if (tryRemoveLegacyGroupLocally(convId, QStringLiteral("移除"))) {
                    return;
                }
                m_mainWindow->setStatusMessage(QStringLiteral("解散群聊失败"), 3000);
                return;
            }

            GroupEvent removeEvent;
            if (groupService.leaveGroup(localClientId, convId, &removeEvent)) {
                if (const auto updatedGroupOpt = groupRepository.findGroupById(convId.toStdWString());
                    updatedGroupOpt.has_value()) {
                    const QStringList activeMembers = activeGroupMemberIds(convId);
                    broadcastGroupMeta(*updatedGroupOpt,
                                       activeMembers,
                                       activeMembers,
                                       QStringLiteral("remove_member"),
                                       localClientId);
                }
                removeGroupConversationLocally(convId);
                m_mainWindow->setStatusMessage(QStringLiteral("已退出群聊"), 3000);
                return;
            }
            if (tryRemoveLegacyGroupLocally(convId, QStringLiteral("退出"))) {
                return;
            }
            m_mainWindow->setStatusMessage(QStringLiteral("退出群聊失败"), 3000);
            return;
        }

        conversationRepository.setConversationFlag(convId, ConversationFlag::Done, true);
        if (currentConversationId == convId) {
            currentConversationId.clear();
            currentTargetId.clear();
            m_mainWindow->clearCurrentConversationView();
        }
        scheduleChatUiRefresh(true, true, true, true, 0);
        if (groupService.isGroupConversation(convId) && !m_mainWindow->isGroupWorkspaceActive()) {
            m_mainWindow->setStatusMessage(QStringLiteral("已从消息工作台收起群聊"), 2500);
        }
    });

    QObject::connect(
        m_mainWindow.get(), &MainWindow::groupFileServiceSaveRequested,
        m_mainWindow.get(),
        [&](const GroupFileServiceConfig& cfg) {
            // 1. Persist locally (QSettings，供发送文件时读取)
            GroupFileServiceSettingsStore::save(cfg);

            // 2. 同步写入运行时架构绑定表（group_service_bindings），
            //    让 syncGroupRuntimeArchitectureStatus() 不再显示"未绑定群服务"
            {
                // 加载已有绑定，不覆盖其他群的记录
                QVector<GroupServiceBindingSnapshot> existingBindings =
                    stage2BindingRepository.loadGroupBindings();
                // 移除本群旧记录
                existingBindings.erase(
                    std::remove_if(existingBindings.begin(), existingBindings.end(),
                                   [&cfg](const GroupServiceBindingSnapshot& b) {
                                       return b.groupId == cfg.groupId;
                                   }),
                    existingBindings.end());
                if (cfg.enabled && !cfg.baseUrl.trimmed().isEmpty()) {
                    // 查找群名
                    QString groupDisplayName = cfg.groupId;
                    if (const auto grpOpt = groupRepository.findGroupById(cfg.groupId.toStdWString())) {
                        groupDisplayName = QString::fromStdWString(grpOpt->groupName);
                    }
                    GroupServiceBindingSnapshot binding;
                    binding.groupId   = cfg.groupId;
                    binding.groupName = groupDisplayName;
                    binding.enabled   = true;
                    binding.binding.boundServiceId      = cfg.baseUrl.trimmed(); // 以 URL 作为 serviceId
                    binding.binding.sharedFilesEnabled  = true;
                    binding.primaryResource.serviceId   = cfg.baseUrl.trimmed();
                    binding.primaryResource.workspaceId = cfg.workspaceId.trimmed();
                    binding.discoverySnapshot.observedAtMs = QDateTime::currentMSecsSinceEpoch();
                    existingBindings.push_back(binding);
                }
                stage2BindingRepository.replaceGroupBindings(existingBindings);
                refreshRuntimeArchitectureState();
            }

            // 3. Fan-out to other group members
            const auto envelopes = groupService.buildGroupFileServiceConfigFanOut(
                localClientId, cfg.groupId, cfg);

            for (const auto& envelope : envelopes) {
                const QString recipId = QString::fromStdString(envelope.targetId);
                PeerConnection* conn = connectionsByTargetId.value(recipId, nullptr);
                if (conn && conn->isConnected())
                    conn->sendPayload(QByteArray::fromStdString(MessageCodec::encode(envelope)));
            }

            // 4. Update sync status label with recipient count
            if (auto* panel = m_mainWindow->groupInfoPanel()) {
                const int sentCount = static_cast<int>(envelopes.size());
                const QString statusText = sentCount > 0
                    ? QStringLiteral("✓ 已同步至 %1 名成员").arg(sentCount)
                    : QStringLiteral("✓ 已保存（暂无其他成员）");
                panel->updateSyncStatus(statusText);
            }
        });

    QObject::connect(m_mainWindow.get(), &MainWindow::groupFileManagerRequested,
        m_mainWindow.get(), [&](const QString& groupId, const GroupFileServiceConfig& config) {
            const auto grpOpt = groupRepository.findGroupById(groupId.toStdWString());
            bool isAdmin = false;
            if (grpOpt) {
                const auto members = groupRepository.loadMembers(groupId.toStdWString());
                isAdmin = std::any_of(members.cbegin(), members.cend(),
                    [&](const GroupMember& m) {
                        return QString::fromStdWString(m.memberClientId) == localClientId
                            && (m.role == L"owner" || m.role == L"admin");
                    });
            }

#ifdef LEYOCHAT_HAS_WEBENGINE
            // 预热 WebEngine，缩短后续 Office 预览打开延迟（~2-3s）
            OnlineEditorWidget::warmUp();
#endif

            GroupFileManagerDialog dlg(groupId, config, localClientId,
                                      localDisplayName.isEmpty() ? localClientId : localDisplayName,
                                      isAdmin, m_mainWindow.get());
            dlg.setWindowModality(Qt::WindowModal);
            QObject::connect(&dlg,
                             &GroupFileManagerDialog::groupFileReminderRequested,
                             &dlg,
                             [&](const QString& requestedGroupId,
                                 const QString& resourceId,
                                 const QString& fileName,
                                 const QString& previewSnapshot) {
                                 const QString trimmedGroupId = requestedGroupId.trimmed();
                                 const QString trimmedResourceId = resourceId.trimmed();
                                 if (trimmedGroupId.isEmpty() || trimmedResourceId.isEmpty()) {
                                     m_mainWindow->setStatusMessage(
                                         QStringLiteral("提醒创建失败：群文件为空"),
                                         3000);
                                     return;
                                 }

                                 ReminderDialog dialog(&dlg);
                                 const QString titleSnapshot =
                                     fileName.trimmed().isEmpty() ? trimmedResourceId : fileName.trimmed();
                                 const QString preview =
                                     previewSnapshot.trimmed().isEmpty()
                                         ? QStringLiteral("群文件提醒")
                                         : previewSnapshot.trimmed().left(160);
                                 dialog.setContextPreview(titleSnapshot, preview);
                                 if (dialog.exec() != QDialog::Accepted) {
                                     return;
                                 }

                                 const QDateTime now = QDateTime::currentDateTime();
                                 const auto item = makeGroupFileReminderItem(
                                     trimmedGroupId,
                                     trimmedResourceId,
                                     fileName,
                                     previewSnapshot,
                                     dialog.note(),
                                     dialog.selectedDueTime().toMSecsSinceEpoch(),
                                     now);

                                 QString error;
                                 if (!item.has_value() || !reminderService.scheduleReminder(*item, &error)) {
                                     m_mainWindow->setStatusMessage(
                                         error.trimmed().isEmpty()
                                             ? QStringLiteral("提醒创建失败")
                                             : QStringLiteral("提醒创建失败：%1").arg(error.trimmed()),
                                         4000);
                                     return;
                                 }
                                 m_mainWindow->setStatusMessage(QStringLiteral("已设置群文件提醒"), 2500);
                             });
            dlg.exec();
        });

    // ── GroupFileTransferService 信号连接 ─────────────────────────────────

    // 上传完成 → 广播 group_file_card 消息
    QObject::connect(&groupFileTransferService, &GroupFileTransferService::uploadFinished,
        m_mainWindow.get(), [&](const QString& groupId, const QJsonObject& fileCard) {
            const QString msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            // sender_file_path is local-only; strip from broadcast card
            QJsonObject broadcastCard = fileCard;
            broadcastCard.remove(QStringLiteral("sender_file_path"));
            const QString broadcastCardJsonStr = QString::fromUtf8(QJsonDocument(broadcastCard).toJson(QJsonDocument::Compact));
            const QString body = QStringLiteral("[群文件] %1").arg(
                fileCard.value(QStringLiteral("file_name")).toString());
            const QString localFilePath =
                fileCard.value(QStringLiteral("sender_file_path")).toString();

            // 广播到群成员
            const QStringList memberIds = groupService.activeMemberIds(groupId);
            std::vector<MessageEnvelope> envelopes;
            envelopes.reserve(static_cast<std::size_t>(memberIds.size()));
            for (const auto& memberId : memberIds) {
                if (memberId == localClientId) continue;
                MessageEnvelope envelope;
                envelope.senderId = localClientId.toStdString();
                envelope.targetId = memberId.toStdString();
                envelope.messageId = msgId.toStdString();
                envelope.type = MessageType::GroupMessage;
                envelope.body = body.toStdString();
                envelope.messageSubtype = "group_file_card";
                envelope.createdAtMs = nowMs;
                envelope.conversationId = groupId.toStdString();
                envelope.payloadJson = broadcastCardJsonStr.toStdString();
                envelopes.push_back(std::move(envelope));
            }
            QString groupTitle = groupId;
            if (const auto groupOpt = groupRepository.findGroupById(groupId.toStdWString())) {
                const QString storedTitle =
                    QString::fromStdWString(groupOpt->groupName).trimmed();
                if (!storedTitle.isEmpty()) {
                    groupTitle = storedTitle;
                }
            }

            const auto sendGroupFileCardViaP2P =
                [&](const ReliableGroupFileMessageP2PRequest& p2pRequest,
                    QString* errorMessage) -> bool {
                    const QString callbackGroupId = p2pRequest.groupId.trimmed();
                    if (callbackGroupId.isEmpty() || p2pRequest.envelopes.empty()) {
                        if (errorMessage) {
                            *errorMessage =
                                QStringLiteral("group file fan-out payload is empty");
                        }
                        return false;
                    }

                    std::vector<GroupFanOutPayload> payloads;
                    const auto pending =
                        buildPendingGroupFanOut(p2pRequest.envelopes, &payloads);
                    QSqlDatabase fanOutDb = QSqlDatabase::database(
                        conversationRepository.connectionName(), false);
                    const bool fanOutTxStarted = fanOutDb.transaction();
                    bool persisted = fanOutTxStarted;
                    for (const auto& pendingEnvelope : pending) {
                        if (!persisted) {
                            break;
                        }
                        persisted = conversationRepository.enqueuePendingGroupEnvelope(
                            pendingEnvelope.targetId,
                            callbackGroupId,
                            pendingEnvelope.envelopeBlob,
                            pendingEnvelope.createdAtMs);
                    }
                    if (!persisted) {
                        if (fanOutTxStarted) {
                            fanOutDb.rollback();
                        }
                        qWarning().noquote()
                            << "[group-file-card] persist fan-out failed group="
                            << callbackGroupId.left(8);
                        if (errorMessage) {
                            *errorMessage =
                                QStringLiteral("group file fan-out persistence failed");
                        }
                        return false;
                    }
                    if (!fanOutDb.commit()) {
                        fanOutDb.rollback();
                        qWarning().noquote()
                            << "[group-file-card] commit failed group="
                            << callbackGroupId.left(8);
                        if (errorMessage) {
                            *errorMessage =
                                QStringLiteral("group file fan-out commit failed");
                        }
                        return false;
                    }

                    GroupFanOutDeliveryOptions deliveryOptions;
                    deliveryOptions.batchSize = 20;
                    deliveryOptions.logPrefix = QStringLiteral("group-file-card");
                    enablePendingCleanupForDelivery(&deliveryOptions);
                    const GroupFanOutDeliveryResult deliveryResult =
                        deliverGroupFanOutPayloads(
                        payloads,
                        nullptr,
                        [&](const GroupFanOutPayload& payloadToSend) {
                            PeerConnection* conn =
                                ConnectionRegistryUtils::connectedConnectionForTarget(
                                    connectionsByTargetId,
                                    peerIdsByConnection,
                                    payloadToSend.targetId);
                            if (!conn || !conn->isConnected()) {
                                return false;
                            }
                            return conn->sendPayload(payloadToSend.blob);
                        },
                        deliveryOptions);
                    if (!isGroupFanOutDeliveryAccepted(
                            p2pRequest.acceptQueuedOnlyDelivery,
                            deliveryResult)) {
                        qWarning().noquote()
                            << "[group-file-card] p2p fan-out was not fully accepted"
                            << "group=" << callbackGroupId.left(8)
                            << "attempted=" << deliveryResult.attemptedCount
                            << "delivered=" << deliveryResult.deliveredCount
                            << "failed=" << deliveryResult.failedCount;
                        if (errorMessage) {
                            *errorMessage = QStringLiteral(
                                "p2p group file fan-out was not fully accepted");
                        }
                        return false;
                    }
                    return true;
                };

            const RemoteChatServiceSettings remoteChatSettings =
                RemoteChatServiceSettingsStore::load();
            ServerMessageClient serverMessageClient(remoteChatSettings, localClientId);
            ReliableGroupFileMessageSender fileMessageSender(
                localClientId,
                &conversationRepository,
                &serverMessageClient,
                sendGroupFileCardViaP2P);

            ReliableGroupFileMessageSendRequest sendRequest;
            sendRequest.messageId = msgId;
            sendRequest.createdAtMs = nowMs;
            sendRequest.groupId = groupId;
            sendRequest.groupTitle = groupTitle;
            sendRequest.body = body;
            sendRequest.fileId = fileCard.value(QStringLiteral("file_id")).toString();
            sendRequest.localFilePath = localFilePath;
            sendRequest.localFileCard = fileCard;
            sendRequest.broadcastFileCard = broadcastCard;
            sendRequest.envelopes = envelopes;
            sendRequest.settings = remoteChatSettings;
            sendRequest.serviceReachable =
                remoteChatServiceRecentlyReachable(remoteChatSettings);
            sendRequest.p2pAvailable = !sendRequest.envelopes.empty();
            const RecipientRoutePartition recipientPartition =
                partitionRecipientsForMessageService(
                    recipientIdsFromEnvelopes(sendRequest.envelopes),
                    remoteChatSettings,
                    serverMessageClient,
                    sendRequest.serviceReachable);
            sendRequest.serverRecipientIds =
                recipientPartition.serverRecipientIds;
            sendRequest.p2pRecipientIds =
                recipientPartition.p2pRecipientIds;

            const ReliableGroupFileMessageSendResult sendResult =
                fileMessageSender.sendFileCard(sendRequest);
            if (sendResult.messageId.isEmpty()) {
                qWarning().noquote()
                    << "[group-file-card] failed before local persistence:"
                    << sendResult.errorMessage;
                m_mainWindow->setStatusMessage(
                    sendResult.errorMessage.trimmed().isEmpty()
                        ? QStringLiteral("群文件卡片保存失败，未发送")
                        : sendResult.errorMessage,
                    3000);
                return;
            }

            qInfo().noquote()
                << "[group-file-card] msgId=" << sendResult.messageId.left(8)
                << "group=" << groupId.left(8)
                << "channel=" << static_cast<int>(sendResult.channelUsed)
                << "sent=" << sendResult.success;

            // 同步 shared_file resource 到 stage2 资源目录
            {
                const QString fileId = fileCard.value(QStringLiteral("file_id")).toString();
                if (!fileId.isEmpty()) {
                    const GroupFileServiceConfig groupCfg =
                        effectiveGroupFileServiceConfigForGroup(groupId);
                    QJsonObject resRef;
                    resRef[QStringLiteral("service_id")]    = QStringLiteral("remote-file-service");
                    resRef[QStringLiteral("workspace_id")]  = groupCfg.workspaceId;
                    resRef[QStringLiteral("resource_id")]   = fileId;
                    resRef[QStringLiteral("resource_kind")] = QStringLiteral("shared_file");
                    resRef[QStringLiteral("title")]          = fileCard.value(QStringLiteral("file_name")).toString();
                    if (SharedFileResourceSync::syncIncomingSharedFileResource(
                            resRef, QStringLiteral("remote-file-service"),
                            groupCfg, stage2ResourceRepository)) {
                        refreshRuntimeArchitectureState();
                    }
                }
            }

            if (sendResult.success) {
                if (sendResult.channelUsed == TransportChannel::MessageService) {
                    m_mainWindow->setStatusMessage(
                        QStringLiteral("群文件已上传并保存到消息服务"), 3000);
                } else if (sendResult.channelUsed == TransportChannel::P2P) {
                    m_mainWindow->setStatusMessage(
                        QStringLiteral("群文件已上传并通过 P2P 通知"), 3000);
                } else {
                    m_mainWindow->setStatusMessage(QStringLiteral("群文件已上传"), 3000);
                }
            } else {
                qWarning().noquote()
                    << "[group-file-card] send failed:" << sendResult.errorMessage;
                m_mainWindow->setStatusMessage(
                    sendResult.errorMessage.trimmed().isEmpty()
                        ? QStringLiteral("群文件已上传，通知待重试")
                        : sendResult.errorMessage,
                    3000);
            }
            scheduleChatUiRefresh();
        });

    QObject::connect(&groupFileTransferService, &GroupFileTransferService::uploadProgress,
        m_mainWindow.get(), [&](const QString&, qint64 sent, qint64 total) {
            if (total > 0) {
                const int pct = static_cast<int>(sent * 100 / total);
                m_mainWindow->setStatusMessage(QStringLiteral("上传群文件 %1%").arg(pct), 0);
            }
        });

    QObject::connect(&groupFileTransferService, &GroupFileTransferService::uploadFailed,
        m_mainWindow.get(), [&](const QString&, const QString& err) {
            m_mainWindow->setStatusMessage(QStringLiteral("群文件上传失败: ") + err, 4000);
        });

    QObject::connect(&groupFileTransferService, &GroupFileTransferService::fallbackToP2P,
        m_mainWindow.get(), [&](const QString& groupId, const QString& filePath) {
            {
                m_mainWindow->setStatusMessage(
                    QStringLiteral("\u6b63\u5728\u53d1\u9001\u7fa4\u6587\u4ef6\u901a\u77e5..."),
                    0);

                const QString fallbackUploaderName =
                    localDisplayName.isEmpty() ? localClientId : localDisplayName;
                const auto fallbackEnvelopes =
                    groupService.buildGroupFileOfferFanOut(
                        localClientId, groupId, filePath, fallbackUploaderName);
                if (fallbackEnvelopes.empty()) {
                    m_mainWindow->setStatusMessage(
                        QStringLiteral("\u7fa4\u6587\u4ef6\u901a\u77e5\u53d1\u9001\u5931\u8d25\uff08\u65e0\u6d3b\u8dc3\u6210\u5458\uff09"),
                        3500);
                    return;
                }

                const auto& fallbackFirstEnv = fallbackEnvelopes.front();
                const QString fallbackMsgId =
                    QString::fromStdString(fallbackFirstEnv.messageId);
                const qint64 fallbackCreatedAtMs = fallbackFirstEnv.createdAtMs;
                const QByteArray fallbackPayloadBytes(
                    fallbackFirstEnv.payloadJson.data(),
                    static_cast<int>(fallbackFirstEnv.payloadJson.size()));
                const QJsonObject fallbackP2PCard =
                    QJsonDocument::fromJson(fallbackPayloadBytes).object();
                const QString fallbackBody =
                    QString::fromStdString(fallbackFirstEnv.body);
                QString fallbackGroupTitle = groupId;
                if (const auto groupOpt =
                        groupRepository.findGroupById(groupId.toStdWString())) {
                    const QString storedTitle =
                        QString::fromStdWString(groupOpt->groupName).trimmed();
                    if (!storedTitle.isEmpty()) {
                        fallbackGroupTitle = storedTitle;
                    }
                }

                const auto sendFallbackGroupFileCardViaP2P =
                    [&](const ReliableGroupFileMessageP2PRequest& p2pRequest,
                        QString* errorMessage) -> bool {
                        const QString callbackGroupId =
                            p2pRequest.groupId.trimmed();
                        if (callbackGroupId.isEmpty() ||
                            p2pRequest.envelopes.empty()) {
                            if (errorMessage) {
                                *errorMessage = QStringLiteral(
                                    "group file fan-out payload is empty");
                            }
                            return false;
                        }

                        std::vector<GroupFanOutPayload> payloads;
                        const auto pending =
                            buildPendingGroupFanOut(p2pRequest.envelopes,
                                                    &payloads);
                        QSqlDatabase fanOutDb = QSqlDatabase::database(
                            conversationRepository.connectionName(), false);
                        const bool fanOutTxStarted = fanOutDb.transaction();
                        bool persisted = fanOutTxStarted;
                        for (const auto& pendingEnvelope : pending) {
                            if (!persisted) {
                                break;
                            }
                            persisted =
                                conversationRepository.enqueuePendingGroupEnvelope(
                                    pendingEnvelope.targetId,
                                    callbackGroupId,
                                    pendingEnvelope.envelopeBlob,
                                    pendingEnvelope.createdAtMs);
                        }
                        if (!persisted) {
                            if (fanOutTxStarted) {
                                fanOutDb.rollback();
                            }
                            qWarning().noquote()
                                << "[group-file-card-fallback] persist fan-out failed group="
                                << callbackGroupId.left(8);
                            if (errorMessage) {
                                *errorMessage = QStringLiteral(
                                    "group file fan-out persistence failed");
                            }
                            return false;
                        }
                        if (!fanOutDb.commit()) {
                            fanOutDb.rollback();
                            qWarning().noquote()
                                << "[group-file-card-fallback] commit failed group="
                                << callbackGroupId.left(8);
                            if (errorMessage) {
                                *errorMessage = QStringLiteral(
                                    "group file fan-out commit failed");
                            }
                            return false;
                        }

                        GroupFanOutDeliveryOptions deliveryOptions;
                        deliveryOptions.batchSize = 20;
                        deliveryOptions.logPrefix =
                            QStringLiteral("group-file-card-fallback");
                        enablePendingCleanupForDelivery(&deliveryOptions);
                        const GroupFanOutDeliveryResult deliveryResult =
                            deliverGroupFanOutPayloads(
                            payloads,
                            nullptr,
                            [&](const GroupFanOutPayload& payloadToSend) {
                                PeerConnection* conn =
                                    connectionsByTargetId.value(
                                        payloadToSend.targetId, nullptr);
                                if (!conn || !conn->isConnected()) {
                                    return false;
                                }
                                return conn->sendPayload(payloadToSend.blob);
                            },
                            deliveryOptions);
                        if (!isGroupFanOutDeliveryAccepted(
                                p2pRequest.acceptQueuedOnlyDelivery,
                                deliveryResult)) {
                            qWarning().noquote()
                                << "[group-file-card-fallback] p2p fan-out was not fully accepted"
                                << "group=" << callbackGroupId.left(8)
                                << "attempted=" << deliveryResult.attemptedCount
                                << "delivered=" << deliveryResult.deliveredCount
                                << "failed=" << deliveryResult.failedCount;
                            if (errorMessage) {
                                *errorMessage = QStringLiteral(
                                    "p2p group file fan-out was not fully accepted");
                            }
                            return false;
                        }
                        return true;
                    };

                const RemoteChatServiceSettings fallbackRemoteChatSettings =
                    RemoteChatServiceSettingsStore::load();
                ServerMessageClient fallbackServerMessageClient(
                    fallbackRemoteChatSettings,
                    localClientId);
                ReliableGroupFileMessageSender fallbackFileMessageSender(
                    localClientId,
                    &conversationRepository,
                    &fallbackServerMessageClient,
                    sendFallbackGroupFileCardViaP2P);

                ReliableGroupFileMessageSendRequest fallbackSendRequest;
                fallbackSendRequest.messageId = fallbackMsgId;
                fallbackSendRequest.createdAtMs = fallbackCreatedAtMs;
                fallbackSendRequest.groupId = groupId;
                fallbackSendRequest.groupTitle = fallbackGroupTitle;
                fallbackSendRequest.body = fallbackBody;
                fallbackSendRequest.localFilePath = filePath;
                fallbackSendRequest.localFileCard = fallbackP2PCard;
                fallbackSendRequest.broadcastFileCard = fallbackP2PCard;
                fallbackSendRequest.envelopes = fallbackEnvelopes;
                fallbackSendRequest.settings = fallbackRemoteChatSettings;
                fallbackSendRequest.serviceReachable =
                    remoteChatServiceRecentlyReachable(
                        fallbackRemoteChatSettings);
                fallbackSendRequest.p2pAvailable =
                    !fallbackSendRequest.envelopes.empty();
                const RecipientRoutePartition fallbackRecipientPartition =
                    partitionRecipientsForMessageService(
                        recipientIdsFromEnvelopes(
                            fallbackSendRequest.envelopes),
                        fallbackRemoteChatSettings,
                        fallbackServerMessageClient,
                        fallbackSendRequest.serviceReachable);
                fallbackSendRequest.serverRecipientIds =
                    fallbackRecipientPartition.serverRecipientIds;
                fallbackSendRequest.p2pRecipientIds =
                    fallbackRecipientPartition.p2pRecipientIds;

                const ReliableGroupFileMessageSendResult fallbackSendResult =
                    fallbackFileMessageSender.sendFileCard(fallbackSendRequest);
                if (fallbackSendResult.success) {
                    if (fallbackSendResult.channelUsed ==
                        TransportChannel::MessageService) {
                        m_mainWindow->setStatusMessage(
                            QStringLiteral("\u7fa4\u6587\u4ef6\u5df2\u4fdd\u5b58\u5230\u6d88\u606f\u670d\u52a1"),
                            3000);
                    } else if (fallbackSendResult.channelUsed ==
                               TransportChannel::Mixed) {
                        m_mainWindow->setStatusMessage(
                            QStringLiteral("\u7fa4\u6587\u4ef6\u5df2\u901a\u8fc7\u6d88\u606f\u670d\u52a1\u548c P2P \u53d1\u9001"),
                            3000);
                    } else {
                        m_mainWindow->setStatusMessage(
                            QStringLiteral("\u7fa4\u6587\u4ef6\u5df2\u901a\u8fc7 P2P \u901a\u77e5"),
                            3000);
                    }
                } else {
                    qWarning().noquote()
                        << "[group-file-card-fallback] send failed:"
                        << fallbackSendResult.errorMessage;
                    m_mainWindow->setStatusMessage(
                        fallbackSendResult.errorMessage.trimmed().isEmpty()
                            ? QStringLiteral("\u7fa4\u6587\u4ef6\u901a\u77e5\u5f85\u91cd\u8bd5")
                            : fallbackSendResult.errorMessage,
                        3000);
                }
                scheduleChatUiRefresh();
                return;
            }
        });

    // 群文件下载请求
    QObject::connect(m_mainWindow.get(), &MainWindow::groupFileDownloadRequested,
        m_mainWindow.get(), [&](const QString& messageId) {
            ChatMessage foundMsg;
            if (!conversationRepository.findMessageById(messageId, &foundMsg)) return;
            const QString fcj = QString::fromStdWString(foundMsg.fileCardJson);
            if (fcj.trimmed().isEmpty()) return;
            const QJsonObject cardObj = QJsonDocument::fromJson(fcj.toUtf8()).object();
            const QString channel = cardObj.value(QStringLiteral("channel")).toString();
            const QString fileName = cardObj.value(QStringLiteral("file_name")).toString();

            if (channel == QStringLiteral("p2p")) {
                // P2P 按需下载：向发送方发送文件请求
                const QString senderId = cardObj.value(QStringLiteral("sender_id")).toString();
                const QString senderFilePath = cardObj.value(QStringLiteral("sender_file_path")).toString();
                if (senderId.isEmpty() || senderFilePath.isEmpty()) {
                    m_mainWindow->setStatusMessage(QStringLiteral("P2P 文件信息不完整"), 3000);
                    return;
                }
                PeerConnection* conn = connectionsByTargetId.value(senderId, nullptr);
                if (!conn || !conn->isConnected()) {
                    m_mainWindow->setStatusMessage(QStringLiteral("发送方不在线，无法下载"), 3000);
                    return;
                }
                // 构造 p2p_file_request 消息
                const QString groupId = QString::fromStdWString(foundMsg.conversationId);
                MessageEnvelope reqEnv;
                reqEnv.messageId      = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
                reqEnv.senderId       = localClientId.toStdString();
                reqEnv.targetId       = senderId.toStdString();
                reqEnv.conversationId = groupId.toStdString();
                reqEnv.type           = MessageType::GroupMessage;
                reqEnv.messageSubtype = "p2p_file_request";
                QJsonObject reqBody;
                reqBody[QStringLiteral("sender_file_path")] = senderFilePath;
                reqBody[QStringLiteral("requester_id")]     = localClientId;
                reqBody[QStringLiteral("file_name")]        = fileName;
                reqBody[QStringLiteral("group_id")]         = groupId;
                reqEnv.payloadJson = QString::fromUtf8(
                    QJsonDocument(reqBody).toJson(QJsonDocument::Compact)).toStdString();
                reqEnv.createdAtMs = QDateTime::currentMSecsSinceEpoch();
                conn->sendPayload(QByteArray::fromStdString(MessageCodec::encode(reqEnv)));

                // 更新本地卡片状态为 downloading
                QJsonObject updCard = cardObj;
                updCard[QStringLiteral("download_state")] = QStringLiteral("downloading");
                conversationRepository.updateMessageFileCardJson(messageId,
                    QString::fromUtf8(QJsonDocument(updCard).toJson(QJsonDocument::Compact)));
                scheduleChatUiRefresh();
                m_mainWindow->setStatusMessage(QStringLiteral("已请求发送方传输文件…"), 0);
                return;
            }

            // FileService 通道下载
            const QString fileId = cardObj.value(QStringLiteral("file_id")).toString();
            if (fileId.isEmpty()) return;

            const GroupFileServiceConfig gfsCfg = effectiveGroupFileServiceConfigForGroup(
                QString::fromStdWString(foundMsg.conversationId));
            if (gfsCfg.baseUrl.trimmed().isEmpty()) {
                m_mainWindow->setStatusMessage(QStringLiteral("无可用的文件服务"), 3000);
                return;
            }
            qInfo() << "[group-file-download] fileId=" << fileId
                    << "baseUrl=" << gfsCfg.baseUrl
                    << "groupId=" << QString::fromStdWString(foundMsg.conversationId)
                    << "fileName=" << fileName;
            const QString uploaderName = cardObj.value(QStringLiteral("uploader_name")).toString();
            const QString downloadDir = ensureIncomingFilesDirectoryForSender(
                uploaderName.isEmpty() ? QStringLiteral("unknown") : uploaderName);
            const QString savePath = downloadDir + QStringLiteral("/") + fileName;
            m_mainWindow->setStatusMessage(QStringLiteral("正在下载群文件…"), 0);
            groupFileTransferService.startDownload(messageId, gfsCfg.baseUrl, gfsCfg.bearerToken,
                                                    fileId, fileName, savePath);
        });

    // 群文件下载完成 → 更新本地路径 + 刷新 UI
    QObject::connect(&groupFileTransferService, &GroupFileTransferService::fileDownloadFinished,
        m_mainWindow.get(), [&](const QString& messageId, const QString& localPath) {
            conversationRepository.updateAttachmentMetadata(messageId, QString{}, localPath);
            QJsonObject updatedCard;
            ChatMessage foundMsg;
            if (conversationRepository.findMessageById(messageId, &foundMsg)) {
                updatedCard = QJsonDocument::fromJson(
                    QString::fromStdWString(foundMsg.fileCardJson).toUtf8()).object();
                updatedCard[QStringLiteral("local_path")] = localPath;
                conversationRepository.updateMessageFileCardJson(messageId,
                    QString::fromUtf8(QJsonDocument(updatedCard).toJson(QJsonDocument::Compact)));
            }
            m_mainWindow->setStatusMessage(QStringLiteral("群文件已下载: ") + localPath, 3000);
            scheduleChatUiRefresh();
        });

    QObject::connect(&groupFileTransferService, &GroupFileTransferService::fileDownloadFailed,
        m_mainWindow.get(), [&](const QString& messageId, const QString& err) {
            qWarning() << "[group-file-download] FAILED msgId=" << messageId << "error=" << err;
            m_mainWindow->setStatusMessage(QStringLiteral("群文件下载失败: ") + err, 4000);
        });

    QObject::connect(&groupFileTransferService, &GroupFileTransferService::fileDownloadProgress,
        m_mainWindow.get(), [&](const QString&, qint64 recv, qint64 total) {
            if (total > 0) {
                const int pct = static_cast<int>(recv * 100 / total);
                m_mainWindow->setStatusMessage(QStringLiteral("下载群文件 %1%").arg(pct), 0);
            }
        });

    currentConversationId.clear();
    currentTargetId.clear();
    m_mainWindow->clearCurrentConversationView();
    qInfo() << "[startup-perf] pre-show():" << startupTimer.elapsed() << "ms";
    if (!pendingRecoveryState.has_value()
        && shouldMaximizeMainWindowOnStartup(m_mainWindow.get())) {
        qInfo() << "[startup] default main window size exceeds available work area, fitting to available geometry";
        const QScreen* screen = m_mainWindow->screen()
            ? m_mainWindow->screen()
            : QGuiApplication::primaryScreen();
        if (screen) {
            QRect available = screen->availableGeometry();
            available.adjust(0, 0, -1, -1);
            const QSize fittedMinimum(qMin(m_mainWindow->minimumWidth(), qMax(960, available.width())),
                                      qMin(m_mainWindow->minimumHeight(), qMax(640, available.height())));
            if (fittedMinimum != m_mainWindow->minimumSize()) {
                m_mainWindow->setMinimumSize(fittedMinimum);
            }
            m_mainWindow->setGeometry(available);
        }
    }
    if (!pendingRecoveryState.has_value()) {
        m_mainWindow->show();
    }
#ifdef Q_OS_WIN
    // 允许从低权限进程（如资源管理器）向本窗口 OLE 拖拽文件，
    // 即使应用意外以管理员身份运行也不会被 UIPI 阻断。
    if (const HWND hwnd = reinterpret_cast<HWND>(m_mainWindow->winId())) {
        ::ChangeWindowMessageFilterEx(hwnd, WM_DROPFILES, MSGFLT_ALLOW, nullptr);
        ::ChangeWindowMessageFilterEx(hwnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
        ::ChangeWindowMessageFilterEx(hwnd, 0x0049 /*WM_COPYGLOBALDATA*/, MSGFLT_ALLOW, nullptr);
        // 注册传统 WM_DROPFILES 回退：当 OLE 拖拽被 UIPI 阻断时,
        // Explorer 仍可通过 WM_DROPFILES 发送文件路径。
        ::DragAcceptFiles(hwnd, TRUE);
    }
#endif
    qInfo() << "[startup-perf] show() returned:" << startupTimer.elapsed() << "ms";
    scheduleRecoverySnapshot();
    logUserObjects("post-show");

    // ── 自动更新初始化 ──
    {
        m_updateChecker = new UpdateChecker(m_mainWindow.get());
        m_updateDownloader = new UpdateDownloader(m_mainWindow.get());
        {
            QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
            m_updateChecker->setUpdateSourcePath(
                cfg.value(QStringLiteral("update/serverPath"), QString()).toString());
        }

        QObject::connect(m_updateChecker, &UpdateChecker::updateAvailable, m_mainWindow.get(),
                [this](const UpdateChecker::UpdateInfo& info) {
            if (m_mainWindow && m_mainWindow->updateBar()) {
                m_mainWindow->updateBar()->showUpdate(info.version);
                m_mainWindow->repositionUpdateBar();
                m_pendingUpdateInfo = info;
            }
        });

        QObject::connect(m_updateChecker, &UpdateChecker::noUpdateAvailable, m_mainWindow.get(),
                [this]() {
            if (m_mainWindow && m_mainWindow->updateBar()) {
                m_mainWindow->updateBar()->hideBar();
                m_pendingUpdateInfo = {};
            }
        });

        QObject::connect(m_mainWindow->updateBar(), &UpdateBar::updateClicked, m_mainWindow.get(),
                [this]() {
            if (m_pendingUpdateInfo.fileName.isEmpty()) return;
            m_updateDownloader->download(
                m_updateChecker->updateSourcePath(),
                m_pendingUpdateInfo.fileName,
                m_pendingUpdateInfo.sha256);
        });

        QObject::connect(m_updateDownloader, &UpdateDownloader::progressChanged, m_mainWindow.get(),
                [this](int percent) {
            if (m_mainWindow && m_mainWindow->updateBar()) {
                m_mainWindow->updateBar()->showProgress(percent);
            }
        });

        QObject::connect(m_updateDownloader, &UpdateDownloader::downloadFinished, m_mainWindow.get(),
                [this](const QString& localPath) {
            if (m_mainWindow && m_mainWindow->updateBar()) {
                m_mainWindow->updateBar()->showReady();
            }
            // ShellExecuteW 能正确触发 UAC（SFX 清单为 requireAdministrator）；
            // QProcess::startDetached 底层调 CreateProcess，遇到需要提权的程序
            // 会直接返回 ERROR_ELEVATION_REQUIRED 而不弹 UAC 导致安装程序起不来。
            // 不传额外参数：SFX 显示完整 QML 安装界面；
            // InstallerBackend 内部已处理 /VERYSILENT 和安装完后重启 LeyoChat。
            ::ShellExecuteW(nullptr, L"open",
                            localPath.toStdWString().c_str(),
                            nullptr, nullptr, SW_SHOWNORMAL);
            // 先隐藏窗口并设置 force_quit，确保主程序快速退出：
            // - hide() 让用户立刻看到主程序"消失"
            // - force_quit 绕过 closeEvent 中的 AskEveryTime / MinimizeToTray 逻辑
            if (m_mainWindow) {
                m_mainWindow->hide();
            }
            m_app.setProperty("leyochat_force_quit", true);
            QApplication::quit();
        });

        QObject::connect(m_updateDownloader, &UpdateDownloader::downloadFailed, m_mainWindow.get(),
                [this](const QString& error) {
            if (m_mainWindow && m_mainWindow->updateBar()) {
                m_mainWindow->updateBar()->showUpdate(m_pendingUpdateInfo.version);
            }
            if (m_mainWindow) {
                m_mainWindow->showChatToast(error, 5000);
            }
        });

        {
            QSettings cfgUpd(AppSettings::organizationName(), AppSettings::applicationName());
            const bool autoEnabled = cfgUpd.value(QStringLiteral("update/autoCheckEnabled"), true).toBool();
            const int intervalMin = cfgUpd.value(QStringLiteral("update/checkIntervalMinutes"), 60).toInt();
            if (autoEnabled) {
                m_updateChecker->start(intervalMin * 60000);
            }
        }
    }

    // ── 持久化出站队列补投 ──
    // socket 写成功和一次 HTTP 尝试都不是最终投递确认。定时扫持久化
    // outbox，使接收端短暂数据库忙、连接未断开或消息服务恢复后无需靠
    // 用户重启客户端才能继续投递。
    {
        auto* pendingDeliveryTimer = new QTimer(m_mainWindow.get());
        pendingDeliveryTimer->setInterval(10000);
        auto pendingDeliverySweepInProgress = std::make_shared<bool>(false);
        const auto runPendingDeliverySweep =
            [&, pendingDeliverySweepInProgress]() {
                if (*pendingDeliverySweepInProgress) {
                    return;
                }
                *pendingDeliverySweepInProgress = true;
                const auto sweepGuard = qScopeGuard(
                    [pendingDeliverySweepInProgress]() {
                        *pendingDeliverySweepInProgress = false;
                    });

                int directRetryCount = 0;
                QSet<QString> directTargets;
                const auto conversations =
                    conversationRepository.loadConversationSummaries();
                for (const ConversationSummary& summary : conversations) {
                    const QString conversationId =
                        QString::fromStdWString(summary.conversationId).trimmed();
                    if (conversationId.isEmpty()
                        || groupService.isGroupConversation(conversationId)) {
                        continue;
                    }
                    const QString targetId =
                        DirectConversationAddressing::otherParticipant(
                            localClientId, conversationId).trimmed();
                    if (targetId.isEmpty() || directTargets.contains(targetId)) {
                        continue;
                    }
                    directTargets.insert(targetId);
                    directRetryCount +=
                        retryPendingDirectMessagesForAvailableRoute(targetId);
                }

                // Service-capable recipients go first. A successful idempotent
                // service retry clears their pending rows before the P2P pass,
                // avoiding unnecessary dual-channel delivery of the same ID.
                const int groupServiceRetryCount =
                    retryPendingGroupEnvelopesViaMessageService();

                int groupP2PRetryCount = 0;
                QSet<QString> connectedTargets;
                for (const auto& entry : peerIdsByConnection.entries()) {
                    const QString targetId = entry.identity.trimmed();
                    if (!targetId.isEmpty()) {
                        connectedTargets.insert(targetId);
                    }
                }
                for (auto it = connectionsByTargetId.cbegin();
                     it != connectionsByTargetId.cend();
                     ++it) {
                    if (!it.key().trimmed().isEmpty()) {
                        connectedTargets.insert(it.key().trimmed());
                    }
                }
                for (const QString& targetId : connectedTargets) {
                    PeerConnection* connection =
                        ConnectionRegistryUtils::connectedConnectionForTarget(
                            connectionsByTargetId,
                            peerIdsByConnection,
                            targetId);
                    if (connection && connection->isConnected()) {
                        groupP2PRetryCount +=
                            flushPendingGroupEnvelopesForTarget(
                                targetId, connection);
                    }
                }

                if (directRetryCount > 0
                    || groupP2PRetryCount > 0
                    || groupServiceRetryCount > 0) {
                    qInfo().noquote()
                        << "[pending-delivery-sweep]"
                        << "direct=" << directRetryCount
                        << "groupP2P=" << groupP2PRetryCount
                        << "groupService=" << groupServiceRetryCount;
                    scheduleChatUiRefresh(true, true, false, false);
                }
            };
        QObject::connect(pendingDeliveryTimer,
                         &QTimer::timeout,
                         m_mainWindow.get(),
                         runPendingDeliverySweep);
        pendingDeliveryTimer->start();
        QTimer::singleShot(3500, m_mainWindow.get(), runPendingDeliverySweep);
    }

    // ── 消息服务健康探测 ──
    {
        auto* remoteChatHealthNetwork = new QNetworkAccessManager(m_mainWindow.get());
        auto* remoteChatHealthTimer = new QTimer(m_mainWindow.get());
        remoteChatHealthTimer->setInterval(30000);

        const auto runRemoteChatHealthCheck = [remoteChatHealthNetwork]() {
            RemoteChatServiceSettings settings =
                RemoteChatServiceSettingsStore::load();
            if (!settings.canUseMessageService()) {
                return;
            }

            const QString requestedBaseUrl =
                normalizeRemoteChatServiceBaseUrl(settings.baseUrl);
            const QUrl healthUrl(requestedBaseUrl + QStringLiteral("/api/v1/health"));
            QNetworkRequest request(healthUrl);
            request.setHeader(QNetworkRequest::ContentTypeHeader,
                              QStringLiteral("application/json"));
            request.setRawHeader(
                "Authorization",
                QStringLiteral("Bearer %1")
                    .arg(settings.bearerToken.trimmed()).toUtf8());

            QNetworkReply* reply = remoteChatHealthNetwork->get(request);
            auto* timeout = new QTimer(reply);
            timeout->setSingleShot(true);
            QObject::connect(timeout, &QTimer::timeout, reply, [reply]() {
                if (reply->isRunning()) {
                    reply->abort();
                }
            });
            timeout->start(4000);

            QObject::connect(reply, &QNetworkReply::finished, reply,
                             [reply, timeout, requestedBaseUrl]() {
                if (timeout->isActive()) {
                    timeout->stop();
                }

                bool ready = false;
                QString errorMessage;
                const QByteArray body = reply->readAll();
                const int statusCode =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (reply->error() != QNetworkReply::NoError) {
                    errorMessage = reply->errorString();
                } else if (statusCode < 200 || statusCode >= 300) {
                    errorMessage =
                        QStringLiteral("health HTTP %1").arg(statusCode);
                } else {
                    QJsonParseError parseError;
                    const QJsonDocument document =
                        QJsonDocument::fromJson(body, &parseError);
                    if (parseError.error != QJsonParseError::NoError
                        || !document.isObject()) {
                        errorMessage =
                            QStringLiteral("health returned invalid JSON");
                    } else {
                        const QJsonObject object = document.object();
                        if (object.contains(QStringLiteral("ok"))
                            && !object.value(QStringLiteral("ok")).toBool()) {
                            errorMessage =
                                object.value(QStringLiteral("error")).toString();
                        } else if (object.contains(QStringLiteral("ready"))
                                   && !object.value(QStringLiteral("ready")).toBool(false)) {
                            errorMessage =
                                object.value(QStringLiteral("status")).toString();
                        } else {
                            ready = true;
                        }
                    }
                }

                const qint64 finishedAtMs = QDateTime::currentMSecsSinceEpoch();
                RemoteChatServiceSettings latest =
                    RemoteChatServiceSettingsStore::load();
                if (normalizeRemoteChatServiceBaseUrl(latest.baseUrl)
                    != requestedBaseUrl) {
                    reply->deleteLater();
                    return;
                }
                latest.lastHealthCheckAtMs = finishedAtMs;
                if (ready) {
                    latest.lastHealthSuccessAtMs = finishedAtMs;
                    latest.lastErrorMessage.clear();
                } else {
                    latest.lastHealthSuccessAtMs = 0;
                    latest.lastErrorMessage = errorMessage.trimmed().isEmpty()
                        ? QStringLiteral("message service is not ready")
                        : errorMessage.trimmed();
                }
                RemoteChatServiceSettingsStore::save(latest);
                reply->deleteLater();
            });
        };

        QObject::connect(remoteChatHealthTimer, &QTimer::timeout,
                         m_mainWindow.get(), runRemoteChatHealthCheck);
        remoteChatHealthTimer->start();
        QTimer::singleShot(800, m_mainWindow.get(), runRemoteChatHealthCheck);
    }

    // Long-run resource diagnostics for field reports: total handles often climbs
    // before USER/GDI counters make the leak obvious.
    {
        logProcessResources("startup");
        auto* resourceWatchTimer = new QTimer(m_mainWindow.get());
        resourceWatchTimer->setInterval(60 * 1000);
        QObject::connect(resourceWatchTimer, &QTimer::timeout, m_mainWindow.get(), [&]() {
            logProcessResources("periodic");
            qInfo().noquote()
                << "[resource-watch] peer-state"
                << "registered=" << connectionsByTargetId.size()
                << "knownPeerIds=" << peerIdsByConnection.size()
                << "pendingConnects=" << globalPendingConnections
                << "queuedConnects=" << pendingConnectQueue.size();
        });
        resourceWatchTimer->start();
    }

    // 延迟 50ms 让窗口完成首帧绘制，再分步执行数据加载。
    // 重要：先启动异步 DB 查询（后台线程），然后才执行同步迁移操作，
    // 避免同步操作阻塞主线程导致 .then() 回调延迟数秒。
    QTimer::singleShot(50, m_mainWindow.get(), [&]() {
        qInfo() << "[startup-perf] singleShot fired:" << startupTimer.elapsed() << "ms";

        // 第 1 步：立即启动异步 UI 数据加载（后台线程，不阻塞主线程）
        // 这是用户最先看到的数据（会话列表、联系人），必须最优先。
        conversationListRefreshPending = true;
        messageListRefreshPending = true;
        chatHeaderRefreshPending = true;
        selectionStateRefreshPending = true;
        contactListRefreshPending = true;
        if (flushScheduledChatUiRefresh) {
            flushScheduledChatUiRefresh();
        }
        qInfo() << "[startup-perf] async flush scheduled:" << startupTimer.elapsed() << "ms";

        // 第 2 步：同步迁移操作（主线程）。
        // 这些操作通常很快（< 50ms），但在极端情况下可能较慢。
        // 放在 async flush 之后，这样 .then() 回调只需等这些完成即可。
        normalizeLegacyDirectConversations();
        qInfo() << "[startup-perf] normalizeLegacy done:" << startupTimer.elapsed() << "ms";
        restoreResumableFileTransferMessages();
        qInfo() << "[startup-perf] restoreResumable done:" << startupTimer.elapsed() << "ms";
        scheduleRemoteMessageSync(QStringLiteral("startup"), 2500);

        // 注意：refreshTransferList 不在这里执行。
        // 它在 .then() 回调中触发（见 startupInitialUiReady 分支），
        // 确保会话列表先显示，再加载传输列表。
    });

    QTimer deferredPresenceRefreshTimer(m_mainWindow.get());
    deferredPresenceRefreshTimer.setSingleShot(true);
    QObject::connect(&deferredPresenceRefreshTimer, &QTimer::timeout, m_mainWindow.get(),
                     [&]() { refreshLocalPresence(false); });
    const auto schedulePresenceRefresh = [&](int delayMs = 0) {
        if (deferredPresenceRefreshTimer.isActive()) {
            deferredPresenceRefreshTimer.stop();
        }
        deferredPresenceRefreshTimer.start(qMax(0, delayMs));
    };

    QTimer* presenceTimer = new QTimer(m_mainWindow.get());
    presenceTimer->setInterval(30000);
    QObject::connect(presenceTimer, &QTimer::timeout, m_mainWindow.get(),
                     [&]() {
        // 始终 forceBroadcast=true，作为心跳让对端持续刷新 lastPresenceAtMs，
        // 避免因长时间无状态更新而被误判为 Away/Offline。
        refreshLocalPresence(true);
    });
    presenceTimer->start();

    // WAL 定期 checkpoint：委托给 DatabaseWorker 线程执行
    QTimer* walCheckpointTimer = new QTimer(m_mainWindow.get());
    walCheckpointTimer->setInterval(5 * 60 * 1000); // 5 分钟
    QObject::connect(walCheckpointTimer, &QTimer::timeout, m_mainWindow.get(), [dbWorker]() {
        QMetaObject::invokeMethod(dbWorker, "runWalCheckpoint", Qt::QueuedConnection);
    });
    walCheckpointTimer->start();

    QObject::connect(&m_app, &QGuiApplication::applicationStateChanged, m_mainWindow.get(),
                     [&](Qt::ApplicationState state) {
                         schedulePresenceRefresh(2000);
                         // 窗口重新获得焦点时清除托盘闪烁
                         if (state == Qt::ApplicationActive) {
                             clearTrayUnreadPresentation();
                         }
                         // Defer the flush to the next event-loop iteration so the window
                         // can repaint before we run SQLite queries on the main thread.
                         // Without this, restoring from the taskbar triggers a visible freeze
                         // because applicationStateChanged fires synchronously inside
                         // activateWindow(), before the first paint event is processed.
                         QTimer::singleShot(0, m_mainWindow.get(), [&, state]() {
                             if (flushDeferredTransferRefreshIfVisible) {
                                 flushDeferredTransferRefreshIfVisible();
                             }
                             // 窗口重新激活时，清除当前正在查看会话的未读状态。
                             // 修复：消息到达时窗口可能瞬间未聚焦，导致未走 viewingCurrentGroup 路径，
                             // 回到窗口后红点/badge 残留。
                             if (state == Qt::ApplicationActive
                                 && !currentConversationId.isEmpty()
                                 && windowCanConsumeIncomingConversation()) {
                                 flushReadReceipts();
                                 scheduleChatUiRefresh(true, false, false, false, 100);
                             }
                         });
                     });
    QTimer::singleShot(0, m_mainWindow.get(), [&]() {
        qInfo() << "[startup-perf] firstEvent: event loop alive at" << startupTimer.elapsed() << "ms";
    });
    QTimer::singleShot(0, m_mainWindow.get(), [&]() {
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        const QString currentVersion = ApplicationInfo::currentVersion();
        const QString previousVersion =
            cfg.value(QStringLiteral("application/lastSeenVersion")).toString().trimmed();
        const bool shouldShowReleaseNotes =
            ApplicationInfo::shouldShowReleaseNotesOnStartup(previousVersion, currentVersion);
        cfg.setValue(QStringLiteral("application/lastSeenVersion"), currentVersion);
        cfg.sync();
        if (shouldShowReleaseNotes) {
            QTimer::singleShot(350, m_mainWindow.get(), [&, previousVersion, currentVersion]() {
                showCurrentReleaseNotes(
                    QStringLiteral("%1 已更新到 %2").arg(appDisplayName, currentVersion),
                    QStringLiteral("升级说明 · 从 %1 升级").arg(previousVersion));
            });
        }
        refreshLocalPresence(true);
    });
    // 头像按钮显示姓名首字
    if (!localDisplayName.isEmpty()) {
        m_mainWindow->setAvatarText(localDisplayName.left(1).toUpper());
    }
    {
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        const QString avatarPath =
            cfg.value(QStringLiteral("avatar/") + localClientId,
                      cfg.value(QStringLiteral("avatar/self"))).toString();
        if (!avatarPath.trimmed().isEmpty()) {
            m_mainWindow->setAvatarImagePath(avatarPath);
        }
    }

    // 历史已知 peer 默认只作为本地目录恢复，不再启动后自动全网重连。
    // 旧的批量重连路径只能通过兼容开关恢复，避免大规模部署时启动即形成全连接风暴。
    if (listening && !knownPeersAtStartup.empty()) {
        const bool mayAutoReconnectKnownPeers =
            P2PConnectionPolicy::shouldStartPeerConnection(
                RemoteChatServiceSettingsStore::load(),
                P2PConnectionTrigger::StartupKnownPeer);
        if (mayAutoReconnectKnownPeers) {
            QTimer::singleShot(800, m_mainWindow.get(), [&]() {
                for (const auto& peer : knownPeersAtStartup) {
                    if (peer.host.empty() || peer.port == 0) {
                        continue;
                    }
                    const QString peerClientId = QString::fromUtf8(
                        peer.clientId.data(), static_cast<int>(peer.clientId.size()));
                    const QString peerHost = QString::fromUtf8(
                        peer.host.data(), static_cast<int>(peer.host.size()));
                    tryAutoConnectPeer(peerClientId, peerHost, peer.port);
                }
                logUserObjects("peer-reconnect-queued");
                qInfo() << "[startup-perf] peer reconnect queued:"
                        << startupTimer.elapsed() << "ms, peers=" << knownPeersAtStartup.size();
            });
        } else {
            qInfo() << "[startup-perf] known peers restored directory-only:"
                    << startupTimer.elapsed() << "ms, peers=" << knownPeersAtStartup.size();
        }
    }

    // 延迟检测本地文件服务，如果已部署但未运行则自动拉起
    QTimer::singleShot(2000, m_mainWindow.get(), [&]() {
        autoStartLocalFileServices(groupRepository, localClientId);
    });

    // ── UIA 诊断 ──
    QTimer::singleShot(5000, m_mainWindow.get(), [this]() {
        QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(m_mainWindow.get());
        if (!iface) {
            qWarning() << "[uia-diag] QAccessible::queryAccessibleInterface(mainWindow) returned null";
        } else {
            qInfo() << "[uia-diag] mainWindow accessible:"
                     << "childCount=" << iface->childCount()
                     << "role=" << iface->role()
                     << "text=" << iface->text(QAccessible::Name)
                     << "isValid=" << iface->isValid();
            for (int i = 0; i < iface->childCount() && i < 10; ++i) {
                QAccessibleInterface* child = iface->child(i);
                if (child) {
                    qInfo() << "[uia-diag]   child[" << i << "]:"
                             << "role=" << child->role()
                             << "text=" << child->text(QAccessible::Name)
                             << "childCount=" << child->childCount()
                             << "className=" << (child->object() ? child->object()->metaObject()->className() : "null");
                }
            }
        }
        // 检查 QMainWindow 的直接子 widget
        qInfo() << "[uia-diag] mainWindow children (QObject):";
        for (QObject* obj : m_mainWindow->children()) {
            QWidget* w = qobject_cast<QWidget*>(obj);
            if (w) {
                qInfo() << "[uia-diag]   widget:" << w->metaObject()->className()
                         << "objectName=" << w->objectName()
                         << "isWindow=" << w->isWindow()
                         << "isVisible=" << w->isVisible()
                         << "isHidden=" << w->isHidden()
                         << "size=" << w->size();
            }
        }
    });

    int exitCode = 0;
    exitCode = m_app.exec();
    *appShuttingDown = true;
    qInfo() << "[app-exit] event loop exiting, exitCode=" << exitCode;

    // ──── 1. 停止 UpdateChecker 定时器，防止再触发后台任务 ────
    if (m_updateChecker) {
        m_updateChecker->stop();
        qInfo() << "[app-exit] UpdateChecker stopped";
    }

    // ──── 2. 等待所有 QtConcurrent 后台任务完成 ────
    //      必须在析构栈变量（数据库连接等）之前完成，
    //      否则后台线程会访问已移除的 QSqlDatabase 连接导致崩溃。
    QThreadPool::globalInstance()->waitForDone(5000);
    qInfo() << "[app-exit] QThreadPool drained";

    // ──── 3. 优雅关闭 FileReceiveWorker 线程 ────
    fileReceiveThread.quit();
    if (!fileReceiveThread.wait(5000)) {
        qWarning() << "[app-exit] FileReceiveWorker thread did not finish within 5s, terminating";
        fileReceiveThread.terminate();
        fileReceiveThread.wait();
    }

    // ──── 4. 优雅关闭 DatabaseWorker 线程 ────
    dbWorkerThread.quit();
    if (!dbWorkerThread.wait(5000)) {
        qWarning() << "[app-exit] DatabaseWorker thread did not finish within 5s, terminating";
        dbWorkerThread.terminate();
        dbWorkerThread.wait();
    }

    return exitCode;
}
