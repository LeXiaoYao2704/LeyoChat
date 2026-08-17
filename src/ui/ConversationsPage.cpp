// 迁移策略：MainWindow.cpp 中以 "MainWindow::" 为前缀的方法逐个改为
// "ConversationsPage::" 前缀，构造函数中 this 指针上下文保持不变（
// ConversationsPage 本身就是 QWidget）。对 ElaWindow 特有方法的调用
// （如 setNodeKeyPoints, navigation）通过 m_mainWindow 指针转发。

#include "ui/ConversationsPage.h"
#include "ui/MainWindow.h"
#include "ui/CallWindow.h"

#include "app/AppSettings.h"
#include "app/GroupFileServiceConfigResolver.h"
#include "app/TestModeContext.h"
#include "integrations/KnowServiceClient.h"
#include "integrations/KnowledgeServiceSettings.h"
#include "ui/AiKnowledgePanel.h"
#include "ui/AppStyle.h"
#include "ui/ChatComposerWidget.h"
#include "ui/ClientAppearance.h"
#include "ui/ClientPreferences.h"
#include "ui/LeyoDialog.h"

#include <ElaDef.h>
#include <ElaCheckBox.h>
#include <ElaDialog.h>
#include <ElaFrame.h>
#include <ElaScrollBar.h>
#include <ElaMenu.h>
#include <ElaLineEdit.h>
#include <ElaListView.h>
#include <ElaListWidget.h>
#include <ElaPushButton.h>
#include <ElaSpinBox.h>
#include <ElaText.h>
#include <ElaToolButton.h>
#include <QCoreApplication>
#include <QEvent>
#include "ui/ChatHeaderWidget.h"
#include "ui/ConnectIpDialog.h"
#include "ui/ContactListModel.h"
#include "ui/ConversationHistoryDialog.h"
#include "ui/ForwardDetailDialog.h"
#include "ui/AlphabetIndexBar.h"
#include "ui/ConversationCardDelegate.h"
#include "ui/ConversationItemWidget.h"
#include "ui/ConversationListModel.h"
#include "ui/CreateGroupDialog.h"
#include "ui/GroupInfoPanel.h"
#include "ui/MessageBubbleDelegate.h"
#include "ui/MessageBubbleWidget.h"
#include "ui/ProfileCardPopup.h"
#include "ui/MessageListModel.h"
#include "ui/ScreenshotEditorWindow.h"
#include "ui/ScreenshotOverlay.h"
#include "ui/TransferListModel.h"
#include "ui/UiIcons.h"
#include "architecture/HybridRoutingPolicy.h"
#include "architecture/RuntimeArchitectureQueryService.h"
#include "architecture/RuntimeArchitecturePresentation.h"
#include "services/DirectConversationAddressing.h"
#include "services/ResourceRefRouter.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QEasingCurve>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QIcon>
#include <QHostAddress>
#include <QInputMethodEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QSet>
#include <QLineEdit>
#include <QListWidget>
#include <QListView>
#include <QMimeData>
#include <QMouseEvent>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QAbstractItemModel>
#include <QKeyEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <ElaStackedWidget.h>
#include <ElaSplitter.h>
#include <ElaTextEdit.h>
#include <QStyleHints>
#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QTextDocumentFragment>
#include <QToolBar>
#include <QFileDialog>
#include <QMenu>
#include <QProcess>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QStringList>
#include <QStandardPaths>
#include <QTimer>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QPixmap>
#include <QUuid>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

#include "ui/FilePreviewWidget.h"
#include "ui/ImageViewerWidget.h"
#include "ui/ChatWorkspaceWidget.h"
#include "ui/ConversationSidebarWidget.h"
#include "ui/ContextPanel.h"
#include "ui/StickerManager.h"

#ifdef LEYOCHAT_HAS_WEBENGINE
#include "ui/FerryBrowserWidget.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineNavigationRequest>
#endif

#include <algorithm>
#include <QDebug>

// ── 从 MainWindow.cpp 迁移的匿名命名空间辅助函数 ──────────────────────
namespace {

QModelIndex findIndexByRole(QAbstractItemModel* model, int role, const QString& value) {
    if (!model || value.trimmed().isEmpty()) return {};
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (index.data(role).toString() == value) return index;
    }
    return {};
}

QString transferRetryActionText(FileTransferDirection direction, FileTransferState state) {
    if (direction == FileTransferDirection::Incoming) {
        return state == FileTransferState::Failed || state == FileTransferState::Canceled
                   ? QStringLiteral("\u91CD\u65B0\u63A5\u6536") : QStringLiteral("\u7EE7\u7EED\u63A5\u6536");
    }
    return state == FileTransferState::Failed || state == FileTransferState::Canceled
               ? QStringLiteral("\u91CD\u65B0\u53D1\u9001") : QStringLiteral("\u7EE7\u7EED\u53D1\u9001");
}

QString transferMenuStylesheet() {
    return QStringLiteral(
        "QMenu { background:%1; color:%2; border:none; border-radius:12px; padding:8px; }"
        "QMenu::item { padding:8px 14px; border-radius:8px; margin:2px 0; }"
        "QMenu::item:selected { background:%4; color:%5; }"
        "QMenu::separator { height:1px; background:%3; margin:8px 6px; }")
        .arg(AppStyle::surface(), AppStyle::textPrimary(), AppStyle::border(),
             AppStyle::hoverBg(), AppStyle::textPrimary());
}

QString conversationDoneActionText(bool isGroupConversation, bool isDone, bool groupWorkspaceActive) {
    if (isGroupConversation && groupWorkspaceActive) {
        return isDone ? QStringLiteral("\u6062\u590D\u7FA4\u804A") : QStringLiteral("\u9000\u51FA\u7FA4\u804A");
    }
    return isDone ? QStringLiteral("\u6062\u590D\u4F1A\u8BDD") : QStringLiteral("\u5173\u95ED\u4F1A\u8BDD");
}

QString conversationListDoneActionText(bool isGroupConversation, bool isDone, bool groupWorkspaceActive) {
    if (isGroupConversation && groupWorkspaceActive) {
        return isDone ? QStringLiteral("\u6062\u590D\u7FA4\u804A") : QStringLiteral("\u9000\u51FA\u7FA4\u804A");
    }
    return conversationDoneActionText(false, isDone, false);
}

QByteArray fingerprintForImage(const QImage& image) {
    if (image.isNull()) return {};
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

QString ensureScreenshotDirectory() {
    QString basePath = TestModeContext::current().screenshotsDirectoryPath();
    if (!basePath.isEmpty()) { QDir().mkpath(basePath); return basePath; }
    basePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString screenshotDir = QDir(basePath).filePath(QStringLiteral("screenshots"));
    QDir().mkpath(screenshotDir);
    return screenshotDir;
}

bool isGroupConversationId(const QString& conversationId) {
    return !conversationId.contains(QLatin1Char('|'));
}

bool launchSystemScreenCapture() {
    if (QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("ms-screenclip:")})) return true;
    if (QProcess::startDetached(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), QStringLiteral("start"), QString(), QStringLiteral("ms-screenclip:")})) return true;
    return QProcess::startDetached(QStringLiteral("SnippingTool.exe"), {});
}

QPointer<CallWindow> visibleCallWindow()
{
    const auto topLevels = QApplication::topLevelWidgets();
    for (QWidget* widget : topLevels) {
        auto* callWindow = qobject_cast<CallWindow*>(widget);
        if (callWindow && callWindow->isVisible()) {
            return callWindow;
        }
    }
    return {};
}

QString stripInlineImageTags(const QString& html) {
    if (html.trimmed().isEmpty()) return html;
    QString sanitized = html;
    static const QRegularExpression imageTagPattern(QStringLiteral(R"(<img\b[^>]*>)"), QRegularExpression::CaseInsensitiveOption);
    sanitized.remove(imageTagPattern);
    return sanitized.trimmed();
}

