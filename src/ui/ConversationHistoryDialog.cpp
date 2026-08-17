#include "ui/ConversationHistoryDialog.h"
#include "ui/ConversationHistoryCalendarWidget.h"
#include "ui/AppStyle.h"

#include <ElaComboBox.h>
#include <ElaLineEdit.h>
#include <ElaListWidget.h>
#include <ElaPivot.h>
#include <ElaPushButton.h>
#include <ElaMenu.h>
#include <ElaScrollBar.h>
#include <ElaText.h>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <ElaFrame.h>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QSet>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <unordered_set>

// 鈹€鈹€ helpers 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

static QString wToQ(const std::wstring& ws) {
    return QString::fromStdWString(ws);
}

static bool containsLink(const QString& body) {
    return body.contains(QStringLiteral("http://"), Qt::CaseInsensitive)
        || body.contains(QStringLiteral("https://"), Qt::CaseInsensitive);
}

static QString compactText(QString text)
{
    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return text.simplified();
}

static bool looksLikeHtmlMessage(const QString& text)
{
    const QString lower = text.left(512).toLower();
    return lower.contains(QStringLiteral("<!doctype"))
        || lower.contains(QStringLiteral("<html"))
        || lower.contains(QStringLiteral("<body"))
        || lower.contains(QStringLiteral("</p>"))
        || lower.contains(QStringLiteral("<br"));
}

static QString visibleMessageText(const QString& body)
{
    const QString compact = compactText(body);
    if (!looksLikeHtmlMessage(compact)) {
        return compact;
    }

    QTextDocument doc;
    doc.setHtml(body);
    return compactText(doc.toPlainText());
}

static bool containsMeaningfulLink(const QString& body)
{
    const QString visibleText = visibleMessageText(body);
    if (containsLink(visibleText)) {
        return true;
    }

    if (looksLikeHtmlMessage(body)) {
        return false;
    }

    return containsLink(body);
}

static QJsonObject parseJsonObject(const std::wstring& json)
{
    const QByteArray bytes = QString::fromStdWString(json).toUtf8();
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    return doc.object();
}