QString stripRiskyColorStyles(const QString& html) {
    if (html.trimmed().isEmpty()) return html;
    QString sanitized = html;
    static const QRegularExpression inlineColorDecl(QStringLiteral(R"((?:^|;)\s*color\s*:\s*[^;"]*\s*;?)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression inlineBackgroundDecl(QStringLiteral(R"((?:^|;)\s*background(?:-color)?\s*:\s*[^;"]*\s*;?)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression styleAttrPattern(QStringLiteral(R"___(style\s*=\s*"([^"]*)")___"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression colorAttrPattern(QStringLiteral(R"(\scolor\s*=\s*("[^"]*"|'[^']*'|[^\s>]+))"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression residualColorDecl(QStringLiteral(R"(color\s*:\s*[^;\"']+\s*;?)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression residualBackgroundDecl(QStringLiteral(R"(background(?:-color)?\s*:\s*[^;\"']+\s*;?)"), QRegularExpression::CaseInsensitiveOption);
    qsizetype offset = 0;
    while (offset < sanitized.size()) {
        const QRegularExpressionMatch match = styleAttrPattern.match(sanitized, offset);
        if (!match.hasMatch()) break;
        QString styleBody = match.captured(1);
        styleBody.remove(inlineColorDecl);
        styleBody.remove(inlineBackgroundDecl);
        styleBody = styleBody.trimmed();
        while (styleBody.startsWith(QLatin1Char(';'))) styleBody.remove(0, 1);
        while (styleBody.endsWith(QLatin1Char(';'))) styleBody.chop(1);
        styleBody = styleBody.trimmed();
        const int start = match.capturedStart(0);
        const int length = match.capturedLength(0);
        if (styleBody.isEmpty()) { sanitized.remove(start, length); offset = start; }
        else { const QString replacement = QStringLiteral("style=\"%1\"").arg(styleBody); sanitized.replace(start, length, replacement); offset = start + replacement.size(); }
    }
    sanitized.remove(colorAttrPattern);
    sanitized.remove(residualColorDecl);
    sanitized.remove(residualBackgroundDecl);
    return sanitized.trimmed();
}

QUrl firstOpenableUrlFromMessageBody(const QString& rawBody) {
    QString body = rawBody;
    if (Qt::mightBeRichText(body)) {
        static const QRegularExpression hrefPattern(QStringLiteral(R"(href\s*=\s*["']([^"']+)["'])"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch hrefMatch = hrefPattern.match(body);
        if (hrefMatch.hasMatch()) {
            const QUrl hrefUrl = QUrl::fromUserInput(hrefMatch.captured(1).trimmed());
            const QString hrefScheme = hrefUrl.scheme().toLower();
            if (hrefUrl.isValid() && (hrefScheme == QStringLiteral("http") || hrefScheme == QStringLiteral("https"))) return hrefUrl;
        }
        body = QTextDocumentFragment::fromHtml(body).toPlainText();
    }
    body = body.trimmed();
    if (body.isEmpty()) return {};
    static const QRegularExpression urlPattern(QStringLiteral(R"((https?://[^\s<>"']+|www\.[^\s<>"']+))"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = urlPattern.match(body);
    if (!match.hasMatch()) return {};
    QString urlText = match.captured(0).trimmed();
    while (!urlText.isEmpty() && QStringLiteral(".,;!?)]}").contains(urlText.back())) urlText.chop(1);
    if (urlText.startsWith(QStringLiteral("www."), Qt::CaseInsensitive)) urlText.prepend(QStringLiteral("https://"));
    const QUrl url = QUrl::fromUserInput(urlText);
    if (!url.isValid()) return {};
    const QString scheme = url.scheme().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) return {};
    return url;
}

bool isAsciiOrFullwidthAt(const QString& text) {
    return text == QStringLiteral("@") || text == QStringLiteral("\uFF20");
}

bool isLocalImageFilePath(const QString& filePath) {
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) return false;
    QImageReader reader(filePath);
    return !reader.format().isEmpty();
}

QUrl firstOpenableUrlFromResourcePayload(const QByteArray& payloadJson) {
    const auto payload = ResourceRefRouter::parsePayload(payloadJson);
    if (!payload.has_value() || payload->actions.isEmpty()) return {};
    const auto extractOpenable = [](const ResourceRefAction& action) -> QUrl {
        const QUrl url = QUrl::fromUserInput(action.target.trimmed());
        const QString scheme = url.scheme().toLower();
        if (!url.isValid() || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) return {};
        return url;
    };
    for (const ResourceRefAction& action : payload->actions) {
        if (!action.primary) continue;
        const QUrl url = extractOpenable(action);
        if (url.isValid()) return url;
    }
    for (const ResourceRefAction& action : payload->actions) {
        const QUrl url = extractOpenable(action);
        if (url.isValid()) return url;
    }
    return {};
}

QString aiServiceDisplayName(const KnowledgeServiceConfig& config) {
    const QString displayName = config.displayName.trimmed();
    if (!displayName.isEmpty()) return displayName;
    const QString teamLabel = config.teamLabel.trimmed();
    if (!teamLabel.isEmpty()) return teamLabel;
    const QString localId = config.localId.trimmed();
    if (!localId.isEmpty()) return localId;
    return config.baseUrl.trimmed();
}

QStringList aiServiceDisplayNames(const QVector<KnowledgeServiceConfig>& configs, const QString& defaultServiceId, int* selectedIndex) {
    QStringList serviceNames;
    int resolvedSelectedIndex = 0;
    for (const KnowledgeServiceConfig& config : configs) {
        serviceNames.push_back(aiServiceDisplayName(config));
        if (!defaultServiceId.isEmpty() && config.localId == defaultServiceId) resolvedSelectedIndex = serviceNames.size() - 1;
    }
    if (selectedIndex) *selectedIndex = resolvedSelectedIndex;
    return serviceNames;
}

const KnowledgeServiceConfig* aiServiceConfigForId(const QVector<KnowledgeServiceConfig>& configs, const QString& serviceId) {
    const QString normalizedId = serviceId.trimmed();
    for (const KnowledgeServiceConfig& config : configs) {
        if (config.localId == normalizedId) return &config;
    }
    return nullptr;
}

QStringList aiServiceLocalIds(const QVector<KnowledgeServiceConfig>& configs) {
    QStringList ids;
    for (const KnowledgeServiceConfig& config : configs) ids.push_back(config.localId);
    return ids;
}

QWidget* buildPageScaffold(QWidget* page, const QString& key)
{
    if (!page) return nullptr;
    page->setObjectName(QStringLiteral("secondaryPageScaffold_%1").arg(key));
    page->setProperty("pageScaffoldRole", QStringLiteral("secondary"));
    page->setProperty("pageScaffoldKey", key);
    return page;
}

GroupFileServiceConfig effectiveGroupFileServiceConfigForGroup(const QString& groupId)
{
    return GroupFileServiceConfigResolver::makeDefaultConfig(
        groupId,
        RemoteChatServiceSettingsStore::load());
}

} // namespace

// ======================================================================
// 构造函数 — 从 MainWindow.cpp 迁移的完整 UI 创建代码
// ======================================================================

ConversationsPage::ConversationsPage(MainWindow* mainWindow, QWidget* parent)
    : QWidget(parent),
      m_mainWindow(mainWindow),
      m_sideStack(new ElaStackedWidget(this)),
      m_contentStack(new ElaStackedWidget(this)),
      m_conversationList(new QListView(this)),
      m_transferList(new ElaListView(this)),
      m_messageList(new QListView(this)),
      m_hostEdit(new ElaLineEdit(this)),
      m_portEdit(new ElaLineEdit(this)),
      m_chatComposerWidget(new ChatComposerWidget(this)),
      m_connectButton(new ElaPushButton(QStringLiteral("\u8FDE\u63A5"), this)),
      m_chatHeaderWidget(new ChatHeaderWidget(this)) {
    setObjectName(QStringLiteral("chatPageRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(false);
    m_inputEdit = m_chatComposerWidget->messageEditor();
    m_sendFileButton = m_chatComposerWidget->fileButton();
    m_sendButton = m_chatComposerWidget->sendButton();
    m_sendModeBtn = m_chatComposerWidget->sendModeButton();
    m_screenshotBtn = m_chatComposerWidget->screenshotButton();
    m_directHistoryBtn = m_chatHeaderWidget->directHistoryButton();
    m_groupAnnouncementBtn = m_chatHeaderWidget->groupAnnouncementButton();
    m_groupAddMemberBtn = m_chatHeaderWidget->groupAddMemberButton();
    m_groupHistoryBtn = m_chatHeaderWidget->groupHistoryButton();
    m_groupSettingsBtn = m_chatHeaderWidget->groupSettingsButton();
    connect(m_chatHeaderWidget, &ChatHeaderWidget::closeRequested, this, [this]() {
        emit closeCurrentConversationRequested();
    });

    const QFont baseUiFont = font();
    const QFont navFont = AppStyle::navFont(baseUiFont);
    const QFontMetrics navMetrics(navFont);
    const int sidePanelWidth =
        qMax(328, navMetrics.horizontalAdvance(QStringLiteral("最近协作与消息列表")) + 136);
    const int sideIconButtonSize = qMax(28, AppStyle::iconButtonSizeForFont(baseUiFont) - 4);
    const int compactIconButtonSize = qMax(20, AppStyle::iconButtonSizeForFont(baseUiFont) - 12);
    const int searchHeight = qMax(32, QFontMetrics(AppStyle::bodyFont(baseUiFont)).height() + 16);
    const int filterButtonHeight = qMax(34, QFontMetrics(AppStyle::bodyFont(baseUiFont)).height() + 14);
    const int sidebarActionHeight = qMax(40, QFontMetrics(AppStyle::strongFont(baseUiFont)).height() + 18);
    const auto addSideSurfaceBand = [](QWidget* parent,
                                       QVBoxLayout* layout,
                                       const QString& modeObjectName,
                                       const QString& modeText,
                                       const QString& statusObjectName,
                                       const QString& statusText) {
        auto* band = new ElaFrame(parent);
        band->setObjectName(QStringLiteral("sideSurfaceBand"));
        auto* bandLayout = new QHBoxLayout(band);
        bandLayout->setContentsMargins(0, 0, 0, 0);
        bandLayout->setSpacing(8);

        auto* modeChip = new ElaText(modeText, band);
        modeChip->setObjectName(modeObjectName);
        modeChip->setProperty("surfaceChipRole", QStringLiteral("mode"));
        modeChip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        modeChip->setMinimumWidth(QFontMetrics(modeChip->font()).horizontalAdvance(modeText) + 28);

        auto* statusChip = new ElaText(statusText, band);
        statusChip->setObjectName(statusObjectName);
        statusChip->setProperty("surfaceChipRole", QStringLiteral("status"));
        statusChip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        statusChip->setMinimumWidth(QFontMetrics(statusChip->font()).horizontalAdvance(statusText) + 28);

        bandLayout->addWidget(modeChip);
        bandLayout->addWidget(statusChip);
        bandLayout->addStretch();
        layout->addWidget(band);
    };

    m_sideStack->setObjectName(QStringLiteral("sideWorkspaceStack"));
    m_contentStack->setObjectName(QStringLiteral("contentWorkspaceStack"));

    m_sideStack->setObjectName(QStringLiteral("sideStack"));
    m_sideStack->setFixedWidth(sidePanelWidth);
    m_sideStack->setAutoFillBackground(false);
    m_contentStack->setAutoFillBackground(false);
    m_sideStack->setAttribute(Qt::WA_StyledBackground, true);
    m_contentStack->setAttribute(Qt::WA_StyledBackground, true);
    m_sideStack->setStyleSheet(QStringLiteral(
        "QStackedWidget#sideStack { background: transparent; border: none; }"));
    m_contentStack->setStyleSheet(QStringLiteral(
        "QStackedWidget#contentWorkspaceStack { background: transparent; border: none; }"));

    // 渚ц竟鏍?Page 0: 娑堟伅鍒楄〃锛堝惈鍙姌鍙犲垎绫荤瓫閫夐潰鏉匡級
    auto* conversationsPage = new ConversationSidebarWidget(m_sideStack);
    auto* conversationsLayout = new QVBoxLayout(conversationsPage);
    conversationsLayout->setContentsMargins(0, 0, 0, 0);
    conversationsLayout->setSpacing(0);

    // 消息侧边栏 header bar（标题 + 折叠按钮）
    auto* convHeaderBar = new ElaFrame(conversationsPage);
    convHeaderBar->setObjectName(QStringLiteral("sideHeaderBar"));
    auto* convHeaderLayout = new QVBoxLayout(convHeaderBar);
    convHeaderLayout->setContentsMargins(12, 12, 12, 12);
    convHeaderLayout->setSpacing(10);

    auto* conversationsHeaderCard = new ElaFrame(convHeaderBar);
    conversationsHeaderCard->setObjectName(QStringLiteral("conversationsHeaderCard"));
    auto* conversationsHeaderCardLayout = new QVBoxLayout(conversationsHeaderCard);
    conversationsHeaderCardLayout->setContentsMargins(0, 0, 0, 0);
    conversationsHeaderCardLayout->setSpacing(8);

    // Header row: conversation count, unread count, and filter control.
    auto* conversationsTitleRow = new QHBoxLayout;
    conversationsTitleRow->setContentsMargins(0, 0, 0, 0);
    conversationsTitleRow->setSpacing(8);

    auto* modeChipWidget = new ElaText(QStringLiteral("\u6D88\u606F"), conversationsHeaderCard);
    modeChipWidget->setObjectName(QStringLiteral("conversationsModeChip"));
    modeChipWidget->setTextPixelSize(20);
    {
        QFont titleFont = modeChipWidget->font();
        titleFont.setWeight(QFont::Bold);
        modeChipWidget->setFont(titleFont);
    }
    modeChipWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_conversationsModeChip = modeChipWidget;

    auto* statusChipWidget = new ElaText(QStringLiteral("\u6682\u65E0\u4F1A\u8BDD"), conversationsHeaderCard);
    statusChipWidget->setObjectName(QStringLiteral("conversationsStatusChip"));
    statusChipWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    statusChipWidget->setMinimumWidth(qMax(
        72,
        QFontMetrics(statusChipWidget->font()).horizontalAdvance(statusChipWidget->text()) + 28));
    statusChipWidget->hide();
    m_conversationsStatusChip = statusChipWidget;

    m_conversationWorkspaceTitle = modeChipWidget;

    auto* filterToggleBtn = new ElaToolButton(conversationsHeaderCard);
    filterToggleBtn->setObjectName(QStringLiteral("sideIconBtn"));
    filterToggleBtn->setFixedSize(76, 32);
    filterToggleBtn->setIsTransparent(true);
    filterToggleBtn->setCheckable(true);
    filterToggleBtn->setText(QStringLiteral("\u5168\u90E8"));
    filterToggleBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    filterToggleBtn->setToolTip(QStringLiteral("\u5206\u7EC4\u7B5B\u9009"));
    m_conversationFilterToggleBtn = filterToggleBtn;

    conversationsTitleRow->addWidget(modeChipWidget);
    conversationsTitleRow->addStretch();
    conversationsTitleRow->addWidget(filterToggleBtn);
    conversationsTitleRow->addWidget(statusChipWidget);

    conversationsHeaderCardLayout->addLayout(conversationsTitleRow);

    convHeaderLayout->addWidget(conversationsHeaderCard);
    conversationsLayout->addWidget(convHeaderBar);

    // 鈹€鈹€ 消息 body锛堢瓫閫夐潰鏉?| 浼氳瘽鍒楄〃锛屾按骞虫帓鍒楋級鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    auto* convBodyWidget = new QWidget(conversationsPage);
    auto* convBodyLayout = new QHBoxLayout(convBodyWidget);
    convBodyLayout->setContentsMargins(0, 0, 0, 0);
    convBodyLayout->setSpacing(0);

    auto* filterPanel = new ElaFrame(this, Qt::Popup | Qt::FramelessWindowHint);
    filterPanel->setObjectName(QStringLiteral("filterPanel"));
    filterPanel->setAttribute(Qt::WA_DeleteOnClose, false);
    filterPanel->setMinimumWidth(188);
    filterPanel->hide();
    filterPanel->installEventFilter(this);
    m_conversationFilterPanel = filterPanel;
    auto* filterPanelLayout = new QVBoxLayout(filterPanel);
    filterPanelLayout->setContentsMargins(8, 8, 8, 8);
    filterPanelLayout->setSpacing(1);

    // "分组" 小标题行
    auto* filterGroupRow = new QWidget(filterPanel);
    auto* filterGroupRowLayout = new QHBoxLayout(filterGroupRow);
    filterGroupRowLayout->setContentsMargins(14, 2, 8, 6);
    filterGroupRowLayout->setSpacing(2);
    auto* filterGroupLabel = new ElaText(QStringLiteral("\u5206\u7EC4"), filterGroupRow); // 分组
    filterGroupLabel->setObjectName(QStringLiteral("filterGroupLabel"));
    auto* filterSettingsBtn = new ElaToolButton(filterGroupRow);
    filterSettingsBtn->setObjectName(QStringLiteral("sideIconBtn"));
    filterSettingsBtn->setIsTransparent(true);
    filterSettingsBtn->setElaIcon(ElaIconType::Gear);
    filterSettingsBtn->setFixedSize(compactIconButtonSize, compactIconButtonSize);
    filterGroupRowLayout->addWidget(filterGroupLabel, 1);
    filterGroupRowLayout->addWidget(filterSettingsBtn);
    filterPanelLayout->addWidget(filterGroupRow);

    // 绛涢€夋潯鐩瀯寤鸿緟鍔?lambda
    QVector<ElaPushButton*> filterBtns;
    const auto addFilterItem = [&](const QString& icon, const QString& label,
                                   int count = -1) -> ElaPushButton* {
        auto* btn = new ElaPushButton(filterPanel);
        btn->setObjectName(QStringLiteral("filterItem"));
        btn->setCheckable(true);
        btn->setFlat(true);
        btn->setFixedHeight(filterButtonHeight);

        // 构建 "  icon  label  [count]" 格式文字
        QString txt = QStringLiteral("  ") + icon + QStringLiteral("  ") + label;
        if (count > 0) {
            txt += QStringLiteral("  \u00B7 ") + QString::number(count);
        }
        btn->setText(txt);
        filterPanelLayout->addWidget(btn);
        filterBtns.append(btn);
        return btn;
    };

    auto* msgFilterBtn    = addFilterItem(QStringLiteral("\u25CF"), QStringLiteral("\u6D88\u606F"), 0);  // 鈼?消息
    addFilterItem(QStringLiteral("\u21BA"), QStringLiteral("\u672A\u8BFB"), 0);  // 未读
    addFilterItem(QStringLiteral("\u2691"), QStringLiteral("\u6807\u8BB0"), 0);  // 标记
    addFilterItem(QStringLiteral("@"), QStringLiteral("@\u6211"), 0);
    addFilterItem(QStringLiteral("\u25BC"), QStringLiteral("\u6807\u7B7E"));
    addFilterItem(QStringLiteral("\u2500"), QStringLiteral("\u5355\u804A"));    // ─ 单聊
    addFilterItem(QStringLiteral("\u25A6"), QStringLiteral("\u7FA4\u7EC4"));    // 鈻?缇ょ粍
    addFilterItem(QStringLiteral("\u2713"), QStringLiteral("\u5DF2\u5B8C\u6210"));
    filterPanelLayout->addStretch();
    msgFilterBtn->setChecked(true);

    // 绛涢€夋寜閽簰鏂ョ偣鍑伙紙radio 琛屼负锛?    // 娉ㄦ剰锛氬繀椤绘寜鍊兼崟鑾?filterBtns锛屽惁鍒欐瀯閫犲嚱鏁扮粨鏉熷悗寮曠敤鎮┖瀵艰嚧宕╂簝
    for (int i = 0; i < filterBtns.size(); ++i) {
        connect(filterBtns[i], &ElaPushButton::clicked, this, [this, i, filterBtns](bool) mutable {
            for (int j = 0; j < filterBtns.size(); ++j) {
                filterBtns[j]->setChecked(j == i);
            }
            emit conversationFilterChanged(i);
            hideConversationFilterPanel();
        });
    }

    m_conversationEmptyLabel = new ElaText(
        QStringLiteral("\u8FD8\u6CA1\u6709\u6700\u8FD1\u6D88\u606F\n\u4F60\u53EF\u4EE5\u4ECE\u8054\u7CFB\u4EBA\u53D1\u8D77\u5BF9\u8BDD\uFF0C\u6216\u624B\u52A8\u8FDE\u63A5\u4E00\u53F0\u8BBE\u5907"),
        convBodyWidget);
    m_conversationEmptyLabel->setAlignment(Qt::AlignCenter);
    m_conversationEmptyLabel->setWordWrap(true);
    m_conversationEmptyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_conversationEmptyLabel->setStyleSheet(QStringLiteral(
        "font-size:13px; color:%1; background:transparent; border:none; padding:18px 20px;")
        .arg(AppStyle::textMuted()));
    convBodyLayout->addWidget(m_conversationEmptyLabel, 1);

    convBodyLayout->addWidget(m_conversationList, 1);
    conversationsLayout->addWidget(convBodyWidget, 1);
    connect(filterToggleBtn, &QAbstractButton::toggled, this, [this](bool open) {
        if (open) {
            showConversationFilterPanel();
            return;
        }
        hideConversationFilterPanel();
    });

    m_sideStack->addWidget(conversationsPage); // index 0

    // 联系人页已迁移到 DirectoryPage

    // 渚ц竟鏍?Page 1: 传输列表（原 index 2，联系人和通知已迁移）
    auto* transfersPage = buildPageScaffold(new QWidget(m_sideStack), QStringLiteral("transfers"));
    auto* transfersLayout = new QVBoxLayout(transfersPage);
    transfersLayout->setContentsMargins(0, 0, 0, 0);
    transfersLayout->setSpacing(0);
    auto* transferSectionLabel = new ElaText(QStringLiteral("\u4F20\u8F93"), transfersPage);
    transferSectionLabel->setObjectName(QStringLiteral("sideHeader"));
    auto* transferHeader = new ElaFrame(transfersPage);
    transferHeader->setObjectName(QStringLiteral("sideHeaderBar"));
    auto* transferHeaderLayout = new QVBoxLayout(transferHeader);
    transferHeaderLayout->setContentsMargins(12, 12, 12, 12);
    transferHeaderLayout->setSpacing(10);

    auto* transferHeaderCard = new ElaFrame(transferHeader);
    transferHeaderCard->setObjectName(QStringLiteral("transferHeaderCard"));
    auto* transferHeaderCardLayout = new QVBoxLayout(transferHeaderCard);
    transferHeaderCardLayout->setContentsMargins(16, 14, 16, 14);
    transferHeaderCardLayout->setSpacing(10);
    addSideSurfaceBand(transferHeaderCard,
                       transferHeaderCardLayout,
                       QStringLiteral("transferModeChip"),
                       QStringLiteral("\u4F20\u8F93\u770B\u677F"),
                       QStringLiteral("transferStatusChip"),
                       QStringLiteral("\u5B9E\u65F6\u8FFD\u8E2A"));
    m_transferStatusChip =
        transferHeaderCard->findChild<ElaText*>(QStringLiteral("transferStatusChip"));

    auto* transferFilterBand = new ElaFrame(transferHeaderCard);
    transferFilterBand->setObjectName(QStringLiteral("transferFilterBand"));
    transferFilterBand->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* transferFilterBandLayout = new QVBoxLayout(transferFilterBand);
    transferFilterBandLayout->setContentsMargins(0, 0, 0, 0);
    transferFilterBandLayout->setSpacing(8);

    auto* transferFilterGrid = new QGridLayout;
    transferFilterGrid->setContentsMargins(0, 0, 0, 0);
    transferFilterGrid->setHorizontalSpacing(6);
    transferFilterGrid->setVerticalSpacing(6);
    const auto buildTransferFilterButton = [transferFilterBand](const QString& text) {
        auto* button = new ElaToolButton(transferFilterBand);
        button->setText(text);
        button->setIsTransparent(true);
        button->setObjectName(QStringLiteral("filterItem"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return button;
    };
    m_transferFilterAllBtn = buildTransferFilterButton(QStringLiteral("\u5168\u90E8"));
    m_transferFilterOutgoingBtn = buildTransferFilterButton(QStringLiteral("\u53D1\u9001"));
    m_transferFilterIncomingBtn = buildTransferFilterButton(QStringLiteral("\u63A5\u6536"));
    m_transferFilterActiveBtn = buildTransferFilterButton(QStringLiteral("\u8FDB\u884C\u4E2D"));
    m_transferFilterFailedBtn = buildTransferFilterButton(QStringLiteral("\u5931\u8D25"));
    m_transferFilterCompletedBtn = buildTransferFilterButton(QStringLiteral("\u5DF2\u5B8C\u6210"));
    m_transferFilterAllBtn->setChecked(true);
    const QList<ElaToolButton*> transferFilterButtons = {m_transferFilterAllBtn,
                                                       m_transferFilterOutgoingBtn,
                                                       m_transferFilterIncomingBtn,
                                                       m_transferFilterActiveBtn,
                                                       m_transferFilterFailedBtn,
                                                       m_transferFilterCompletedBtn};
    for (int buttonIndex = 0; buttonIndex < transferFilterButtons.size(); ++buttonIndex) {
        transferFilterGrid->addWidget(transferFilterButtons.at(buttonIndex),
                                      buttonIndex / 3,
                                      buttonIndex % 3);
    }
    transferFilterBandLayout->addLayout(transferFilterGrid);
    transferHeaderCardLayout->addWidget(transferFilterBand);
    transferHeaderLayout->addWidget(transferHeaderCard);
    transferSectionLabel->hide();
    transfersLayout->addWidget(transferHeader);
    auto* transfersBodyWidget = new QWidget(transfersPage);
    auto* transfersBodyLayout = new QVBoxLayout(transfersBodyWidget);
    transfersBodyLayout->setContentsMargins(0, 0, 0, 0);
    transfersBodyLayout->setSpacing(0);
    m_transferEmptyLabel = new ElaText(
        QStringLiteral("\u6682\u65E0\u4F20\u8F93\u4EFB\u52A1\n\u6587\u4EF6\u53D1\u9001\u3001\u63A5\u6536\u548C\u7EED\u4F20\u8BB0\u5F55\u4F1A\u663E\u793A\u5728\u8FD9\u91CC"),
        transfersBodyWidget);
    m_transferEmptyLabel->setAlignment(Qt::AlignCenter);
    m_transferEmptyLabel->setWordWrap(true);
    m_transferEmptyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_transferEmptyLabel->setStyleSheet(QStringLiteral(
        "font-size:13px; color:%1; background:transparent; border:none; padding:18px 20px;")
        .arg(AppStyle::textMuted()));
    transfersBodyLayout->addWidget(m_transferEmptyLabel, 1);
    transfersBodyLayout->addWidget(m_transferList, 1);
    transfersLayout->addWidget(transfersBodyWidget, 1);
    m_sideStack->addWidget(transfersPage); // index 1 (ԭ index 2)

    // 通知页已迁移到 NotificationsPage

    // 内容区 Page 0: 欢迎页（未选择会话时显示）
    auto* welcomePage = new QWidget(m_contentStack);
    welcomePage->setObjectName(QStringLiteral("welcomePage"));
    welcomePage->setAutoFillBackground(false);
    welcomePage->setAttribute(Qt::WA_TranslucentBackground, true);
    welcomePage->setStyleSheet(QStringLiteral("QWidget#welcomePage { background: transparent; border: none; }"));
    auto* welcomeLayout = new QVBoxLayout(welcomePage);
    welcomeLayout->setContentsMargins(36, 36, 36, 36);
    welcomeLayout->setAlignment(Qt::AlignCenter);
    welcomeLayout->setSpacing(0);

    auto* welcomeHeroShell = new ElaFrame(welcomePage);
    welcomeHeroShell->setObjectName(QStringLiteral("welcomeHeroShell"));
    welcomeHeroShell->setMaximumWidth(1000);
    welcomeHeroShell->setMinimumHeight(520);
    auto* welcomeHeroShellLayout = new QVBoxLayout(welcomeHeroShell);
    welcomeHeroShellLayout->setContentsMargins(24, 20, 24, 24);
    welcomeHeroShellLayout->setSpacing(18);

    auto* welcomeHeroChrome = new ElaFrame(welcomeHeroShell);
    welcomeHeroChrome->setObjectName(QStringLiteral("welcomeHeroChrome"));
    auto* welcomeHeroChromeLayout = new QHBoxLayout(welcomeHeroChrome);
    welcomeHeroChromeLayout->setContentsMargins(14, 8, 14, 8);
    welcomeHeroChromeLayout->setSpacing(10);

    for (int dotIndex = 0; dotIndex < 3; ++dotIndex) {
        auto* chromeDot = new ElaFrame(welcomeHeroChrome);
        chromeDot->setObjectName(QStringLiteral("welcomeHeroChromeDot"));
        chromeDot->setProperty("chromeTone",
                               dotIndex == 0 ? QStringLiteral("accent")
                                             : (dotIndex == 1 ? QStringLiteral("soft")
                                                              : QStringLiteral("ghost")));
        chromeDot->setFixedSize(10, 10);
        welcomeHeroChromeLayout->addWidget(chromeDot);
    }

    const QStringList welcomeWarmNotes = {
        QStringLiteral("今天也慢慢来，把重要的消息安稳处理好"),
        QStringLiteral("从最近的对话开始，事情会一点点清楚起来"),
        QStringLiteral("保持轻一点的节奏，先看见此刻最重要的事"),
        QStringLiteral("把消息理顺，今天的协作就已经向前了一步")
    };
    const int welcomeWarmIndex =
        static_cast<int>(QDateTime::currentSecsSinceEpoch() % welcomeWarmNotes.size());

    auto* welcomeHeroChromeMode =
        new ElaText(QStringLiteral("\u4ECA\u65E5\u5C0F\u8BB0"), welcomeHeroChrome);
    welcomeHeroChromeMode->setObjectName(QStringLiteral("welcomeHeroChromeMode"));
    auto* welcomeHeroChromeStatus =
        new ElaText(welcomeWarmNotes.value(welcomeWarmIndex), welcomeHeroChrome);
    welcomeHeroChromeStatus->setObjectName(QStringLiteral("welcomeHeroChromeStatus"));
    m_welcomeRuntimeChromeStatus = welcomeHeroChromeStatus;
    welcomeHeroChromeLayout->addSpacing(6);
    welcomeHeroChromeLayout->addWidget(welcomeHeroChromeMode);
    welcomeHeroChromeLayout->addStretch();
    welcomeHeroChromeLayout->addWidget(welcomeHeroChromeStatus);

    auto* welcomeHeroStage = new ElaFrame(welcomeHeroShell);
    welcomeHeroStage->setObjectName(QStringLiteral("welcomeHeroStage"));
    auto* welcomeHeroStageLayout = new QHBoxLayout(welcomeHeroStage);
    welcomeHeroStageLayout->setContentsMargins(0, 0, 0, 0);
    welcomeHeroStageLayout->setSpacing(24);

    auto* welcomeAtmospherePanel = new ElaFrame(welcomeHeroStage);
    welcomeAtmospherePanel->setObjectName(QStringLiteral("welcomeAtmospherePanel"));
    welcomeAtmospherePanel->setFixedWidth(280);
    auto* welcomeAtmosphereLayout = new QVBoxLayout(welcomeAtmospherePanel);
    welcomeAtmosphereLayout->setContentsMargins(20, 20, 20, 20);
    welcomeAtmosphereLayout->setSpacing(14);

    auto* atmosphereKicker = new ElaText(QStringLiteral("\u6D88\u606F\u8282\u594F"), welcomeAtmospherePanel);
    atmosphereKicker->setObjectName(QStringLiteral("welcomeAtmosphereKicker"));
    auto* atmosphereTitle = new ElaText(QStringLiteral("\u8BA9\u6700\u8FD1\u7684\u4F1A\u8BDD\u548C\u7FA4\u804A\u4FDD\u6301\u6E05\u6670"), welcomeAtmospherePanel);
    atmosphereTitle->setObjectName(QStringLiteral("welcomeAtmosphereTitle"));
    atmosphereTitle->setWordWrap(true);
    auto* atmosphereBody = new ElaText(QStringLiteral("\u9ED8\u8BA4\u4FDD\u6301\u5B89\u9759\uFF0C\u5728\u9700\u8981\u51FA\u955C\u7684\u65F6\u5019\u518D\u628A\u5173\u952E\u72B6\u6001\u70B9\u4EAE\u3002"), welcomeAtmospherePanel);
    atmosphereBody->setObjectName(QStringLiteral("welcomeAtmosphereBody"));
    atmosphereBody->setWordWrap(true);

    auto* atmosphereSpotlight = new ElaFrame(welcomeAtmospherePanel);
    atmosphereSpotlight->setObjectName(QStringLiteral("welcomeAtmosphereSpotlight"));
    atmosphereSpotlight->setFixedHeight(176);
    auto* atmosphereSpotlightLayout = new QVBoxLayout(atmosphereSpotlight);
    atmosphereSpotlightLayout->setContentsMargins(16, 16, 16, 16);
    atmosphereSpotlightLayout->setSpacing(10);

    auto* atmospherePulseRow = new QHBoxLayout();
    atmospherePulseRow->setContentsMargins(0, 0, 0, 0);
    atmospherePulseRow->setSpacing(8);
    const auto addAtmospherePulse =
        [atmosphereSpotlight, atmospherePulseRow](const QSize& size, const QString& tone) {
            auto* pulse = new ElaFrame(atmosphereSpotlight);
            pulse->setObjectName(QStringLiteral("welcomeAtmospherePulse"));
            pulse->setProperty("pulseTone", tone);
            pulse->setFixedSize(size);
            atmospherePulseRow->addWidget(pulse);
            return pulse;
        };
    addAtmospherePulse(QSize(84, 12), QStringLiteral("lead"));
    addAtmospherePulse(QSize(58, 12), QStringLiteral("soft"));
    addAtmospherePulse(QSize(42, 12), QStringLiteral("thin"));
    atmospherePulseRow->addStretch();

    atmosphereSpotlightLayout->addLayout(atmospherePulseRow);

    for (int laneIndex = 0; laneIndex < 3; ++laneIndex) {
        auto* lane = new ElaFrame(atmosphereSpotlight);
        lane->setObjectName(QStringLiteral("welcomeAtmosphereLane"));
        auto* laneLayout = new QHBoxLayout(lane);
        laneLayout->setContentsMargins(12, 10, 12, 10);
        laneLayout->setSpacing(10);

        auto* node = new ElaFrame(lane);
        node->setObjectName(QStringLiteral("welcomeAtmosphereNode"));
        node->setProperty("nodeTone",
                          laneIndex == 1 ? QStringLiteral("accent") : QStringLiteral("muted"));
        node->setFixedSize(laneIndex == 1 ? QSize(16, 16) : QSize(12, 12));

        auto* bar = new ElaFrame(lane);
        bar->setObjectName(QStringLiteral("welcomeAtmosphereBar"));
        bar->setProperty("barTone",
                         laneIndex == 1 ? QStringLiteral("accent") : QStringLiteral("soft"));
        bar->setFixedHeight(laneIndex == 1 ? 12 : 10);

        auto* tail = new ElaFrame(lane);
        tail->setObjectName(QStringLiteral("welcomeAtmosphereBar"));
        tail->setProperty("barTone", QStringLiteral("ghost"));
        tail->setFixedSize(laneIndex == 2 ? QSize(44, 8) : QSize(30, 8));

        laneLayout->addWidget(node, 0, Qt::AlignVCenter);
        laneLayout->addWidget(bar, 1, Qt::AlignVCenter);
        laneLayout->addWidget(tail, 0, Qt::AlignVCenter);
        atmosphereSpotlightLayout->addWidget(lane);
    }

    auto* atmosphereDockRow = new QHBoxLayout();
    atmosphereDockRow->setContentsMargins(0, 2, 0, 0);
    atmosphereDockRow->setSpacing(10);
    const auto addDockCard = [atmosphereSpotlight, atmosphereDockRow](const QString& tone) {
        auto* dockCard = new ElaFrame(atmosphereSpotlight);
        dockCard->setObjectName(QStringLiteral("welcomeAtmosphereDockCard"));
        dockCard->setProperty("dockTone", tone);
        auto* dockLayout = new QVBoxLayout(dockCard);
        dockLayout->setContentsMargins(12, 12, 12, 12);
        dockLayout->setSpacing(8);

        auto* dockBar = new ElaFrame(dockCard);
        dockBar->setObjectName(QStringLiteral("welcomeAtmosphereDockBar"));
        dockBar->setProperty("dockTone", tone);
        dockBar->setFixedSize(tone == QStringLiteral("accent") ? QSize(74, 10) : QSize(62, 10));

        auto* dockNodeRow = new QHBoxLayout();
        dockNodeRow->setContentsMargins(0, 0, 0, 0);
        dockNodeRow->setSpacing(6);
        for (int nodeIndex = 0; nodeIndex < 2; ++nodeIndex) {
            auto* dockNode = new ElaFrame(dockCard);
            dockNode->setObjectName(QStringLiteral("welcomeAtmosphereNode"));
            dockNode->setProperty("nodeTone",
                                  tone == QStringLiteral("accent") && nodeIndex == 0
                                      ? QStringLiteral("accent")
                                      : QStringLiteral("muted"));
            dockNode->setFixedSize(10, 10);
            dockNodeRow->addWidget(dockNode);
        }
        dockNodeRow->addStretch();

        dockLayout->addWidget(dockBar, 0, Qt::AlignLeft);
        dockLayout->addLayout(dockNodeRow);
        atmosphereDockRow->addWidget(dockCard, 1);
        return dockCard;
    };
    addDockCard(QStringLiteral("accent"));
    addDockCard(QStringLiteral("soft"));
    atmosphereSpotlightLayout->addLayout(atmosphereDockRow);

    const auto addAtmosphereSignal = [welcomeAtmospherePanel, welcomeAtmosphereLayout](const QString& title,
                                                                                       const QString& detail) {
        auto* signalCard = new ElaFrame(welcomeAtmospherePanel);
        signalCard->setObjectName(QStringLiteral("welcomeAtmosphereSignalCard"));
        auto* signalLayout = new QVBoxLayout(signalCard);
        signalLayout->setContentsMargins(14, 12, 14, 12);
        signalLayout->setSpacing(4);

        auto* signalTitle = new ElaText(title, signalCard);
        signalTitle->setObjectName(QStringLiteral("welcomeAtmosphereSignalTitle"));
        auto* signalDetail = new ElaText(detail, signalCard);
        signalDetail->setObjectName(QStringLiteral("welcomeAtmosphereSignalDetail"));
        signalDetail->setWordWrap(true);

        signalLayout->addWidget(signalTitle);
        signalLayout->addWidget(signalDetail);
        welcomeAtmosphereLayout->addWidget(signalCard);
    };

    welcomeAtmosphereLayout->addWidget(atmosphereKicker);
    welcomeAtmosphereLayout->addWidget(atmosphereTitle);
    welcomeAtmosphereLayout->addWidget(atmosphereBody);
    welcomeAtmosphereLayout->addWidget(atmosphereSpotlight);
    addAtmosphereSignal(QStringLiteral("\u6D88\u606F\u57FA\u5EA7"),
                        QStringLiteral("\u4F1A\u8BDD\u3001\u672A\u8BFB\u4E0E\u91CD\u70B9\u72B6\u6001\u6536\u675F\u5230\u540C\u4E00\u5DE5\u4F5C\u533A"));
    addAtmosphereSignal(QStringLiteral("\u534F\u4F5C\u7EBF\u7D22"),
                        QStringLiteral("\u7FA4\u516C\u544A\u3001\u7FA4\u6210\u5458\u548C\u4E0A\u4E0B\u6587\u90FD\u653E\u5728\u9700\u8981\u65F6\u518D\u6253\u5F00"));
    welcomeAtmosphereLayout->addStretch();

    auto* welcomeCard = new ElaFrame(welcomeHeroStage);
    welcomeCard->setObjectName(QStringLiteral("welcomeCard"));
    welcomeCard->setMaximumWidth(900);
    welcomeCard->setMinimumWidth(520);
    m_welcomeCard = welcomeCard;

    auto* welcomeCardLayout = new QHBoxLayout(welcomeCard);
    welcomeCardLayout->setContentsMargins(40, 38, 40, 38);
    welcomeCardLayout->setSpacing(24);

    auto* welcomeLeftColumn = new QVBoxLayout();
    welcomeLeftColumn->setContentsMargins(0, 0, 0, 0);
    welcomeLeftColumn->setSpacing(16);
    welcomeLeftColumn->setAlignment(Qt::AlignVCenter);

    auto* welcomeKicker = new ElaText(QStringLiteral("LEYOCHAT DESKTOP"), welcomeCard);
    welcomeKicker->setObjectName(QStringLiteral("welcomeKicker"));
    welcomeKicker->setAlignment(Qt::AlignCenter);

    auto* welcomeMark = new ElaText(QStringLiteral("H"), welcomeCard);
    welcomeMark->setObjectName(QStringLiteral("welcomeMark"));
    welcomeMark->setAlignment(Qt::AlignCenter);

    auto* welcomeTitle = new ElaText(QStringLiteral("\u4ECE\u4E00\u6B21\u6E05\u723D\u7684\u5BF9\u8BDD\u5F00\u59CB"), welcomeCard);
    welcomeTitle->setObjectName(QStringLiteral("welcomeTitle"));
    welcomeTitle->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    welcomeTitle->setWordWrap(true);
    welcomeTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* welcomeText = new ElaText(
        QStringLiteral("\u4ECE\u6700\u8FD1\u6D88\u606F\u7EE7\u7EED\uFF0C\u6216\u8005\u6309\u4F1A\u8BDD\u3001\u6587\u672C\u3001\u56FE\u7247\u548C\u94FE\u63A5\u67E5\u627E\u5386\u53F2\u8BB0\u5F55\u3002"),
        welcomeCard);
    welcomeText->setObjectName(QStringLiteral("welcomeSubtitle"));
    welcomeText->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    welcomeText->setWordWrap(true);

    auto* welcomeMetaRow = new QHBoxLayout();
    welcomeMetaRow->setContentsMargins(0, 0, 0, 0);
    welcomeMetaRow->setSpacing(10);

    auto* metaChat = new ElaText(QStringLiteral("\u5355\u804A\u4E0E\u7FA4\u804A"), welcomeCard);
    metaChat->setObjectName(QStringLiteral("welcomeMeta"));
    auto* metaTransfer = new ElaText(QStringLiteral("\u5386\u53F2\u67E5\u627E"), welcomeCard);
    metaTransfer->setObjectName(QStringLiteral("welcomeMeta"));
    auto* metaFuture = new ElaText(QStringLiteral("\u7FA4\u6210\u5458\u4E0A\u4E0B\u6587"), welcomeCard);
    metaFuture->setObjectName(QStringLiteral("welcomeMeta"));
    welcomeMetaRow->addWidget(metaChat);
    welcomeMetaRow->addWidget(metaTransfer);
    welcomeMetaRow->addWidget(metaFuture);
    welcomeMetaRow->addStretch();

    auto* welcomeActionRow = new QHBoxLayout();
    welcomeActionRow->setContentsMargins(0, 4, 0, 0);
    welcomeActionRow->setSpacing(10);

    auto* welcomeConnectBtn = new ElaPushButton(UiIcons::actionSearch() + QStringLiteral(" \u641C\u7D22\u804A\u5929\u8BB0\u5F55"), welcomeCard);
    welcomeConnectBtn->setObjectName(QStringLiteral("welcomePrimaryAction"));
    auto* welcomeCreateGroupBtn = new ElaPushButton(UiIcons::actionAdd() + QStringLiteral(" \u65B0\u5EFA\u7FA4\u804A"), welcomeCard);
    welcomeCreateGroupBtn->setObjectName(QStringLiteral("welcomeSecondaryAction"));
    welcomeActionRow->addWidget(welcomeConnectBtn);
    welcomeActionRow->addWidget(welcomeCreateGroupBtn);
    welcomeActionRow->addStretch();

    auto* welcomePreviewCard = new ElaFrame(welcomeCard);
    welcomePreviewCard->setObjectName(QStringLiteral("welcomePreviewCard"));
    m_welcomePreviewCard = welcomePreviewCard;
    auto* welcomePreviewLayout = new QVBoxLayout(welcomePreviewCard);
    welcomePreviewLayout->setContentsMargins(18, 18, 18, 18);
    welcomePreviewLayout->setSpacing(14);

    auto* welcomeMetricRow = new QHBoxLayout();
    welcomeMetricRow->setContentsMargins(0, 0, 0, 0);
    welcomeMetricRow->setSpacing(12);
    const auto addPreviewMetric =
        [welcomePreviewCard, welcomeMetricRow](const QString& valueObjectName,
                                               const QString& metric,
                                               const QString& label) {
            auto* metricBlock = new ElaFrame(welcomePreviewCard);
            metricBlock->setObjectName(QStringLiteral("welcomeMetricCard"));
            auto* metricLayout = new QVBoxLayout(metricBlock);
            metricLayout->setContentsMargins(14, 12, 14, 12);
            metricLayout->setSpacing(2);

            auto* metricValue = new ElaText(metric, metricBlock);
            metricValue->setObjectName(valueObjectName);
            metricValue->setProperty("welcomeMetricRole", QStringLiteral("value"));
            auto* metricLabel = new ElaText(label, metricBlock);
            metricLabel->setObjectName(QStringLiteral("welcomePreviewLabel"));
            metricLabel->setAlignment(Qt::AlignCenter);
            metricLabel->setWordWrap(true);
            metricLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            const int labelWidth = QFontMetrics(metricLabel->font()).horizontalAdvance(label) + 20;
            const int valueWidth = QFontMetrics(metricValue->font()).horizontalAdvance(metric) + 24;
            metricBlock->setMinimumWidth(qMax(120, qMax(labelWidth, valueWidth)));

            metricLayout->addWidget(metricValue);
            metricLayout->addWidget(metricLabel);
            welcomeMetricRow->addWidget(metricBlock, 1);
            return metricValue;
        };
    m_welcomeMessagesMetricValue =
        addPreviewMetric(QStringLiteral("welcomeMessagesMetricValue"),
                         QStringLiteral("0"),
                         QStringLiteral("\u672A\u8BFB"));
    m_welcomeContactsMetricValue =
        addPreviewMetric(QStringLiteral("welcomeContactsMetricValue"),
                         QStringLiteral("0"),
                         QStringLiteral("\u5728\u7EBF"));
    m_welcomeTransfersMetricValue =
        addPreviewMetric(QStringLiteral("welcomeTransfersMetricValue"),
                         QStringLiteral("0"),
                         QStringLiteral("\u7FA4\u804A"));

    auto* welcomePreviewBody = new ElaText(
        QStringLiteral("\u5DE6\u4FA7\u770B\u6700\u8FD1\u4F1A\u8BDD\uFF0C\u8FDB\u5165\u7FA4\u804A\u540E\u518D\u6309\u9700\u6253\u5F00\u7FA4\u6210\u5458\u4E0E\u516C\u544A\uFF0C\u9ED8\u8BA4\u4FDD\u6301\u6E05\u723D\u3002"),
        welcomePreviewCard);
    welcomePreviewBody->setObjectName(QStringLiteral("welcomePreviewBody"));
    welcomePreviewBody->setAlignment(Qt::AlignCenter);
    welcomePreviewBody->setWordWrap(true);

    m_welcomeRuntimeSummary = new ElaText(
        QStringLiteral("\u8FD8\u6CA1\u6709\u9009\u4E2D\u4F1A\u8BDD"),
        welcomePreviewCard);
    m_welcomeRuntimeSummary->setObjectName(QStringLiteral("welcomeRuntimeSummary"));
    m_welcomeRuntimeSummary->setAlignment(Qt::AlignCenter);
    m_welcomeRuntimeSummary->setWordWrap(true);
    m_welcomeRuntimeSummary->setTextPixelSize(16);

    m_welcomeRuntimeDetail = new ElaText(
        QStringLiteral("\u9009\u62E9\u4E00\u6761\u6D88\u606F\u540E\uFF0C\u8FD9\u91CC\u4F1A\u5207\u6362\u5230\u5BF9\u5E94\u7684\u804A\u5929\u4E0A\u4E0B\u6587\u3002"),
        welcomePreviewCard);
    m_welcomeRuntimeDetail->setObjectName(QStringLiteral("welcomeRuntimeDetail"));
    m_welcomeRuntimeDetail->setAlignment(Qt::AlignCenter);
    m_welcomeRuntimeDetail->setWordWrap(true);
    m_welcomeRuntimeDetail->setTextPixelSize(13);

    welcomePreviewLayout->addLayout(welcomeMetricRow);
    welcomePreviewLayout->addWidget(welcomePreviewBody);
    welcomePreviewLayout->addWidget(m_welcomeRuntimeSummary);
    welcomePreviewLayout->addWidget(m_welcomeRuntimeDetail);

    welcomeLeftColumn->addWidget(welcomeKicker, 0, Qt::AlignLeft);
    welcomeLeftColumn->addWidget(welcomeMark, 0, Qt::AlignLeft);
    welcomeLeftColumn->addWidget(welcomeTitle);
    welcomeLeftColumn->addWidget(welcomeText);
    welcomeLeftColumn->addLayout(welcomeMetaRow);
    welcomeLeftColumn->addLayout(welcomeActionRow);
    welcomeLeftColumn->addStretch();

    welcomeCardLayout->addLayout(welcomeLeftColumn, 5);
    welcomeCardLayout->addWidget(welcomePreviewCard, 4);

    welcomeHeroStageLayout->addWidget(welcomeAtmospherePanel, 0, Qt::AlignVCenter);
    welcomeHeroStageLayout->addWidget(welcomeCard, 1, Qt::AlignVCenter);
    welcomeHeroShellLayout->addWidget(welcomeHeroChrome);
    welcomeHeroShellLayout->addWidget(welcomeHeroStage, 1);

    welcomeLayout->addWidget(welcomeHeroShell, 1, Qt::AlignCenter);

    connect(welcomeCreateGroupBtn, &QAbstractButton::clicked, this, [this]() {
        emit createGroupRequested();
    });
    m_contentStack->addWidget(welcomePage); // index 0

    // 鈹€鈹€ 鍐呭鍖?Page 1: 瀵硅瘽椤碉紙閫変腑浼氳瘽鍚庢樉绀猴級鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    auto* chatPage = new ChatWorkspaceWidget(m_contentStack);
    chatPage->setAutoFillBackground(false);
    chatPage->setMouseTracking(true);
    chatPage->installEventFilter(this);
    auto* chatPageLayout = new QVBoxLayout(chatPage);
    chatPageLayout->setContentsMargins(0, 0, 0, 0);
    chatPageLayout->setSpacing(0);

    m_conversationList->setSelectionMode(QAbstractItemView::NoSelection);
    m_conversationList->setFrameShape(QFrame::NoFrame);
    m_conversationList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_conversationList->setFocusPolicy(Qt::NoFocus);
    m_conversationList->setMouseTracking(true);
    m_conversationList->setUniformItemSizes(true);
    m_conversationList->setVerticalScrollBar(new ElaScrollBar(m_conversationList));
    m_conversationList->viewport()->setAttribute(Qt::WA_Hover, true);
    m_conversationCardDelegate = new ConversationCardDelegate(m_conversationList);
    m_conversationList->setItemDelegate(m_conversationCardDelegate);
    m_conversationList->viewport()->installEventFilter(m_conversationCardDelegate);
    connect(m_conversationCardDelegate, &ConversationCardDelegate::clicked,
            this, &ConversationsPage::onConversationItemClicked);
    connect(m_conversationCardDelegate, &ConversationCardDelegate::contextMenuRequested,
            this, &ConversationsPage::onConversationItemContextMenu);
    connect(m_conversationCardDelegate, &ConversationCardDelegate::avatarHovered,
            this, &ConversationsPage::conversationAvatarHovered);
    connect(m_conversationCardDelegate, &ConversationCardDelegate::avatarHoverLeft,
            this, [this]() { if (m_profileCard) m_profileCard->scheduleHide(); });
    m_transferList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_transferList->setWordWrap(true);
    m_transferList->setUniformItemSizes(false);
    m_transferList->setSpacing(4);
    m_transferList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_transferList->setMouseTracking(true);
    m_transferList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_conversationList->hide();
    m_transferList->hide();
    m_messageList->setFrameShape(QFrame::NoFrame);
    m_messageList->setSelectionMode(QAbstractItemView::NoSelection);
    m_messageList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_messageList->setFocusPolicy(Qt::ClickFocus);
    m_messageList->setMouseTracking(true);
    m_messageList->setVerticalScrollBar(new ElaScrollBar(m_messageList));
    m_messageList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_messageList->setResizeMode(QListView::Adjust);
    m_messageList->setSpacing(10);
    m_messageList->setObjectName(QStringLiteral("messageListView"));
    m_messageList->setStyleSheet(QStringLiteral(
        "QListView#messageListView { background: transparent; border: none; outline: none; }"
        "QListView#messageListView::item { background: transparent; border: none; padding: 0; }"));
    m_messageBubbleDelegate = new MessageBubbleDelegate(m_messageList);
    m_messageList->setItemDelegate(m_messageBubbleDelegate);

    connect(m_messageBubbleDelegate, &MessageBubbleDelegate::reactionToggled,
            this, &ConversationsPage::reactionRequested);

    // 右键菜单：通过 view 的 customContextMenuRequested 触发
    m_messageList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_messageList, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex index = m_messageList->indexAt(pos);
        if (!index.isValid()) return;
        const QString messageId = index.data(MessageListModel::MessageIdRole).toString();
        if (messageId.isEmpty()) return;
        onMessageBubbleContextMenu(messageId, m_messageList->viewport()->mapToGlobal(pos));
    });
    // Delegate 信号连接
    connect(m_messageBubbleDelegate, &MessageBubbleDelegate::messageFileOpenRequested,
            this, [this](const QString& msgId) {
                if (!msgId.trimmed().isEmpty()) emit openMessageFileRequested(msgId.trimmed());
            });
    connect(m_messageBubbleDelegate, &MessageBubbleDelegate::messageFileRevealRequested,
            this, [this](const QString& msgId) {
                if (!msgId.trimmed().isEmpty()) emit revealMessageFileRequested(msgId.trimmed());
            });
    connect(m_messageBubbleDelegate, &MessageBubbleDelegate::messageFileDownloadRequested,
            this, [this](const QString& msgId) {
                if (msgId.trimmed().isEmpty()) return;
                if (!m_messageModel) { emit fileServiceDownloadRequested(msgId.trimmed()); return; }
                for (int i = 0; i < m_messageModel->rowCount(); ++i) {
                    const QModelIndex ix = m_messageModel->index(i, 0);
                    if (ix.data(MessageListModel::MessageIdRole).toString() == msgId) {
                        if (!ix.data(MessageListModel::FileCardJsonRole).toString().trimmed().isEmpty()) {
                            emit groupFileDownloadRequested(msgId.trimmed());
                            return;
                        }
                        break;
                    }
                }
                emit fileServiceDownloadRequested(msgId.trimmed());
            });
    connect(m_messageBubbleDelegate, &MessageBubbleDelegate::messageFileVersionHistoryRequested,
            this, [this](const QString& msgId) {
                if (!msgId.trimmed().isEmpty()) emit fileServiceVersionHistoryRequested(msgId.trimmed());
            });
    connect(m_messageBubbleDelegate, &MessageBubbleDelegate::messageFilePreviewRequested,
            this, [this](const QString& msgId) {
                qDebug() << "[BUG2-DIAG] messageFilePreviewRequested received, msgId=" << msgId;
                if (msgId.trimmed().isEmpty() || !m_messageModel) return;
                for (int i = 0; i < m_messageModel->rowCount(); ++i) {
                    const QModelIndex ix = m_messageModel->index(i, 0);
                    if (ix.data(MessageListModel::MessageIdRole).toString() != msgId) continue;
                    QString aName = ix.data(MessageListModel::AttachmentNameRole).toString();
                    QString lPath = ix.data(MessageListModel::LocalFilePathRole).toString();
                    const QString fcj = ix.data(MessageListModel::FileCardJsonRole).toString().trimmed();
                    if (!fcj.isEmpty()) {
                        const QJsonObject obj = QJsonDocument::fromJson(fcj.toUtf8()).object();
                        if (aName.isEmpty()) aName = obj.value(QStringLiteral("file_name")).toString();
                        if (lPath.isEmpty()) lPath = obj.value(QStringLiteral("local_path")).toString();
                    }
                    // 图片文件：直接在此打开自定义查看器
                    if (!lPath.isEmpty() && QFileInfo::exists(lPath)) {
                        static const QStringList imgExts = {
                            QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                            QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("webp"),
                            QStringLiteral("ico"), QStringLiteral("svg")
                        };
                        if (imgExts.contains(QFileInfo(lPath).suffix().toLower())) {
                            qDebug() << "[BUG2-DIAG] Opening ImageViewerWidget for" << lPath;
                            auto* viewer = new ImageViewerWidget(lPath, aName, window());
                            viewer->show();
                            return;
                        }
                    }
                    emit openMessageFileRequested(msgId);
                    return;
                }
            });
    connect(m_messageBubbleDelegate, &MessageBubbleDelegate::messageTransferCancelRequested,
            this, [this](const QString& taskId) {
                if (!taskId.trimmed().isEmpty()) emit cancelTransferRequested(taskId.trimmed());
            });
    connect(m_messageBubbleDelegate, &MessageBubbleDelegate::readReceiptDetailRequested,
            this, [this](const QString& msgId) {
                if (!msgId.trimmed().isEmpty()) emit readReceiptDetailRequested(msgId.trimmed());
            });
    connect(m_messageBubbleDelegate, &MessageBubbleDelegate::linkClicked,
            this, [this](const QUrl& url) {
                emit messageUrlOpenRequested(url.toString());
            });
    connect(m_messageBubbleDelegate, &MessageBubbleDelegate::avatarClicked,
            this, [this](const QString& senderId, const QPoint& globalPos) {
                emit avatarProfileRequested(senderId, globalPos);
            });
    connect(m_messageBubbleDelegate, &MessageBubbleDelegate::forwardCardClicked,
            this, [this](const QString& messageId) {
                if (!m_messageModel) return;
                // 查找消息获取 payloadJson
                const auto& items = m_messageModel->items();
                for (const auto& item : items) {
                    if (QString::fromStdWString(item.messageId) == messageId) {
                        const QString payload = QString::fromStdWString(item.payloadJson);
                        if (payload.isEmpty()) return;
                        const QJsonObject pkg = QJsonDocument::fromJson(payload.toUtf8()).object();
                        auto* dlg = new ForwardDetailDialog(this->window());
                        dlg->setPackage(pkg);
                        dlg->open();
                        return;
                    }
                }
            });
    // 点击回复引用块 → 滚动到原消息并闪烁高亮
    connect(m_messageBubbleDelegate, &MessageBubbleDelegate::replyQuoteClicked,
            this, [this](const QString& replyToMessageId) {
                if (!m_messageModel || !m_messageList) return;
                const auto& items = m_messageModel->items();
                for (int row = 0; row < static_cast<int>(items.size()); ++row) {
                    if (QString::fromStdWString(items[row].messageId) == replyToMessageId) {
                        const QModelIndex targetIdx = m_messageModel->index(row, 0);
                        m_messageList->scrollTo(targetIdx, QAbstractItemView::PositionAtCenter);
                        // 设置闪烁高亮属性，delegate 读取后渲染高亮背景
                        m_messageList->setProperty("flashHighlightMessageId",
                                                   replyToMessageId);
                        m_messageList->viewport()->update();
                        QTimer::singleShot(1500, this, [this]() {
                            if (m_messageList) {
                                m_messageList->setProperty("flashHighlightMessageId", QString());
                                m_messageList->viewport()->update();
                            }
                        });
                        return;
                    }
                }
            });
    m_hostEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));
    m_portEdit->setPlaceholderText(QStringLiteral("\u7AEF\u53E3"));
    connect(m_chatHeaderWidget, &ChatHeaderWidget::directHistoryRequested, this, [this]() {
        auto* model = m_messageModel;
        if (model) {
            const QString title = m_chatHeaderWidget->titleText();
            auto* dlg = new ConversationHistoryDialog(title,
                                                      model->items(),
                                                      m_localDisplayName,
                                                      model->senderDisplayNameMap(),
                                                      this);
            connect(dlg, &ConversationHistoryDialog::messageJumpRequested,
                    this, [this](const QString& /*convId*/, const QString& messageId) {
                        if (!messageId.isEmpty() && m_messageList && m_messageModel) {
                            const int row = m_messageModel->findRowByMessageId(messageId);
                            if (row >= 0) {
                                m_messageList->scrollTo(m_messageModel->index(row),
                                                       QAbstractItemView::PositionAtCenter);
                                m_messageList->setCurrentIndex(m_messageModel->index(row));
                            }
                        }
                    });
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        }
    });
    connect(m_chatHeaderWidget, &ChatHeaderWidget::directVoiceCallRequested, this, [this]() {
        QString clientId = m_selectedConversationId.trimmed();
        if (clientId.contains(QLatin1Char('|')) && m_messageModel) {
            const QString peerId =
                DirectConversationAddressing::otherParticipant(m_messageModel->localClientId(), clientId);
            if (!peerId.trimmed().isEmpty()) {
                clientId = peerId.trimmed();
            }
        }
        if (clientId.isEmpty()) {
            setStatusMessage(QStringLiteral("请先选择联系人"), 2000);
            return;
        }
        emit voiceCallRequested(clientId);
    });
    connect(m_chatHeaderWidget, &ChatHeaderWidget::directCreateGroupRequested, this, [this]() {
        QString peerId = m_selectedConversationId.trimmed();
        if (peerId.contains(QLatin1Char('|')) && m_messageModel) {
            const QString other =
                DirectConversationAddressing::otherParticipant(m_messageModel->localClientId(), peerId);
            if (!other.trimmed().isEmpty()) {
                peerId = other.trimmed();
            }
        }
        emit createGroupWithPeerRequested(peerId);
    });
    connect(m_chatHeaderWidget, &ChatHeaderWidget::groupAnnouncementRequested, this, [this]() {
        toggleGroupInfoPanel();
    });
    connect(m_chatHeaderWidget, &ChatHeaderWidget::groupInfoPanelRequested, this, [this]() {
        toggleGroupInfoPanel();
    });
    connect(m_chatHeaderWidget, &ChatHeaderWidget::groupAddMemberRequested, this, [this]() {
        emit groupAddMemberRequested(m_currentGroupId);
    });
    connect(m_chatHeaderWidget, &ChatHeaderWidget::groupHistoryRequested, this, [this]() {
        auto* model = m_messageModel;
        if (model) {
            const QString title = m_chatHeaderWidget->titleText();
            auto* dlg = new ConversationHistoryDialog(title,
                                                      model->items(),
                                                      m_localDisplayName,
                                                      model->senderDisplayNameMap(),
                                                      this);
            connect(dlg, &ConversationHistoryDialog::messageJumpRequested,
                    this, [this](const QString& /*convId*/, const QString& messageId) {
                        if (!messageId.isEmpty() && m_messageList && m_messageModel) {
                            const int row = m_messageModel->findRowByMessageId(messageId);
                            if (row >= 0) {
                                m_messageList->scrollTo(m_messageModel->index(row),
                                                       QAbstractItemView::PositionAtCenter);
                                m_messageList->setCurrentIndex(m_messageModel->index(row));
                            }
                        }
                    });
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        }
    });
    connect(m_chatHeaderWidget, &ChatHeaderWidget::groupSettingsRequested, this, [this]() {
        if (!m_currentGroupId.trimmed().isEmpty()) {
            emit groupSettingsRequested(m_currentGroupId);
        }
    });
    connect(m_chatHeaderWidget, &ChatHeaderWidget::groupFileServiceSettingsRequested, this, [this]() {
        if (m_currentGroupId.trimmed().isEmpty()) {
            return;
        }
        setStatusMessage(QStringLiteral("群文件服务已默认启用，可在设置中修改消息服务器地址"), 3000);
    });
    connect(m_chatHeaderWidget, &ChatHeaderWidget::groupFileManagerRequested, this, [this]() {
        if (m_currentGroupId.isEmpty()) return;

        const auto fsConfig = effectiveGroupFileServiceConfigForGroup(m_currentGroupId);
        if (!fsConfig.enabled || fsConfig.baseUrl.isEmpty()) {
            setStatusMessage(QStringLiteral("群文件服务不可用，请在设置中检查消息服务器地址"), 3000);
            return;
        }

        emit groupFileManagerRequested(m_currentGroupId, fsConfig);
    });
    m_screenshotPollTimer = new QTimer(this);
    m_screenshotPollTimer->setInterval(350);
    connect(m_screenshotPollTimer, &QTimer::timeout, this, [this]() {
        const QImage clipboardImage = QGuiApplication::clipboard()->image();
        const QByteArray fingerprint = fingerprintForImage(clipboardImage);
        ++m_screenshotPollAttempts;
        if (!fingerprint.isEmpty() && fingerprint != m_lastClipboardImageFingerprint) {
            m_lastClipboardImageFingerprint = fingerprint;
            m_screenshotPollTimer->stop();
            if (m_restoreAfterScreenshot && m_mainWindow) {
                m_mainWindow->showNormal();
                m_mainWindow->raise();
                m_mainWindow->activateWindow();
            }
            m_restoreAfterScreenshot = false;
            m_screenshotPollAttempts = 0;

            // Open screenshot editor instead of directly importing
            auto* editor = new ScreenshotEditorWindow(clipboardImage, this);
            editor->setWindowModality(Qt::WindowModal);
            if (editor->exec() == QDialog::Accepted) {
                const QImage editedImage = editor->editedImage();
                const QImage finalImage = editedImage.isNull() ? clipboardImage : editedImage;
                // Copy to clipboard
                QGuiApplication::clipboard()->setImage(finalImage);
                importScreenshotPreview(finalImage);
                setStatusMessage(QStringLiteral("\u622A\u56FE\u5DF2\u52A0\u5165\u8F93\u5165\u6846\u5E76\u590D\u5236\u5230\u526A\u8D34\u677F"), 3000);
            }
            editor->deleteLater();
            return;
        }

        if (m_screenshotPollAttempts >= 60) {
            m_screenshotPollTimer->stop();
            if (m_restoreAfterScreenshot && m_mainWindow) {
                m_mainWindow->showNormal();
                m_mainWindow->raise();
                m_mainWindow->activateWindow();
            }
            m_restoreAfterScreenshot = false;
            m_screenshotPollAttempts = 0;
        }
    });
    connect(m_screenshotBtn, &QAbstractButton::clicked, this, [this]() {
        triggerScreenshot(false);
    });
    connect(m_sendModeBtn, &QAbstractButton::clicked, this, [this]() {
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        const bool isEnterMode = cfg.value(QStringLiteral("sendMode"), QStringLiteral("enter")).toString()
                                 == QStringLiteral("enter");
        ElaMenu menu(this);
        auto* enterAct = menu.addAction(QStringLiteral("Enter \u53d1\u9001 / Shift+Enter \u6362\u884c"));
        auto* ctrlAct = menu.addAction(QStringLiteral("Ctrl+Enter \u53d1\u9001"));
        enterAct->setCheckable(true);
        ctrlAct->setCheckable(true);
        enterAct->setChecked(isEnterMode);
        ctrlAct->setChecked(!isEnterMode);
        const QAction* chosen = menu.exec(m_sendModeBtn->mapToGlobal(QPoint(0, m_sendModeBtn->height())));
        if (!chosen) {
            return;
        }
        const bool newEnter = (chosen == enterAct);
        cfg.setValue(QStringLiteral("sendMode"),
                     newEnter ? QStringLiteral("enter") : QStringLiteral("ctrl+enter"));
        cfg.sync();
        syncSendModePresentation();
    });
    m_screenshotBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_screenshotBtn->setToolTip(QStringLiteral("截图，右键可设置截图前隐藏主窗口"));
    connect(m_screenshotBtn, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        const bool hideFirst = cfg.value(QStringLiteral("screenshotHideWindow"), false).toBool();
        ElaMenu menu(this);
        auto* act = menu.addAction(QStringLiteral("\u622a\u56fe\u524d\u9690\u85cf\u4e3b\u7a97\u53e3"));
        act->setCheckable(true);
        act->setChecked(hideFirst);
        menu.exec(m_screenshotBtn->mapToGlobal(pos));
        cfg.setValue(QStringLiteral("screenshotHideWindow"), act->isChecked());
        cfg.sync();
    });

    m_hostEdit->hide();
    m_portEdit->hide();
    m_connectButton->hide();

    // connectRow/inputRow 不再加入布局

    // ── 群聊右侧信息面板（初始隐藏，切入群会话时显示）────────────────
    m_contextPanel = new ContextPanel(chatPage);

    m_groupInfoPanel = new GroupInfoPanel(chatPage);
    m_groupPanelExpandedWidth = qMax(280, m_groupInfoPanel->sizeHint().width());
    m_groupInfoPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_groupInfoPanel->setMinimumWidth(0);
    m_groupInfoPanel->setMaximumWidth(0);
    m_groupInfoPanel->hide();
    connect(m_groupInfoPanel, &GroupInfoPanel::memberActivated, this,
            [this](const QString& clientId) {
                if (!clientId.trimmed().isEmpty()) {
                    emit groupMemberDirectChatRequested(clientId.trimmed());
                }
            });
    connect(m_groupInfoPanel, &GroupInfoPanel::memberAvatarHovered, this,
            [this](const QString& clientId, const QPoint& globalPos) {
                const QString normalizedClientId = clientId.trimmed();
                if (!normalizedClientId.isEmpty()) {
                    emit avatarProfileRequested(normalizedClientId, globalPos);
                }
            });
    connect(m_groupInfoPanel, &GroupInfoPanel::memberAvatarHoverLeft, this,
            [this]() {
                if (m_profileCard) {
                    m_profileCard->scheduleHide();
                }
            });
    connect(m_groupInfoPanel, &GroupInfoPanel::sharedFilesClicked, this, [this]() {
        if (m_currentGroupId.isEmpty()) return;
        const auto fsConfig = effectiveGroupFileServiceConfigForGroup(m_currentGroupId);
        if (!fsConfig.enabled || fsConfig.baseUrl.isEmpty()) {
            setStatusMessage(QStringLiteral("群文件服务不可用，请在设置中检查消息服务器地址"), 3000);
            return;
        }
        emit groupFileManagerRequested(m_currentGroupId, fsConfig);
    });
    connect(m_groupInfoPanel, &GroupInfoPanel::announcementEditRequested, this, [this]() {
        if (!m_currentGroupId.isEmpty()) {
            emit groupAnnouncementRequested(m_currentGroupId);
        }
    });
    connect(m_groupInfoPanel, &GroupInfoPanel::groupAnnouncementReminderRequested,
            this, &ConversationsPage::groupAnnouncementReminderRequested);
    connect(m_groupInfoPanel, &GroupInfoPanel::memberContextMenuRequested, this,
            [this](const QString& clientId, const QPoint& globalPos) {
                const QString normalizedClientId = clientId.trimmed();
                if (normalizedClientId.isEmpty()) {
                    return;
                }

                ElaMenu menu(this);
                QAction* directChatAction =
                    menu.addAction(QStringLiteral("\u53D1\u8D77\u79C1\u804A"));
                connect(directChatAction, &QAction::triggered, this, [this, normalizedClientId]() {
                    emit groupMemberDirectChatRequested(normalizedClientId);
                });

                menu.addSeparator();
                // 判断目标是否已经是管理员
                const bool targetIsAdmin = std::any_of(
                    m_currentGroupMemberEntries.cbegin(),
                    m_currentGroupMemberEntries.cend(),
                    [&normalizedClientId](const GroupMemberListEntry& entry) {
                        return entry.clientId == normalizedClientId && entry.isAdmin;
                    });
                // 判断目标是否为群主
                const bool targetIsOwner = std::any_of(
                    m_currentGroupMemberEntries.cbegin(),
                    m_currentGroupMemberEntries.cend(),
                    [&normalizedClientId](const GroupMemberListEntry& entry) {
                        return entry.clientId == normalizedClientId && entry.isOwner;
                    });
                // 判断当前用户是否为群主（仅群主可设置管理员）
                const bool selfIsOwner = std::any_of(
                    m_currentGroupMemberEntries.cbegin(),
                    m_currentGroupMemberEntries.cend(),
                    [](const GroupMemberListEntry& entry) {
                        return entry.isSelf && entry.isOwner;
                    });
                QAction* adminAction =
                    menu.addAction(targetIsAdmin
                        ? QStringLiteral("\u53D6\u6D88\u7BA1\u7406\u5458")
                        : QStringLiteral("\u8BBE\u4E3A\u7BA1\u7406\u5458"));
                QAction* removeAction =
                    menu.addAction(QStringLiteral("\u79FB\u51FA\u7FA4\u804A"));
                QAction* muteAction =
                    menu.addAction(QStringLiteral("\u9759\u97F3\u6210\u5458\uFF08\u5F85\u5B8C\u5584\uFF09"));

                const bool isSelf = std::any_of(
                    m_currentGroupMemberEntries.cbegin(),
                    m_currentGroupMemberEntries.cend(),
                    [&normalizedClientId](const GroupMemberListEntry& entry) {
                        return entry.clientId == normalizedClientId && entry.isSelf;
                    });
                const bool canManage = m_currentUserCanManageGroupMembers && !isSelf;

                // 管理员操作仅群主可用，且不能对群主操作
                adminAction->setEnabled(selfIsOwner && !isSelf && !targetIsOwner);
                removeAction->setEnabled(canManage && !targetIsOwner);
                muteAction->setEnabled(canManage);

                connect(adminAction, &QAction::triggered, this, [this, normalizedClientId]() {
                    emit groupMemberAdminRequested(m_currentGroupId, normalizedClientId);
                });
                connect(removeAction, &QAction::triggered, this, [this, normalizedClientId]() {
                    emit groupMemberRemoveRequested(m_currentGroupId, normalizedClientId);
                });
                connect(muteAction, &QAction::triggered, this, [this, normalizedClientId]() {
                    emit groupMemberMuteRequested(m_currentGroupId, normalizedClientId);
                });

                menu.exec(globalPos);
            });
    m_groupPanelAnimation =
        new QPropertyAnimation(m_groupInfoPanel, QByteArrayLiteral("maximumWidth"), this);
    m_groupPanelAnimation->setDuration(220);
    m_groupPanelAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_groupPanelAnimation, &QPropertyAnimation::valueChanged, this, [this]() {
        positionGroupPanelToggleButton();
        // 隐藏时：负左边距保持内容全宽 + setMask 裁剪左侧，实现从左到右的擦除效果
        if (!m_groupPanelVisibleState && m_groupInfoPanel && m_groupInfoPanel->layout()) {
            const int cur = m_groupInfoPanel->maximumWidth();
            const int offset = qMax(0, m_groupPanelExpandedWidth - cur);
            m_groupInfoPanel->layout()->setContentsMargins(-offset, 0, 0, 0);
            // 用 mask 裁剪：只显示面板可见区域内的内容（右侧部分）
            if (auto* sw = m_groupInfoPanel->findChild<QWidget*>(
                    QStringLiteral("groupInfoStackedWidget"))) {
                if (cur > 0) {
                    sw->setMask(QRegion(offset, 0, cur, m_groupInfoPanel->height()));
                } else {
                    sw->clearMask();
                }
            }
        }
    });
    connect(m_groupPanelAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (!m_groupInfoPanel) {
            return;
        }
        // 清除动画期间设置的 mask
        if (auto* sw = m_groupInfoPanel->findChild<QWidget*>(
                QStringLiteral("groupInfoStackedWidget"))) {
            sw->clearMask();
        }
        if (m_groupPanelVisibleState) {
            // 显示完成：重置边距（防止中断的 hide 残留）
            if (m_groupInfoPanel->layout()) {
                m_groupInfoPanel->layout()->setContentsMargins(0, 0, 0, 0);
            }
            if (m_chatWorkspaceSplitter) {
                m_chatWorkspaceSplitter->setHandleWidth(2);
            }
            m_groupInfoPanel->setMaximumWidth(520);
            m_groupInfoPanel->show();
            if (m_chatWorkspaceSplitter) {
                const int totalWidth =
                    qMax(m_chatWorkspaceSplitter->width(), m_groupPanelExpandedWidth + 620);
                m_chatWorkspaceSplitter->setSizes(
                    {qMax(520, totalWidth - m_groupPanelExpandedWidth), m_groupPanelExpandedWidth});
            }
            // 面板展开后消息列表宽度变化，强制 delegate 重新计算 sizeHint
            if (m_messageList) {
                QTimer::singleShot(0, m_messageList, [this]() {
                    if (m_messageList) m_messageList->doItemsLayout();
                });
            }
        } else {
            // 重置内容边距和 mask
            if (m_groupInfoPanel->layout()) {
                m_groupInfoPanel->layout()->setContentsMargins(0, 0, 0, 0);
            }
            if (m_chatWorkspaceSplitter) {
                m_chatWorkspaceSplitter->setHandleWidth(0);
            }
            m_groupInfoPanel->setMaximumWidth(0);
            m_groupInfoPanel->hide();
            if (m_chatWorkspaceSplitter) {
                m_chatWorkspaceSplitter->setSizes({1, 0});
            }
            // 面板收起后消息列表宽度变化，强制 delegate 重新计算 sizeHint
            if (m_messageList) {
                QTimer::singleShot(0, m_messageList, [this]() {
                    if (m_messageList) m_messageList->doItemsLayout();
                });
            }
        }
        syncGroupPanelToggleButton();
        positionGroupPanelToggleButton();
    });

    m_groupPanelToggleBtn = new ElaToolButton(chatPage);
    m_groupPanelToggleBtn->setObjectName(QStringLiteral("groupPanelEdgeToggleButton"));
    m_groupPanelToggleBtn->setFocusPolicy(Qt::NoFocus);
    m_groupPanelToggleBtn->setCursor(Qt::PointingHandCursor);
    m_groupPanelToggleBtn->setMouseTracking(true);
    m_groupPanelToggleBtn->installEventFilter(this);
    m_groupPanelToggleBtn->setIconSize(QSize(14, 14));
    m_groupPanelToggleBtn->setBorderRadius(10);
    m_groupPanelToggleBtn->setIsTransparent(false);
    m_groupPanelToggleBtn->hide();
    connect(m_groupPanelToggleBtn, &QToolButton::clicked, this, &ConversationsPage::toggleGroupInfoPanel);

    // 鈹€鈹€ 对话页（chatPage锛夌粍瑁?鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    auto* chatBody = new QWidget(chatPage);
    chatBody->setObjectName(QStringLiteral("chatWorkspaceBody"));
    chatBody->setAutoFillBackground(false);
    chatBody->setAttribute(Qt::WA_StyledBackground, true);
    chatBody->setMouseTracking(true);
    chatBody->installEventFilter(this);
    auto* chatBodyLayout = new QHBoxLayout(chatBody);
    chatBodyLayout->setContentsMargins(0, 0, 0, 0);
    chatBodyLayout->setSpacing(0);

    auto* chatLeft = new QWidget(chatBody);
    chatLeft->setObjectName(QStringLiteral("chatWorkspacePrimaryColumn"));
    chatLeft->setAutoFillBackground(false);
    chatLeft->setAttribute(Qt::WA_StyledBackground, true);
    chatLeft->setMouseTracking(true);
    chatLeft->installEventFilter(this);
    auto* chatLeftLayout = new QVBoxLayout(chatLeft);
    chatLeftLayout->setContentsMargins(0, 0, 0, 0);
    chatLeftLayout->setSpacing(0);
    chatLeft->setMinimumWidth(420);

    m_messageStageFrame = new ElaFrame(chatLeft);
    m_messageStageFrame->setObjectName(QStringLiteral("messageStageFrame"));
    m_messageStageFrame->setMouseTracking(true);
    m_messageStageFrame->installEventFilter(this);
    auto* messageStageLayout = new QVBoxLayout(m_messageStageFrame);
    messageStageLayout->setContentsMargins(0, 0, 0, 0);
    messageStageLayout->setSpacing(0);

    m_messageStageTopBand = new ElaFrame(m_messageStageFrame);
    m_messageStageTopBand->setObjectName(QStringLiteral("messageStageTopBand"));
    auto* messageStageTopBandLayout = new QHBoxLayout(m_messageStageTopBand);
    messageStageTopBandLayout->setContentsMargins(18, 8, 18, 8);
    messageStageTopBandLayout->setSpacing(8);

    m_messageStageModeChip =
        new ElaText(QStringLiteral("\u76F4\u8FDE\u4F1A\u8BDD"), m_messageStageTopBand);
    m_messageStageModeChip->setObjectName(QStringLiteral("messageStageModeChip"));
    m_messageStageContextChip =
        new ElaText(QStringLiteral("\u672A\u9009\u62E9\u4F1A\u8BDD"), m_messageStageTopBand);
    m_messageStageContextChip->setObjectName(QStringLiteral("messageStageContextChip"));
    m_messageStageHintLabel =
        new ElaText(QStringLiteral("\u5B9E\u65F6\u6D88\u606F"), m_messageStageTopBand);
    m_messageStageHintLabel->setObjectName(QStringLiteral("messageStageHintLabel"));
    m_messageStageHintLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    messageStageTopBandLayout->addWidget(m_messageStageModeChip);
    messageStageTopBandLayout->addWidget(m_messageStageContextChip);
    messageStageTopBandLayout->addStretch();
    messageStageTopBandLayout->addWidget(m_messageStageHintLabel);
    messageStageLayout->addWidget(m_messageStageTopBand);
    m_messageStageTopBand->hide();

    auto* messageViewport = new QWidget(m_messageStageFrame);
    messageViewport->setObjectName(QStringLiteral("messageStageViewport"));
    messageViewport->setAutoFillBackground(false);
    messageViewport->setAttribute(Qt::WA_StyledBackground, true);
    messageViewport->setMouseTracking(true);
    messageViewport->installEventFilter(this);
    auto* messageViewportLayout = new QVBoxLayout(messageViewport);
    messageViewportLayout->setContentsMargins(18, 0, 18, 0);
    messageViewportLayout->setSpacing(0);

    m_messageStageEmptyCard = new ElaFrame(messageViewport);
    m_messageStageEmptyCard->setObjectName(QStringLiteral("messageStageEmptyCard"));
    auto* messageEmptyLayout = new QVBoxLayout(m_messageStageEmptyCard);
    messageEmptyLayout->setContentsMargins(8, 10, 8, 14);
    messageEmptyLayout->setSpacing(8);

    m_messageStageEmptyTitle = new ElaText(QStringLiteral("\u4F1A\u8BDD\u5C31\u7EEA"), m_messageStageEmptyCard);
    m_messageStageEmptyTitle->setObjectName(QStringLiteral("messageStageEmptyTitle"));
    m_messageStageEmptyBody = new ElaText(
        QStringLiteral("\u4ECE\u8FD9\u91CC\u5F00\u59CB\u53D1\u9001\u6587\u5B57\u3001\u6587\u4EF6\u6216\u622A\u56FE\uFF0C\u8BA9\u8FD9\u6761\u5BF9\u8BDD\u7EBF\u4FDD\u6301\u6E05\u6670\u4E0E\u8FDE\u8D2F\u3002"),
        m_messageStageEmptyCard);
    m_messageStageEmptyBody->setObjectName(QStringLiteral("messageStageEmptyBody"));
    m_messageStageEmptyBody->setWordWrap(true);

    messageEmptyLayout->addWidget(m_messageStageEmptyTitle);
    messageEmptyLayout->addWidget(m_messageStageEmptyBody);

    messageViewportLayout->addWidget(m_messageStageEmptyCard, 0, Qt::AlignTop);

    // 置顶消息卡片容器（最多3张卡片从左到右排列）
    m_pinnedCardsContainer = new ElaFrame(messageViewport);
    m_pinnedCardsContainer->setObjectName(QStringLiteral("pinnedCardsContainer"));
    m_pinnedCardsContainer->setVisible(false);
    m_pinnedCardsContainer->setStyleSheet(QStringLiteral(
        "QFrame#pinnedCardsContainer { background: transparent; border: none; padding: 6px 8px; }")
        .arg(AppStyle::border()));
    m_pinnedCardsLayout = new QHBoxLayout(m_pinnedCardsContainer);
    m_pinnedCardsLayout->setContentsMargins(8, 4, 8, 4);
    m_pinnedCardsLayout->setSpacing(8);
    m_pinnedCardsLayout->addStretch(1);
    messageViewportLayout->addWidget(m_pinnedCardsContainer);

    messageViewportLayout->addWidget(m_messageList, 1);
    if (m_messageList->viewport()) {
        m_messageList->viewport()->setMouseTracking(true);
        m_messageList->viewport()->installEventFilter(this);
    }

    // ── 多选操作栏 ──
    m_multiSelectBar = new ElaFrame(messageViewport);
    m_multiSelectBar->setObjectName(QStringLiteral("multiSelectBar"));
    m_multiSelectBar->setFixedHeight(48);
    m_multiSelectBar->setStyleSheet(QStringLiteral(
        "QFrame#multiSelectBar {"
        "  background: %1; border: none;"
        "}"
        "QPushButton { border: none; border-radius: 6px; padding: 6px 16px;"
        "  font-size: 13px; background: %1; color: %3; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton#multiSelectForwardBtn { border: none; background: %5; color: white; }"
        "QPushButton#multiSelectForwardBtn:hover { background: %6; }"
        "QPushButton#multiSelectForwardBtn:disabled { background: %2; color: %7; }")
        .arg(AppStyle::surface(), AppStyle::border(), AppStyle::textPrimary(),
             AppStyle::hoverBg(), AppStyle::accent(), AppStyle::accentHover(),
             AppStyle::textMuted()));
    auto* multiSelectLayout = new QHBoxLayout(m_multiSelectBar);
    multiSelectLayout->setContentsMargins(12, 6, 12, 6);
    multiSelectLayout->setSpacing(10);
    auto* multiSelectCancelBtn = new ElaPushButton(QStringLiteral("\u53D6\u6D88"), m_multiSelectBar);
    m_multiSelectForwardBtn = new ElaPushButton(QStringLiteral("\u8F6C\u53D1(0)"), m_multiSelectBar);
    m_multiSelectForwardBtn->setObjectName(QStringLiteral("multiSelectForwardBtn"));
    m_multiSelectForwardBtn->setEnabled(false);
    multiSelectLayout->addStretch();
    multiSelectLayout->addWidget(multiSelectCancelBtn);
    multiSelectLayout->addWidget(m_multiSelectForwardBtn);
    messageViewportLayout->addWidget(m_multiSelectBar);
    m_multiSelectBar->hide();
    connect(multiSelectCancelBtn, &QAbstractButton::clicked, this, [this]() {
        exitMessageMultiSelectMode();
    });
    connect(m_multiSelectForwardBtn, &QAbstractButton::clicked, this, [this]() {
        if (m_multiSelectedMessageIds.isEmpty() || !m_messageList || !m_messageModel) {
            return;
        }
        // 按时间顺序收集选中消息
        struct MsgEntry { qint64 ts; QString sender; QString body; bool isFile; QString attachName; QString localPath; };
        QVector<MsgEntry> entries;
        auto* model = m_messageModel;
        for (int i = 0; i < model->rowCount(); ++i) {
            const QModelIndex idx = model->index(i, 0);
            const QString msgId = idx.data(MessageListModel::MessageIdRole).toString();
            if (!m_multiSelectedMessageIds.contains(msgId)) continue;
            const bool recalled = idx.data(MessageListModel::RecalledRole).toBool();
            if (recalled) continue;
            MsgEntry e;
            e.ts = idx.data(MessageListModel::CreatedAtRole).toLongLong();
            // 转发给第三方时不能显示"我"，需要用真实名称
            QString senderName = idx.data(MessageListModel::SenderNameRole).toString();
            if (senderName == QStringLiteral("\u6211") && !m_localDisplayName.isEmpty()) {
                senderName = m_localDisplayName;
            }
            e.sender = senderName;
            e.body = idx.data(MessageListModel::BodyRole).toString();
            e.isFile = idx.data(MessageListModel::FileMessageRole).toBool();
            e.attachName = idx.data(MessageListModel::AttachmentNameRole).toString();
            e.localPath = idx.data(MessageListModel::LocalFilePathRole).toString();
            entries.append(e);
        }
        std::sort(entries.begin(), entries.end(),
                  [](const MsgEntry& a, const MsgEntry& b) { return a.ts < b.ts; });
        if (entries.isEmpty()) return;

        // 构建 forward_package JSON
        static const QStringList imgExts = {
            QStringLiteral(".png"), QStringLiteral(".jpg"),
            QStringLiteral(".jpeg"), QStringLiteral(".gif"),
            QStringLiteral(".bmp"), QStringLiteral(".webp")
        };
        QJsonArray messagesArr;
        for (const MsgEntry& e : entries) {
            QJsonObject msgObj;
            msgObj[QStringLiteral("ts")] = e.ts;
            msgObj[QStringLiteral("sender")] = e.sender;
            if (e.isFile) {
                bool isImage = false;
                for (const QString& ext : imgExts) {
                    if (e.attachName.endsWith(ext, Qt::CaseInsensitive)) { isImage = true; break; }
                }
                msgObj[QStringLiteral("type")] = isImage ? QStringLiteral("image") : QStringLiteral("file");
                msgObj[QStringLiteral("fileName")] = e.attachName;
                // 图片 ≤200KB 时嵌入 base64
                if (isImage && !e.localPath.isEmpty() && QFileInfo::exists(e.localPath)) {
                    QFileInfo fi(e.localPath);
                    if (fi.size() <= 200 * 1024) {
                        QFile f(e.localPath);
                        if (f.open(QIODevice::ReadOnly)) {
                            msgObj[QStringLiteral("imageBase64")] = QString::fromLatin1(f.readAll().toBase64());
                            f.close();
                        }
                    }
                }
            } else {
                msgObj[QStringLiteral("type")] = QStringLiteral("text");
                QString plain = e.body;
                if (Qt::mightBeRichText(plain)) {
                    QTextDocument doc;
                    doc.setHtml(plain);
                    plain = doc.toPlainText();
                }
                msgObj[QStringLiteral("text")] = plain;
            }
            messagesArr.append(msgObj);
        }
        QJsonObject package;
        package[QStringLiteral("title")] = QStringLiteral("\u804a\u5929\u8bb0\u5f55");
        package[QStringLiteral("count")] = entries.size();
        package[QStringLiteral("messages")] = messagesArr;

        exitMessageMultiSelectMode();
        emit mergedForwardPackageRequested(package);
    });

    messageStageLayout->addWidget(messageViewport, 1);

    // 浮动 toast 通知（覆盖在消息区域顶部居中）
    m_chatToastLabel = new ElaText(m_messageStageFrame);
    m_chatToastLabel->setObjectName(QStringLiteral("chatToastLabel"));
    m_chatToastLabel->setAlignment(Qt::AlignCenter);
    m_chatToastLabel->setWordWrap(true);
    m_chatToastLabel->setStyleSheet(QStringLiteral(
        "QLabel#chatToastLabel {"
        "  background: rgba(0,0,0,0.72);"
        "  color: #FFFFFF;"
        "  border-radius: 16px;"
        "  padding: 8px 20px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "}"));
    m_chatToastLabel->setMinimumHeight(36);
    m_chatToastLabel->hide();

    m_chatToastTimer = new QTimer(this);
    m_chatToastTimer->setSingleShot(true);
    connect(m_chatToastTimer, &QTimer::timeout, this, [this]() {
        if (m_chatToastLabel) m_chatToastLabel->hide();
    });

    chatLeftLayout->addWidget(m_messageStageFrame, 1);
    m_chatComposerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_chatComposerWidget->setMaximumHeight(168);
    chatLeftLayout->addWidget(m_chatComposerWidget);

    m_chatWorkspaceSplitter = new ElaSplitter(Qt::Horizontal, chatBody);
    m_chatWorkspaceSplitter->setObjectName(QStringLiteral("chatWorkspaceSplitter"));
    m_chatWorkspaceSplitter->setChildrenCollapsible(false);
    m_chatWorkspaceSplitter->setHandleWidth(0);
    m_chatWorkspaceSplitter->addWidget(chatLeft);
    m_chatWorkspaceSplitter->addWidget(m_groupInfoPanel);
    m_chatWorkspaceSplitter->setStretchFactor(0, 1);
    m_chatWorkspaceSplitter->setStretchFactor(1, 0);
    m_chatWorkspaceSplitter->setSizes({900, 0});
    connect(m_chatWorkspaceSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        if (!m_groupInfoPanel || !m_groupPanelVisibleState) {
            return;
        }
        const int currentWidth = m_groupInfoPanel->width();
        if (currentWidth > 0) {
            m_groupPanelExpandedWidth = qBound(240, currentWidth, 420);
        }
        positionGroupPanelToggleButton();
    });
    chatBodyLayout->addWidget(m_chatWorkspaceSplitter, 1);

    chatPageLayout->addWidget(m_chatHeaderWidget);
    chatPageLayout->addWidget(chatBody, 1);
    m_contentStack->addWidget(chatPage); // index 1

    // 通知、联系人页、独立页、AI知识面板已迁移到独立 Page 中

    // ── 根布局：导航栏 | 侧边栏 | 内容区 ─────────────────────
// 导航徽章 — 转发到 MainWindow
    auto* workspaceShellFrame = new QFrame(this);
    workspaceShellFrame->setObjectName(QStringLiteral("workspaceShellFrame"));
    workspaceShellFrame->setFrameShape(QFrame::NoFrame);
    workspaceShellFrame->setAttribute(Qt::WA_StyledBackground, true);
    {
        const auto mode = AppStyle::currentThemeMode();
        const QString shellBorder = AppStyle::isDarkTheme(mode)
            ? QStringLiteral("#4B5563")
            : QStringLiteral("#D0D5DD");
        workspaceShellFrame->setStyleSheet(QStringLiteral(
            "QFrame#workspaceShellFrame {"
            "  background: transparent;"
            "  border: 1px solid %1;"
            "  border-radius: 16px;"
            "}").arg(shellBorder));
    }

    auto* workspaceShellLayout = new QHBoxLayout(workspaceShellFrame);
    workspaceShellLayout->setContentsMargins(1, 1, 1, 1);
    workspaceShellLayout->setSpacing(0);
    workspaceShellLayout->addWidget(m_sideStack, 0);
    workspaceShellLayout->addWidget(m_contentStack, 1);

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 20, 20);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(workspaceShellFrame, 1);

    // 工作区嵌入已由根布局完成（见构造函数末尾）
    // UpdateBar 由 MainWindow 管理

    auto* sendShortcut = new QAction(this);
    sendShortcut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    addAction(sendShortcut);

    const auto submitMessage = [this]() { submitCurrentComposer(); };
    connect(m_sendButton, &QAbstractButton::clicked, this, submitMessage);
    connect(sendShortcut, &QAction::triggered, this, submitMessage);
    connect(m_sendFileButton, &QAbstractButton::clicked, this, [this]() {
        const QString filePath = QFileDialog::getOpenFileName(this,
                                                              QStringLiteral("\u9009\u62E9\u8981\u53D1\u9001\u7684\u6587\u4EF6"));
        if (!filePath.isEmpty()) {
            emit fileSendRequested(filePath);
        }
    });
    connect(m_connectButton, &QAbstractButton::clicked, this, [this]() {
        bool ok = false;
        const quint16 port = m_portEdit->text().trimmed().toUShort(&ok);
        if (!ok || m_hostEdit->text().trimmed().isEmpty()) {
            return;
        }

        emit connectRequested(m_hostEdit->text().trimmed(), port);
    });
    connect(m_chatComposerWidget, &ChatComposerWidget::nudgeTriggered, this, [this]() {
        emit nudgeRequested();
    });
    connect(m_chatComposerWidget, &ChatComposerWidget::devOpsTriggered, this, [this]() {
        emit devOpsInsertRequested();
    });
    connect(m_chatComposerWidget, &ChatComposerWidget::stickerSelected, this,
            [this](const QString& packId, const QString& stickerId) {
        const QByteArray gifData = StickerManager::instance().readStickerData(packId, stickerId);
        if (gifData.isEmpty()) {
            setStatusMessage(QStringLiteral("\u8868\u60C5\u6587\u4EF6\u8BFB\u53D6\u5931\u8D25"), 2500);
            return;
        }
        emit stickerSendRequested(packId, stickerId, gifData);
    });
    connect(m_chatComposerWidget, &ChatComposerWidget::recoveryContextChanged,
            this, &ConversationsPage::composerRecoveryContextChanged);
    connect(m_chatComposerWidget, &ChatComposerWidget::recoveryContextCommitted,
            this, &ConversationsPage::composerRecoveryContextCommitted);
    const auto applyTransferFilter = [this](TransferListFilter filter, ElaToolButton* activeButton) {
        if (auto* model = qobject_cast<TransferListModel*>(m_transferList->model())) {
            model->setFilter(filter);
        }
        for (ElaToolButton* button : {m_transferFilterAllBtn,
                                    m_transferFilterOutgoingBtn,
                                    m_transferFilterIncomingBtn,
                                    m_transferFilterActiveBtn,
                                    m_transferFilterFailedBtn,
                                    m_transferFilterCompletedBtn}) {
            if (button) {
                button->setChecked(button == activeButton);
            }
        }
    };
    connect(m_transferFilterAllBtn, &QAbstractButton::clicked, this,
            [applyTransferFilter, this]() { applyTransferFilter(TransferListFilter::All, m_transferFilterAllBtn); });
    connect(m_transferFilterOutgoingBtn, &QAbstractButton::clicked, this,
            [applyTransferFilter, this]() { applyTransferFilter(TransferListFilter::OutgoingOnly, m_transferFilterOutgoingBtn); });
    connect(m_transferFilterIncomingBtn, &QAbstractButton::clicked, this,
            [applyTransferFilter, this]() { applyTransferFilter(TransferListFilter::IncomingOnly, m_transferFilterIncomingBtn); });
    connect(m_transferFilterActiveBtn, &QAbstractButton::clicked, this,
            [applyTransferFilter, this]() { applyTransferFilter(TransferListFilter::ActiveOnly, m_transferFilterActiveBtn); });
    connect(m_transferFilterFailedBtn, &QAbstractButton::clicked, this,
            [applyTransferFilter, this]() { applyTransferFilter(TransferListFilter::FailedOnly, m_transferFilterFailedBtn); });
    connect(m_transferFilterCompletedBtn, &QAbstractButton::clicked, this,
            [applyTransferFilter, this]() { applyTransferFilter(TransferListFilter::CompletedOnly, m_transferFilterCompletedBtn); });
    // Conversation click handled by ConversationCardDelegate::clicked → onConversationItemClicked
    connect(m_transferList, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        const QString taskId = index.data(TransferListModel::TaskIdRole).toString();
        const bool openable = index.data(TransferListModel::OpenableRole).toBool();
        if (taskId.isEmpty() || !openable) {
            return;
        }

        emit openTransferFileRequested(taskId);
    });
    connect(m_transferList, &QListView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex index = m_transferList->indexAt(pos);
        const QString taskId = index.data(TransferListModel::TaskIdRole).toString();
        const bool hasTask = index.isValid() && !taskId.isEmpty();
        const bool openable = index.data(TransferListModel::OpenableRole).toBool();
        const bool revealable = index.data(TransferListModel::RevealableRole).toBool();
        const bool retryable = index.data(TransferListModel::RetryableRole).toBool();
        const auto direction =
            static_cast<FileTransferDirection>(index.data(TransferListModel::DirectionRole).toInt());
        const auto state =
            static_cast<FileTransferState>(index.data(TransferListModel::StateRole).toInt());
        const bool hasTransferItems = m_transferList->model() && m_transferList->model()->rowCount() > 0;
        if (!hasTask && !hasTransferItems) {
            return;
        }

        ElaMenu menu(this);
        menu.setStyleSheet(transferMenuStylesheet());
        QAction* retryAction = nullptr;
        QAction* cancelAction = nullptr;
        QAction* cancelSameNameAction = nullptr;
        QAction* openAction = nullptr;
        QAction* revealAction = nullptr;
        QAction* deleteAction = nullptr;
        QAction* clearPendingAction = nullptr;
        QAction* clearCompletedAction = nullptr;
        QAction* clearFailedAction = nullptr;

        const bool isCancelable = hasTask
            && state != FileTransferState::Completed
            && state != FileTransferState::Failed
            && state != FileTransferState::Canceled;

        if (hasTask && retryable) {
            retryAction = menu.addAction(transferRetryActionText(direction, state));
        }
        if (hasTask && isCancelable) {
            cancelAction = menu.addAction(QStringLiteral("\u53D6\u6D88\u4F20\u8F93"));
        }
        if (hasTask && openable) {
            openAction = menu.addAction(QStringLiteral("\u6253\u5F00\u6587\u4EF6"));
        }
        if (hasTask && revealable) {
            revealAction = menu.addAction(QStringLiteral("\u6253\u5F00\u6240\u5728\u76EE\u5F55"));
        }
        if (hasTask) {
            if (retryAction || cancelAction || openAction || revealAction) {
                menu.addSeparator();
            }
            deleteAction = menu.addAction(QStringLiteral("\u5220\u9664\u8FD9\u6761\u4F20\u8F93\u8BB0\u5F55"));
            const QString fileName = index.data(Qt::DisplayRole).toString().trimmed();
            if (!fileName.isEmpty()) {
                cancelSameNameAction = menu.addAction(
                    QStringLiteral("\u53D6\u6D88\u540C\u540D\u6587\u4EF6\u7684\u6240\u6709\u4F20\u8F93"));
            }
        }
        if (hasTransferItems) {
            if (deleteAction || retryAction || cancelAction || openAction || revealAction) {
                menu.addSeparator();
            }
            clearPendingAction = menu.addAction(QStringLiteral("批量删除准备接收/待发送"));
            clearCompletedAction = menu.addAction(QStringLiteral("\u6279\u91CF\u6E05\u7406\u5DF2\u5B8C\u6210"));
            clearFailedAction = menu.addAction(QStringLiteral("\u6279\u91CF\u6E05\u7406\u5931\u8D25/\u4E2D\u65AD"));
        }

        QAction* selectedAction = menu.exec(m_transferList->viewport()->mapToGlobal(pos));
        if (selectedAction == retryAction) {
            emit retryTransferRequested(taskId);
            return;
        }
        if (selectedAction == cancelAction) {
            emit cancelTransferRequested(taskId);
            return;
        }
        if (selectedAction == cancelSameNameAction) {
            const QString fileName = index.data(Qt::DisplayRole).toString().trimmed();
            if (!fileName.isEmpty()) {
                emit cancelSameNameTransfersRequested(fileName);
            }
            return;
        }
        if (selectedAction == openAction) {
            emit openTransferFileRequested(taskId);
            return;
        }
        if (selectedAction == revealAction) {
            emit revealTransferFileRequested(taskId);
            return;
        }
        if (selectedAction == deleteAction) {
            emit deleteTransferRequested(taskId);
            return;
        }
        if (selectedAction == clearPendingAction) {
            emit clearPendingTransfersRequested();
            return;
        }
        if (selectedAction == clearCompletedAction) {
            emit clearCompletedTransfersRequested();
            return;
        }
        if (selectedAction == clearFailedAction) {
            emit clearFailedTransfersRequested();
        }
    });
    const auto openableUrlFromMessageIndex = [](const QModelIndex& index) -> QUrl {
        if (!index.isValid()) {
            return {};
        }

        const bool isResourceReference =
            index.data(MessageListModel::ResourceReferenceRole).toBool();
        if (isResourceReference) {
            const QUrl resourceUrl =
                firstOpenableUrlFromResourcePayload(index.data(MessageListModel::PayloadJsonRole).toByteArray());
            if (resourceUrl.isValid()) {
                return resourceUrl;
            }
        }

        const QString body = index.data(MessageListModel::BodyRole).toString();
        return firstOpenableUrlFromMessageBody(body);
    };
    const auto openUrlFromMessageIndex = [this, openableUrlFromMessageIndex](const QModelIndex& index) -> bool {
        const QUrl url = openableUrlFromMessageIndex(index);
        if (!url.isValid()) {
            return false;
        }

        emit messageUrlOpenRequested(url.toString());
        if (QGuiApplication::platformName().compare(QStringLiteral("offscreen"),
                                                    Qt::CaseInsensitive) == 0) {
            return true;
        }
        if (!QDesktopServices::openUrl(url)) {
            setStatusMessage(QStringLiteral("链接打开失败"), 2500);
            return false;
        }
        setStatusMessage(QStringLiteral("已打开链接：%1").arg(url.toString()), 1800);
        return true;
    };
    connect(m_messageList, &QAbstractItemView::doubleClicked, this, openUrlFromMessageIndex);
    connect(m_messageList, &QAbstractItemView::clicked, this, [openUrlFromMessageIndex](const QModelIndex& index) {
        if (!index.data(MessageListModel::ResourceReferenceRole).toBool()) {
            return;
        }
        openUrlFromMessageIndex(index);
    });

    // MessageBubbleWidget handles normal message interactions in refreshMessageListWidgets().
    // Keep list-level URL fallback so resource-reference rows stay usable in tests and legacy views.

    m_messageList->setAcceptDrops(true);
    m_messageList->viewport()->setAcceptDrops(true);
    m_inputEdit->setAcceptDrops(true);
    m_inputEdit->viewport()->setAcceptDrops(true);
    m_messageList->installEventFilter(this);
    m_messageList->viewport()->installEventFilter(this);
    m_inputEdit->installEventFilter(this);
    m_inputEdit->viewport()->installEventFilter(this);
    if (QScrollBar* scrollBar = m_messageList->verticalScrollBar()) {
        scrollBar->setSingleStep(36);
        scrollBar->setPageStep(120);
        connect(scrollBar, &QScrollBar::valueChanged, this, [this, scrollBar](int value) {
            if (m_programmaticMessageScroll) {
                return;
            }
            const bool wasNearBottom = m_followLatestMessages;
            m_followLatestMessages = (scrollBar->maximum() - value) <= 24;
            if (m_followLatestMessages && !wasNearBottom) {
                emit viewportReachedBottom();
            }
            refreshVisibleMessageBubbleThemes();
            if (value <= 24 && m_messageModel && m_messageModel->hasMoreMessagesBefore()) {
                const QString beforeMessageId = m_messageModel->firstMessageId();
                const QString requestKey = m_selectedConversationId + QLatin1Char('|') + beforeMessageId;
                if (!beforeMessageId.isEmpty()
                    && !m_selectedConversationId.isEmpty()
                    && requestKey != m_lastOlderMessagesRequestKey) {
                    m_lastOlderMessagesRequestKey = requestKey;
                    emit olderMessagesRequested(m_selectedConversationId, beforeMessageId);
                }
            }
        });
    }

    m_mentionPopup = new ElaListWidget(this);
    m_mentionPopup->setObjectName(QStringLiteral("mentionPopup"));
    m_mentionPopup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    m_mentionPopup->setAttribute(Qt::WA_ShowWithoutActivating);
    m_mentionPopup->setFocusPolicy(Qt::NoFocus);
    m_mentionPopup->setFixedWidth(228);
    m_mentionPopup->setMaximumHeight(240);
    m_mentionPopup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_mentionPopup->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_mentionPopup->installEventFilter(this);
    m_mentionPopup->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background:%1; border:none; border-radius:10px;"
        "  font-size:13px; color:%3; outline:none;"
        "}"
        "QListWidget::item { padding:8px 14px; min-height:20px; }"
        "QListWidget::item:selected { background:%4; color:%5; font-weight:700; }")
        .arg(AppStyle::surface(),
             AppStyle::border(),
             AppStyle::textPrimary(),
             AppStyle::selectedBg(),
             AppStyle::accent()));
    m_mentionPopup->hide();
    connect(m_mentionPopup, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item || !replaceCurrentMentionToken(item->text())) {
            return;
        }
        m_mentionPopup->hide();
        m_inputEdit->setFocus();
    });
    connect(m_inputEdit, &QTextEdit::textChanged, this, [this]() {
        if (!m_currentGroupMembers.isEmpty()) {
            refreshMentionPopup();
        } else if (m_mentionPopup && m_mentionPopup->isVisible()) {
            m_mentionPopup->hide();
        }
    });
    connect(m_inputEdit, &QTextEdit::cursorPositionChanged, this, [this]() {
        if (!m_currentGroupMembers.isEmpty() && m_mentionPopup && m_mentionPopup->isVisible()) {
            refreshMentionPopup();
        }
    });

    // 窗口级初始化（resize/setTitle/applyAppearance/styleHints connect）已由 MainWindow 处理
    refreshTheme();
    syncSendModePresentation();
    updateMessageStageContext(false, QString(), QStringLiteral("\u5DF2\u8FDE\u63A5"));
    syncConversationSidebarMode();
    syncMessageStageEmptyState();
    syncComposerDraftState();
}

// ======================================================================
// 业务方法 — 从 MainWindow.cpp 迁移
// ======================================================================

void ConversationsPage::setConversationModel(ConversationListModel* model) {
    m_conversationModel = model;

    // Delegate 架构：QListView 直接绑定 model，无需手动 widget 管理
    m_conversationList->setModel(model);

    const auto syncConversationState = [this]() {
        if (!m_conversationEmptyLabel || !m_conversationModel) return;
        const bool hasItems = m_conversationModel->rowCount() > 0;
        m_conversationEmptyLabel->setVisible(!hasItems);
        m_conversationList->setVisible(hasItems);
        syncConversationWorkspaceStatus();
    };

    if (model) {
        connect(model, &QAbstractItemModel::modelReset, this, syncConversationState);
        connect(model, &QAbstractItemModel::modelReset, this, [this]() {
            if (!m_selectedConversationId.trimmed().isEmpty()) {
                setSelectedConversationId(m_selectedConversationId);
            }
        }, Qt::QueuedConnection);
        connect(model, &QAbstractItemModel::rowsInserted, this, syncConversationState);
        connect(model, &QAbstractItemModel::rowsRemoved, this, syncConversationState);
        connect(model, &QAbstractItemModel::layoutChanged, this, syncConversationState);
        connect(model, &QAbstractItemModel::dataChanged, this, syncConversationState);
    }
    if (m_conversationEmptyLabel && model) {
        const bool hasItems = model->rowCount() > 0;
        m_conversationEmptyLabel->setVisible(!hasItems);
        m_conversationList->setVisible(hasItems);
        syncConversationWorkspaceStatus();
    }
}