static QString firstJsonString(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const QString value = object.value(QLatin1String(key)).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

// 鈹€鈹€ constructor 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

ConversationHistoryDialog::ConversationHistoryDialog(
    const QString& conversationTitle,
    const std::vector<ChatMessage>& records,
    const QString& localDisplayName,
    const QHash<QString, QString>& senderDisplayNames,
    QWidget* parent)
    : ElaDialog(parent)
    , m_records(records)
    , m_localDisplayName(localDisplayName)
    , m_senderDisplayNames(senderDisplayNames)
    , m_conversationTitle(conversationTitle)
{
    setWindowTitle(QStringLiteral("%1 - \u804a\u5929\u8bb0\u5f55").arg(conversationTitle));
    resize(1100, 720);
    setMinimumSize(960, 640);

    // 鈹€鈹€ 椤堕儴锛氭悳绱㈡爮 + 绫诲瀷绛涢€?鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    auto* searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(0, 0, 0, 0);

    m_searchEdit = new ElaLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("\u641c\u7d22\u6d88\u606f\u5185\u5bb9\u6216\u9644\u4ef6\u540d..."));
    m_searchEdit->setFixedHeight(34);
    m_searchEdit->setIsClearButtonEnable(true);
    searchRow->addWidget(m_searchEdit, 1);

    auto* exportBtn = new ElaPushButton(QStringLiteral("\u5bfc\u51fa"), this);
    exportBtn->setFixedSize(64, 34);
    connect(exportBtn, &ElaPushButton::clicked, this, &ConversationHistoryDialog::onExportClicked);
    searchRow->addWidget(exportBtn);

    // 鈹€鈹€ 绫诲瀷 Pivot: 鍏ㄩ儴 / 鏂囨湰 / 鏂囨。 / 鍥剧墖 / 閾炬帴 / 鍏朵粬
    m_typePivot = new ElaPivot(this);
    m_typePivot->appendPivot(QStringLiteral("\u5168\u90e8"));
    m_typePivot->appendPivot(QStringLiteral("\u6587\u672c"));
    m_typePivot->appendPivot(QStringLiteral("\u6587\u6863"));
    m_typePivot->appendPivot(QStringLiteral("\u56fe\u7247"));
    m_typePivot->appendPivot(QStringLiteral("\u94fe\u63a5"));
    m_typePivot->appendPivot(QStringLiteral("\u5176\u4ed6"));
    m_typePivot->setCurrentIndex(0);
    m_typePivot->setFixedHeight(32);

    // 鈹€鈹€ 宸︿晶缁撴灉鍒楄〃 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    m_resultList = new ElaListWidget(this);
    m_resultList->setObjectName(QStringLiteral("conversationHistoryResultList"));
    m_resultList->setFrameShape(QFrame::NoFrame);
    m_resultList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    new ElaScrollBar(m_resultList->verticalScrollBar(), m_resultList);
    m_resultList->setStyleSheet(QStringLiteral(
        "QListWidget { background:%1; border:none; color:%3; outline:0; }"
        "QListWidget::item { padding:0; border-bottom:1px solid %2; color:%3; }"
        "QListWidget::item:selected { background:%4; color:%3; }"
        "QListWidget::item:selected:active { color:%3; }"
        "QListWidget::item:selected:!active { color:%3; }"
        "QListWidget::item:hover { background:%5; color:%3; }")
            .arg(AppStyle::surface(),
                 AppStyle::border(),
                 AppStyle::textPrimary(),
                 AppStyle::selectedBg(),
                 AppStyle::hoverBg()));

    auto* leftPanel = new QVBoxLayout;
    leftPanel->setContentsMargins(0, 0, 0, 0);
    leftPanel->setSpacing(8);
    leftPanel->addLayout(searchRow);
    leftPanel->addWidget(m_typePivot);
    leftPanel->addWidget(m_resultList, 1);

    // 鈹€鈹€ 鍙充晶闈㈡澘锛氭垚鍛?+ 鏃ュ巻 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    auto* rightPanel = new QVBoxLayout;
    rightPanel->setContentsMargins(0, 0, 0, 0);
    rightPanel->setSpacing(12);

    // 鎴愬憳閫夋嫨鍣?
    auto* memberLabel = new ElaText(QStringLiteral("\u53d1\u9001\u4eba"), this);
    memberLabel->setStyleSheet(QStringLiteral("font-size:13px; font-weight:600; color:%1;")
                                   .arg(AppStyle::textPrimary()));
    m_memberCombo = new ElaComboBox(this);
    m_memberCombo->setFixedHeight(32);
    m_memberCombo->setStyleSheet(QStringLiteral(
        "QComboBox { background:%1; border:1px solid %2; border-radius:6px; padding:4px 8px; color:%3; }"
        "QComboBox:hover { border-color:%4; }"
        "QComboBox::drop-down { border:none; }"
        "QComboBox QAbstractItemView { background:%1; border:1px solid %2; selection-background-color:%5; }")
            .arg(AppStyle::surface(),
                 AppStyle::border(),
                 AppStyle::textPrimary(),
                 AppStyle::accent(),
                 AppStyle::selectedBg()));
    populateMemberCombo();

    // 鏃ュ巻
    auto* dateLabel = new ElaText(QStringLiteral("\u65e5\u671f"), this);
    dateLabel->setStyleSheet(QStringLiteral("font-size:13px; font-weight:600; color:%1;")
                                 .arg(AppStyle::textPrimary()));
    m_calendar = new ConversationHistoryCalendarWidget(this);
    m_calendar->setActiveDates(collectActiveDates());
    m_calendar->setStyleSheet(QStringLiteral("background:%1; border:1px solid %2; border-radius:8px;")
                                  .arg(AppStyle::surface(), AppStyle::border()));

    rightPanel->addWidget(memberLabel);
    rightPanel->addWidget(m_memberCombo);
    rightPanel->addSpacing(4);
    rightPanel->addWidget(dateLabel);
    rightPanel->addWidget(m_calendar);
    rightPanel->addStretch();

    auto* rightFrame = new ElaFrame(this);
    rightFrame->setFixedWidth(280);
    rightFrame->setLayout(rightPanel);

    // 鈹€鈹€ 涓绘按骞冲竷灞€ 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(16);
    contentRow->addLayout(leftPanel, 1);
    contentRow->addWidget(rightFrame);

    auto* mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(12);
    mainLayout->addLayout(contentRow, 1);

    // ElaDialog 闇€瑕侀€氳繃 centralWidget 璁剧疆甯冨眬
    auto* centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    // ElaDialog 浣跨敤 QVBoxLayout锛屽唴瀹瑰～鍏呭湪 appBar 涓嬫柟
    auto* dialogLayout = qobject_cast<QVBoxLayout*>(layout());
    if (dialogLayout) {
        dialogLayout->addWidget(centralWidget);
    } else {
        auto* fallbackLayout = new QVBoxLayout(this);
        fallbackLayout->setContentsMargins(0, 0, 0, 0);
        fallbackLayout->addWidget(centralWidget);
    }

    // 鈹€鈹€ 淇″彿杩炴帴 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    connect(m_searchEdit, &ElaLineEdit::textChanged, this, [this]() {
        applyFilters();
    });
    connect(m_typePivot, &ElaPivot::pivotClicked, this, &ConversationHistoryDialog::onPivotClicked);
    connect(m_memberCombo, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, [this]() {
        applyFilters();
    });
    connect(m_calendar, &ConversationHistoryCalendarWidget::dateSelected,
            this, &ConversationHistoryDialog::onDateSelected);
    connect(m_resultList, &QListWidget::itemActivated,
            this, &ConversationHistoryDialog::onResultActivated);
    connect(m_resultList, &QListWidget::itemDoubleClicked,
            this, &ConversationHistoryDialog::onResultActivated);

    // 鍒濆鏄剧ず鎵€鏈夎褰?
    applyFilters();
}