void ConversationsPage::setContactModel(ContactListModel* model)
{
    m_contactModel = model;

    const auto refreshWelcomeContacts = [this]() {
        syncWelcomeContactMetric();
    };
    if (model) {
        connect(model, &QAbstractItemModel::modelReset, this, refreshWelcomeContacts);
        connect(model, &QAbstractItemModel::rowsInserted, this, refreshWelcomeContacts);
        connect(model, &QAbstractItemModel::rowsRemoved, this, refreshWelcomeContacts);
        connect(model, &QAbstractItemModel::layoutChanged, this, refreshWelcomeContacts);
        connect(model, &QAbstractItemModel::dataChanged, this, refreshWelcomeContacts);
    }
    syncWelcomeContactMetric();
}

void ConversationsPage::refreshConversationListWidgets()
{
    // Delegate 架构：QListView 自动从 model 读取数据并调用 delegate::paint()
    // 无需手动管理 widget。仅触发 viewport 刷新。
    if (m_conversationList && m_conversationList->viewport())
        m_conversationList->viewport()->update();
}

void ConversationsPage::insertConversationListRow(int /*row*/)
{
    // Delegate 架构：model rowsInserted 自动由 QListView 处理
}

void ConversationsPage::removeConversationListRows(int /*first*/, int /*last*/)
{
    // Delegate 架构：model rowsRemoved 自动由 QListView 处理
}