// 鈹€鈹€ 妲?/ 杩囨护 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

void ConversationHistoryDialog::onPivotClicked(int index)
{
    m_currentTypeFilter = index;
    applyFilters();
}

void ConversationHistoryDialog::onDateSelected(const QDate& date)
{
    m_selectedDate = date;
    applyFilters();
}

void ConversationHistoryDialog::onResultActivated(QListWidgetItem* item)
{
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
        return;
    }

    const QString messageId = item->data(Qt::UserRole).toString().trimmed();
    const QString conversationId = item->data(Qt::UserRole + 1).toString().trimmed();
    if (messageId.isEmpty() || conversationId.isEmpty()) {
        return;
    }

    emit messageJumpRequested(conversationId, messageId);
    close();
}

void ConversationHistoryDialog::applyFilters()
{
    const QString keyword = m_searchEdit->text().trimmed();
    const QString selectedMember = m_memberCombo->currentData().toString();

    std::vector<const ChatMessage*> filtered;
    filtered.reserve(m_records.size());

    for (const auto& msg : m_records) {
        // 绫诲瀷绛涢€?
        if (m_currentTypeFilter != 0 && !matchesTypeFilter(msg, m_currentTypeFilter))
            continue;

        // 鎴愬憳绛涢€?
        if (!selectedMember.isEmpty() && wToQ(msg.senderId) != selectedMember)
            continue;

        // 鏃ユ湡绛涢€?
        if (m_selectedDate.isValid()) {
            const QDate msgDate = QDateTime::fromMSecsSinceEpoch(msg.createdAtMs).date();
            if (msgDate != m_selectedDate)
                continue;
        }

        // 鍏抽敭璇嶇瓫閫?
        if (!keyword.isEmpty() && !messageSearchText(msg).contains(keyword, Qt::CaseInsensitive))
            continue;

        filtered.push_back(&msg);
    }

    m_filteredResults = filtered;
    populateResults(filtered);
}

bool ConversationHistoryDialog::matchesTypeFilter(const ChatMessage& msg, int filterIndex) const
{
    const QString type = wToQ(msg.messageType);

    switch (filterIndex) {
    case 1: // 鏂囨湰
        return type == QStringLiteral("text") && !isLinkMessage(msg);
    case 2: // 鏂囨。
        return isDocumentMessage(msg);
    case 3: // 鍥剧墖
        return isImageMessage(msg);
    case 4: // 閾炬帴
        return isLinkMessage(msg);
    case 5: // 鍏朵粬
        return !isLinkMessage(msg)
            && !isImageMessage(msg)
            && !isDocumentMessage(msg)
            && type != QStringLiteral("text");
    default:
        return true;
    }
}

bool ConversationHistoryDialog::isImageExtension(const QString& name) const
{
    static const QStringList exts = {
        QStringLiteral(".png"), QStringLiteral(".jpg"), QStringLiteral(".jpeg"),
        QStringLiteral(".gif"), QStringLiteral(".bmp"), QStringLiteral(".webp"),
        QStringLiteral(".svg"), QStringLiteral(".ico")
    };
    const QString lower = name.toLower();
    for (const auto& ext : exts) {
        if (lower.endsWith(ext))
            return true;
    }
    return false;
}

// 鈹€鈹€ 缁撴灉鍒楄〃濉厖 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

bool ConversationHistoryDialog::isImageMessage(const ChatMessage& msg) const
{
    const QString type = wToQ(msg.messageType);
    if (type == QStringLiteral("image")) {
        return true;
    }

    const QString name = attachmentDisplayName(msg);
    if (isImageExtension(name)) {
        return true;
    }

    const QString localPath = wToQ(msg.localFilePath).trimmed();
    return !localPath.isEmpty() && !QImageReader(localPath).format().isEmpty();
}

bool ConversationHistoryDialog::isDocumentMessage(const ChatMessage& msg) const
{
    const QString type = wToQ(msg.messageType);
    const bool fileLike = type == QStringLiteral("file")
        || type == QStringLiteral("file_attachment")
        || !wToQ(msg.attachmentName).trimmed().isEmpty()
        || !wToQ(msg.localFilePath).trimmed().isEmpty()
        || !wToQ(msg.fileCardJson).trimmed().isEmpty();
    return fileLike && !isImageMessage(msg);
}

bool ConversationHistoryDialog::isLinkMessage(const ChatMessage& msg) const
{
    if (containsMeaningfulLink(wToQ(msg.body))) {
        return true;
    }

    const QJsonObject payload = parseJsonObject(msg.payloadJson);
    return containsLink(firstJsonString(payload, {"url", "href", "link", "target", "path"}));
}

QString ConversationHistoryDialog::attachmentDisplayName(const ChatMessage& msg) const
{
    const QString attachment = wToQ(msg.attachmentName).trimmed();
    if (!attachment.isEmpty()) {
        return attachment;
    }

    const QJsonObject fileCard = parseJsonObject(msg.fileCardJson);
    const QString cardName = firstJsonString(fileCard, {"file_name", "name", "title"});
    if (!cardName.isEmpty()) {
        return cardName;
    }

    const QString localPath = wToQ(msg.localFilePath).trimmed();
    if (!localPath.isEmpty()) {
        return QFileInfo(localPath).fileName();
    }

    const QJsonObject payload = parseJsonObject(msg.payloadJson);
    return firstJsonString(payload, {"file_name", "name", "title"});
}