void ConversationsPage::updateConversationListRow(int /*row*/)
{
    // Delegate 架构：model dataChanged 自动由 QListView 处理
}

void ConversationsPage::scheduleConversationListRebuild()
{
    if (m_convListRebuildTimer) {
        m_convListRebuildTimer->start(); // restart the 16ms debounce
    } else {
        refreshConversationListWidgets();
    }
}

// ── Widget 回收池 (#1) ──
// NOTE: Qt 6 的 removeItemWidget 内部调用 deleteLater()，所以无法安全回收。
// 每次创建新 widget，由 Qt 负责生命周期管理。

ConversationItemWidget* ConversationsPage::acquireConversationWidget()
{
    auto* widget = new ConversationItemWidget(m_conversationList);
    connect(widget, &ConversationItemWidget::clicked,
            this, &ConversationsPage::onConversationItemClicked);
    connect(widget, &ConversationItemWidget::contextMenuRequested,
            this, &ConversationsPage::onConversationItemContextMenu);
    connect(widget, &ConversationItemWidget::avatarHovered,
            this, &ConversationsPage::conversationAvatarHovered);
    connect(widget, &ConversationItemWidget::avatarHoverLeft,
            this, [this]() {
                if (m_profileCard) {
                    m_profileCard->scheduleHide();
                }
            });
    return widget;
}

void ConversationsPage::releaseConversationWidget(ConversationItemWidget* widget)
{
    // Qt 6: removeItemWidget 已调度 deleteLater，无需手动管理
    Q_UNUSED(widget);
}

MessageBubbleWidget* ConversationsPage::acquireBubbleWidget()
{
    // Delegate 架构：不再需要创建 widget，保留方法签名以兼容
    return nullptr;
}

void ConversationsPage::releaseBubbleWidget(MessageBubbleWidget* widget)
{
    Q_UNUSED(widget);
}

void ConversationsPage::onConversationItemClicked(const QString& conversationId)
{
    if (conversationId.isEmpty()) return;
    setSelectedConversationId(conversationId);
    emit conversationSelected(conversationId);
}

void ConversationsPage::onConversationItemContextMenu(const QString& convId,
                                                       const QPoint& globalPos)
{
    if (convId.isEmpty()) return;

    // Resolve the current state from the conversation model.
    if (!m_conversationModel) return;
    QModelIndex idx;
    for (int row = 0; row < m_conversationModel->rowCount(); ++row) {
        const QModelIndex candidate = m_conversationModel->index(row, 0);
        if (candidate.data(ConversationListModel::ConversationIdRole).toString() == convId) {
            idx = candidate;
            break;
        }
    }
    if (!idx.isValid()) return;

    const bool pinned  = idx.data(ConversationListModel::IsPinnedRole).toBool();
    const bool starred = idx.data(ConversationListModel::IsStarredRole).toBool();
    const bool muted   = idx.data(ConversationListModel::IsMutedRole).toBool();
    const bool done    = idx.data(ConversationListModel::IsDoneRole).toBool();
    const bool unread  = idx.data(ConversationListModel::HasUnreadRole).toBool();
    const bool isGroup = isGroupConversationId(convId);

    ElaMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background:%1; border:none; border-radius:8px; padding:4px 0; }"
        "QMenu::item { padding:7px 20px; font-size:13px; color:%3; }"
        "QMenu::item:selected { background:%4; color:%5; }"
        "QMenu::separator { height:1px; background:%2; margin:4px 0; }")
        .arg(AppStyle::surface(), AppStyle::border(), AppStyle::textPrimary(),
             AppStyle::hoverBg(), AppStyle::accent()));

    auto* pinAction    = menu.addAction(pinned  ? QStringLiteral("\u53D6\u6D88\u7F6E\u9876")
                                                : QStringLiteral("\u7F6E\u9876"));
    auto* unreadAction = menu.addAction(unread  ? QStringLiteral("\u6807\u4E3A\u5DF2\u8BFB")
                                                : QStringLiteral("\u6807\u4E3A\u672A\u8BFB"));
    auto* starAction   = menu.addAction(starred ? QStringLiteral("\u53D6\u6D88\u661F\u6807")
                                                : QStringLiteral("\u661F\u6807"));
    menu.addSeparator();
    auto* muteAction   = menu.addAction(muted   ? QStringLiteral("\u53D6\u6D88\u514D\u6253\u6270")
                                                : QStringLiteral("\u6D88\u606F\u514D\u6253\u6270"));
    menu.addSeparator();
    auto* doneAction = menu.addAction(conversationListDoneActionText(
        isGroup, done, m_groupWorkspaceMode));

    const QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;

    if (chosen == pinAction) {
        emit conversationPinToggled(convId, !pinned);
    } else if (chosen == unreadAction) {
        emit conversationMarkUnread(convId, !unread);
    } else if (chosen == starAction) {
        emit conversationStarToggled(convId, !starred);
    } else if (chosen == muteAction) {
        emit conversationMuteToggled(convId, !muted);
    } else if (chosen == doneAction) {
        emit conversationMarkDone(convId);
    }
}

// ==========================================================================
// refreshMessageListWidgets — delegate-based message list
// ==========================================================================

void ConversationsPage::insertMessageListRow(int /*row*/)
{
    // Delegate 架构：model rowsInserted 自动由 QListView 处理
}

void ConversationsPage::updateMessageListRow(int /*row*/)
{
    // Delegate 架构：model dataChanged 自动由 QListView 处理
}

void ConversationsPage::removeMessageListRows(int /*first*/, int /*last*/)
{
    // Delegate 架构：model rowsRemoved 自动由 QListView 处理
}

QString ConversationsPage::topVisibleMessageId() const
{
    if (!m_messageList || !m_messageModel) return {};
    const QModelIndex topIdx = m_messageList->indexAt(QPoint(0, 0));
    if (topIdx.isValid())
        return topIdx.data(MessageListModel::MessageIdRole).toString();
    return {};
}

void ConversationsPage::restoreTopVisibleMessage(const QString& messageId)
{
    if (messageId.isEmpty() || !m_messageList || !m_messageModel) return;
    for (int row = 0; row < m_messageModel->rowCount(); ++row) {
        if (m_messageModel->index(row, 0).data(MessageListModel::MessageIdRole).toString()
            == messageId) {
            m_messageList->scrollTo(m_messageModel->index(row, 0),
                                    QAbstractItemView::PositionAtTop);
            return;
        }
    }
}

void ConversationsPage::syncMessageListAfterLayoutChanged()
{
    if (!m_messageModel || !m_messageList) return;
    // Delegate 架构：layout change 由 QListView 自动处理，仅需刷新 viewport
    m_messageList->viewport()->update();
    syncMessageStageEmptyState();
}

void ConversationsPage::processPendingInsertBatch()
{
    // Delegate 架构：不再需要分批插入，model 变更即时反映
    m_pendingInsert.reset();
}

void ConversationsPage::refreshVisibleMessageBubbleThemes()
{
    // Delegate 架构：主题切换只需刷新 viewport，paint() 实时读取 AppStyle 颜色
    if (m_messageList && m_messageList->viewport())
        m_messageList->viewport()->update();
}

// ── Viewport-only 实例化 (#4) ──

void ConversationsPage::recycleOffscreenWidgets()
{
    // Delegate 架构：无需回收 widget
}

void ConversationsPage::ensureViewportWidgets()
{
    // Delegate 架构：无需确保 viewport widget
}

void ConversationsPage::refreshMessageListWidgets()
{
    if (!m_messageModel || !m_messageList) return;

    // Cancel any in-flight deferred batch
    m_pendingInsert.reset();

    // Delegate 架构：model 已经绑定到 view，只需刷新
    m_messageListFirstModelRow = 0;
    if (m_messageList->viewport())
        m_messageList->viewport()->update();

    if (m_followLatestMessages) {
        scheduleMessageViewportToBottom();
    }
}

// ==========================================================================
// onMessageBubbleContextMenu — right-click context menu on message widget
// ==========================================================================

void ConversationsPage::onMessageBubbleContextMenu(const QString& messageId, const QPoint& globalPos)
{
    if (messageId.isEmpty() || !m_messageModel) return;

    // Find model index by messageId
    QModelIndex index;
    for (int i = 0; i < m_messageModel->rowCount(); ++i) {
        const QModelIndex idx = m_messageModel->index(i, 0);
        if (idx.data(MessageListModel::MessageIdRole).toString() == messageId) {
            index = idx;
            break;
        }
    }
    if (!index.isValid()) return;

    const bool outgoing = index.data(MessageListModel::OutgoingRole).toBool();
    const bool isFileMessage = index.data(MessageListModel::FileMessageRole).toBool();
    const QString localFilePath = index.data(MessageListModel::LocalFilePathRole).toString();
    const auto deliveryState =
        static_cast<MessageDeliveryState>(index.data(MessageListModel::DeliveryStateRole).toInt());
    const bool isRecalled = index.data(MessageListModel::RecalledRole).toBool();
    const qint64 createdAtMs = index.data(MessageListModel::CreatedAtRole).toLongLong();
    const QString messageType = index.data(MessageListModel::MessageTypeRole).toString();
    const bool isFile = index.data(MessageListModel::FileMessageRole).toBool();
    const bool isResourceRef = index.data(MessageListModel::ResourceReferenceRole).toBool();

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool withinWindow = (nowMs - createdAtMs) <= 120000LL;
    const bool canEdit = outgoing && withinWindow && !isRecalled && !isFile && !isResourceRef
                         && (messageType == QStringLiteral("text") || messageType.isEmpty());
    const bool canRecall = outgoing && withinWindow && !isRecalled;

    ElaMenu menu(this);
    QAction* copyAction = nullptr;
    QAction* retryAction = nullptr;
    QAction* openAction = nullptr;
    QAction* revealAction = nullptr;
    QAction* recallAction = nullptr;
    QAction* editAction = nullptr;
    QAction* replyAction = nullptr;
    QAction* forwardAction = nullptr;
    QAction* reminderAction = nullptr;
    QAction* pinAction = nullptr;
    QAction* multiSelectAction = nullptr;
    QAction* saveStickerAction = nullptr;

    const QString copyText = messageCopyTextForIndex(index);
    if (!copyText.trimmed().isEmpty())
        copyAction = menu.addAction(QStringLiteral("\u590D\u5236\u6D88\u606F"));

    if (outgoing && deliveryState == MessageDeliveryState::Pending && !isFileMessage)
        retryAction = menu.addAction(QStringLiteral("\u91CD\u8BD5\u53D1\u9001"));

    if (isFileMessage && !localFilePath.trimmed().isEmpty()) {
        openAction = menu.addAction(QStringLiteral("\u6253\u5F00\u6587\u4EF6"));
        revealAction = menu.addAction(QStringLiteral("\u6253\u5F00\u6240\u5728\u76EE\u5F55"));
    }

    if (canRecall) recallAction = menu.addAction(QStringLiteral("\u64A4\u56DE\u6D88\u606F"));
    if (canEdit)   editAction = menu.addAction(QStringLiteral("\u7F16\u8F91\u6D88\u606F"));

    if (!isRecalled && messageType != QStringLiteral("system"))
        replyAction = menu.addAction(QStringLiteral("\u56DE\u590D"));

    if (!isRecalled && messageType != QStringLiteral("system"))
        forwardAction = menu.addAction(QStringLiteral("\u8F6C\u53D1"));

    if (!isRecalled && messageType != QStringLiteral("system"))
        reminderAction = menu.addAction(QStringLiteral("稍后提醒我回复"));

    // 表情回应子菜单
    if (!isRecalled && messageType != QStringLiteral("system")) {
        auto* reactionMenu = menu.addMenu(QStringLiteral("\U0001F44D \u8868\u60C5\u56DE\u5E94"));
        static const QStringList kEmojis = {
            QStringLiteral("\xF0\x9F\x91\x8D"), QStringLiteral("\xE2\x9D\xA4\xEF\xB8\x8F"),
            QStringLiteral("\xF0\x9F\x98\x82"), QStringLiteral("\xF0\x9F\x98\xAE"),
            QStringLiteral("\xF0\x9F\x8E\x89"), QStringLiteral("\xF0\x9F\x91\x80"),
            QStringLiteral("\xF0\x9F\x91\x8C")
        };
        for (const QString& emoji : kEmojis) {
            auto* act = reactionMenu->addAction(emoji);
            connect(act, &QAction::triggered, this, [this, messageId, emoji]() {
                emit reactionRequested(messageId, emoji);
            });
        }
    }

    if (isGroupConversationId(m_activeComposerContextId) && !isRecalled && messageType != QStringLiteral("system"))
        pinAction = menu.addAction(QStringLiteral("\U0001F4CC \u7F6E\u9876"));

    if (!isRecalled && messageType != QStringLiteral("system")) {
        menu.addSeparator();
        multiSelectAction = menu.addAction(QStringLiteral("\u591A\u9009"));
    }

    if (messageType == QStringLiteral("sticker"))
        saveStickerAction = menu.addAction(QStringLiteral("\u2B50 \u6DFB\u52A0\u5230\u6211\u7684\u8868\u60C5"));

    if (menu.actions().isEmpty()) return;

    QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;

    if (chosen == multiSelectAction) {
        enterMessageMultiSelectMode();
        toggleMessageMultiSelect(messageId);
    } else if (chosen == copyAction) {
        // Delegate 架构：检查 delegate 文本选择
        if (m_messageBubbleDelegate && m_messageBubbleDelegate->hasSelection()) {
            if (auto* cb = QGuiApplication::clipboard()) {
                cb->setText(m_messageBubbleDelegate->selectedText());
                setStatusMessage(QStringLiteral("\u9009\u4E2D\u6587\u672C\u5DF2\u590D\u5236"), 1500);
                return;
            }
        }
        copyMessageAtIndexToClipboard(index);
    } else if (chosen == retryAction) {
        emit retryMessageRequested(messageId);
    } else if (chosen == openAction) {
        emit openMessageFileRequested(messageId);
    } else if (chosen == revealAction) {
        emit revealMessageFileRequested(messageId);
    } else if (chosen == recallAction) {
        emit recallMessageRequested(messageId);
    } else if (chosen == editAction) {
        const QString body = index.data(MessageListModel::BodyRole).toString();
        if (m_chatComposerWidget) m_chatComposerWidget->enterEditMode(messageId, body);
    } else if (chosen == replyAction) {
        const QString senderName = index.data(MessageListModel::SenderNameRole).toString();
        const QString senderId = index.data(MessageListModel::SenderIdRole).toString();
        QString bodyPreview = index.data(MessageListModel::BodyRole).toString();
        if (Qt::mightBeRichText(bodyPreview)) {
            QTextDocument doc; doc.setHtml(bodyPreview); bodyPreview = doc.toPlainText();
        }
        // 文件/图片消息：生成有意义的预览文本
        const bool isFileMsg = index.data(MessageListModel::FileMessageRole).toBool();
        if (isFileMsg) {
            const QString attachName = index.data(MessageListModel::AttachmentNameRole).toString();
            const QString localPath = index.data(MessageListModel::LocalFilePathRole).toString();
            static const QStringList imgExts = {QStringLiteral(".png"), QStringLiteral(".jpg"),
                QStringLiteral(".jpeg"), QStringLiteral(".gif"), QStringLiteral(".bmp"),
                QStringLiteral(".webp"), QStringLiteral(".svg")};
            const QString nameToCheck = attachName.isEmpty() ? localPath : attachName;
            bool isImage = false;
            for (const auto& ext : imgExts) {
                if (nameToCheck.endsWith(ext, Qt::CaseInsensitive)) { isImage = true; break; }
            }
            if (isImage) {
                bodyPreview = QStringLiteral("[\u56FE\u7247]");
            } else {
                bodyPreview = QStringLiteral("[\u6587\u4EF6] %1").arg(
                    attachName.isEmpty() ? QFileInfo(localPath).fileName() : attachName);
            }
        }
        if (bodyPreview.length() > 100) bodyPreview = bodyPreview.left(100) + QStringLiteral("...");
        if (m_chatComposerWidget) {
            emit replyToMessageRequested(messageId, senderId, senderName, bodyPreview);
            m_chatComposerWidget->setReplyContext(messageId, senderId, senderName, bodyPreview);
        }
    } else if (chosen == pinAction) {
        const QString body = index.data(MessageListModel::BodyRole).toString();
        const QString authorName = index.data(MessageListModel::SenderNameRole).toString();
        QString bodyPreview = body;
        if (Qt::mightBeRichText(bodyPreview)) {
            QTextDocument doc; doc.setHtml(bodyPreview); bodyPreview = doc.toPlainText();
        }
        if (bodyPreview.length() > 200) bodyPreview = bodyPreview.left(200) + QStringLiteral("...");
        emit pinMessageRequested(messageId, bodyPreview, authorName);
    } else if (chosen == reminderAction) {
        QString preview = index.data(MessageListModel::BodyRole).toString();
        if (Qt::mightBeRichText(preview)) {
            QTextDocument doc;
            doc.setHtml(preview);
            preview = doc.toPlainText();
        }
        if (isFileMessage) {
            const QString attachmentName = index.data(MessageListModel::AttachmentNameRole).toString();
            const QString filePath = index.data(MessageListModel::LocalFilePathRole).toString();
            const QString fileName = attachmentName.trimmed().isEmpty()
                ? QFileInfo(filePath).fileName()
                : attachmentName.trimmed();
            preview = fileName.trimmed().isEmpty()
                ? QStringLiteral("[文件消息]")
                : QStringLiteral("[文件] %1").arg(fileName);
        }
        if (preview.trimmed().isEmpty()) {
            preview = QStringLiteral("稍后回复这条消息");
        }
        if (preview.length() > 200) {
            preview = preview.left(200) + QStringLiteral("...");
        }

        QString title = index.data(MessageListModel::SenderNameRole).toString().trimmed();
        if (title.isEmpty()) {
            title = QStringLiteral("消息提醒");
        }

        const QString conversationId = m_activeComposerContextId.trimmed();
        emit messageReminderRequested(messageId, conversationId, title, preview.trimmed());
    } else if (chosen == forwardAction) {
        QString body = index.data(MessageListModel::BodyRole).toString();
        if (Qt::mightBeRichText(body)) {
            QTextDocument doc; doc.setHtml(body); body = doc.toPlainText();
        }
        const QString lp = index.data(MessageListModel::LocalFilePathRole).toString();
        const QString an = index.data(MessageListModel::AttachmentNameRole).toString();
        const QString sn = index.data(MessageListModel::SenderNameRole).toString();
        emit forwardMessageRequested(body, isFileMessage, lp, an, sn);
    } else if (chosen == saveStickerAction) {
        const QString pj = index.data(MessageListModel::PayloadJsonRole).toString();
        const QJsonObject sObj = QJsonDocument::fromJson(pj.toUtf8()).object();
        const QString sid = sObj.value(QStringLiteral("sticker_id")).toString();
        const QString pid = sObj.value(QStringLiteral("pack_id")).toString();
        QByteArray gifData;
        const QString b64 = sObj.value(QStringLiteral("gif_base64")).toString();
        if (!b64.isEmpty()) {
            gifData = QByteArray::fromBase64(b64.toLatin1());
        } else {
            gifData = StickerManager::instance().readStickerData(pid, sid);
        }
        if (!gifData.isEmpty()) {
            if (StickerManager::instance().addToFavorites(sid, gifData))
                setStatusMessage(QStringLiteral("\u5DF2\u6DFB\u52A0\u5230\u6211\u7684\u8868\u60C5"), 2000);
        } else {
            setStatusMessage(QStringLiteral("\u8868\u60C5\u6570\u636E\u4E0D\u53EF\u7528"), 2000);
        }
    }
}

void ConversationsPage::setMessageModel(MessageListModel* model) {
    m_messageModel = model;
    if (m_messageList && model) {
        m_messageList->setModel(model);
        // setModel() 可能重置 viewport 的 acceptDrops，重新确保拖拽可用
        m_messageList->setAcceptDrops(true);
        m_messageList->viewport()->setAcceptDrops(true);
    }
    if (model) {
        connect(model, &QAbstractItemModel::modelReset, this, [this]() {
            syncMessageStageEmptyState();
            // 用户正在查看历史消息时不滚到底部；
            // scheduleMessageViewportToBottom 内部会二次检查 m_followLatestMessages
            scheduleMessageViewportToBottom();
            // 同步 localClientId 到 delegate 用于 reaction pill 自参与高亮
            if (m_messageBubbleDelegate && m_messageModel) {
                m_messageBubbleDelegate->setLocalClientId(m_messageModel->localClientId());
                m_messageBubbleDelegate->setNameResolver([this](const QString& clientId) {
                    return m_messageModel ? m_messageModel->displayNameForClientId(clientId) : clientId;
                });
            }
        });
        connect(model, &QAbstractItemModel::rowsInserted, this,
                [this](const QModelIndex&, int first, int last) {
                    syncMessageStageEmptyState();
                    if (first == 0 && !m_followLatestMessages) {
                        // 历史消息在顶部插入：保持视口位置不变
                        // 插入前第一条可见项现在的索引 = count（被向下推）
                        const int count = last - first + 1;
                        QTimer::singleShot(0, this, [this, count]() {
                            if (!m_messageList || !m_messageList->model()) return;
                            const int totalRows = m_messageList->model()->rowCount();
                            if (count >= totalRows) return;
                            const QModelIndex targetIdx = m_messageList->model()->index(count, 0);
                            m_programmaticMessageScroll = true;
                            m_messageList->scrollTo(targetIdx, QAbstractItemView::PositionAtTop);
                            m_programmaticMessageScroll = false;
                        });
                    } else if (m_followLatestMessages) {
                        scheduleMessageViewportToBottom();
                    }
                });
        connect(model, &QAbstractItemModel::rowsRemoved, this,
                [this]() {
                    syncMessageStageEmptyState();
                });
        connect(model, &QAbstractItemModel::dataChanged, this,
                [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>&) {
                    // Delegate 自动处理重绘，只需保持滚动稳定
                    Q_UNUSED(topLeft); Q_UNUSED(bottomRight);
                    // 使用 isMessageViewportNearBottom 兜底：布局变化可能将 scrollbar 推离底部
                    // 导致 m_followLatestMessages 被 valueChanged 置 false，但用户本意仍是跟随最新消息
                    if (m_followLatestMessages || isMessageViewportNearBottom()) {
                        scheduleMessageViewportToBottom();
                    }
                });
        connect(model, &QAbstractItemModel::layoutChanged, this,
                [this]() { syncMessageListAfterLayoutChanged(); });
    }
    syncMessageStageEmptyState();
    scheduleMessageViewportToBottom();
}

void ConversationsPage::setTransferModel(QAbstractItemModel* model) {
    m_transferList->setModel(model);
    if (auto* transferModel = qobject_cast<TransferListModel*>(model)) {
        transferModel->setFilter(TransferListFilter::All);
    }
    const auto syncTransferState = [this, model]() {
        if (!m_transferEmptyLabel) {
            return;
        }
        const bool hasItems = model && model->rowCount() > 0;
        m_transferEmptyLabel->setVisible(!hasItems);
        m_transferList->setVisible(hasItems);
        syncTransferWorkspaceStatus();
    };
    if (model) {
        connect(model, &QAbstractItemModel::modelReset, this, syncTransferState);
        connect(model, &QAbstractItemModel::rowsInserted, this, syncTransferState);
        connect(model, &QAbstractItemModel::rowsRemoved, this, syncTransferState);
        connect(model, &QAbstractItemModel::layoutChanged, this, syncTransferState);
        connect(model, &QAbstractItemModel::dataChanged, this, syncTransferState);
    }
    syncTransferState();
}

void ConversationsPage::setListenPort(quint16 port) {
    m_portEdit->setText(QString::number(port));
}

void ConversationsPage::setChatHeader(const QString& title, const QString& status) {
    if (title.trimmed().isEmpty()) {
        m_activeComposerContextId.clear();
        // 用户在通知页面或 AI知识面板时，不抢占 contentStack
        if (true) {
            m_contentStack->setCurrentIndex(0);
            hideGroupInfoPanelAnimated();
            playWelcomeReveal();
        }
        return;
    }
    setChatHeaderDirect(title, status, QString(), QString());
}

void ConversationsPage::setChatHeaderDirect(const QString& name,
                                     const QString& status,
                                     const QString& signature,
                                     const QString& avatarImagePath) {
    const bool hasConversation = !name.trimmed().isEmpty();
    m_groupWorkspaceMode = false;
        // 用户在通知页面或 AI知识面板时，不抢占 contentStack
    if (true) {
        m_contentStack->setCurrentIndex(hasConversation ? 1 : 0);
    }
    // 直聊模式隐藏群聊右侧面板
    hideGroupInfoPanelAnimated();
    m_currentGroupId.clear();
    m_currentGroupMembers.clear(); // 清除 @ 提及成员列表
    m_currentGroupMemberEntries.clear();
    m_currentUserCanManageGroupMembers = false;
    syncGroupPanelToggleButton();
    if (!hasConversation) {
        m_activeComposerContextId.clear();
        playWelcomeReveal();
        return;
    }
    m_chatHeaderWidget->clearGroupRuntimeState();
    updateMessageStageContext(false, name, status);
    m_chatHeaderWidget->setDirectChatState(name, status, signature, avatarImagePath);
    syncNudgeAvailability();
}

void ConversationsPage::setChatHeaderGroup(const QString& groupId,
                                     const QString& groupName,
                                     int memberCount) {
    m_groupWorkspaceMode = true;
    m_currentGroupId = groupId;
    m_currentGroupName = groupName;
    m_activeComposerContextId = groupId.trimmed();
        // 用户在通知页面或 AI知识面板时，不抢占 contentStack
    if (true) {
        m_contentStack->setCurrentIndex(1);
    }
    const int onlineCount = std::count_if(m_currentGroupMemberEntries.cbegin(),
                                          m_currentGroupMemberEntries.cend(),
                                          [](const GroupMemberListEntry& e) { return e.isOnline; });
    const QString onlineText = QStringLiteral("%1/%2 \u5728\u7EBF").arg(onlineCount).arg(memberCount);
    updateMessageStageContext(true, groupName, onlineText);
    m_chatHeaderWidget->setGroupChatState(
        groupName.isEmpty() ? QStringLiteral("\u672A\u547D\u540D\u7FA4\u7EC4") : groupName,
        onlineText);
    // 群聊模式只准备右侧信息面板数据，默认保持收起；用户点击群公告/侧边按钮时再展开。
    const auto fsCfg = effectiveGroupFileServiceConfigForGroup(groupId);
    if (m_groupInfoPanel) {
        m_groupInfoPanel->showDetailView();
        m_groupInfoPanel->setGroupId(groupId);
        const bool canEditFs = std::any_of(m_currentGroupMemberEntries.cbegin(),
                                           m_currentGroupMemberEntries.cend(),
                                           [](const GroupMemberListEntry& e) {
                                               return e.isSelf && (e.isOwner || e.isAdmin);
                                           });
        m_groupInfoPanel->setGroupFileServiceConfig(fsCfg, canEditFs);
        if (m_groupPanelVisibleState) {
            showGroupInfoPanelAnimated();
        } else {
            m_groupInfoPanel->setMaximumWidth(0);
            m_groupInfoPanel->hide();
        }
    }
    m_chatHeaderWidget->setGroupFileManagerVisible(true);
    syncGroupRuntimeArchitectureStatus();
    syncGroupSharedFileCount();
    syncGroupPanelToggleButton();
}