QString ConversationHistoryDialog::messagePreviewText(const ChatMessage& msg) const
{
    const QString type = wToQ(msg.messageType);
    const QString body = visibleMessageText(wToQ(msg.body));
    const QString attachment = attachmentDisplayName(msg);

    QString prefix;
    if (isImageMessage(msg)) {
        prefix = QStringLiteral("[\u56fe\u7247] ");
    } else if (isDocumentMessage(msg)) {
        prefix = QStringLiteral("[\u6587\u4ef6] ");
    } else if (type == QStringLiteral("sticker")) {
        prefix = QStringLiteral("[\u8d34\u7eb8] ");
    } else if (isLinkMessage(msg)) {
        prefix = QStringLiteral("[\u94fe\u63a5] ");
    } else if (type == QStringLiteral("resource_ref") || type == QStringLiteral("shared_file")) {
        prefix = QStringLiteral("[\u5f15\u7528] ");
    }

    QString detail = body;
    if (detail.isEmpty()) {
        detail = attachment;
    } else if (!attachment.isEmpty() && !detail.contains(attachment, Qt::CaseInsensitive)) {
        detail = QStringLiteral("%1 - %2").arg(attachment, detail);
    }

    if (detail.isEmpty()) {
        const QJsonObject payload = parseJsonObject(msg.payloadJson);
        detail = firstJsonString(payload, {"title", "subtitle", "summary", "text", "name"});
    }

    if (detail.isEmpty()) {
        detail = looksLikeHtmlMessage(wToQ(msg.body))
            ? QStringLiteral("\u5bcc\u6587\u672c\u6d88\u606f")
            : QStringLiteral("\u6d88\u606f\u5185\u5bb9\u6682\u4e0d\u53ef\u9884\u89c8");
    }

    detail = compactText(detail);
    if (detail.length() > 180) {
        detail = detail.left(180) + QStringLiteral("...");
    }
    return prefix + detail;
}

QString ConversationHistoryDialog::messageSearchText(const ChatMessage& msg) const
{
    const QJsonObject payload = parseJsonObject(msg.payloadJson);
    const QJsonObject fileCard = parseJsonObject(msg.fileCardJson);
    const QStringList parts = {
        senderDisplayName(msg),
        wToQ(msg.senderId),
        visibleMessageText(wToQ(msg.body)),
        wToQ(msg.attachmentName),
        wToQ(msg.localFilePath),
        attachmentDisplayName(msg),
        messagePreviewText(msg),
        wToQ(msg.messageType),
        firstJsonString(payload, {"title", "subtitle", "summary", "text", "name", "url", "href", "link", "target", "path"}),
        firstJsonString(fileCard, {"file_name", "name", "title"})
    };
    return parts.join(QLatin1Char(' '));
}

QWidget* ConversationHistoryDialog::createResultRowWidget(const ChatMessage& msg) const
{
    auto* row = new QFrame;
    row->setObjectName(QStringLiteral("historyResultRow"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setStyleSheet(QStringLiteral(
        "QFrame#historyResultRow { background:transparent; }"
        "QLabel { background:transparent; }"));

    auto* root = new QVBoxLayout(row);
    root->setContentsMargins(12, 8, 12, 8);
    root->setSpacing(5);

    auto* metaRow = new QHBoxLayout;
    metaRow->setContentsMargins(0, 0, 0, 0);
    metaRow->setSpacing(8);

    auto* senderLabel = new QLabel(senderDisplayName(msg), row);
    QFont senderFont = senderLabel->font();
    senderFont.setBold(true);
    senderLabel->setFont(senderFont);
    senderLabel->setStyleSheet(QStringLiteral("color:%1;").arg(AppStyle::textPrimary()));

    const QString timeStr = QDateTime::fromMSecsSinceEpoch(msg.createdAtMs)
                                .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    auto* timeLabel = new QLabel(timeStr, row);
    timeLabel->setStyleSheet(QStringLiteral("color:%1;").arg(AppStyle::textMuted()));

    metaRow->addWidget(senderLabel, 0, Qt::AlignVCenter);
    metaRow->addWidget(timeLabel, 0, Qt::AlignVCenter);
    metaRow->addStretch(1);
    root->addLayout(metaRow);

    auto* previewRow = new QHBoxLayout;
    previewRow->setContentsMargins(0, 0, 0, 0);
    previewRow->setSpacing(8);

    const QString localPath = wToQ(msg.localFilePath).trimmed();
    if (isImageMessage(msg) && !localPath.isEmpty()) {
        QPixmap pixmap(localPath);
        if (!pixmap.isNull()) {
            auto* thumb = new QLabel(row);
            thumb->setFixedSize(52, 38);
            thumb->setPixmap(pixmap.scaled(thumb->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            thumb->setAlignment(Qt::AlignCenter);
            thumb->setStyleSheet(QStringLiteral("border:1px solid %1; border-radius:4px;").arg(AppStyle::border()));
            previewRow->addWidget(thumb, 0, Qt::AlignTop);
        }
    }

    auto* previewLabel = new QLabel(row);
    previewLabel->setStyleSheet(QStringLiteral("color:%1;").arg(AppStyle::textSecondary()));
    previewLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    previewLabel->setWordWrap(false);
    previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    previewLabel->setText(QFontMetrics(previewLabel->font()).elidedText(messagePreviewText(msg), Qt::ElideRight, 720));
    previewRow->addWidget(previewLabel, 1, Qt::AlignVCenter);
    root->addLayout(previewRow);

    return row;
}

void ConversationHistoryDialog::populateResults(const std::vector<const ChatMessage*>& filtered)
{
    m_resultList->clear();

    const QString mutedColor = AppStyle::textMuted();

    for (const auto* msg : filtered) {
        auto* item = new QListWidgetItem(m_resultList);
        item->setData(Qt::UserRole, wToQ(msg->messageId));
        item->setData(Qt::UserRole + 1, wToQ(msg->conversationId));
        item->setSizeHint(QSize(0, isImageMessage(*msg) ? 76 : 62));
        m_resultList->setItemWidget(item, createResultRowWidget(*msg));
    }

    if (filtered.empty()) {
        auto* emptyItem = new QListWidgetItem(QStringLiteral("\u6ca1\u6709\u627e\u5230\u5339\u914d\u7684\u8bb0\u5f55"), m_resultList);
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setForeground(QColor(mutedColor));
        emptyItem->setTextAlignment(Qt::AlignCenter);
        emptyItem->setSizeHint(QSize(0, 52));
    }
}
void ConversationHistoryDialog::populateMemberCombo()
{
    m_memberCombo->clear();
    m_memberCombo->addItem(QStringLiteral("\u5168\u90e8\u6210\u5458"), QString());

    std::unordered_set<std::wstring> seen;
    for (const auto& msg : m_records) {
        if (seen.count(msg.senderId)) continue;
        seen.insert(msg.senderId);
        const QString id = wToQ(msg.senderId);
        const QString name = senderDisplayName(msg);
        m_memberCombo->addItem(name, id);
    }
}

// 鈹€鈹€ 鏀堕泦鏈夋秷鎭殑鏃ユ湡 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

QSet<QDate> ConversationHistoryDialog::collectActiveDates() const
{
    QSet<QDate> dates;
    for (const auto& msg : m_records) {
        dates.insert(QDateTime::fromMSecsSinceEpoch(msg.createdAtMs).date());
    }
    return dates;
}

// 鈹€鈹€ 鍙戦€佷汉鍚嶇О 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

QString ConversationHistoryDialog::senderDisplayName(const ChatMessage& msg) const
{
    const QString id = wToQ(msg.senderId).trimmed();
    if (id.isEmpty()) return QStringLiteral("\u672a\u77e5");
    const QString mappedName = m_senderDisplayNames.value(id).trimmed();
    if (!mappedName.isEmpty()) {
        return mappedName;
    }
    return id.section(QLatin1Char('/'), -1);
}

// ── 导出 ────────────────────────────────────────────────────────────────

void ConversationHistoryDialog::onExportClicked()
{
    ElaMenu menu(this);
    menu.addAction(QStringLiteral("导出为 TXT"), this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("导出聊天记录"),
            QStringLiteral("%1.txt").arg(m_conversationTitle),
            QStringLiteral("文本文件 (*.txt)"));
        if (!path.isEmpty()) exportToTxt(path);
    });
    menu.addAction(QStringLiteral("导出为 HTML"), this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("导出聊天记录"),
            QStringLiteral("%1.html").arg(m_conversationTitle),
            QStringLiteral("HTML 文件 (*.html)"));
        if (!path.isEmpty()) exportToHtml(path);
    });
    menu.exec(QCursor::pos());
}