void ConversationsPage::setGroupInfoPanel(const QString& announcement,
                                   const GroupMemberListEntries& members,
                                   bool currentUserCanManageMembers) {
    if (!m_groupInfoPanel) {
        return;
    }
    if (m_currentGroupMemberEntries != members
        || m_currentUserCanManageGroupMembers != currentUserCanManageMembers) {
        setGroupMembers(members, currentUserCanManageMembers);
    }
    const QString effectiveGroupName = m_currentGroupName.trimmed().isEmpty()
                                            ? m_groupInfoPanel->groupTitleText()
                                            : m_currentGroupName;
    m_groupInfoPanel->setGroupSummary(effectiveGroupName,
                                      announcement,
                                      m_currentGroupMemberEntries,
                                      m_currentUserCanManageGroupMembers);
    syncGroupRuntimeArchitectureStatus();
}

void ConversationsPage::setPinnedMessageCards(const std::vector<PinnedCardInfo>& cards) {
    if (!m_pinnedCardsContainer) { return; }
    // Short-circuit: 如果置顶消息ID列表未变，跳过重建避免闪烁
    QStringList newIds;
    newIds.reserve(static_cast<int>(cards.size()));
    for (const auto& c : cards) { newIds.append(c.messageId); }
    if (newIds == m_currentPinnedIds) { return; }
    m_currentPinnedIds = newIds;
    // 清除旧卡片（保留末尾的stretch）
    while (m_pinnedCardsLayout->count() > 1) {
        auto* item = m_pinnedCardsLayout->takeAt(0);
        if (item->widget()) { delete item->widget(); }
        delete item;
    }
    if (cards.empty()) {
        m_pinnedCardsContainer->setVisible(false);
        return;
    }
    for (const auto& card : cards) {
        auto* cardFrame = new ElaFrame(m_pinnedCardsContainer);
        cardFrame->setObjectName(QStringLiteral("pinnedCard"));
        cardFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        cardFrame->setStyleSheet(QStringLiteral(
            "QFrame#pinnedCard {"
            "  background: %1;"
            "  border: none;"
            "  border-left: 3px solid %2;"
            "  border-radius: 6px;"
            "  padding: 6px 10px;"
            "}"
            "QFrame#pinnedCard:hover {"
            "  background: %3;"
            "}")
            .arg(AppStyle::bubbleIn(),
                 AppStyle::accent(),
                 AppStyle::bubbleOut()));
        auto* cardLayout = new QVBoxLayout(cardFrame);
        cardLayout->setContentsMargins(8, 4, 20, 4);
        cardLayout->setSpacing(2);

        // 第一行：消息内容预览（支持文本选择与复制）
        auto* bodyLabel = new ElaText(cardFrame);
        bodyLabel->setText(card.pinnedBody);
        bodyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        bodyLabel->setCursor(Qt::IBeamCursor);
        bodyLabel->setContextMenuPolicy(Qt::CustomContextMenu);
        bodyLabel->setWordWrap(false);
        bodyLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 12px; font-weight: 500; border: none; background: transparent; }")
            .arg(AppStyle::textPrimary()));
        bodyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        connect(bodyLabel, &QLabel::customContextMenuRequested, this,
                [bodyLabel, fullText = card.pinnedBody](const QPoint& pos) {
                    ElaMenu menu;
                    const QString selected = bodyLabel->selectedText();
                    QAction* copySelAct = menu.addAction(QStringLiteral("\u590D\u5236\u9009\u4E2D\u5185\u5BB9"));
                    copySelAct->setEnabled(!selected.isEmpty());
                    QAction* copyAllAct = menu.addAction(QStringLiteral("\u590D\u5236\u5168\u90E8\u5185\u5BB9"));
                    QAction* chosen = menu.exec(bodyLabel->mapToGlobal(pos));
                    if (chosen == copySelAct) {
                        QGuiApplication::clipboard()->setText(selected);
                    } else if (chosen == copyAllAct) {
                        QGuiApplication::clipboard()->setText(fullText);
                    }
                });

        // 第二行：左边作者名，右边"由 XXX 置顶"
        auto* metaWidget = new QWidget(cardFrame);
        metaWidget->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        auto* metaLayout = new QHBoxLayout(metaWidget);
        metaLayout->setContentsMargins(0, 0, 0, 0);
        metaLayout->setSpacing(4);

        auto* authorLabel = new ElaText(card.authorName, metaWidget);
        authorLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; font-weight: 400; border: none; background: transparent; }")
            .arg(AppStyle::textSecondary()));
        auto* pinnerLabel = new ElaText(
            QStringLiteral("\u7531 %1 \u7F6E\u9876").arg(card.pinnerName), metaWidget);
        pinnerLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; font-weight: 400; border: none; background: transparent; }")
            .arg(AppStyle::textMuted()));
        pinnerLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        metaLayout->addWidget(authorLabel);
        metaLayout->addStretch(1);
        metaLayout->addWidget(pinnerLabel);

        cardLayout->addWidget(bodyLabel);
        cardLayout->addWidget(metaWidget);

        // 右上角关闭按钮
        auto* closeBtn = new ElaPushButton(QStringLiteral("\u2715"), cardFrame);
        closeBtn->setFixedSize(16, 16);
        closeBtn->setToolTip(QStringLiteral("\u53D6\u6D88\u7F6E\u9876"));
        closeBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  border: none; background: transparent; color: %1;"
            "  font-size: 11px; font-weight: bold;"
            "  padding: 0;"
            "}"
            "QPushButton:hover {"
            "  color: %2; background: %3; border-radius: 8px;"
            "}")
            .arg(AppStyle::textMuted(),
                 AppStyle::danger(),
                 AppStyle::hoverBg()));
        closeBtn->move(cardFrame->width() - 18, 2);
        // 使用绝对定位，在resize时重新定位
        connect(closeBtn, &QPushButton::clicked, this, [this, msgId = card.messageId]() {
            emit unpinMessageRequested(msgId);
        });
        // 让关闭按钮始终在右上角
        cardFrame->installEventFilter(this);
        closeBtn->setProperty("_pinnedCardCloseBtn", true);

        // 在stretch之前插入
        m_pinnedCardsLayout->insertWidget(m_pinnedCardsLayout->count() - 1, cardFrame, 1);
    }
    m_pinnedCardsContainer->setVisible(true);
}

void ConversationsPage::clearPinnedMessageCards() {
    if (!m_pinnedCardsContainer) { return; }
    if (m_currentPinnedIds.isEmpty() && !m_pinnedCardsContainer->isVisible()) { return; }
    m_currentPinnedIds.clear();
    while (m_pinnedCardsLayout->count() > 1) {
        auto* item = m_pinnedCardsLayout->takeAt(0);
        if (item->widget()) { delete item->widget(); }
        delete item;
    }
    m_pinnedCardsContainer->setVisible(false);
}

void ConversationsPage::setCurrentUserIsGroupOwner(bool isOwner) {
    m_currentUserIsGroupOwner = isOwner;
}

void ConversationsPage::setGroupMembers(const GroupMemberListEntries& members,
                                 bool currentUserCanManageMembers) {
    if (m_currentGroupMemberEntries == members
        && m_currentUserCanManageGroupMembers == currentUserCanManageMembers) {
        syncNudgeAvailability();
        return;
    }
    m_currentGroupMemberEntries = members;
    m_currentUserCanManageGroupMembers = currentUserCanManageMembers;
    m_currentGroupMembers.clear();
    m_currentGroupMembers.reserve(members.size() + 1);
    QSet<QString> seenMembers;
    for (const GroupMemberListEntry& member : members) {
        const QString displayName = member.displayName.trimmed().isEmpty()
                                        ? member.clientId.trimmed()
                                        : member.displayName.trimmed();
        if (displayName.isEmpty() || seenMembers.contains(displayName)) {
            continue;
        }
        seenMembers.insert(displayName);
        m_currentGroupMembers.append(displayName);
    }
    if (!members.isEmpty() && !seenMembers.contains(QStringLiteral("所有人"))) {
        m_currentGroupMembers.append(QStringLiteral("所有人"));
    }
    if (m_groupInfoPanel && m_groupPanelVisibleState) {
        const QString effectiveGroupName2 = m_currentGroupName.trimmed().isEmpty()
                                                ? m_groupInfoPanel->groupTitleText()
                                                : m_currentGroupName;
        m_groupInfoPanel->setGroupSummary(effectiveGroupName2,
                                          m_groupInfoPanel->announcementText(),
                                          members,
                                          currentUserCanManageMembers);
        syncGroupRuntimeArchitectureStatus();
    }
    // Re-push file service config now that member list is populated
    if (m_groupInfoPanel && !m_currentGroupId.isEmpty()) {
        const auto fsCfg = effectiveGroupFileServiceConfigForGroup(m_currentGroupId);
        const bool canEditFs = std::any_of(m_currentGroupMemberEntries.begin(),
                                           m_currentGroupMemberEntries.end(),
                                           [](const GroupMemberListEntry& e) {
                                               return e.isSelf && (e.isOwner || e.isAdmin);
                                           });
        m_groupInfoPanel->setGroupFileServiceConfig(fsCfg, canEditFs);
    }
    syncNudgeAvailability();
}

void ConversationsPage::syncNudgeAvailability()
{
    if (!m_chatComposerWidget || !m_chatComposerWidget->nudgeButton()) {
        return;
    }

    const bool inGroupConversation = !m_currentGroupId.trimmed().isEmpty();
    bool enabled = true;
    QString toolTip = QStringLiteral("窗口抖动提醒");
    if (inGroupConversation) {
        enabled = std::any_of(m_currentGroupMemberEntries.cbegin(),
                              m_currentGroupMemberEntries.cend(),
                              [](const GroupMemberListEntry& entry) {
                                  return entry.isSelf && (entry.isOwner || entry.isAdmin);
                              });
        if (!enabled) {
            toolTip = QStringLiteral("仅群主或管理员可发送窗口抖动提醒");
        }
    }

    m_chatComposerWidget->nudgeButton()->setEnabled(enabled);
    m_chatComposerWidget->nudgeButton()->setToolTip(toolTip);
}

void ConversationsPage::syncGroupRuntimeArchitectureStatus()
{
    if (m_currentGroupId.trimmed().isEmpty()) {
        return;
    }

    if (!m_hasRuntimeArchitectureSnapshot) {
        if (m_chatHeaderWidget && m_chatHeaderWidget->isGroupMode()) {
            m_chatHeaderWidget->clearGroupRuntimeState();
        }
        return;
    }

    const RuntimeArchitectureQueryService query(m_runtimeArchitectureSnapshot);
    const GroupServiceBindingSnapshot* matchedBinding = query.findGroupBinding(m_currentGroupId);
    const HybridRoutingDecision routingDecision =
        HybridRoutingPolicy::decideGroupFileRouting(m_runtimeArchitectureSnapshot,
                                                    m_currentGroupId);
    const QString routeLabel =
        routingDecision.mode == HybridRouteMode::ServicePreferred
            ? QStringLiteral("服务优先")
            : QStringLiteral("P2P 优先");

    if (!matchedBinding || !matchedBinding->enabled || matchedBinding->binding.boundServiceId.trimmed().isEmpty()) {
        if (m_chatHeaderWidget && m_chatHeaderWidget->isGroupMode()) {
            m_chatHeaderWidget->setGroupRuntimeState(
                                      QStringLiteral("删除联系人"),
                QStringLiteral("当前群 %1 仍以 P2P 为主，尚未绑定可持久化的混合架构服务")
                    .arg(m_currentGroupId),
                routeLabel);
        }
        if (!m_groupInfoPanel || !m_groupPanelVisibleState) {
            return;
        }
        // 如果文件服务已通过 syncGroupSharedFileCount 填充了缓存，保留其显示
        if (m_groupSharedFileCountCache.contains(m_currentGroupId)) {
            const int cachedCount = m_groupSharedFileCountCache.value(m_currentGroupId);
            m_groupInfoPanel->setHybridRuntimeSummary(
                QStringLiteral("%1 个文件").arg(cachedCount), QString());
        } else {
            // 检查群文件服务是否已独立启用
            const auto fsCfg = effectiveGroupFileServiceConfigForGroup(m_currentGroupId);
            if (!fsCfg.enabled || fsCfg.baseUrl.isEmpty()) {
                m_groupInfoPanel->setHybridRuntimeSummary(QString(), QString());
            }
        // 如果文件服务已通过 syncGroupSharedFileCount 填充了缓存，保留其显示
        }
        return;
    }

    const QString serviceName = query.serviceNameForGroup(m_currentGroupId);
    const QVector<ResourceReference> visibleResources = query.visibleResourcesForGroup(m_currentGroupId);
    const QVector<ResourceReference> sharedFileResources =
        query.sharedFileResourcesForGroup(m_currentGroupId);
    const int relatedResourceCount = visibleResources.size();
    const int localSharedFileCount = sharedFileResources.size();
    const int sharedFileCount =
        m_groupSharedFileCountCache.value(m_currentGroupId, localSharedFileCount);

    const QString workspaceId = matchedBinding->primaryResource.workspaceId.trimmed().isEmpty()
                                    ? QStringLiteral("default")
                                    : matchedBinding->primaryResource.workspaceId.trimmed();
    const QString primaryResourceTitle =
        matchedBinding->primaryResource.title.trimmed().isEmpty()
            ? matchedBinding->primaryResource.resourceId.trimmed()
            : matchedBinding->primaryResource.title.trimmed();
    const QString runtimeDetail =
        primaryResourceTitle.isEmpty()
            ? QStringLiteral("当前群 %1 已绑定 %2，%5，工作区 %3 可见资源 %4 个，共享文件 %6 个")
                  .arg(m_currentGroupId,
                       serviceName,
                       workspaceId,
                       QString::number(relatedResourceCount),
                       routeLabel,
                       QString::number(sharedFileCount))
            : QStringLiteral("当前群 %1 已绑定 %2，主资源 %3，%6，工作区 %4 可见资源 %5 个，共享文件 %7 个")
                  .arg(m_currentGroupId,
                       serviceName,
                       primaryResourceTitle,
                       workspaceId,
                       QString::number(relatedResourceCount),
                       routeLabel,
                       QString::number(sharedFileCount));
    if (m_chatHeaderWidget && m_chatHeaderWidget->isGroupMode()) {
        m_chatHeaderWidget->setGroupRuntimeState(QStringLiteral("已绑定群服务"),
                                                 runtimeDetail,
                                                 routeLabel);
    }
    if (!m_groupInfoPanel || !m_groupPanelVisibleState) {
        return;
    }
    m_groupInfoPanel->setHybridRuntimeSummary(QStringLiteral("已绑定群服务"),
                                              runtimeDetail);
}

void ConversationsPage::syncGroupSharedFileCount()
{
    if (m_currentGroupId.isEmpty() || !m_groupInfoPanel) {
        return;
    }
    const auto fsCfg = effectiveGroupFileServiceConfigForGroup(m_currentGroupId);
    if (!fsCfg.enabled || fsCfg.baseUrl.isEmpty()) {
        m_groupSharedFileCountCache.remove(m_currentGroupId);
        syncGroupRuntimeArchitectureStatus();
        return;
    }
    if (!m_groupFileNam) {
        m_groupFileNam = new QNetworkAccessManager(this);
    }
    // 始终用群 ID 作为 workspaceId 查询，与 GroupFileManagerDialog 保持一致
    const QString url = fsCfg.baseUrl
                        + QStringLiteral("/api/v1/files?workspaceId=%1").arg(m_currentGroupId);
    const QUrl requestUrl(url);
    QNetworkRequest req(requestUrl);
    req.setRawHeader("Authorization",
                     QStringLiteral("Bearer %1").arg(fsCfg.bearerToken).toUtf8());
    const QString capturedGroupId = m_currentGroupId;
    QNetworkReply* reply = m_groupFileNam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, capturedGroupId]() {
        reply->deleteLater();
        if (capturedGroupId != m_currentGroupId || !m_groupInfoPanel) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            qWarning().noquote()
                << QStringLiteral("[group-files] syncGroupSharedFileCount failed for %1: %2")
                       .arg(capturedGroupId, reply->errorString());
            return;
        }
        const auto arr = QJsonDocument::fromJson(reply->readAll()).array();
        m_groupSharedFileCountCache.insert(capturedGroupId, arr.size());
        syncGroupRuntimeArchitectureStatus();
    });
}

void ConversationsPage::setStatusMessage(const QString& message, int timeoutMs) {
    if (message.trimmed().isEmpty()) return;
    showChatToast(message.trimmed(), timeoutMs > 0 ? timeoutMs : 2000);
}

void ConversationsPage::showChatToast(const QString& message, int timeoutMs) {
    if (!m_chatToastLabel || message.trimmed().isEmpty()) return;
    const QString text = message.trimmed();
    m_chatToastLabel->setText(text);
    // 居中定位在消息区域顶部
    const int parentWidth = m_messageStageFrame ? m_messageStageFrame->width() : 400;
    const int maxToastWidth = qMax(180, parentWidth - 80);
    const QFontMetrics fm(m_chatToastLabel->font());
    const int desiredWidth = fm.horizontalAdvance(text) + 48;
    const int toastWidth = qBound(180, desiredWidth, maxToastWidth);
    const int textWidth = qMax(80, toastWidth - 48);
    const QRect textRect = fm.boundingRect(QRect(0, 0, textWidth, 1000),
                                           Qt::TextWordWrap | Qt::AlignCenter,
                                           text);
    const int toastHeight = qBound(36, textRect.height() + 20, 96);
    m_chatToastLabel->setFixedSize(toastWidth, toastHeight);
    m_chatToastLabel->move((parentWidth - toastWidth) / 2, 50);
    m_chatToastLabel->raise();
    m_chatToastLabel->show();
    if (m_chatToastTimer) {
        m_chatToastTimer->start(timeoutMs);
    }
}

void ConversationsPage::showProfileCard(const ProfileCardPopup::ProfileInfo& info, const QPoint& globalPos)
{
    if (info.clientId.trimmed().isEmpty()) return;
    if (!m_profileCard) {
        m_profileCard = new ProfileCardPopup(this);
        connect(m_profileCard, &ProfileCardPopup::sendMessageRequested, this,
                [this](const QString& cid) {
                    emit contactSelected(cid);
                });
    }
    m_profileCard->cancelHide();
    m_profileCard->showProfile(info, globalPos);
}

void ConversationsPage::showConversationAvatarPopup(const QModelIndex& index)
{
    if (!index.isValid() || !m_conversationModel) return;
    const QString conversationId =
        index.data(ConversationListModel::ConversationIdRole).toString();
    if (conversationId.isEmpty()) return;

    // Delegate 架构：通过 visualRect 计算头像位置
    const QRect itemRect = m_conversationList->visualRect(index);
    if (itemRect.isValid()) {
        const QPoint globalPos = m_conversationList->viewport()->mapToGlobal(
            QPoint(itemRect.x() + AppStyle::kAvatarSize + 16, itemRect.y()));
        emit conversationAvatarHovered(conversationId, globalPos);
    }
}

void ConversationsPage::setRuntimeArchitectureSummary(int serviceCount,
                                               int workspaceBindingCount,
                                               int groupBindingCount,
                                               int resourceCount,
                                               bool bound,
                                               const QString& activeServiceName)
{
    const RuntimeArchitecturePresentation presentation =
        buildRuntimeArchitecturePresentation(serviceCount,
                                            workspaceBindingCount,
                                            groupBindingCount,
                                            resourceCount,
                                            bound,
                                            activeServiceName);
    applyRuntimeArchitecturePresentation(presentation);
}

void ConversationsPage::applyRuntimeArchitecturePresentation(
    const RuntimeArchitecturePresentation& presentation)
{
    if (m_welcomeRuntimeChromeStatus) {
        m_welcomeRuntimeChromeStatus->setText(presentation.chromeStatus);
    }
    if (m_welcomeRuntimeSummary) {
        m_welcomeRuntimeSummary->setText(presentation.welcomeSummary);
    }
    if (m_welcomeRuntimeDetail) {
        m_welcomeRuntimeDetail->setText(presentation.welcomeDetail);
    }
    // 群文件面板的文件计数由 syncGroupRuntimeArchitectureStatus /
    // syncGroupSharedFileCount 统一管理，此处不再覆盖以避免抖动
}

void ConversationsPage::setRuntimeArchitectureSnapshot(const RuntimeArchitectureSnapshot& snapshot)
{
    m_runtimeArchitectureSnapshot = snapshot;
    m_hasRuntimeArchitectureSnapshot = true;
    applyRuntimeArchitecturePresentation(buildRuntimeArchitecturePresentation(snapshot));
    syncGroupRuntimeArchitectureStatus();
}

void ConversationsPage::setSelectedConversationId(const QString& conversationId) {
    const QString prevId = m_selectedConversationId;
    m_selectedConversationId = conversationId.trimmed();

    if (prevId == m_selectedConversationId) return;

    // Delegate 架构：通知 delegate 更新选中状态，一次 viewport update 即可
    if (m_conversationCardDelegate) {
        m_conversationCardDelegate->setSelectedConversationId(m_selectedConversationId);
    }
    if (m_conversationList && m_conversationList->viewport()) {
        m_conversationList->viewport()->update();
    }
}

void ConversationsPage::setSelectedTransferId(const QString& taskId) {
    const QModelIndex index =
        findIndexByRole(qobject_cast<QAbstractItemModel*>(m_transferList->model()),
                        TransferListModel::TaskIdRole,
                        taskId.trimmed());
    if (!index.isValid()) {
        m_transferList->clearSelection();
        return;
    }

    if (m_transferList->currentIndex() == index) {
        return;
    }

    m_transferList->setCurrentIndex(index);
    m_transferList->selectionModel()->select(index,
                                             QItemSelectionModel::ClearAndSelect
                                                 | QItemSelectionModel::Rows);
}

void ConversationsPage::focusChatInput() {
    m_inputEdit->setFocus();
}

bool ConversationsPage::isShowingWelcomePage() const {
    return m_contentStack && m_contentStack->currentIndex() == 0;
}

bool ConversationsPage::isShowingChatPage() const {
    return m_contentStack && m_contentStack->currentIndex() == 1;
}

bool ConversationsPage::isGroupPanelVisible() const {
    return m_groupPanelVisibleState;
}

bool ConversationsPage::isGroupWorkspaceActive() const
{
    return m_groupWorkspaceMode;
}

void ConversationsPage::updateMessageStageContext(bool groupMode, const QString& title, const QString& detail)
{
    if (!m_messageStageEmptyTitle || !m_messageStageEmptyBody) {
        return;
    }

    const QString trimmedTitle = title.trimmed();
    const QString trimmedDetail = detail.trimmed();
    if (groupMode) {
        if (m_messageStageModeChip) m_messageStageModeChip->setText(QStringLiteral("\u7FA4\u534F\u4F5C\u7A7A\u95F4"));
        if (m_messageStageContextChip) m_messageStageContextChip->setText(trimmedDetail.isEmpty()
                                               ? QStringLiteral("\u6210\u5458\u5DF2\u5C31\u7EEA")
                                               : trimmedDetail);
        if (m_messageStageHintLabel) m_messageStageHintLabel->setText(QStringLiteral("\u7FA4\u6D88\u606F"));
        m_messageStageEmptyTitle->setText(trimmedTitle.isEmpty()
                                              ? QStringLiteral("\u5F00\u59CB\u7FA4\u804A")
                                              : trimmedTitle);
        m_messageStageEmptyBody->setText(
            QStringLiteral("\u53EF\u4EE5\u4ECE\u8FD9\u91CC\u53D1\u9001\u6D88\u606F\u3001\u6587\u4EF6\u548C\u516C\u544A\u76F8\u5173\u5185\u5BB9\u3002"));
        return;
    }

    if (m_messageStageModeChip) m_messageStageModeChip->setText(QStringLiteral("\u76F4\u8FDE\u4F1A\u8BDD"));
    if (m_messageStageContextChip) m_messageStageContextChip->setText(trimmedDetail.isEmpty()
                                           ? QStringLiteral("\u5DF2\u8FDE\u63A5")
                                           : trimmedDetail);
    if (m_messageStageHintLabel) m_messageStageHintLabel->setText(QStringLiteral("\u5B9E\u65F6\u6D88\u606F"));
    m_messageStageEmptyTitle->setText(trimmedTitle.isEmpty()
                                          ? QStringLiteral("\u5F00\u59CB\u4F1A\u8BDD")
                                          : QStringLiteral("\u4E0E %1 \u7684\u5BF9\u8BDD").arg(trimmedTitle));
    m_messageStageEmptyBody->setText(
        QStringLiteral("\u53EF\u4EE5\u4ECE\u8FD9\u91CC\u53D1\u9001\u6587\u5B57\u3001\u6587\u4EF6\u6216\u622A\u56FE\u3002"));
}

void ConversationsPage::syncMessageStageEmptyState()
{
    if (!m_messageList || !m_messageStageEmptyCard) {
        return;
    }

    const auto* model = m_messageModel;
    const bool hasMessages = model && model->rowCount() > 0;
    m_messageStageEmptyCard->setVisible(!hasMessages);
    m_messageList->setVisible(hasMessages);
}

void ConversationsPage::syncComposerDraftState()
{
    if (!m_chatComposerWidget) {
        return;
    }

    const int attachmentCount = m_pendingAttachmentPaths.size();
    if (attachmentCount <= 0) {
        m_chatComposerWidget->setDraftMetaText(QStringLiteral("\u622A\u56FE / \u6587\u4EF6 / \u5F85\u53D1"));
        return;
    }

    m_chatComposerWidget->setDraftMetaText(
        QStringLiteral("%1 \u4E2A\u9644\u4EF6\u5F85\u53D1").arg(attachmentCount));
}

void ConversationsPage::syncConversationSidebarMode()
{
        syncConversationWorkspaceStatus();
    if (m_conversationsModeChip) {
        const QString baseText = m_groupWorkspaceMode
                                     ? QStringLiteral("\u7FA4\u804A")
                                     : QStringLiteral("\u6D88\u606F");
        m_conversationsModeChip->setText(baseText);
        m_conversationsModeChip->setMinimumWidth(0);
    }

    if (m_conversationFilterToggleBtn) {
        if (m_groupWorkspaceMode) {
            m_conversationFilterToggleBtn->setChecked(false);
            m_conversationFilterToggleBtn->hide();
            hideConversationFilterPanel();
        } else {
            m_conversationFilterToggleBtn->show();
        }
    }

    if (m_conversationEmptyLabel) {
        m_conversationEmptyLabel->setText(
            m_groupWorkspaceMode
                ? QStringLiteral("还没有群聊记录\n你可以新建群聊，或等待别人把你加入一个群")
                : QStringLiteral("还没有最近消息\n你可以从联系人发起对话，或手动连接一台设备"));
    }

    syncConversationWorkspaceStatus();
}

void ConversationsPage::syncConversationWorkspaceStatus()
{
    if (!m_conversationsModeChip || !m_conversationsStatusChip || !m_conversationList) {
        return;
    }

    auto* conversationModel = m_conversationModel;
    const int totalCount = conversationModel ? conversationModel->rowCount() : 0;

    if (totalCount == 0) {
        const QString modeText = m_groupWorkspaceMode
                                     ? QStringLiteral("\u7FA4\u804A 0")
                                     : QStringLiteral("\u6D88\u606F 0");
        m_conversationsModeChip->setText(modeText);
        m_conversationsModeChip->setMinimumWidth(0);
        m_conversationsStatusChip->hide();
        m_mainWindow->setNavUnreadCount(conversationModel ? conversationModel->totalUnreadCount() : 0);
        m_mainWindow->setNavGroupUnreadCount(conversationModel ? conversationModel->totalGroupUnreadCount() : 0);
        if (m_welcomeMessagesMetricValue) {
            m_welcomeMessagesMetricValue->setText(QStringLiteral("0"));
        }
        if (m_welcomeMessagesSignal) {
            m_welcomeMessagesSignal->setText(QStringLiteral("\u6682\u65E0\u4F1A\u8BDD"));
        }
        if (m_welcomeTransfersMetricValue) {
            m_welcomeTransfersMetricValue->setText(QStringLiteral("0"));
        }
        return;
    }

    int unreadCount = 0;
    int groupUnreadCount = 0;
    int groupCount = 0;
    for (int row = 0; row < totalCount; ++row) {
        const QModelIndex index = conversationModel->index(row, 0);
        const bool groupConversation =
            isGroupConversationId(index.data(ConversationListModel::ConversationIdRole).toString());
        if (groupConversation) {
            ++groupCount;
        }
        if (index.data(ConversationListModel::HasUnreadRole).toBool()) {
            ++unreadCount;
            if (groupConversation) {
                ++groupUnreadCount;
            }
        }
    }

    if (conversationModel) {
        m_mainWindow->setNavUnreadCount(conversationModel->totalUnreadCount());
        m_mainWindow->setNavGroupUnreadCount(conversationModel->totalGroupUnreadCount());
    } else {
        m_mainWindow->setNavUnreadCount(unreadCount);
        m_mainWindow->setNavGroupUnreadCount(groupUnreadCount);
    }

    // The mode chip shows either the direct-message or group-chat count.
    const QString modeText = m_groupWorkspaceMode
                                 ? QStringLiteral("\u7FA4\u804A %1").arg(totalCount)
                                 : QStringLiteral("\u6D88\u606F %1").arg(totalCount);
    m_conversationsModeChip->setText(modeText);
    m_conversationsModeChip->setMinimumWidth(0);

    // Show the status chip only when unread conversations exist.
    if (unreadCount > 0) {
        const QString statusText = QStringLiteral("\u672A\u8BFB %1").arg(unreadCount);
        m_conversationsStatusChip->setText(statusText);
        m_conversationsStatusChip->setMinimumWidth(qMax(
            72,
            QFontMetrics(m_conversationsStatusChip->font()).horizontalAdvance(statusText) + 28));
        m_conversationsStatusChip->hide();
    } else {
        m_conversationsStatusChip->hide();
    }

    if (m_welcomeMessagesMetricValue) {
        m_welcomeMessagesMetricValue->setText(QString::number(unreadCount > 0 ? unreadCount
                                                                              : totalCount));
    }
    if (m_welcomeMessagesSignal) {
        m_welcomeMessagesSignal->setText(unreadCount > 0
                                             ? QStringLiteral("%1\u672A\u8BFB").arg(unreadCount)
                                             : QStringLiteral("%1\u4F1A\u8BDD")
                                                   .arg(totalCount));
    }
    if (m_welcomeTransfersMetricValue) {
        m_welcomeTransfersMetricValue->setText(QString::number(groupCount));
    }
}

void ConversationsPage::syncWelcomeContactMetric()
{
    if (!m_welcomeContactsMetricValue) {
        return;
    }
    if (!m_contactModel || m_contactModel->rowCount() == 0) {
        m_welcomeContactsMetricValue->setText(QStringLiteral("0"));
        return;
    }

    int onlineCount = 0;
    for (int row = 0; row < m_contactModel->rowCount(); ++row) {
        const QModelIndex index = m_contactModel->index(row, 0);
        if (index.data(ContactListModel::IsSectionHeaderRole).toBool()) {
            continue;
        }
        if (index.data(ContactListModel::StatusTextRole).toString()
            == QStringLiteral("\u5728\u7EBF")) {
            ++onlineCount;
        }
    }
    m_welcomeContactsMetricValue->setText(QString::number(onlineCount));
}

void ConversationsPage::syncTransferWorkspaceStatus()
{
    if (!m_transferStatusChip || !m_transferList) {
        return;
    }

    auto* model = m_transferList->model();
    if (!model || model->rowCount() == 0) {
        m_transferStatusChip->setText(QStringLiteral("\u5B9E\u65F6\u8FFD\u8E2A"));
        if (m_welcomeTransfersSignal) {
            m_welcomeTransfersSignal->setText(QStringLiteral("\u5B9E\u65F6\u8FFD\u8E2A"));
        }
        return;
    }

    m_transferStatusChip->setText(QStringLiteral("%1 \u9879\u4EFB\u52A1").arg(model->rowCount()));
    m_transferStatusChip->setMinimumWidth(
        QFontMetrics(m_transferStatusChip->font()).horizontalAdvance(m_transferStatusChip->text()) + 28);
    if (m_welcomeTransfersSignal) {
        m_welcomeTransfersSignal->setText(QStringLiteral("%1 \u9879\u4F20\u8F93").arg(model->rowCount()));
    }
}

void ConversationsPage::playWelcomeReveal()
{
    // The welcome hero is rendered directly instead of through opacity effects.
    // This keeps the primary card stable in offscreen review screenshots and
    // avoids effect-layer drift in the visual review environment.
}

void ConversationsPage::showGroupInfoPanelAnimated()
{
    if (!m_groupInfoPanel || !m_groupPanelAnimation) {
        return;
    }

    m_groupPanelVisibleState = true;
    syncGroupPanelToggleButton();
    if (m_groupPanelToggleBtn) {
        m_groupPanelToggleBtn->hide();
    }

    // 面板已经完全展开时，跳过动画避免闪烁
    if (m_groupInfoPanel->isVisible()
        && m_groupInfoPanel->width() >= m_groupPanelExpandedWidth) {
        m_groupInfoPanel->setMaximumWidth(m_groupPanelExpandedWidth);
        return;
    }

    m_groupPanelAnimation->stop();
    // 清除可能残留的 hide 动画 mask 和边距
    if (m_groupInfoPanel->layout()) {
        m_groupInfoPanel->layout()->setContentsMargins(0, 0, 0, 0);
    }
    if (auto* sw = m_groupInfoPanel->findChild<QWidget*>(
            QStringLiteral("groupInfoStackedWidget"))) {
        sw->clearMask();
    }
    if (m_chatWorkspaceSplitter) {
        m_chatWorkspaceSplitter->setHandleWidth(2);
    }
    m_groupInfoPanel->show();
    m_groupInfoPanel->raise();
    m_groupInfoPanel->setMaximumWidth(m_groupPanelExpandedWidth);
    m_groupPanelAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_groupPanelAnimation->setStartValue(qMin(m_groupInfoPanel->width(), m_groupPanelExpandedWidth));
    m_groupPanelAnimation->setEndValue(m_groupPanelExpandedWidth);
    m_groupPanelAnimation->start();
    QTimer::singleShot(0, this, [this]() {
        if (!m_groupPanelVisibleState || !m_groupPanelToggleBtn) {
            return;
        }
        syncGroupPanelToggleButton();
        positionGroupPanelToggleButton();
    });
}

void ConversationsPage::hideGroupInfoPanelAnimated()
{
    if (!m_groupInfoPanel || !m_groupPanelAnimation) {
        return;
    }

    m_groupPanelVisibleState = false;
    syncGroupPanelToggleButton();
    m_groupPanelAnimation->stop();
    if (!m_groupInfoPanel->isVisible()) {
        m_groupInfoPanel->setMaximumWidth(0);
        return;
    }
    const int currentWidth = qMax(0, m_groupInfoPanel->width());
    if (currentWidth > 0) {
        m_groupPanelExpandedWidth = qBound(240, currentWidth, 420);
    }
    m_groupInfoPanel->setMaximumWidth(currentWidth);
    m_groupPanelAnimation->setEasingCurve(QEasingCurve::InCubic);
    m_groupPanelAnimation->setStartValue(currentWidth);
    m_groupPanelAnimation->setEndValue(0);
    m_groupPanelAnimation->start();
    positionGroupPanelToggleButton();
}

void ConversationsPage::toggleGroupInfoPanel()
{
    if (m_groupPanelVisibleState) {
        hideGroupInfoPanelAnimated();
    } else {
        prepareGroupInfoPanelDetail();
        showGroupInfoPanelAnimated();
        syncGroupRuntimeArchitectureStatus();
    }
}

void ConversationsPage::prepareGroupInfoPanelDetail()
{
    if (!m_groupInfoPanel || m_currentGroupId.trimmed().isEmpty()) {
        return;
    }
    m_groupInfoPanel->showDetailView();
    m_groupInfoPanel->setGroupId(m_currentGroupId);
    const auto fsCfg = effectiveGroupFileServiceConfigForGroup(m_currentGroupId);
    const bool canEditFs = m_currentUserCanManageGroupMembers || m_currentUserIsGroupOwner;
    m_groupInfoPanel->setGroupFileServiceConfig(fsCfg, canEditFs);
}

void ConversationsPage::syncGroupPanelToggleButton()
{
    if (!m_groupPanelToggleBtn) {
        return;
    }
    const bool shouldExpose = m_groupWorkspaceMode && !m_currentGroupId.trimmed().isEmpty();
    m_groupPanelToggleBtn->setVisible(shouldExpose && m_groupPanelVisibleState);
    m_groupPanelToggleBtn->setElaIcon(m_groupPanelVisibleState
                                      ? ElaIconType::AngleRight
                                      : ElaIconType::AngleLeft);
    m_groupPanelToggleBtn->setToolTip(m_groupPanelVisibleState
                                      ? QStringLiteral("收起群成员")
                                      : QStringLiteral("展开群成员"));
    m_groupPanelToggleBtn->setStyleSheet(QStringLiteral(
        "QToolButton#groupPanelEdgeToggleButton {"
        "  background:%1;"
        "  border:1px solid %2;"
        "  color:%3;"
        "  border-radius:8px;"
        "  padding:0;"
        "}"
        "QToolButton#groupPanelEdgeToggleButton:hover {"
        "  background:%4;"
        "  border:1px solid %5;"
        "}")
            .arg(AppStyle::surface(),
                 AppStyle::border(),
                 AppStyle::textSecondary(),
                 AppStyle::hoverBg(),
                 AppStyle::accent()));
    positionGroupPanelToggleButton();
}

void ConversationsPage::positionGroupPanelToggleButton()
{
    if (!m_groupPanelToggleBtn || !m_groupPanelToggleBtn->parentWidget()) {
        return;
    }
    constexpr int buttonWidth = 20;
    constexpr int buttonHeight = 40;
    m_groupPanelToggleBtn->setFixedSize(buttonWidth, buttonHeight);
    QWidget* parent = m_groupPanelToggleBtn->parentWidget();
    QPoint stageTopLeft(0, 0);
    QSize stageSize = parent->size();
    if (m_groupPanelVisibleState && m_groupInfoPanel && m_groupInfoPanel->isVisible()) {
        const QPoint panelTopLeft = m_groupInfoPanel->mapTo(parent, QPoint(0, 0));
        if (panelTopLeft.x() > 0) {
            stageTopLeft.setX(qMax(0, panelTopLeft.x() - buttonWidth));
            stageTopLeft.setY(panelTopLeft.y());
            stageSize = m_groupInfoPanel->size();
        }
    } else if (m_messageStageFrame) {
        stageTopLeft = m_messageStageFrame->mapTo(parent, QPoint(0, 0));
        stageSize = m_messageStageFrame->size();
    }
    const int x = m_groupPanelVisibleState && m_groupInfoPanel && m_groupInfoPanel->isVisible()
        ? qMax(0, stageTopLeft.x())
        : qMax(0, stageTopLeft.x() + stageSize.width() - buttonWidth);
    const int y = qMax(stageTopLeft.y() + 72,
                       stageTopLeft.y() + (stageSize.height() - buttonHeight) / 2);
    m_groupPanelToggleBtn->move(x, y);
    m_groupPanelToggleBtn->raise();
}

void ConversationsPage::showDirectConversation(const QString& conversationId, const QString& title)
{
    m_followLatestMessages = true;
    // 切换会话前清空输入框和附件草稿，防止消息发给错误的人
    if (m_activeComposerContextId != conversationId.trimmed()) {
        if (m_inputEdit) { m_inputEdit->clear(); }
        m_pendingAttachmentPaths.clear();
        m_screenshotPreviewToPath.clear();
        syncComposerDraftState();
    }
    m_activeComposerContextId = conversationId.trimmed();
    if (m_contextPanel) {
        m_contextPanel->showPrivateProfile(m_activeComposerContextId);
    }
    setChatHeaderDirect(title, QStringLiteral("已连接"), QString(), QString());
    syncNudgeAvailability();
}

void ConversationsPage::showGroupConversation(const QString& conversationId, const QString& title)
{
    m_followLatestMessages = true;
    // 切换会话前清空输入框和附件草稿，防止消息发给错误的人
    if (m_activeComposerContextId != conversationId.trimmed()) {
        if (m_inputEdit) { m_inputEdit->clear(); }
        m_pendingAttachmentPaths.clear();
        m_screenshotPreviewToPath.clear();
        syncComposerDraftState();
    }
    m_activeComposerContextId = conversationId.trimmed();
    if (m_contextPanel) {
        m_contextPanel->showGroupContext(m_activeComposerContextId);
    }
    setChatHeaderGroup(conversationId,
                       title,
                       m_currentGroupMemberEntries.isEmpty() ? 0 : m_currentGroupMemberEntries.size());
    syncNudgeAvailability();
}

void ConversationsPage::clearComposerDraft(const QString& outgoingConversationId)
{
    // 保存当前会话的草稿文本（非空时才存，空时清除条目）
    // 优先使用调用方传入的会话 ID，回落到 m_activeComposerContextId
    const QString saveKey = outgoingConversationId.isEmpty()
        ? m_activeComposerContextId : outgoingConversationId;
    if (!saveKey.isEmpty() && m_inputEdit) {
        // 同步附件列表：移除已从文档中删除的图片，避免残留草稿
        if (!m_pendingAttachmentPaths.isEmpty()) {
            QSet<QString> liveImages;
            QTextBlock block = m_inputEdit->document()->begin();
            while (block.isValid()) {
                for (auto it = block.begin(); !it.atEnd(); ++it) {
                    const QTextFragment frag = it.fragment();
                    if (frag.isValid() && frag.charFormat().isImageFormat()) {
                        liveImages.insert(frag.charFormat().toImageFormat().name());
                    }
                }
                block = block.next();
            }
            for (auto it = m_screenshotPreviewToPath.begin(); it != m_screenshotPreviewToPath.end(); ) {
                if (!liveImages.contains(it.key())) {
                    m_pendingAttachmentPaths.removeAll(it.value());
                    it = m_screenshotPreviewToPath.erase(it);
                } else {
                    ++it;
                }
            }
        }
        const bool hasAttachments = !m_pendingAttachmentPaths.isEmpty();
        const QString text = m_inputEdit->toPlainText().trimmed();
        if (!text.isEmpty() || hasAttachments) {
            // 对于含附件（图片）的草稿，保存友好摘要文本用于会话卡片显示
            if (hasAttachments && text.isEmpty()) {
                m_composerDrafts.insert(saveKey, QStringLiteral("[\u56FE\u7247]"));
            } else if (hasAttachments) {
                // 文本中可能含内嵌图片的 object replacement char，清理后保存
                QString cleanText = text;
                cleanText.remove(QChar::ObjectReplacementCharacter);
                cleanText = cleanText.trimmed();
                m_composerDrafts.insert(saveKey,
                    cleanText.isEmpty() ? QStringLiteral("[\u56FE\u7247]") : cleanText);
            } else {
                m_composerDrafts.insert(saveKey, text);
            }
            // 保存附件状态和 HTML 内容，以便切换回来时恢复
            if (hasAttachments) {
                m_composerDraftHtml.insert(saveKey, m_inputEdit->toHtml());
                m_draftAttachmentPaths.insert(saveKey, m_pendingAttachmentPaths);
                m_draftScreenshotMap.insert(saveKey, m_screenshotPreviewToPath);
            } else {
                m_composerDraftHtml.remove(saveKey);
                m_draftAttachmentPaths.remove(saveKey);
                m_draftScreenshotMap.remove(saveKey);
            }
        } else {
            m_composerDrafts.remove(saveKey);
            m_composerDraftHtml.remove(saveKey);
            m_draftAttachmentPaths.remove(saveKey);
            m_draftScreenshotMap.remove(saveKey);
        }
    }
    if (m_inputEdit) { m_inputEdit->clear(); }
    m_pendingAttachmentPaths.clear();
    m_screenshotPreviewToPath.clear();
    syncComposerDraftState();
    syncDraftsToModel();
}

void ConversationsPage::restoreComposerDraft(const QString& conversationId)
{
    if (!m_inputEdit || conversationId.isEmpty()) { return; }
    // 确保 m_activeComposerContextId 指向当前会话，
    // 私聊路径下 setChatHeaderDirect 不会设置此字段，必须在此处更新
    m_activeComposerContextId = conversationId;

    // 恢复附件状态（如有）
    if (m_draftAttachmentPaths.contains(conversationId)) {
        m_pendingAttachmentPaths = m_draftAttachmentPaths.take(conversationId);
        m_screenshotPreviewToPath = m_draftScreenshotMap.take(conversationId);
        m_attachmentDraftContextId = conversationId;
        // 恢复包含内嵌图片的 HTML 内容
        const QString html = m_composerDraftHtml.take(conversationId);
        if (!html.isEmpty()) {
            m_inputEdit->setHtml(html);
        }
        m_composerDrafts.remove(conversationId);
    } else {
        const QString draft = m_composerDrafts.value(conversationId);
        if (!draft.isEmpty()) {
            m_inputEdit->setPlainText(draft);
        }
    }
    const auto recovered = m_recoveredComposerContexts.find(conversationId);
    if (recovered != m_recoveredComposerContexts.end()) {
        const ComposerRecoveryContext context = recovered.value();
        m_recoveredComposerContexts.erase(recovered);
        m_chatComposerWidget->restoreRecoveryContext(context);
    }
    // 光标移到末尾
    auto cursor = m_inputEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_inputEdit->setTextCursor(cursor);
    syncComposerDraftState();
    syncDraftsToModel();
}

ComposerRecoveryContext ConversationsPage::activeComposerRecoveryContext() const
{
    if (m_activeComposerContextId.isEmpty() || !m_chatComposerWidget) {
        return {};
    }
    return m_chatComposerWidget->recoveryContext();
}

QString ConversationsPage::activeComposerContextId() const
{
    return m_activeComposerContextId;
}

void ConversationsPage::stageRecoveredComposerContext(
    const QString& conversationId, const ComposerRecoveryContext& context)
{
    const QString key = conversationId.trimmed();
    if (!key.isEmpty()) {
        m_recoveredComposerContexts.insert(key, context);
    }
}

void ConversationsPage::syncDraftsToModel()
{
    auto* model = m_conversationModel;
    if (model) {
        // 当前活跃会话不显示草稿标签（用户正在编辑）
        QHash<QString, QString> visibleDrafts = m_composerDrafts;
        if (!m_activeComposerContextId.isEmpty()) {
            visibleDrafts.remove(m_activeComposerContextId);
        }
        model->setDraftTexts(visibleDrafts);
    }
}

void ConversationsPage::clearCurrentConversationView()
{
    m_activeComposerContextId.clear();
    m_attachmentDraftContextId.clear();
    m_selectedConversationId.clear();
    m_currentGroupId.clear();
    m_currentGroupName.clear();
    m_currentGroupMembers.clear();
    m_currentGroupMemberEntries.clear();
    m_currentUserCanManageGroupMembers = false;
    m_followLatestMessages = true;
    m_pendingAttachmentPaths.clear();
    m_screenshotPreviewToPath.clear();
    syncComposerDraftState();
    syncNudgeAvailability();
    if (m_inputEdit) {
        m_inputEdit->clear();
    }
    if (m_contextPanel) {
        m_contextPanel->showPrivateProfile(QString());
    }
    m_selectedConversationId.clear();
    if (m_conversationCardDelegate) {
        m_conversationCardDelegate->setSelectedConversationId(QString());
    }
    if (m_conversationList && m_conversationList->viewport()) {
        m_conversationList->viewport()->update();
    }
    m_transferList->clearSelection();
    setChatHeader(QString(), QString());
}

void ConversationsPage::enterMessageMultiSelectMode()
{
    if (m_messageMultiSelectMode) return;
    m_messageMultiSelectMode = true;
    m_multiSelectedMessageIds.clear();
    window()->setProperty("messageMultiSelectMode", true);
    window()->setProperty("multiSelectedMessageIds", QVariant::fromValue(m_multiSelectedMessageIds));
    if (m_multiSelectBar) m_multiSelectBar->show();
    updateMultiSelectBar();

    // 多选模式下点击消息行切换勾选
    m_messageList->setProperty("multiSelectClickConnection",
        QVariant::fromValue(connect(m_messageList, &QAbstractItemView::clicked, this, [this](const QModelIndex& idx) {
            if (!m_messageMultiSelectMode || !idx.isValid() || !m_messageModel) return;
            const QString msgId = idx.data(MessageListModel::MessageIdRole).toString();
            if (msgId.isEmpty()) return;
            toggleMessageMultiSelect(msgId);
        })));
}

void ConversationsPage::exitMessageMultiSelectMode()
{
    if (!m_messageMultiSelectMode) return;
    m_messageMultiSelectMode = false;
    m_multiSelectedMessageIds.clear();
    window()->setProperty("messageMultiSelectMode", false);
    window()->setProperty("multiSelectedMessageIds", QVariant::fromValue(m_multiSelectedMessageIds));
    if (m_multiSelectBar) m_multiSelectBar->hide();

    // 断开点击连接
    const auto conn = m_messageList->property("multiSelectClickConnection").value<QMetaObject::Connection>();
    if (conn) disconnect(conn);

    // 刷新所有行去掉勾选框
    if (m_messageList && m_messageModel) {
        m_messageList->viewport()->update();
    }
}

void ConversationsPage::toggleMessageMultiSelect(const QString& messageId)
{
    if (m_multiSelectedMessageIds.contains(messageId)) {
        m_multiSelectedMessageIds.remove(messageId);
    } else {
        m_multiSelectedMessageIds.insert(messageId);
    }
    updateMultiSelectBar();
    if (m_messageList && m_messageList->viewport()) {
        m_messageList->viewport()->update();
    }
}

void ConversationsPage::updateMultiSelectBar()
{
    if (!m_multiSelectForwardBtn) return;
    const int count = m_multiSelectedMessageIds.size();
    m_multiSelectForwardBtn->setText(QStringLiteral("\u8F6C\u53D1(%1)").arg(count));
    m_multiSelectForwardBtn->setEnabled(count > 0);
    window()->setProperty("multiSelectedMessageIds", QVariant::fromValue(m_multiSelectedMessageIds));
}

void ConversationsPage::triggerScreenshot(bool forceHideWindow)
{
    if (!m_mainWindow) {
        return;
    }

    QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
    const bool hideFirst = forceHideWindow
        || cfg.value(QStringLiteral("screenshotHideWindow"), false).toBool();

    // 截图前临时隐藏仍存活的 CallWindow，避免两个 StaysOnTopHint 窗口争抢焦点。
    QPointer<CallWindow> cw = visibleCallWindow();
    const bool callWasVisible = !cw.isNull();
    if (callWasVisible) {
        cw->hide();
    }

    if (hideFirst) {
        m_mainWindow->setWindowState(m_mainWindow->windowState() | Qt::WindowMinimized);
        m_mainWindow->hide();
        QGuiApplication::processEvents();
    }

    // 延迟 150ms 确保窗口完全隐藏后再截图
    const int delayMs = hideFirst ? 150 : 0;
    QTimer::singleShot(delayMs, this, [this, hideFirst, cw, callWasVisible]() {
        if (!m_mainWindow) {
            return;
        }
        auto* overlay = new ScreenshotOverlay(m_mainWindow);

        connect(overlay, &ScreenshotOverlay::accepted, this, [this, overlay, hideFirst, cw, callWasVisible]() {
            if (callWasVisible && cw) {
                cw->show();
                cw->raise();
            }
            if (hideFirst) {
                m_mainWindow->showNormal();
                m_mainWindow->raise();
                m_mainWindow->activateWindow();
            }
            const QImage captured = overlay->croppedImage();
            if (captured.isNull()) {
                return;
            }
            QGuiApplication::clipboard()->setImage(captured);
            importScreenshotPreview(captured);
            setStatusMessage(QStringLiteral("\u622A\u56FE\u5DF2\u52A0\u5165\u8F93\u5165\u6846\u5E76\u590D\u5236\u5230\u526A\u8D34\u677F"), 3000);
        });

        connect(overlay, &ScreenshotOverlay::rejected, this, [this, hideFirst, cw, callWasVisible]() {
            if (callWasVisible && cw) {
                cw->show();
                cw->raise();
            }
            if (hideFirst) {
                m_mainWindow->showNormal();
                m_mainWindow->raise();
                m_mainWindow->activateWindow();
            }
        });
    });
}

bool ConversationsPage::importScreenshotPreview(const QImage& image) {
    if (image.isNull() || !m_inputEdit) {
        return false;
    }

    const QString screenshotDir = ensureScreenshotDirectory();
    const QString filePath = QDir(screenshotDir).filePath(
        QStringLiteral("shot-%1-%2.png")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")))
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)));
    if (!image.save(filePath, "PNG")) {
        return false;
    }

    m_pendingAttachmentPaths.append(filePath);
    m_attachmentDraftContextId = m_activeComposerContextId;
    syncComposerDraftState();

    QTextCursor cursor = m_inputEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!m_inputEdit->document()->isEmpty()) {
        cursor.insertBlock();
    }

    QByteArray previewBytes;
    {
        QBuffer previewBuffer(&previewBytes);
        previewBuffer.open(QIODevice::WriteOnly);
        image.save(&previewBuffer, "PNG");
    }

    const QString previewName =
        QStringLiteral("data:image/png;base64,%1").arg(QString::fromLatin1(previewBytes.toBase64()));
    const int previewWidth = qMin(360, image.width());
    const int previewHeight =
        image.width() > 0 ? qMax(1, image.height() * previewWidth / image.width()) : image.height();

    m_screenshotPreviewToPath.insert(previewName, filePath);

    cursor.insertHtml(QStringLiteral("<img src=\"%1\" width=\"%2\" height=\"%3\" />")
                          .arg(previewName.toHtmlEscaped())
                          .arg(previewWidth)
                          .arg(previewHeight));
    cursor.insertBlock();

    m_inputEdit->setTextCursor(cursor);
    m_inputEdit->setFocus();
    return true;
}

void ConversationsPage::refreshTheme()
{
    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();

    setStyleSheet(AppStyle::activeStylesheet(mode));
    if (auto* workspaceShellFrame = findChild<QFrame*>(QStringLiteral("workspaceShellFrame"))) {
        const QString shellBorder = AppStyle::isDarkTheme(mode)
            ? QStringLiteral("#4B5563")
            : QStringLiteral("#D0D5DD");
        workspaceShellFrame->setStyleSheet(QStringLiteral(
            "QFrame#workspaceShellFrame {"
            "  background: transparent;"
            "  border: 1px solid %1;"
            "  border-radius: 16px;"
            "}").arg(shellBorder));
    }
    if (m_sideStack) {
        m_sideStack->setStyleSheet(QStringLiteral(
            "QStackedWidget#sideStack { background: transparent; border: none; }"));
    }
    if (m_contentStack) {
        m_contentStack->setStyleSheet(QStringLiteral(
            "QStackedWidget#contentWorkspaceStack { background: transparent; border: none; }"));
    }
    if (auto* conversationSidebar = m_sideStack ? m_sideStack->widget(0) : nullptr) {
        conversationSidebar->setStyleSheet(QStringLiteral(
            "QWidget#conversationSidebarWidget {"
            "  background: transparent;"
            "  border-left: none;"
            "  border-right: none;"
            "}"));
    }
    if (m_conversationList) {
        m_conversationList->setStyleSheet(QStringLiteral(
            "QListWidget { background: transparent; border: none; outline: none; padding: 0 0 10px 0; }"
            "QListWidget::item { background: transparent; border: none; padding: 0; margin: 0; }"
            "QScrollBar:vertical { width: 0px; background: transparent; border: none; }"
            "QScrollBar::handle:vertical, QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "  background: transparent; border: none;"
            "}"));
        m_conversationList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    if (m_messageList) {
        m_messageList->setStyleSheet(QStringLiteral(
            "QListView#messageListView { background: transparent; border: none; outline: none; padding: 0; }"
            "QListView#messageListView::item { background: transparent; border: none; padding: 0; margin: 0; }"
            "QListView#messageListView::item:hover { background: transparent; border: none; }"
            "QListView#messageListView::item:selected { background: transparent; border: none; }"));
    }

    if (m_conversationEmptyLabel) {
        m_conversationEmptyLabel->setStyleSheet(QStringLiteral("color:%1;").arg(AppStyle::textMuted(mode)));
    }
    if (m_transferEmptyLabel) {
        m_transferEmptyLabel->setStyleSheet(QStringLiteral("color:%1;").arg(AppStyle::textMuted(mode)));
    }
    if (m_mentionPopup) {
        m_mentionPopup->setStyleSheet(QStringLiteral(
            "QListWidget {"
            "  background:%1;"
            "  color:%2;"
            "  border:none;"
            "  border-radius:10px;"
            "  padding:4px;"
            "}"
            "QListWidget::item {"
            "  padding:6px 8px;"
            "  border-radius:8px;"
            "}"
            "QListWidget::item:selected {"
            "  background:%4;"
            "  color:%2;"
            "}").arg(AppStyle::surface(mode),
                      AppStyle::textPrimary(mode),
                      AppStyle::border(mode),
                      AppStyle::selectedBg(mode)));
    }
    {
        const QColor baseBg(Qt::transparent);
        auto fixViewport = [&](QAbstractScrollArea* area) {
            if (!area || !area->viewport()) return;
            QPalette vp = area->viewport()->palette();
            area->viewport()->setAutoFillBackground(false);
            vp.setColor(QPalette::Base, baseBg);
            vp.setColor(QPalette::Window, baseBg);
            area->viewport()->setPalette(vp);
            area->viewport()->update();
        };
        fixViewport(m_conversationList);
        fixViewport(m_messageList);
        fixViewport(m_transferList);
    }
    if (m_transferList && m_transferList->viewport()) {
        m_transferList->viewport()->update();
    }
    // Delegate 架构：会话列表主题切换只需一次 viewport update
    // delegate paint() 中实时读取 AppStyle 当前颜色
    if (m_conversationList && m_conversationList->viewport()) {
        m_conversationList->viewport()->update();
    }
    refreshVisibleMessageBubbleThemes();
    if (m_chatHeaderWidget) {
        m_chatHeaderWidget->refreshTheme();
    }
    if (m_chatComposerWidget) {
        m_chatComposerWidget->refreshTheme();
    }
    if (m_groupInfoPanel) {
        m_groupInfoPanel->refreshTheme();
    }
    syncGroupPanelToggleButton();
}

void ConversationsPage::syncSendModePresentation()
{
    QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
    const bool enterMode = cfg.value(QStringLiteral("sendMode"),
                                     QStringLiteral("enter")).toString()
                           == QStringLiteral("enter");
    if (m_sendButton) {
        m_sendButton->setToolTip(enterMode
                                     ? QStringLiteral("Enter 发送")
                                     : QStringLiteral("Ctrl+Enter 发送"));
    }
    if (m_sendModeBtn) {
        const QString buttonText = enterMode
                                       ? QStringLiteral("Enter 发送 ▾")
                                       : QStringLiteral("Ctrl+Enter ?");
        m_sendModeBtn->setText(buttonText);
        const int minWidth =
            qMax(92, QFontMetrics(m_sendModeBtn->font()).horizontalAdvance(buttonText) + 24);
        m_sendModeBtn->setFixedWidth(minWidth);
        m_sendModeBtn->setToolTip(enterMode
                                      ? QStringLiteral("当前为 Enter 发送，点击切换")
                                      : QStringLiteral("当前为 Ctrl+Enter 发送，点击切换"));
    }
    if (m_inputEdit) {
        m_inputEdit->setPlaceholderText(
            enterMode
                ? QStringLiteral("输入消息，Enter 发送，Shift+Enter 换行")
                : QStringLiteral("输入消息，Ctrl+Enter 发送，Enter 换行"));
    }
}

void ConversationsPage::showConversationFilterPanel()
{
    if (!m_conversationFilterPanel || !m_conversationFilterToggleBtn) {
        return;
    }
    positionConversationFilterPanel();
    m_conversationFilterPanel->show();
    m_conversationFilterPanel->raise();
    m_conversationFilterPanel->activateWindow();
}

void ConversationsPage::hideConversationFilterPanel()
{
    if (m_conversationFilterPanel && m_conversationFilterPanel->isVisible()) {
        m_conversationFilterPanel->hide();
    }
    if (m_conversationFilterToggleBtn && m_conversationFilterToggleBtn->isChecked()) {
        QSignalBlocker blocker(m_conversationFilterToggleBtn);
        m_conversationFilterToggleBtn->setChecked(false);
    }
}

void ConversationsPage::positionConversationFilterPanel()
{
    if (!m_conversationFilterPanel || !m_conversationFilterToggleBtn) {
        return;
    }

    const QSize popupSize(260, 220);
    m_conversationFilterPanel->resize(popupSize);
    const QPoint anchor = m_conversationFilterToggleBtn->mapTo(this,
        QPoint(0, m_conversationFilterToggleBtn->height() + 6));
    const QRect available = rect().adjusted(8, 8, -8, -8);
    const int x = qBound(available.left(), anchor.x(),
                         available.right() - popupSize.width() - 8);
    const int y = qBound(available.top(), anchor.y(),
                         available.bottom() - popupSize.height() - 8);
    m_conversationFilterPanel->move(x, y);
}

bool ConversationsPage::isMessageViewportNearBottom() const
{
    if (!m_messageList) {
        return true;
    }
    QScrollBar* scrollBar = m_messageList->verticalScrollBar();
    if (!scrollBar) {
        return true;
    }
    return (scrollBar->maximum() - scrollBar->value()) <= 24;
}

void ConversationsPage::scheduleMessageViewportToBottom()
{
    if (!m_messageList || !m_messageModel) {
        if (m_messageList && !m_messageList->updatesEnabled()) {
            m_messageList->setUpdatesEnabled(true);
        }
        return;
    }
    const bool shouldFollow = m_followLatestMessages || !m_messageList->verticalScrollBar()
                              || isMessageViewportNearBottom();
    if (!shouldFollow) {
        if (m_messageList && !m_messageList->updatesEnabled()) {
            m_messageList->setUpdatesEnabled(true);
        }
        return;
    }

    // 防抖：多次调用只执行一次滚动
    if (!m_scrollToBottomTimer) {
        m_scrollToBottomTimer = new QTimer(this);
        m_scrollToBottomTimer->setSingleShot(true);
        m_scrollToBottomTimer->setInterval(16); // ~1 帧，合并同一事件循环的多次请求
        connect(m_scrollToBottomTimer, &QTimer::timeout, this, [this]() {
            if (!m_messageList || !m_messageModel
                || m_messageModel->rowCount() <= 0) {
                m_programmaticMessageScroll = false;
                if (m_messageList) m_messageList->setUpdatesEnabled(true);
                return;
            }
            m_messageList->scrollToBottom();
            if (QScrollBar* scrollBar = m_messageList->verticalScrollBar()) {
                scrollBar->setValue(scrollBar->maximum());
            }
            m_programmaticMessageScroll = false;
            m_followLatestMessages = true;
            if (m_messageList) m_messageList->setUpdatesEnabled(true);
        });
    }
    m_programmaticMessageScroll = true;
    m_scrollToBottomTimer->start(); // 重置防抖计时器
}

QString ConversationsPage::messageCopyTextForIndex(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return {};
    }

    const QString rawBody = index.data(MessageListModel::BodyRole).toString();
    const bool isFileMessage = index.data(MessageListModel::FileMessageRole).toBool();
    const QString attachmentName = index.data(MessageListModel::AttachmentNameRole).toString().trimmed();
    const QString localFilePath = index.data(MessageListModel::LocalFilePathRole).toString().trimmed();
    const QString body = Qt::mightBeRichText(rawBody)
        ? QTextDocumentFragment::fromHtml(rawBody).toPlainText().trimmed()
        : rawBody.trimmed();

    if (isFileMessage) {
        QStringList lines;
        const QString fileName = attachmentName.isEmpty() ? body.trimmed() : attachmentName;
        lines << QStringLiteral("[文件] %1").arg(fileName.isEmpty() ? QStringLiteral("未命名文件") : fileName);
        if (!localFilePath.isEmpty()) {
            lines << localFilePath;
        }
        return lines.join(QLatin1Char('\n')).trimmed();
    } else if (!body.trimmed().isEmpty()) {
        return body.trimmed();
    }

    return {};
}