void ConversationHistoryDialog::exportToTxt(const QString& filePath) const
{
    // 预先收集数据（在主线程，数据量小仅为字符串拷贝），然后在工作线程写文件
    struct ExportEntry { QString time; QString sender; QString text; };
    auto entries = std::make_shared<std::vector<ExportEntry>>();
    entries->reserve(m_filteredResults.size());
    for (const auto* msg : m_filteredResults) {
        ExportEntry e;
        e.time = QDateTime::fromMSecsSinceEpoch(msg->createdAtMs)
                     .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        e.sender = senderDisplayName(*msg);
        e.text = visibleMessageText(wToQ(msg->body));
        entries->push_back(std::move(e));
    }

    auto* watcher = new QFutureWatcher<bool>(const_cast<ConversationHistoryDialog*>(this));
    QObject::connect(watcher, &QFutureWatcher<bool>::finished, this, [watcher, filePath, this]() {
        watcher->deleteLater();
        if (watcher->result()) {
            QMessageBox::information(const_cast<ConversationHistoryDialog*>(this),
                QStringLiteral("导出完成"),
                QStringLiteral("聊天记录已导出到:\n%1").arg(filePath));
        } else {
            QMessageBox::warning(const_cast<ConversationHistoryDialog*>(this),
                QStringLiteral("导出失败"),
                QStringLiteral("无法写入文件:\n%1").arg(filePath));
        }
    });
    watcher->setFuture(QtConcurrent::run([filePath, entries]() -> bool {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        for (const auto& e : *entries) {
            out << QStringLiteral("[%1] %2: %3\n").arg(e.time, e.sender, e.text);
        }
        return true;
    }));
}

void ConversationHistoryDialog::exportToHtml(const QString& filePath) const
{
    // 预先收集数据（在主线程），然后在工作线程写文件
    struct ExportEntry { QString time; QString sender; QString text; };
    auto entries = std::make_shared<std::vector<ExportEntry>>();
    entries->reserve(m_filteredResults.size());
    for (const auto* msg : m_filteredResults) {
        ExportEntry e;
        e.time = QDateTime::fromMSecsSinceEpoch(msg->createdAtMs)
                     .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        e.sender = senderDisplayName(*msg).toHtmlEscaped();
        e.text = visibleMessageText(wToQ(msg->body)).toHtmlEscaped();
        entries->push_back(std::move(e));
    }
    const QString title = m_conversationTitle.toHtmlEscaped();

    auto* watcher = new QFutureWatcher<bool>(const_cast<ConversationHistoryDialog*>(this));
    QObject::connect(watcher, &QFutureWatcher<bool>::finished, this, [watcher, filePath, this]() {
        watcher->deleteLater();
        if (watcher->result()) {
            QMessageBox::information(const_cast<ConversationHistoryDialog*>(this),
                QStringLiteral("导出完成"),
                QStringLiteral("聊天记录已导出到:\n%1").arg(filePath));
        } else {
            QMessageBox::warning(const_cast<ConversationHistoryDialog*>(this),
                QStringLiteral("导出失败"),
                QStringLiteral("无法写入文件:\n%1").arg(filePath));
        }
    });
    watcher->setFuture(QtConcurrent::run([filePath, entries, title]() -> bool {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);

        out << QStringLiteral(
            "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n"
            "<title>%1</title>\n"
            "<style>\n"
            "body { font-family: 'Microsoft YaHei', sans-serif; max-width: 800px; margin: 0 auto; padding: 24px; background: #f5f5f5; }\n"
            "h1 { font-size: 18px; border-bottom: 1px solid #ddd; padding-bottom: 8px; }\n"
            ".msg { margin: 8px 0; padding: 10px 14px; border-radius: 10px; background: #fff; }\n"
            ".sender { font-weight: 700; font-size: 13px; color: #333; }\n"
            ".time { font-size: 11px; color: #999; margin-left: 8px; }\n"
            ".body { margin-top: 4px; font-size: 14px; color: #222; white-space: pre-wrap; word-break: break-all; }\n"
            "</style>\n</head><body>\n"
            "<h1>%1</h1>\n")
               .arg(title);

        for (const auto& e : *entries) {
            out << QStringLiteral(
                "<div class=\"msg\"><span class=\"sender\">%1</span>"
                "<span class=\"time\">%2</span>"
                "<div class=\"body\">%3</div></div>\n")
                   .arg(e.sender, e.time, e.text);
        }

        out << QStringLiteral("</body></html>\n");
        return true;
    }));
}