bool ConversationsPage::copyMessageAtIndexToClipboard(const QModelIndex& index)
{
    if (!index.isValid()) {
        return false;
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        return false;
    }

    const bool isFileMessage = index.data(MessageListModel::FileMessageRole).toBool();
    const QString localFilePath = index.data(MessageListModel::LocalFilePathRole).toString().trimmed();
    if (isFileMessage && !localFilePath.isEmpty() && isLocalImageFilePath(localFilePath)) {
        const QImage image(localFilePath);
        if (!image.isNull()) {
            auto* mimeData = new QMimeData;
            mimeData->setImageData(image);
            mimeData->setUrls({QUrl::fromLocalFile(localFilePath)});
            clipboard->setMimeData(mimeData);
                    setStatusMessage(QStringLiteral("选中文本已复制"), 1500);
            return true;
        }
    }

    const QString text = messageCopyTextForIndex(index);
    if (text.isEmpty()) {
        return false;
    }

    clipboard->setText(text);
                    setStatusMessage(QStringLiteral("选中文本已复制"), 1500);
    return true;
}

bool ConversationsPage::handleMentionPopupKeyPress(QKeyEvent* keyEvent)
{
    if (!keyEvent || !m_mentionPopup || !m_mentionPopup->isVisible()) {
        return false;
    }

    const int candidateCount = m_mentionPopup->count();
    if (candidateCount <= 0) {
        m_mentionPopup->hide();
        return false;
    }

    if (keyEvent->key() == Qt::Key_Escape) {
        m_mentionPopup->hide();
        if (m_inputEdit) {
            m_inputEdit->setFocus();
        }
        return true;
    }

    if (keyEvent->key() == Qt::Key_Down) {
        const int next = (m_mentionPopup->currentRow() + 1 + candidateCount) % candidateCount;
        m_mentionPopup->setCurrentRow(next);
        return true;
    }

    if (keyEvent->key() == Qt::Key_Up) {
        const int prev = (m_mentionPopup->currentRow() - 1 + candidateCount) % candidateCount;
        m_mentionPopup->setCurrentRow(prev);
        return true;
    }

    if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
        if (QListWidgetItem* item = m_mentionPopup->currentItem()) {
            replaceCurrentMentionToken(item->text());
        }
        m_mentionPopup->hide();
        if (m_inputEdit) {
            m_inputEdit->setFocus();
        }
        return true;
    }

    return false;
}

QString ConversationsPage::currentMentionQuery() const
{
    if (!m_inputEdit) {
        return {};
    }

    const QString text = m_inputEdit->toPlainText();
    const int cursorPosition = m_inputEdit->textCursor().position();
    if (cursorPosition <= 0 || cursorPosition > text.size()) {
        return {};
    }

    for (int i = cursorPosition - 1; i >= 0; --i) {
        const QChar ch = text.at(i);
        if (ch.isSpace()) {
            return {};
        }
        if (ch == QChar('@') || ch == QChar(0xFF20)) {
            return text.mid(i + 1, cursorPosition - i - 1);
        }
    }

    return {};
}

QStringList ConversationsPage::filteredMentionCandidates(const QString& query) const
{
    if (m_currentGroupMembers.isEmpty()) {
        return {};
    }

    const QString normalizedQuery = query.trimmed();
    if (normalizedQuery.isEmpty()) {
        return m_currentGroupMembers;
    }

    QStringList candidates;
    for (const QString& member : m_currentGroupMembers) {
        if (member.contains(normalizedQuery, Qt::CaseInsensitive)) {
            candidates.append(member);
        }
    }
    return candidates;
}

bool ConversationsPage::replaceCurrentMentionToken(const QString& mentionText)
{
    if (!m_inputEdit || mentionText.trimmed().isEmpty()) {
        return false;
    }

    QTextCursor cursor = m_inputEdit->textCursor();
    const QString text = m_inputEdit->toPlainText();
    const int cursorPosition = cursor.position();
    if (cursorPosition <= 0 || cursorPosition > text.size()) {
        return false;
    }

    int mentionStart = -1;
    for (int i = cursorPosition - 1; i >= 0; --i) {
        const QChar ch = text.at(i);
        if (ch.isSpace()) {
            break;
        }
        if (ch == QChar('@') || ch == QChar(0xFF20)) {
            mentionStart = i;
            break;
        }
    }

    if (mentionStart < 0) {
        return false;
    }

    const QTextCharFormat baseFormat = m_inputEdit->currentCharFormat();
    QTextCharFormat mentionFormat = baseFormat;
    mentionFormat.setForeground(QColor(AppStyle::accent()));
    mentionFormat.setFontWeight(QFont::DemiBold);

    cursor.beginEditBlock();
    cursor.setPosition(mentionStart);
    cursor.setPosition(cursorPosition, QTextCursor::KeepAnchor);
    cursor.insertText(QStringLiteral("@") + mentionText.trimmed(), mentionFormat);
    cursor.insertText(QStringLiteral(" "), baseFormat);
    cursor.endEditBlock();
    m_inputEdit->setTextCursor(cursor);
    m_inputEdit->setCurrentCharFormat(baseFormat);
    return true;
}

void ConversationsPage::refreshMentionPopup()
{
    if (!m_mentionPopup || !m_inputEdit) {
        return;
    }

    const QString query = currentMentionQuery();
    if (query.isNull()) {
        m_mentionPopup->hide();
        return;
    }

    const QStringList candidates = filteredMentionCandidates(query);
    if (candidates.isEmpty()) {
        m_mentionPopup->hide();
        return;
    }

    const QString previousSelection =
        m_mentionPopup->currentItem() ? m_mentionPopup->currentItem()->text() : QString();
    m_mentionPopup->clear();
    for (const QString& candidate : candidates) {
        m_mentionPopup->addItem(candidate);
    }

    int selectedRow = 0;
    if (!previousSelection.isEmpty()) {
        const int previousIndex = candidates.indexOf(previousSelection);
        if (previousIndex >= 0) {
            selectedRow = previousIndex;
        }
    }

    const QRect cursorRect = m_inputEdit->cursorRect();
    const QPoint globalPos = m_inputEdit->mapToGlobal(cursorRect.bottomLeft());
    m_mentionPopup->adjustSize();
    m_mentionPopup->move(globalPos);
    m_mentionPopup->show();
    m_mentionPopup->setCurrentRow(selectedRow);
    m_inputEdit->setFocus();
}

int ConversationsPage::pendingAttachmentCount() const {
    return m_pendingAttachmentPaths.size();
}

void ConversationsPage::submitCurrentComposer() {
    if (!m_inputEdit) {
        return;
    }

    const QString html = m_inputEdit->toHtml().trimmed();
    const QString sanitizedHtml = stripRiskyColorStyles(stripInlineImageTags(html));
    QString normalizedPlain = m_inputEdit->toPlainText();
    normalizedPlain.remove(QChar::ObjectReplacementCharacter);
    const QString plain = normalizedPlain.trimmed();

    if (m_chatComposerWidget && m_chatComposerWidget->isInEditMode()) {
        if (!plain.isEmpty()) {
            const QString editingMessageId = m_chatComposerWidget->editingMessageId();
            m_chatComposerWidget->exitEditMode();
            m_inputEdit->clear();
            emit editSaveRequested(editingMessageId, sanitizedHtml);
        } else {
            m_chatComposerWidget->exitEditMode();
            setStatusMessage(QStringLiteral("请先选择联系人"), 2000);
        }
        return;
    }

    if (plain.isEmpty() && m_pendingAttachmentPaths.isEmpty()) {
        return;
    }

    // 清理已从输入框删除的截图附件
    if (!m_screenshotPreviewToPath.isEmpty() && m_inputEdit) {
        QSet<QString> liveImages;
        QTextBlock block = m_inputEdit->document()->begin();
        while (block.isValid()) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QTextFragment frag = it.fragment();
                if (frag.isValid() && frag.charFormat().isImageFormat()) {
                    liveImages.insert(frag.charFormat().toImageFormat().name());
                }
            }
            block = block.next();
        }
        for (auto it = m_screenshotPreviewToPath.begin(); it != m_screenshotPreviewToPath.end(); ) {
            if (!liveImages.contains(it.key())) {
                m_pendingAttachmentPaths.removeAll(it.value());
                it = m_screenshotPreviewToPath.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (plain.isEmpty() && m_pendingAttachmentPaths.isEmpty()) {
        return;
    }

    QStringList normalizedAttachments;
    normalizedAttachments.reserve(m_pendingAttachmentPaths.size());
    for (const QString& path : std::as_const(m_pendingAttachmentPaths)) {
        normalizedAttachments.append(path.trimmed());
    }
    const QString submitFingerprint = QStringLiteral("%1|%2|%3")
                                          .arg(plain,
                                               normalizedAttachments.join(QStringLiteral(";")),
                                               QString::number(normalizedAttachments.size()));
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!submitFingerprint.isEmpty()
        && m_activeComposerContextId == m_lastSubmitContextId
        && submitFingerprint == m_lastSubmitFingerprint
        && (nowMs - m_lastSubmitAtMs) <= 700) {
        return;
    }

    const bool allowAttachmentSend = m_attachmentDraftContextId.isEmpty()
                                     || m_attachmentDraftContextId == m_activeComposerContextId;
    const QStringList attachmentsToSend = allowAttachmentSend
        ? m_pendingAttachmentPaths
        : QStringList();
    const QString sanitizedPlain =
        QTextDocumentFragment::fromHtml(sanitizedHtml).toPlainText()
            .remove(QChar::ObjectReplacementCharacter)
            .trimmed();
    m_pendingAttachmentPaths.clear();
    m_screenshotPreviewToPath.clear();
    m_attachmentDraftContextId.clear();
    m_lastSubmitContextId = m_activeComposerContextId;
    m_lastSubmitFingerprint = submitFingerprint;
    m_lastSubmitAtMs = nowMs;
    m_composerDrafts.remove(m_activeComposerContextId);
    m_composerDraftHtml.remove(m_activeComposerContextId);
    m_draftAttachmentPaths.remove(m_activeComposerContextId);
    m_draftScreenshotMap.remove(m_activeComposerContextId);
    syncComposerDraftState();
    syncDraftsToModel();
    m_inputEdit->clear();
    m_inputEdit->setCurrentCharFormat(QTextCharFormat());

    for (const QString& path : attachmentsToSend) {
        if (!path.trimmed().isEmpty() && QFileInfo::exists(path)) {
            emit fileSendRequested(path);
        }
    }
    if (!sanitizedPlain.isEmpty()) {
        emit sendRequested(sanitizedHtml);
    }

    // 用户主动发送消息后，强制滚动到最新（无论当前是否在底部）
    m_followLatestMessages = true;
    scheduleMessageViewportToBottom();
}

// ======================================================================
// eventFilter — 从 MainWindow.cpp 迁移
// ======================================================================

bool ConversationsPage::eventFilter(QObject* watched, QEvent* event) {
    // 置顶卡片关闭按钮定位
    if (event && event->type() == QEvent::Resize) {
        auto* frame = qobject_cast<QFrame*>(watched);
        if (frame && frame->objectName() == QStringLiteral("pinnedCard")) {
            const auto children = frame->findChildren<QPushButton*>();
            for (auto* btn : children) {
                if (btn->property("_pinnedCardCloseBtn").toBool()) {
                    btn->move(frame->width() - 18, 2);
                }
            }
        }
        if (m_groupPanelToggleBtn && watched == m_groupPanelToggleBtn->parentWidget()) {
            positionGroupPanelToggleButton();
        }
    }
    const QString watchedObjectName =
        watched ? watched->objectName() : QString();
    const bool watchesGroupPanelHover =
        m_groupPanelToggleBtn
        && (watched == m_groupPanelToggleBtn
            || watched == m_groupPanelToggleBtn->parentWidget()
            || watched == m_messageStageFrame
            || watched == m_messageList
            || (m_messageList && watched == m_messageList->viewport())
            || watchedObjectName == QStringLiteral("chatWorkspaceBody")
            || watchedObjectName == QStringLiteral("chatWorkspacePrimaryColumn")
            || watchedObjectName == QStringLiteral("messageStageViewport"));
    if (watchesGroupPanelHover && event) {
        if (event->type() == QEvent::MouseMove) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            QWidget* sourceWidget = qobject_cast<QWidget*>(watched);
            QWidget* parentWidget = m_groupPanelToggleBtn->parentWidget();
            const QPoint globalPos = sourceWidget
                ? sourceWidget->mapToGlobal(mouseEvent->position().toPoint())
                : QCursor::pos();
            const QPoint parentPos = parentWidget
                ? parentWidget->mapFromGlobal(globalPos)
                : mouseEvent->position().toPoint();
            const int parentWidth = parentWidget ? parentWidget->width() : 0;
            const bool overToggleButton = m_groupPanelToggleBtn->rect().contains(
                m_groupPanelToggleBtn->mapFromGlobal(globalPos));
            const bool nearRightEdge = parentPos.x() >= parentWidth - 112;
            const bool shouldExpose = m_groupWorkspaceMode && !m_currentGroupId.trimmed().isEmpty();
            m_groupPanelToggleBtn->setVisible(
                shouldExpose && (m_groupPanelVisibleState || nearRightEdge || overToggleButton));
            if (m_groupPanelToggleBtn->isVisible()) {
                positionGroupPanelToggleButton();
            }
        } else if (event->type() == QEvent::Leave) {
            const bool shouldExpose = m_groupWorkspaceMode && !m_currentGroupId.trimmed().isEmpty();
            const bool overToggleButton = m_groupPanelToggleBtn->rect().contains(
                m_groupPanelToggleBtn->mapFromGlobal(QCursor::pos()));
            m_groupPanelToggleBtn->setVisible(
                shouldExpose && (m_groupPanelVisibleState || overToggleButton));
        } else if (event->type() == QEvent::Enter && watched == m_groupPanelToggleBtn) {
            const bool shouldExpose = m_groupWorkspaceMode && !m_currentGroupId.trimmed().isEmpty();
            m_groupPanelToggleBtn->setVisible(shouldExpose);
        }
    }
    if (watched == m_conversationFilterPanel && event) {
        if (event->type() == QEvent::Hide) {
            if (m_conversationFilterToggleBtn && m_conversationFilterToggleBtn->isChecked()) {
                QSignalBlocker blocker(m_conversationFilterToggleBtn);
                m_conversationFilterToggleBtn->setChecked(false);
            }
        } else if (event->type() == QEvent::Show) {
            positionConversationFilterPanel();
        }
    }
    if (watched == m_messageList && event && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->matches(QKeySequence::Copy)) {
            // Delegate 架构：检查 delegate 文本选择
            if (m_messageBubbleDelegate && m_messageBubbleDelegate->hasSelection()) {
                if (QClipboard* clipboard = QGuiApplication::clipboard()) {
                    clipboard->setText(m_messageBubbleDelegate->selectedText());
                    setStatusMessage(QStringLiteral("\u9009\u4E2D\u6587\u672C\u5DF2\u590D\u5236"), 1500);
                    return true;
                }
            }
            if (copyMessageAtIndexToClipboard(m_messageList->currentIndex())) {
                return true;
            }
        }
    }
    // 图片消息双击打开查看器（viewport 事件比 editorEvent 更可靠）
    if (m_messageList && watched == m_messageList->viewport() && event
        && event->type() == QEvent::MouseButtonDblClick) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            const QModelIndex idx = m_messageList->indexAt(me->pos());
            if (idx.isValid() && m_messageModel && m_messageBubbleDelegate
                && m_messageBubbleDelegate->isPointInPureImageBubble(idx, me->pos())) {
                const bool isFile = idx.data(MessageListModel::FileMessageRole).toBool();
                const QString lPath = idx.data(MessageListModel::LocalFilePathRole).toString();
                if (isFile && !lPath.isEmpty() && QFileInfo::exists(lPath)) {
                    static const QStringList imgExts = {
                        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                        QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("webp"),
                        QStringLiteral("ico"), QStringLiteral("svg")
                    };
                    if (imgExts.contains(QFileInfo(lPath).suffix().toLower())) {
                        const QString aName = idx.data(MessageListModel::AttachmentNameRole).toString();
                        auto* viewer = new ImageViewerWidget(lPath, aName, window());
                        viewer->show();
                        return true;
                    }
                }
            }
        }
    }

    if (watched == m_inputEdit || watched == m_mentionPopup) {
        if (watched == m_inputEdit && event->type() == QEvent::InputMethod && !m_currentGroupMembers.isEmpty()) {
            const auto* imeEvent = static_cast<QInputMethodEvent*>(event);
            const QString commitText = imeEvent->commitString();
            if (commitText.contains(QChar('@')) || commitText.contains(QChar(0xFF20))) {
                QTimer::singleShot(0, this, [this]() { refreshMentionPopup(); });
            }
        }
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (m_mentionPopup && m_mentionPopup->isVisible() && handleMentionPopupKeyPress(ke)) {
                return true;
            }
            if (watched == m_mentionPopup && m_inputEdit) {
                const bool shouldForwardToEditor =
                    ke->key() == Qt::Key_Backspace
                    || ke->key() == Qt::Key_Delete
                    || ke->key() == Qt::Key_Left
                    || ke->key() == Qt::Key_Right
                    || ke->key() == Qt::Key_Home
                    || ke->key() == Qt::Key_End
                    || !ke->text().isEmpty();
                if (shouldForwardToEditor) {
                    m_inputEdit->setFocus();
                    QKeyEvent forwardedEvent(ke->type(),
                                             ke->key(),
                                             ke->modifiers(),
                                             ke->text(),
                                             ke->isAutoRepeat(),
                                             ke->count());
                    QCoreApplication::sendEvent(m_inputEdit, &forwardedEvent);
                    return true;
                }
            }
            if (ke->matches(QKeySequence::Paste)) {
                const QMimeData* mimeData = QGuiApplication::clipboard()
                                                ? QGuiApplication::clipboard()->mimeData()
                                                : nullptr;
                bool handledPaste = false;
                if (mimeData) {
                    if (mimeData->hasImage()) {
                        const QImage clipboardImage =
                            qvariant_cast<QImage>(mimeData->imageData());
                        if (importScreenshotPreview(clipboardImage)) {
                            handledPaste = true;
                        }
                    }

                    // 从 QTextEdit 复制内嵌图片时，剪贴板只有 HTML（含 base64 data URI），
                    // hasImage() 为 false。需要解析 HTML 提取图片。
                    if (!handledPaste && mimeData->hasHtml()) {
                        const QString pastedHtml = mimeData->html();
                        static const QRegularExpression base64ImgRx(
                            QStringLiteral(R"___(<img[^>]+src\s*=\s*"(data:image/[^;]+;base64,([^"]+))")___"),
                            QRegularExpression::CaseInsensitiveOption);
                        auto it = base64ImgRx.globalMatch(pastedHtml);
                        while (it.hasNext()) {
                            const auto m = it.next();
                            const QByteArray imageData = QByteArray::fromBase64(m.captured(2).toLatin1());
                            QImage image;
                            if (image.loadFromData(imageData) && importScreenshotPreview(image)) {
                                handledPaste = true;
                            }
                        }
                    }

                    if (!handledPaste && mimeData->hasUrls()) {
                        int importedCount = 0;
                        QStringList filesToSend;
                        for (const QUrl& url : mimeData->urls()) {
                            if (!url.isLocalFile()) {
                                continue;
                            }
                            const QString localPath = QFileInfo(url.toLocalFile()).absoluteFilePath();
                            if (localPath.isEmpty() || !QFileInfo::exists(localPath)) {
                                continue;
                            }
                            if (isLocalImageFilePath(localPath)) {
                                QImage image(localPath);
                                if (importScreenshotPreview(image)) {
                                    ++importedCount;
                                }
                            } else {
                                filesToSend.append(localPath);
                                ++importedCount;
                            }
                        }
                        if (!filesToSend.isEmpty()) {
                            QTimer::singleShot(0, this, [this, filesToSend]() {
                                for (const QString& fp : filesToSend) {
                                    emit fileSendRequested(fp);
                                }
                            });
                        }
                        if (importedCount > 0) {
                            handledPaste = true;
                        }
                    }

                    if (!handledPaste && mimeData->hasText()) {
                        const QString pastedText = mimeData->text().trimmed();
                        QFileInfo pastedFile(pastedText);
                        if (pastedFile.exists() && pastedFile.isFile()) {
                            const QString localPath = pastedFile.absoluteFilePath();
                            if (isLocalImageFilePath(localPath)) {
                                QImage image(localPath);
                                handledPaste = importScreenshotPreview(image);
                            } else {
                                QTimer::singleShot(0, this, [this, localPath]() {
                                    emit fileSendRequested(localPath);
                                });
                                handledPaste = true;
                            }
                        }
                    }
                }
                if (handledPaste) {
                    return true;
                }
                // 非图片/文件粘贴：强制纯文本，避免从外部复制带入背景色等富文本样式
                if (mimeData && mimeData->hasText()) {
                    m_inputEdit->insertPlainText(mimeData->text());
                    return true;
                }
            }
            // @ 键：在群聊模式（有成员列表）时弹出提及选单
            if (watched == m_inputEdit
                && isAsciiOrFullwidthAt(ke->text())
                && !m_currentGroupMembers.isEmpty()) {
                m_inputEdit->insertPlainText(QStringLiteral("@"));
                refreshMentionPopup();
                return true;
            }
            if (m_mentionPopup && m_mentionPopup->isVisible()) {
                const bool mayChangeMentionQuery =
                    ke->key() == Qt::Key_Backspace
                    || ke->key() == Qt::Key_Delete
                    || !ke->text().isEmpty();
                if (mayChangeMentionQuery && watched == m_inputEdit) {
                    QTimer::singleShot(0, this, [this]() { refreshMentionPopup(); });
                }
            }
            if (watched == m_inputEdit && ke->key() == Qt::Key_Escape) {
                if (m_chatComposerWidget && m_chatComposerWidget->isInEditMode()) {
                    m_chatComposerWidget->exitEditMode();
                    return true;
                }
                // Escape 也可清除回复上下文
                if (m_chatComposerWidget && !m_chatComposerWidget->replyToMessageId().isEmpty()) {
                    m_chatComposerWidget->clearReplyContext();
                    return true;
                }
            }
            // 输入框为空时按退格键清除回复上下文
            if (watched == m_inputEdit && ke->key() == Qt::Key_Backspace
                && m_chatComposerWidget && !m_chatComposerWidget->replyToMessageId().isEmpty()
                && m_inputEdit->toPlainText().trimmed().isEmpty()
                && m_pendingAttachmentPaths.isEmpty()) {
                m_chatComposerWidget->clearReplyContext();
                return true;
            }
            if (watched == m_inputEdit && (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)) {
                const bool shiftHeld = ke->modifiers() & Qt::ShiftModifier;
                const bool ctrlHeld  = ke->modifiers() & Qt::ControlModifier;
                QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
                const bool enterMode = cfg.value(QStringLiteral("sendMode"),
                                                 QStringLiteral("enter")).toString()
                                     == QStringLiteral("enter");
                if (enterMode && !shiftHeld && !ctrlHeld) {
                    submitCurrentComposer();
                    return true;
                }
                if (!enterMode && ctrlHeld && !shiftHeld) {
                    submitCurrentComposer();
                    return true;
                }
                if (enterMode && shiftHeld) {
                    return false;
                }
            }
        }
    }
    if (watched == m_messageList || watched == m_inputEdit
        || (m_inputEdit && watched == m_inputEdit->viewport())
        || (m_messageList && watched == m_messageList->viewport())) {
        if (event->type() == QEvent::DragEnter) {
            auto* de = static_cast<QDragEnterEvent*>(event);
            qInfo() << "[drag-drop] ConversationsPage DragEnter on" << watched->objectName()
                    << "hasUrls:" << de->mimeData()->hasUrls()
                    << "formats:" << de->mimeData()->formats();
            if (de->mimeData()->hasUrls()) {
                de->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::DragMove) {
            auto* de = static_cast<QDragMoveEvent*>(event);
            if (de->mimeData()->hasUrls()) {
                de->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto* de = static_cast<QDropEvent*>(event);
            const QList<QUrl> urls = de->mimeData()->urls();
            qInfo() << "[file-drop] urls=" << urls << "watched=" << watched;
            QStringList filesToSend;
            for (const QUrl& url : urls) {
                if (url.isLocalFile()) {
                    filesToSend.append(url.toLocalFile());
                }
            }
            if (!filesToSend.isEmpty()) {
                QTimer::singleShot(0, this, [this, filesToSend]() {
                    for (const QString& fp : filesToSend) {
                        emit fileSendRequested(fp);
                    }
                });
            }
            de->acceptProposedAction();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
