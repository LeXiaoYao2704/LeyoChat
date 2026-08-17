// MessageBubbleWidget.cpp — 消息气泡 Widget 实现
#include "ui/MessageBubbleWidget.h"

#include "app/ImageFileTypeHelpers.h"
#include "ui/AppStyle.h"
#include "ui/FilePreviewWidget.h"
#include "ui/MessageListModel.h"
#include "ui/MessageDeliveryPresentation.h"
#include "ui/MessageThumbnailCache.h"
#include "ui/StickerManager.h"
#include "ui/UiIcons.h"
#include "services/ResourceRefRouter.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QStyle>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QPushButton>
#include <QGraphicsOpacityEffect>
#include <QStringList>
#include <QTextDocument>
#include <QVBoxLayout>

namespace {

// ── 字体工具 ──
QFont messageBodyFont()
{
    QFont f = QApplication::font();
    if (f.pointSizeF() > 0.0)
        f.setPointSizeF(qMax(10.5, f.pointSizeF()));
    else if (f.pixelSize() > 0)
        f.setPixelSize(qMax(14, f.pixelSize()));
    return f;
}

QFont messageMetaFont()
{
    return AppStyle::captionFont(QApplication::font());
}

// ── 头像 ──
constexpr int kAvatarSize = 34;
constexpr int kStickerSize = 120;
constexpr int kImagePreviewMax = 480;
constexpr int kImageThumbMaxW = 280;
constexpr int kImageThumbMaxH = 200;

static const QColor kAvatarPalette[] = {
    QColor(0x52, 0x73, 0xE8), QColor(0x2F, 0xA4, 0x84),
    QColor(0xD9, 0x96, 0x3A), QColor(0x7B, 0x68, 0xE6),
    QColor(0xD8, 0x5A, 0x9A), QColor(0x32, 0x96, 0xC4),
};

QPixmap roundedAvatarPixmap(const QString& avatarPath, int size)
{
    if (avatarPath.trimmed().isEmpty() || size <= 0) return {};

    const QString cacheKey = QStringLiteral("mbw-avatar|%1|%2").arg(avatarPath).arg(size);
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached)) return cached;

    QImageReader reader(avatarPath);
    reader.setAutoTransform(true);
    const QSize orig = reader.size();
    if (orig.isValid() && (orig.width() > size * 2 || orig.height() > size * 2))
        reader.setScaledSize(QSize(size * 2, size * 2));
    const QImage image = reader.read();
    if (image.isNull()) return {};

    QPixmap scaled = QPixmap::fromImage(image).scaled(
        size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    QPixmap result(size, size);
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addEllipse(0, 0, size, size);
    p.setClipPath(clip);
    p.drawPixmap(0, 0, scaled);
    p.end();

    QPixmapCache::insert(cacheKey, result);
    return result;
}

QPixmap letterAvatarPixmap(const QString& name, const QColor& bg, int size)
{
    const QString letter = name.trimmed().isEmpty()
        ? QStringLiteral("?")
        : name.trimmed().left(1).toUpper();
    const QString cacheKey = QStringLiteral("mbw-letter|%1|%2|%3")
                                 .arg(letter, bg.name()).arg(size);
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached)) return cached;

    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(0, 0, size, size);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSizeF(qMax(9.0, f.pointSizeF() - 0.5));
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, letter);
    p.end();

    QPixmapCache::insert(cacheKey, pm);
    return pm;
}

bool isImageAttachment(const QString& attachmentName, const QString& localFilePath)
{
    if (localFilePath.trimmed().isEmpty()) {
        return isChatPreviewImageAttachmentName(attachmentName);
    }
    return !QImageReader::imageFormat(localFilePath).isEmpty();
}

QString fileKindIcon(const QString& fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    if (QStringList{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                    QStringLiteral("webp"), QStringLiteral("bmp"), QStringLiteral("gif")}.contains(ext))
        return QStringLiteral("IMG");
    if (QStringList{QStringLiteral("doc"), QStringLiteral("docx"), QStringLiteral("odt")}.contains(ext))
        return QStringLiteral("DOC");
    if (QStringList{QStringLiteral("xls"), QStringLiteral("xlsx"), QStringLiteral("csv")}.contains(ext))
        return QStringLiteral("XLS");
    if (QStringList{QStringLiteral("ppt"), QStringLiteral("pptx")}.contains(ext))
        return QStringLiteral("PPT");
    if (QStringList{QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("7z")}.contains(ext))
        return QStringLiteral("ZIP");
    if (QStringList{QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("json"),
                    QStringLiteral("log"), QStringLiteral("xml")}.contains(ext))
        return QStringLiteral("TXT");
    return ext.isEmpty() ? QStringLiteral("FILE") : ext.left(4).toUpper();
}

QString fileKindColor(const QString& fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    if (QStringList{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                    QStringLiteral("webp"), QStringLiteral("bmp"), QStringLiteral("gif")}.contains(ext))
        return QStringLiteral("#0EA5E9");
    if (QStringList{QStringLiteral("doc"), QStringLiteral("docx"), QStringLiteral("odt")}.contains(ext))
        return QStringLiteral("#2563EB");
    if (QStringList{QStringLiteral("xls"), QStringLiteral("xlsx"), QStringLiteral("csv")}.contains(ext))
        return QStringLiteral("#16A34A");
    if (QStringList{QStringLiteral("ppt"), QStringLiteral("pptx")}.contains(ext))
        return QStringLiteral("#EA580C");
    if (QStringList{QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("7z")}.contains(ext))
        return QStringLiteral("#9333EA");
    return AppStyle::accent();
}

bool hasLocalFile(const QString& path)
{
    const QFileInfo fi(path);
    return fi.exists() && fi.isFile();
}

bool isAttachmentSummaryBody(const QString& body, bool isFile, bool isImageFile)
{
    if (!isFile) return false;
    const QString t = body.trimmed();
    if (t.isEmpty()) return false;
    if (t.startsWith(QStringLiteral("[File]"), Qt::CaseInsensitive)) return true;
    if (isImageFile && (t.startsWith(QStringLiteral("[Image]"), Qt::CaseInsensitive)
                        || t.startsWith(QStringLiteral("[图片]")))) return true;
    return false;
}

QString displayBytes(qint64 bytes)
{
    if (bytes < 0) return QStringLiteral("--");
    static const QStringList u = {QStringLiteral("B"), QStringLiteral("KB"),
                                   QStringLiteral("MB"), QStringLiteral("GB")};
    double v = static_cast<double>(bytes);
    int i = 0;
    while (v >= 1024.0 && i < u.size() - 1) { v /= 1024.0; ++i; }
    return QStringLiteral("%1 %2").arg(QString::number(v, 'f', i == 0 ? 0 : 1), u.at(i));
}

qint64 transferDisplayBytesForState(int stateInt, qint64 bytesCompleted, qint64 fileSize)
{
    const auto state = static_cast<FileTransferState>(stateInt);
    if (fileSize <= 0) {
        return qMax<qint64>(0, bytesCompleted);
    }

    const qint64 clampedBytes = qBound<qint64>(0, bytesCompleted, fileSize);
    if (state == FileTransferState::Completed) {
        return fileSize;
    }
    if (state == FileTransferState::Completing) {
        return qMin(clampedBytes, fileSize - 1);
    }
    return clampedBytes;
}

int transferDisplayPercent(qint64 displayBytes, qint64 fileSize)
{
    if (fileSize <= 0) {
        return 0;
    }

    const int percent = static_cast<int>(
        (static_cast<long double>(displayBytes) * 100.0L)
        / static_cast<long double>(fileSize));
    return qBound(0, percent, 100);
}

qreal transferDisplayRatio(qint64 displayBytes, qint64 fileSize)
{
    if (fileSize <= 0) {
        return 0.0;
    }

    return qBound<qreal>(
        0.0,
        static_cast<qreal>(displayBytes) / static_cast<qreal>(fileSize),
        1.0);
}

QString deliveryIndicatorText(int state, bool outgoing, int groupReadCount, int groupActive)
{
    return messageDeliveryIndicatorText(static_cast<MessageDeliveryState>(state),
                                        outgoing,
                                        groupReadCount,
                                        groupActive);
}

QString transferStateLabel(int stateInt, qint64 bytesCompleted, qint64 fileSize)
{
    const auto state = static_cast<FileTransferState>(stateInt);
    switch (state) {
    case FileTransferState::PendingOffer:    return QStringLiteral("准备发送");
    case FileTransferState::WaitingAccept:   return QStringLiteral("等待接收");
    case FileTransferState::ReadyToTransfer: return QStringLiteral("准备传输");
    case FileTransferState::Transferring:
        if (fileSize > 0)
            return QStringLiteral("%1 / %2").arg(displayBytes(bytesCompleted), displayBytes(fileSize));
        return QStringLiteral("正在传输");
    case FileTransferState::Paused:      return QStringLiteral("已暂停");
    case FileTransferState::Interrupted: return QStringLiteral("传输中断");
    case FileTransferState::Completing:  return QStringLiteral("正在完成");
    case FileTransferState::Completed:   return QStringLiteral("已完成");
    case FileTransferState::Failed:      return QStringLiteral("传输失败");
    case FileTransferState::Canceled:    return QStringLiteral("已取消");
    }
    return QStringLiteral("准备发送");
}

QString resolvedGroupFileCardLocalPath(const QModelIndex& index)
{
    const QString direct = index.data(MessageListModel::LocalFilePathRole).toString().trimmed();
    if (!direct.isEmpty()) return direct;
    const QString fcj = index.data(MessageListModel::FileCardJsonRole).toString().trimmed();
    if (fcj.isEmpty()) return {};
    const QJsonObject obj = QJsonDocument::fromJson(fcj.toUtf8()).object();
    const QString lp = obj.value(QStringLiteral("local_path")).toString().trimmed();
    if (!lp.isEmpty()) return lp;
    if (index.data(MessageListModel::OutgoingRole).toBool())
        return obj.value(QStringLiteral("sender_file_path")).toString().trimmed();
    return {};
}

bool groupFileCardUsesLocalActions(const QModelIndex& index)
{
    const QString lp = resolvedGroupFileCardLocalPath(index);
    if (lp.isEmpty()) return false;
    if (index.data(MessageListModel::OutgoingRole).toBool()) return true;
    return QFile::exists(lp);
}

QSize imagePreviewSize(const QString& localPath, int maxW, int maxH)
{
    if (maxW <= 0 || maxH <= 0) return {qMax(1, maxW), qMax(1, maxH)};
    static QHash<QString, QSize> sCache;
    QSize sz;
    auto it = sCache.constFind(localPath);
    if (it != sCache.constEnd()) {
        sz = it.value();
    } else {
        QImageReader r(localPath);
        r.setAutoTransform(true);
        sz = r.size();
        sCache.insert(localPath, sz);
    }
    if (!sz.isValid() || sz.width() <= 0 || sz.height() <= 0)
        return {qMin(maxW, 240), qMin(maxH, 180)};
    if (sz.width() > maxW || sz.height() > maxH)
        sz.scale(maxW, maxH, Qt::KeepAspectRatio);
    sz.setWidth(qMax(72, sz.width()));
    sz.setHeight(qMax(56, sz.height()));
    return sz;
}

QPixmap loadImagePreview(const QString& localPath, const QSize& target)
{
    if (target.width() <= 0 || target.height() <= 0) return {};
    const QString key = QStringLiteral("mbw-preview|%1|%2x%3")
                            .arg(localPath).arg(target.width()).arg(target.height());
    QPixmap cached;
    if (QPixmapCache::find(key, &cached)) return cached;

    QImageReader reader(localPath);
    reader.setAutoTransform(true);
    const QSize orig = reader.size();
    if (orig.isValid() && (orig.width() > target.width() * 2 || orig.height() > target.height() * 2))
        reader.setScaledSize(orig.scaled(target * 2, Qt::KeepAspectRatio));
    const QImage img = reader.read();
    if (img.isNull()) return {};

    QPixmap pm = QPixmap::fromImage(img).scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmapCache::insert(key, pm);
    return pm;
}

// ── 操作芯片按钮样式 ──
QString chipStyleSheet(const QString& bgColor, const QString& textColor)
{
    return QStringLiteral(
        "QPushButton { background: %1; color: %2; border: none; border-radius: 10px; "
        "padding: 2px 10px; font-size: 11px; }"
        "QPushButton:hover { background: %1; opacity: 0.85; }")
        .arg(bgColor, textColor);
}

// ── 文本宽度估算（模仿 EchoChat 的 estimateBubbleTextWidth）──
QString textBubbleStyleSheet(bool outgoing)
{
    // 缓存结果：同一主题下 outgoing/incoming 样式表不变
    static QString s_cachedOut;
    static QString s_cachedIn;
    static QString s_cachedTheme;
    const QString currentTheme = AppStyle::themeModeToString(AppStyle::currentThemeMode());
    if (s_cachedTheme != currentTheme) {
        s_cachedTheme = currentTheme;
        s_cachedOut.clear();
        s_cachedIn.clear();
    }
    QString& cached = outgoing ? s_cachedOut : s_cachedIn;
    if (cached.isEmpty()) {
        const QString bg = outgoing ? AppStyle::bubbleOut() : AppStyle::bubbleIn();
        const QString border = outgoing ? AppStyle::bubbleOutBorder() : AppStyle::bubbleInBorder();
        cached = QStringLiteral(
            "QFrame#MsgBubble { background-color: %1; border: 1px solid %2; border-radius: %3px; }")
            .arg(bg, border)
            .arg(AppStyle::kBubbleRadius);
    }
    return cached;
}

constexpr int kMaxTextBubbleWidth = 520;
constexpr int kMinTextBubbleWidth = 132;
constexpr int kBubbleHorizontalOverhead = 80; // avatar(40) + messageArea margins(8+8) + bubble padding(10+10) + spacing
constexpr int kBubbleFrameMinWidth = 120;

int resolvedBubbleFrameWidth(int availableWidth, int fallbackWidth)
{
    if (availableWidth <= 0) {
        return fallbackWidth;
    }
    return qBound(kBubbleFrameMinWidth, availableWidth - 84, fallbackWidth);
}

int resolvedBubbleContentWidth(int availableWidth, int fallbackWidth)
{
    if (availableWidth <= 0) {
        return fallbackWidth;
    }
    return qBound(kMinTextBubbleWidth, availableWidth - kBubbleHorizontalOverhead, fallbackWidth);
}

int estimateBubbleTextWidth(const QFont& font, const QString& text, int maxWidth)
{
    const QFontMetrics fm(font);
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return qMin(maxWidth, kMinTextBubbleWidth);
    }

    const QRect bounds = fm.boundingRect(QRect(0, 0, maxWidth, 2000),
                                         Qt::TextWordWrap,
                                         text);
    return qBound(kMinTextBubbleWidth, bounds.width() + 28, maxWidth);
}

QString normalizedBubbleText(const QString& text)
{
    // 如果是富文本，提取纯文本显示
    if (Qt::mightBeRichText(text)) {
        QTextDocument doc;
        doc.setHtml(text);
        return doc.toPlainText();
    }
    return text;
}

} // namespace

// ==========================================================================
// 构造
// ==========================================================================

MessageBubbleWidget::MessageBubbleWidget(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("MessageBubble"));
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 不在构造时连接全局 thumbnailReady —— 改为 requestThumbnail 时按需连接
}

void MessageBubbleWidget::resetForRecycling()
{
    clearContent();
    setVisible(false);
    setParent(nullptr);
}

void MessageBubbleWidget::buildLayout()
{
    if (m_layoutBuilt) return;
    m_layoutBuilt = true;

    // 连接缩略图缓存（延迟到第一次实际使用）
    connect(&MessageThumbnailCache::instance(), &MessageThumbnailCache::thumbnailReady,
            this, &MessageBubbleWidget::onThumbnailReady);

    // 日期分隔
    m_dateSeparator = new QLabel(this);
    m_dateSeparator->setObjectName(QStringLiteral("MsgDateSep"));
    m_dateSeparator->setAlignment(Qt::AlignCenter);
    m_dateSeparator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_dateSeparator->setVisible(false);
    m_mainLayout->addWidget(m_dateSeparator);
    m_mainLayout->setAlignment(m_dateSeparator, Qt::AlignHCenter);

    // 居中标签（系统消息/通话记录）
    m_centeredLabel = new QLabel(this);
    m_centeredLabel->setObjectName(QStringLiteral("MsgCentered"));
    m_centeredLabel->setAlignment(Qt::AlignCenter);
    m_centeredLabel->setWordWrap(true);
    m_centeredLabel->setVisible(false);
    m_mainLayout->addWidget(m_centeredLabel);

    // 消息区域 (头像 + 消息体)
    m_messageArea = new QWidget(this);
    m_messageArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_messageLayout = new QHBoxLayout(m_messageArea);
    m_messageLayout->setContentsMargins(8, 0, 8, 0);
    m_messageLayout->setSpacing(8);
    m_messageArea->setVisible(false);

    // 左头像
    m_avatarLabel = new QLabel(m_messageArea);
    m_avatarLabel->setFixedSize(kAvatarSize, kAvatarSize);
    m_avatarLabel->setCursor(Qt::PointingHandCursor);
    m_avatarLabel->setVisible(false);

    // 消息体容器
    m_bodyWidget = new QWidget(m_messageArea);
    m_bodyLayout = new QVBoxLayout(m_bodyWidget);
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(2);
    m_bodyWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    // 头部
    m_headerLabel = new QLabel(m_bodyWidget);
    m_headerLabel->setObjectName(QStringLiteral("MsgHeader"));
    m_bodyLayout->addWidget(m_headerLabel);

    // 气泡
    m_bubbleFrame = new QFrame(m_bodyWidget);
    m_bubbleFrame->setObjectName(QStringLiteral("MsgBubble"));
    m_bubbleFrame->setAttribute(Qt::WA_StyledBackground, true);
    m_bubbleLayout = new QVBoxLayout(m_bubbleFrame);
    m_bubbleLayout->setContentsMargins(10, 6, 10, 6);
    m_bubbleLayout->setSpacing(4);
    m_bodyLayout->addWidget(m_bubbleFrame);

    // 引用
    m_quoteFrame = new QFrame(m_bubbleFrame);
    m_quoteFrame->setObjectName(QStringLiteral("MsgQuote"));
    auto* ql = new QVBoxLayout(m_quoteFrame);
    ql->setContentsMargins(8, 6, 6, 6);
    ql->setSpacing(2);
    m_quoteSenderLabel = new QLabel(m_quoteFrame);
    m_quoteSenderLabel->setObjectName(QStringLiteral("MsgQuoteSender"));
    m_quoteBodyLabel = new QLabel(m_quoteFrame);
    m_quoteBodyLabel->setObjectName(QStringLiteral("MsgQuoteBody"));
    m_quoteBodyLabel->setWordWrap(false);
    ql->addWidget(m_quoteSenderLabel);
    ql->addWidget(m_quoteBodyLabel);
    m_quoteFrame->setVisible(false);
    m_bubbleLayout->addWidget(m_quoteFrame);

    // 正文
    m_bodyLabel = new QLabel(m_bubbleFrame);
    m_bodyLabel->setObjectName(QStringLiteral("MsgBodyLabel"));
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->setTextFormat(Qt::PlainText);
    m_bodyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    m_bodyLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_bodyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_bodyLabel->setVisible(false);
    m_bubbleLayout->addWidget(m_bodyLabel);

    // 贴纸
    m_stickerLabel = new QLabel(m_bubbleFrame);
    m_stickerLabel->setFixedSize(kStickerSize, kStickerSize);
    m_stickerLabel->setAlignment(Qt::AlignCenter);
    m_stickerLabel->setVisible(false);
    m_bubbleLayout->addWidget(m_stickerLabel);

    // 文件卡片
    m_fileCardFrame = new QFrame(m_bubbleFrame);
    m_fileCardFrame->setObjectName(QStringLiteral("MsgFileCard"));
    m_fileCardFrame->setVisible(false);
    m_bubbleLayout->addWidget(m_fileCardFrame);

    // 图片预览
    m_imagePreviewLabel = new QLabel(m_bubbleFrame);
    m_imagePreviewLabel->setAlignment(Qt::AlignCenter);
    m_imagePreviewLabel->setVisible(false);
    m_imagePreviewLabel->installEventFilter(this);
    m_bubbleLayout->addWidget(m_imagePreviewLabel);

    // 传输状态
    m_transferStatusLabel = new QLabel(m_bubbleFrame);
    m_transferStatusLabel->setObjectName(QStringLiteral("MsgTransferStatus"));
    m_transferStatusLabel->setVisible(false);
    m_bubbleLayout->addWidget(m_transferStatusLabel);

    // 操作按钮
    m_actionChipsWidget = new QWidget(m_bubbleFrame);
    auto* chipsLayout = new QHBoxLayout(m_actionChipsWidget);
    chipsLayout->setContentsMargins(0, 4, 0, 0);
    chipsLayout->setSpacing(6);
    m_actionChipsWidget->setVisible(false);
    m_bubbleLayout->addWidget(m_actionChipsWidget);

    // 撤回标签
    m_recalledLabel = new QLabel(m_bubbleFrame);
    m_recalledLabel->setObjectName(QStringLiteral("MsgRecalled"));
    m_recalledLabel->setVisible(false);
    m_bubbleLayout->addWidget(m_recalledLabel);

    // 送达指示
    m_deliveryLabel = new QLabel(m_bodyWidget);
    m_deliveryLabel->setObjectName(QStringLiteral("MsgDelivery"));
    m_deliveryLabel->setVisible(false);
    m_bodyLayout->addWidget(m_deliveryLabel);

    // 右头像
    m_avatarLabelRight = new QLabel(m_messageArea);
    m_avatarLabelRight->setFixedSize(kAvatarSize, kAvatarSize);
    m_avatarLabelRight->setVisible(false);

    // 组装布局 — incoming: [avatar][body][stretch]  outgoing: [stretch][body][avatar]
    // 初始组装，具体对齐在 populateFromIndex 之后由 arrangeMessageAlignment 调整
    m_messageLayout->addWidget(m_avatarLabel);
    m_messageLayout->addWidget(m_bodyWidget, 0);
    m_messageLayout->addWidget(m_avatarLabelRight);

    m_mainLayout->addWidget(m_messageArea);
}

// ==========================================================================
// clearContent
// ==========================================================================

void MessageBubbleWidget::clearContent()
{
    MessageThumbnailCache::instance().cancelRequest(m_messageId);
    m_currentThumbnailKey.clear();

    m_messageId.clear();
    m_senderId.clear();
    m_transferTaskId.clear();
    m_outgoing = false;

    if (m_dateSeparator) m_dateSeparator->setVisible(false);
    if (m_centeredLabel) { m_centeredLabel->clear(); m_centeredLabel->setVisible(false); }
    if (m_messageArea) m_messageArea->setVisible(false);
    if (m_avatarLabel) { m_avatarLabel->clear(); m_avatarLabel->setVisible(false); }
    if (m_avatarLabelRight) { m_avatarLabelRight->clear(); m_avatarLabelRight->setVisible(false); }
    if (m_quoteFrame) m_quoteFrame->setVisible(false);
    if (m_bodyLabel) { m_bodyLabel->setVisible(false); m_bodyLabel->clear(); }
    if (m_stickerLabel) {
        m_stickerLabel->setMovie(nullptr);
        m_stickerLabel->clear();
        m_stickerLabel->setVisible(false);
    }
    // 文件卡片：清除子控件防止累积泄漏
    if (m_fileCardFrame) {
        if (auto* lay = m_fileCardFrame->layout()) {
            QLayoutItem* child;
            while ((child = lay->takeAt(0)) != nullptr) {
                delete child->widget();
                delete child;
            }
            delete lay;
        }
        m_fileCardFrame->setVisible(false);
    }
    if (m_imagePreviewLabel) {
        m_imagePreviewLabel->setGraphicsEffect(nullptr); // 释放 QGraphicsOpacityEffect
        m_imagePreviewLabel->clear();
        m_imagePreviewLabel->setCursor(Qt::ArrowCursor);
        m_imagePreviewLabel->setToolTip(QString());
        m_imagePreviewLabel->setMinimumSize(0, 0);
        m_imagePreviewLabel->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_imagePreviewLabel->setVisible(false);
    }
    if (m_transferStatusLabel) m_transferStatusLabel->setVisible(false);
    if (m_actionChipsWidget) {
        // 清除旧的操作按钮
        auto* lay = m_actionChipsWidget->layout();
        if (lay) {
            QLayoutItem* child;
            while ((child = lay->takeAt(0)) != nullptr) {
                delete child->widget();
                delete child;
            }
        }
        m_actionChipsWidget->setVisible(false);
    }
    if (m_recalledLabel) m_recalledLabel->setVisible(false);
    if (m_deliveryLabel) m_deliveryLabel->setVisible(false);
    if (m_headerLabel) { m_headerLabel->clear(); m_headerLabel->setVisible(false); }
    if (m_bubbleFrame) {
        m_bubbleFrame->setVisible(false);
        m_bubbleFrame->setStyleSheet(QString());
        m_bubbleLayout->setContentsMargins(12, 8, 12, 8);
    }

    if (m_stickerMovie) {
        m_stickerMovie->stop();
    }
    m_stickerMovie.reset();
}

void MessageBubbleWidget::setAvailableWidth(int width)
{
    m_availableWidth = width;
}

// ==========================================================================
// populateFromIndex
// ==========================================================================

void MessageBubbleWidget::populateFromIndex(const QModelIndex& index)
{
    if (!m_layoutBuilt) buildLayout();
    clearContent();

    if (!index.isValid()) return;

    m_messageId = index.data(MessageListModel::MessageIdRole).toString();
    m_outgoing = index.data(MessageListModel::OutgoingRole).toBool();
    m_senderId = index.data(MessageListModel::SenderIdRole).toString();
    m_transferTaskId = index.data(MessageListModel::TransferTaskIdRole).toString();

    const int frameWidth = resolvedBubbleFrameWidth(m_availableWidth, kMaxTextBubbleWidth + 48);
    if (m_bodyWidget) {
        m_bodyWidget->setMaximumWidth(frameWidth);
    }
    if (m_bubbleFrame) {
        m_bubbleFrame->setMaximumWidth(frameWidth);
    }

    // 日期分隔
    const bool showDateSep = index.data(MessageListModel::ShowDateSeparatorRole).toBool();
    if (showDateSep) {
        const QString dateText = index.data(MessageListModel::DateLabelRole).toString();
        m_dateSeparator->setText(dateText);
        const int pillWidth = qMax(54, m_dateSeparator->fontMetrics().horizontalAdvance(dateText) + 28);
        m_dateSeparator->setFixedWidth(pillWidth);
        m_dateSeparator->setVisible(true);
    }

    // 消息类型分发
    const QString msgType = index.data(MessageListModel::MessageTypeRole).toString();
    const bool isRecalled = index.data(MessageListModel::RecalledRole).toBool();

    if (msgType == QStringLiteral("system")) {
        populateSystemMessage(index);
    } else if (msgType == QStringLiteral("call_record")) {
        populateCallRecord(index);
    } else if (isRecalled) {
        populateRecalledMessage(index);
    } else if (msgType == QStringLiteral("sticker")) {
        populateStickerMessage(index);
    } else if (!index.data(MessageListModel::FileCardJsonRole).toString().trimmed().isEmpty()) {
        populateGroupFileCard(index);
    } else if (index.data(MessageListModel::ResourceReferenceRole).toBool()) {
        populateResourceReference(index);
    } else {
        populateNormalMessage(index);
    }

    // 根据 outgoing 调整头像和气泡的左右对齐
    arrangeMessageAlignment();

    applyThemeStyleSheet();
}

// ==========================================================================
// 系统消息
// ==========================================================================

void MessageBubbleWidget::populateSystemMessage(const QModelIndex& index)
{
    const QString body = index.data(MessageListModel::BodyRole).toString();
    const QString time = index.data(MessageListModel::TimeLabelRole).toString();

    m_centeredLabel->setText(QStringLiteral("<div style='text-align:center'>"
        "<span style='color:%1;font-size:11px'>%2</span><br>"
        "<span style='color:#3AA55F'>%3</span></div>")
        .arg(AppStyle::textSecondary(), time.toHtmlEscaped(), body.toHtmlEscaped()));
    m_centeredLabel->setVisible(true);
}

// ==========================================================================
// 通话记录
// ==========================================================================

void MessageBubbleWidget::populateCallRecord(const QModelIndex& index)
{
    const QString body = index.data(MessageListModel::BodyRole).toString();
    const QJsonObject callObj = QJsonDocument::fromJson(body.toUtf8()).object();
    const QString result = callObj.value(QStringLiteral("result")).toString();
    const qint64 durationMs = static_cast<qint64>(callObj.value(QStringLiteral("durationMs")).toDouble());

    QString text;
    if (result == QStringLiteral("completed")) {
        const int s = static_cast<int>(durationMs / 1000);
        text = QStringLiteral("\xF0\x9F\x93\x9E 语音通话  %1:%2")
                   .arg(s / 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'));
    } else if (result == QStringLiteral("rejected")) {
        text = QStringLiteral("\xF0\x9F\x93\x9E 对方已拒绝");
    } else if (result == QStringLiteral("no_answer")) {
        text = QStringLiteral("\xF0\x9F\x93\x9E 无人接听");
    } else if (result == QStringLiteral("busy")) {
        text = QStringLiteral("\xF0\x9F\x93\x9E 对方忙线");
    } else if (result == QStringLiteral("cancelled")) {
        text = QStringLiteral("\xF0\x9F\x93\x9E 已取消");
    } else {
        text = QStringLiteral("\xF0\x9F\x93\x9E 通话结束");
    }

    m_centeredLabel->setText(QStringLiteral("<span style='color:#70C0A0'>%1</span>").arg(text.toHtmlEscaped()));
    m_centeredLabel->setVisible(true);
}

// ==========================================================================
// 撤回消息
// ==========================================================================

void MessageBubbleWidget::populateRecalledMessage(const QModelIndex& index)
{
    const QString sender = index.data(MessageListModel::SenderNameRole).toString();
    const QString time = index.data(MessageListModel::TimeLabelRole).toString();
    const QString avatarPath = index.data(MessageListModel::SenderAvatarPathRole).toString();

    m_messageArea->setVisible(true);
    setupAvatar(avatarPath, sender, m_outgoing);
    setupHeader(sender, time, m_outgoing);

    m_bubbleFrame->setVisible(true);
    m_recalledLabel->setText(QStringLiteral("此消息已被撤回"));
    m_recalledLabel->setVisible(true);

    const int delivState = index.data(MessageListModel::DeliveryStateRole).toInt();
    const int grp = index.data(MessageListModel::GroupReadCountRole).toInt();
    const int grpA = index.data(MessageListModel::GroupActiveMemberCountRole).toInt();
    setupDeliveryIndicator(delivState, m_outgoing, grp, grpA);
}

// ==========================================================================
// 贴纸消息
// ==========================================================================

void MessageBubbleWidget::populateStickerMessage(const QModelIndex& index)
{
    const QString sender = index.data(MessageListModel::SenderNameRole).toString();
    const QString time = index.data(MessageListModel::TimeLabelRole).toString();
    const QString avatarPath = index.data(MessageListModel::SenderAvatarPathRole).toString();
    const QString payloadJson = index.data(MessageListModel::PayloadJsonRole).toString();
    const QJsonObject sObj = QJsonDocument::fromJson(payloadJson.toUtf8()).object();
    const QString packId = sObj.value(QStringLiteral("pack_id")).toString();
    const QString stickerId = sObj.value(QStringLiteral("sticker_id")).toString();

    m_messageArea->setVisible(true);
    setupAvatar(avatarPath, sender, m_outgoing);
    setupHeader(sender, time, m_outgoing);

    // 不绘制气泡背景
    m_bubbleFrame->setVisible(true);
    m_bubbleFrame->setStyleSheet(QStringLiteral("QFrame#MsgBubble { background: transparent; border: none; }"));
    m_bubbleLayout->setContentsMargins(0, 0, 0, 0);

    m_stickerLabel->setMovie(nullptr);
    m_stickerLabel->clear();
    m_stickerLabel->setVisible(true);

    QString stickerPath = StickerManager::instance().stickerFilePath(packId, stickerId);
    if (stickerPath.isEmpty()) {
        const QString base64 = sObj.value(QStringLiteral("gif_base64")).toString();
        if (!base64.isEmpty()) {
            const QByteArray data = QByteArray::fromBase64(base64.toLatin1());
            stickerPath = StickerManager::instance().cacheReceivedSticker(packId, stickerId, data);
        }
    }

    if (!stickerPath.isEmpty()) {
        if (stickerPath.endsWith(QStringLiteral(".gif"), Qt::CaseInsensitive)) {
            auto movie = std::make_shared<QMovie>(stickerPath);
            movie->setCacheMode(QMovie::CacheAll);
            if (movie->isValid()) {
                m_stickerMovie = movie;
                m_stickerLabel->setMovie(m_stickerMovie.get());
                m_stickerMovie->start();
            } else {
                const QPixmap pm = StickerManager::instance().stickerThumbnail(packId, stickerId, kStickerSize);
                if (!pm.isNull()) {
                    m_stickerLabel->setPixmap(pm.scaled(kStickerSize, kStickerSize,
                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
            }
        } else {
            const QPixmap pm = StickerManager::instance().stickerThumbnail(packId, stickerId, kStickerSize);
            if (!pm.isNull()) {
                m_stickerLabel->setPixmap(pm.scaled(kStickerSize, kStickerSize,
                    Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
    } else {
        const QString emoji = sObj.value(QStringLiteral("emoji")).toString();
        m_stickerLabel->setText(emoji.isEmpty() ? QStringLiteral("🎭") : emoji);
        QFont f = m_stickerLabel->font();
        f.setPixelSize(64);
        m_stickerLabel->setFont(f);
    }

    const int delivState = index.data(MessageListModel::DeliveryStateRole).toInt();
    const int grp = index.data(MessageListModel::GroupReadCountRole).toInt();
    const int grpA = index.data(MessageListModel::GroupActiveMemberCountRole).toInt();
    setupDeliveryIndicator(delivState, m_outgoing, grp, grpA);
}

// ==========================================================================
// 群文件卡片
// ==========================================================================

void MessageBubbleWidget::populateGroupFileCard(const QModelIndex& index)
{
    const QString sender = index.data(MessageListModel::SenderNameRole).toString();
    const QString time = index.data(MessageListModel::TimeLabelRole).toString();
    const QString avatarPath = index.data(MessageListModel::SenderAvatarPathRole).toString();
    const QString fileCardJson = index.data(MessageListModel::FileCardJsonRole).toString();
    const QJsonObject cardObj = QJsonDocument::fromJson(fileCardJson.toUtf8()).object();
    const QString cardFileName = cardObj.value(QStringLiteral("file_name")).toString();
    const qint64 cardFileSize = cardObj.value(QStringLiteral("file_size")).toInteger();
    const QString cardChannel = cardObj.value(QStringLiteral("channel")).toString();
    const bool fileDownloaded = groupFileCardUsesLocalActions(index);

    m_messageArea->setVisible(true);
    setupAvatar(avatarPath, sender, m_outgoing);
    setupHeader(sender, time, m_outgoing);

    m_bubbleFrame->setVisible(true);
    setupFileCard(index, m_outgoing);

    // 操作芯片
    setupActionChips(index, m_outgoing);

    const int delivState = index.data(MessageListModel::DeliveryStateRole).toInt();
    const int grp = index.data(MessageListModel::GroupReadCountRole).toInt();
    const int grpA = index.data(MessageListModel::GroupActiveMemberCountRole).toInt();
    setupDeliveryIndicator(delivState, m_outgoing, grp, grpA);
}

// ==========================================================================
// 资源引用
// ==========================================================================

void MessageBubbleWidget::populateResourceReference(const QModelIndex& index)
{
    const QString sender = index.data(MessageListModel::SenderNameRole).toString();
    const QString time = index.data(MessageListModel::TimeLabelRole).toString();
    const QString avatarPath = index.data(MessageListModel::SenderAvatarPathRole).toString();

    m_messageArea->setVisible(true);
    setupAvatar(avatarPath, sender, m_outgoing);
    setupHeader(sender, time, m_outgoing);

    m_bubbleFrame->setVisible(true);

    // 解析资源引用
    const auto payload = ResourceRefRouter::parsePayload(
        index.data(MessageListModel::PayloadJsonRole).toByteArray());
    if (payload) {
        // 显示为文件卡片样式
        m_fileCardFrame->setVisible(true);
        auto* cardLayout = new QVBoxLayout(m_fileCardFrame);
        cardLayout->setContentsMargins(12, 8, 12, 8);
        cardLayout->setSpacing(4);

        auto* titleLabel = new QLabel(payload->title, m_fileCardFrame);
        titleLabel->setWordWrap(false);
        QFont tf = messageBodyFont();
        tf.setBold(true);
        titleLabel->setFont(tf);
        titleLabel->setStyleSheet(QStringLiteral("color: %1").arg(AppStyle::textPrimary()));
        cardLayout->addWidget(titleLabel);

        const QString kindLabel = payload->kind == QStringLiteral("shared_file")
            ? QStringLiteral("共享文件")
            : payload->kind;
        auto* metaLabel = new QLabel(kindLabel, m_fileCardFrame);
        metaLabel->setFont(messageMetaFont());
        metaLabel->setStyleSheet(QStringLiteral("color: %1").arg(AppStyle::textSecondary()));
        cardLayout->addWidget(metaLabel);

        // 操作按钮
        if (payload->kind == QStringLiteral("shared_file")) {
            m_actionChipsWidget->setVisible(true);
            auto* chipsLay = qobject_cast<QHBoxLayout*>(m_actionChipsWidget->layout());

            auto* dlBtn = new QPushButton(QStringLiteral("下载"), m_actionChipsWidget);
            dlBtn->setCursor(Qt::PointingHandCursor);
            dlBtn->setStyleSheet(chipStyleSheet(AppStyle::accentSoft(), AppStyle::accent()));
            connect(dlBtn, &QPushButton::clicked, this, [this]() {
                emit messageFileDownloadRequested(m_messageId);
            });
            chipsLay->addWidget(dlBtn);

            auto* openBtn = new QPushButton(QStringLiteral("打开文件夹"), m_actionChipsWidget);
            openBtn->setCursor(Qt::PointingHandCursor);
            openBtn->setStyleSheet(chipStyleSheet(AppStyle::accentSoft(), AppStyle::accent()));
            connect(openBtn, &QPushButton::clicked, this, [this]() {
                emit messageFileVersionHistoryRequested(m_messageId);
            });
            chipsLay->addWidget(openBtn);
            chipsLay->addStretch();
        }
    }

    const int delivState = index.data(MessageListModel::DeliveryStateRole).toInt();
    const int grp = index.data(MessageListModel::GroupReadCountRole).toInt();
    const int grpA = index.data(MessageListModel::GroupActiveMemberCountRole).toInt();
    setupDeliveryIndicator(delivState, m_outgoing, grp, grpA);
}

void MessageBubbleWidget::updateFromIndex(const QModelIndex& index, const QList<int>& roles)
{
    if (!index.isValid()) return;

    // If roles is empty, treat as full update
    if (roles.isEmpty()) {
        populateFromIndex(index);
        return;
    }

    bool needTransfer = false;
    bool needDelivery = false;
    for (int r : roles) {
        switch (r) {
        case MessageListModel::TransferStateRole:
        case MessageListModel::TransferBytesCompletedRole:
        case MessageListModel::TransferFileSizeRole:
            needTransfer = true;
            break;
        case MessageListModel::DeliveryStateRole:
        case MessageListModel::GroupReadCountRole:
        case MessageListModel::GroupActiveMemberCountRole:
            needDelivery = true;
            break;
        default:
            // Unknown role — fall back to full repopulate
            populateFromIndex(index);
            return;
        }
    }

    if (needTransfer && isFileCard()) {
        const int transferState = index.data(MessageListModel::TransferStateRole).toInt();
        const qint64 bytesCompleted = index.data(MessageListModel::TransferBytesCompletedRole).toLongLong();
        const qint64 fileSize = index.data(MessageListModel::TransferFileSizeRole).toLongLong();
        const qint64 displayBytes = transferDisplayBytesForState(transferState, bytesCompleted, fileSize);
        const int pct = transferDisplayPercent(displayBytes, fileSize);
        updateTransferProgress(pct, QStringLiteral("%1%").arg(pct));
    }

    if (needDelivery) {
        updateDeliveryState(
            index.data(MessageListModel::DeliveryStateRole).toInt(),
            index.data(MessageListModel::GroupReadCountRole).toInt(),
            index.data(MessageListModel::GroupActiveMemberCountRole).toInt());
    }
}

// ==========================================================================
// 普通文本/文件消息
// ==========================================================================

void MessageBubbleWidget::populateNormalMessage(const QModelIndex& index)
{
    const QString sender = index.data(MessageListModel::SenderNameRole).toString();
    const QString time = index.data(MessageListModel::TimeLabelRole).toString();
    const QString avatarPath = index.data(MessageListModel::SenderAvatarPathRole).toString();
    const QString body = index.data(MessageListModel::BodyRole).toString();
    const bool isFile = index.data(MessageListModel::FileMessageRole).toBool();
    const QString attachName = index.data(MessageListModel::AttachmentNameRole).toString();
    const QString localPath = index.data(MessageListModel::LocalFilePathRole).toString();
    const bool isImageFile = isFile && isImageAttachment(attachName, localPath);
    const bool isFileSummary = isAttachmentSummaryBody(body, isFile, isImageFile);
    const bool hasVisibleBody = !body.trimmed().isEmpty() && !isFileSummary;
    const bool hasLocalPreview = isImageFile && hasLocalFile(localPath);
    const int delivState = index.data(MessageListModel::DeliveryStateRole).toInt();
    const bool hasTransfer = !m_transferTaskId.trimmed().isEmpty();
    const bool fileComplete = hasLocalFile(localPath)
        && (m_outgoing || delivState >= static_cast<int>(MessageDeliveryState::Received));
    const bool pureImageBubble = isImageFile && hasLocalPreview && !hasTransfer;

    m_messageArea->setVisible(true);
    setupAvatar(avatarPath, sender, m_outgoing);
    setupHeader(sender, time, m_outgoing);
    m_bubbleFrame->setVisible(true);
    m_bubbleFrame->setStyleSheet(textBubbleStyleSheet(m_outgoing));

    // 引用回复
    const QString replyId = index.data(MessageListModel::ReplyToMessageIdRole).toString();
    if (!replyId.trimmed().isEmpty()) {
        const QString replySender = index.data(MessageListModel::ReplyToSenderNameRole).toString();
        const QString replyBody = index.data(MessageListModel::ReplyToBodyRole).toString();
        setupReplyQuote(replySender, replyBody);
    }

    // 消息正文
    if (hasVisibleBody && !isFile) {
        m_bodyLabel->setVisible(true);
        const QFont bf = messageBodyFont();
        m_bodyLabel->setFont(bf);

        // 提取纯文本并计算合适的宽度
        const QString displayText = normalizedBubbleText(body);
        // 优先使用 viewport 传入的可用宽度，否则回退到固定常量
        const int maxWidth = resolvedBubbleContentWidth(m_availableWidth, kMaxTextBubbleWidth);
        const int preferredWidth = estimateBubbleTextWidth(bf, displayText, maxWidth);
        m_bodyLabel->setMaximumWidth(maxWidth);
        m_bodyLabel->setMinimumWidth(qMin(preferredWidth, maxWidth));
        m_bodyLabel->setText(displayText);

        // 样式
        const QString textColor = m_outgoing ? AppStyle::bubbleOutText() : AppStyle::bubbleInText();
        m_bodyLabel->setStyleSheet(QStringLiteral(
            "QLabel#MsgBodyLabel { background: transparent; border: none; color: %1; padding: 0; }")
            .arg(textColor));
    }

    // 文件内容
    if (isFile && !attachName.trimmed().isEmpty()) {
        if (pureImageBubble) {
            // 纯图片气泡 — 透明背景，无边框，仅图片带圆角
            m_bubbleFrame->setStyleSheet(
                QStringLiteral("QFrame#MsgBubble { background: transparent; border: none; border-radius: %1px; }")
                    .arg(AppStyle::kBubbleRadius));
            setupImagePreview(localPath, m_outgoing, true);
        } else if (isImageFile && hasLocalPreview) {
            // 带文本的图片
            setupImagePreview(localPath, m_outgoing, false);
        } else if (hasTransfer && !isImageFile && !fileComplete) {
            // 传输中
            setupFileCard(index, m_outgoing);
        } else {
            // 普通文件卡片
            setupFileCard(index, m_outgoing);
        }

        // 文件操作按钮
        if (fileComplete || hasTransfer) {
            setupActionChips(index, m_outgoing);
        }
    }

    // 编辑标记
    const bool isEdited = index.data(MessageListModel::EditedRole).toBool();
    if (isEdited && m_deliveryLabel) {
        // 将在 delivery indicator 中体现
    }

    const int grp = index.data(MessageListModel::GroupReadCountRole).toInt();
    const int grpA = index.data(MessageListModel::GroupActiveMemberCountRole).toInt();
    setupDeliveryIndicator(delivState, m_outgoing, grp, grpA);
}

// ==========================================================================
// 辅助方法
// ==========================================================================

void MessageBubbleWidget::setupAvatar(const QString& avatarPath, const QString& senderName, bool outgoing)
{
    QLabel* target = outgoing ? m_avatarLabelRight : m_avatarLabel;
    target->setVisible(true);

    QPixmap pm = roundedAvatarPixmap(avatarPath, kAvatarSize);
    if (pm.isNull()) {
        pm = letterAvatarPixmap(senderName, avatarFallbackColor(senderName), kAvatarSize);
    }
    target->setPixmap(pm);

    // 点击处理（仅对方头像）
    if (!outgoing) {
        target->setCursor(Qt::PointingHandCursor);
        // 使用 eventFilter 或 直接重写 — 这里用 lambda 连接
        target->installEventFilter(this);
    }

    // 显示正确侧的头像
    if (outgoing) {
        m_avatarLabel->setVisible(false);
    } else {
        m_avatarLabelRight->setVisible(false);
    }
}

void MessageBubbleWidget::setupHeader(const QString& sender, const QString& timeLabel, bool outgoing)
{
    const QString text = outgoing
        ? QStringLiteral("%1  %2").arg(timeLabel, sender)
        : QStringLiteral("%1  %2").arg(sender, timeLabel);
    m_headerLabel->setText(text);
    m_headerLabel->setAlignment(outgoing ? Qt::AlignRight : Qt::AlignLeft);
    m_headerLabel->setFont(messageMetaFont());
    m_headerLabel->setStyleSheet(QStringLiteral("color: %1;").arg(AppStyle::textSecondary()));
}

void MessageBubbleWidget::setupDeliveryIndicator(int deliveryState, bool outgoing,
                                                  int groupReadCount, int groupActiveMemberCount)
{
    const QString text = deliveryIndicatorText(deliveryState, outgoing,
                                                groupReadCount, groupActiveMemberCount);
    if (text.isEmpty()) return;

    m_deliveryLabel->setText(text);
    m_deliveryLabel->setFont(messageMetaFont());
    m_deliveryLabel->setAlignment(outgoing ? Qt::AlignRight : Qt::AlignLeft);
    m_deliveryLabel->setStyleSheet(QStringLiteral("color: %1;").arg(AppStyle::textSecondary()));
    m_deliveryLabel->setVisible(true);

    if (groupActiveMemberCount > 0 && outgoing) {
        m_deliveryLabel->setCursor(Qt::PointingHandCursor);
        // 已读回执详情点击
        connect(m_deliveryLabel, &QLabel::linkActivated, this, [this](const QString&) {
            emit readReceiptDetailRequested(m_messageId);
        }, Qt::UniqueConnection);
        m_deliveryLabel->setText(QStringLiteral("<a href='#' style='color:%1;text-decoration:none'>%2</a>")
            .arg(AppStyle::textSecondary(), text.toHtmlEscaped()));
    }
}

void MessageBubbleWidget::setupReplyQuote(const QString& replyToSenderName, const QString& replyToBody)
{
    m_quoteFrame->setVisible(true);
    m_quoteSenderLabel->setText(QStringLiteral("回复 %1").arg(replyToSenderName));
    m_quoteSenderLabel->setFont(messageMetaFont());

    const QString preview = replyToBody.trimmed().isEmpty()
        ? QStringLiteral("[消息]")
        : replyToBody;
    const QFontMetrics fm(messageMetaFont());
    m_quoteBodyLabel->setText(fm.elidedText(preview, Qt::ElideRight, 300));
    m_quoteBodyLabel->setFont(messageMetaFont());
}

void MessageBubbleWidget::setupFileCard(const QModelIndex& index, bool outgoing)
{
    Q_UNUSED(outgoing);
    m_fileCardFrame->setVisible(true);

    if (m_fileCardFrame->layout()) {
        QLayoutItem* child;
        while ((child = m_fileCardFrame->layout()->takeAt(0))) {
            delete child->widget();
            delete child;
        }
        delete m_fileCardFrame->layout();
    }

    QString fileName;
    const QString fcj = index.data(MessageListModel::FileCardJsonRole).toString().trimmed();
    const QJsonObject cardObj = fcj.isEmpty()
        ? QJsonObject{}
        : QJsonDocument::fromJson(fcj.toUtf8()).object();
    if (!fcj.isEmpty()) {
        fileName = cardObj.value(QStringLiteral("file_name")).toString();
    }
    if (fileName.isEmpty())
        fileName = index.data(MessageListModel::AttachmentNameRole).toString();
    if (fileName.trimmed().isEmpty())
        fileName = QStringLiteral("未命名文件");

    qint64 fileSize = 0;
    if (!fcj.isEmpty()) {
        fileSize = cardObj.value(QStringLiteral("file_size")).toInteger();
    } else {
        fileSize = index.data(MessageListModel::TransferFileSizeRole).toLongLong();
    }
    const QString channel = cardObj.value(QStringLiteral("channel")).toString().trimmed();
    const QString downloadState = cardObj.value(QStringLiteral("download_state")).toString().trimmed();
    const bool hasTransfer = !m_transferTaskId.trimmed().isEmpty();
    const int transferState = index.data(MessageListModel::TransferStateRole).toInt();
    const qint64 bytesCompleted = index.data(MessageListModel::TransferBytesCompletedRole).toLongLong();
    const qint64 displayBytesCompleted =
        transferDisplayBytesForState(transferState, bytesCompleted, fileSize);

    m_fileCardFrame->setMinimumWidth(qMin(resolvedBubbleContentWidth(m_availableWidth, 360), 360));
    m_fileCardFrame->setStyleSheet(QStringLiteral(
        "QFrame#MsgFileCard { background:%1; border:1px solid %2; border-radius:12px; }")
        .arg(AppStyle::surfaceAlt(), AppStyle::border()));

    auto* layout = new QHBoxLayout(m_fileCardFrame);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(10);

    auto* iconLabel = new QLabel(m_fileCardFrame);
    iconLabel->setFixedSize(48, 48);
    iconLabel->setAlignment(Qt::AlignCenter);
    QFont iconFont = messageMetaFont();
    iconFont.setBold(true);
    iconFont.setPixelSize(11);
    iconLabel->setFont(iconFont);
    iconLabel->setText(fileKindIcon(fileName));
    iconLabel->setStyleSheet(QStringLiteral(
        "QLabel { background:%1; color:white; border-radius:12px; padding:0px; }")
        .arg(fileKindColor(fileName)));
    layout->addWidget(iconLabel);

    auto* infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);

    auto* nameLabel = new QLabel(m_fileCardFrame);
    QFont nf = messageBodyFont();
    nf.setBold(true);
    nameLabel->setFont(nf);
    nameLabel->setStyleSheet(QStringLiteral("color: %1").arg(AppStyle::textPrimary()));
    const QFontMetrics nameFm(nf);
    nameLabel->setText(nameFm.elidedText(fileName, Qt::ElideMiddle, 230));
    infoLayout->addWidget(nameLabel);

    QStringList metaParts;
    metaParts << displayBytes(fileSize);
    if (!fcj.isEmpty()) {
        metaParts << (channel == QStringLiteral("p2p") ? QStringLiteral("P2P 群文件") : QStringLiteral("群文件"));
        if (!downloadState.isEmpty())
            metaParts << (downloadState == QStringLiteral("downloading") ? QStringLiteral("下载中") : downloadState);
    }
    if (hasTransfer)
        metaParts << transferStateLabel(transferState, displayBytesCompleted, fileSize);

    auto* metaLabel = new QLabel(metaParts.join(QStringLiteral(" · ")), m_fileCardFrame);
    metaLabel->setFont(messageMetaFont());
    metaLabel->setStyleSheet(QStringLiteral("color: %1").arg(AppStyle::textSecondary()));
    infoLayout->addWidget(metaLabel);

    if (hasTransfer && fileSize > 0) {
        const qreal ratio = transferDisplayRatio(displayBytesCompleted, fileSize);
        auto* bar = new QFrame(m_fileCardFrame);
        bar->setFixedHeight(4);
        const qreal nextStop = qMin<qreal>(1.0, ratio + 0.001);
        const QString barBackground = QStringLiteral(
            "qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 %1, stop:%2 %1, stop:%3 %4, stop:1 %4)")
            .arg(AppStyle::accent())
            .arg(QString::number(ratio, 'f', 4))
            .arg(QString::number(nextStop, 'f', 4))
            .arg(AppStyle::surfaceMuted());
        bar->setStyleSheet(QStringLiteral("background:%1;border-radius:2px;").arg(barBackground));
        infoLayout->addWidget(bar);
    }

    layout->addLayout(infoLayout, 1);
}

void MessageBubbleWidget::setupImagePreview(const QString& localFilePath, bool outgoing, bool pureImageBubble)
{
    const int maxW = resolvedBubbleContentWidth(
        m_availableWidth,
        pureImageBubble ? kImagePreviewMax : kImageThumbMaxW);
    const int maxH = pureImageBubble
        ? qMin(kImagePreviewMax, qMax(160, maxW))
        : qMin(kImageThumbMaxH, qMax(120, maxW));
    const QSize sz = imagePreviewSize(localFilePath, maxW, maxH);

    m_imagePreviewLabel->setVisible(true);
    m_imagePreviewLabel->setFixedSize(sz);
    m_imagePreviewLabel->setCursor(Qt::PointingHandCursor);
    m_imagePreviewLabel->setToolTip(QStringLiteral("点击查看图片"));
    m_imagePreviewLabel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    // 异步加载缩略图
    m_currentThumbnailKey = QStringLiteral("%1|%2x%3")
        .arg(localFilePath).arg(sz.width()).arg(sz.height());
    auto& cache = MessageThumbnailCache::instance();
    const QPixmap pm = cache.requestThumbnail(localFilePath, sz, m_messageId);
    if (!pm.isNull()) {
        m_imagePreviewLabel->setPixmap(pm);
    } else {
        m_imagePreviewLabel->setText(QStringLiteral("图片预览暂不可用"));
        m_imagePreviewLabel->setCursor(Qt::ArrowCursor);
        m_imagePreviewLabel->setToolTip(QString());
        m_imagePreviewLabel->setStyleSheet(QStringLiteral(
            "background: %1; border-radius: 12px; color: %2;")
            .arg(AppStyle::accentSoft(), AppStyle::textMuted()));
    }
}

void MessageBubbleWidget::onThumbnailReady(const QString& key, const QPixmap& pm)
{
    if (key != m_currentThumbnailKey || !m_imagePreviewLabel) return;
    m_imagePreviewLabel->setPixmap(pm);
}

void MessageBubbleWidget::setupTransferCard(const QModelIndex& index, bool outgoing)
{
    m_fileCardFrame->setVisible(true);

    if (m_fileCardFrame->layout()) {
        QLayoutItem* child;
        while ((child = m_fileCardFrame->layout()->takeAt(0))) {
            delete child->widget();
            delete child;
        }
        delete m_fileCardFrame->layout();
    }

    auto* layout = new QVBoxLayout(m_fileCardFrame);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(2);

    const QString attachName = index.data(MessageListModel::AttachmentNameRole).toString();
    const int transferState = index.data(MessageListModel::TransferStateRole).toInt();
    const qint64 bytesCompleted = index.data(MessageListModel::TransferBytesCompletedRole).toLongLong();
    const qint64 fileSize = index.data(MessageListModel::TransferFileSizeRole).toLongLong();
    const qint64 displayBytesCompleted =
        transferDisplayBytesForState(transferState, bytesCompleted, fileSize);

    // 文件名
    auto* nameLabel = new QLabel(m_fileCardFrame);
    QFont nf = messageBodyFont();
    nf.setBold(true);
    nameLabel->setFont(nf);
    nameLabel->setStyleSheet(QStringLiteral("color: %1").arg(AppStyle::textPrimary()));
    nameLabel->setText(QFontMetrics(nf).elidedText(attachName, Qt::ElideMiddle, 280));
    layout->addWidget(nameLabel);

    // 传输进度
    const qreal ratio = transferDisplayRatio(displayBytesCompleted, fileSize);
    const int pct = transferDisplayPercent(displayBytesCompleted, fileSize);

    auto* progressLabel = new QLabel(m_fileCardFrame);
    progressLabel->setFont(messageMetaFont());
    progressLabel->setStyleSheet(QStringLiteral("color: %1").arg(AppStyle::textSecondary()));
    progressLabel->setText(QStringLiteral("%1  %2%")
        .arg(transferStateLabel(transferState, displayBytesCompleted, fileSize)).arg(pct));
    layout->addWidget(progressLabel);

    // 简单进度条
    auto* bar = new QFrame(m_fileCardFrame);
    bar->setFixedHeight(4);
    QString barBackground;
    if (ratio <= 0.0) {
        barBackground = AppStyle::surfaceMuted();
    } else if (ratio >= 1.0) {
        barBackground = AppStyle::accent();
    } else {
        const qreal nextStop = qMin<qreal>(1.0, ratio + 0.001);
        barBackground = QStringLiteral(
            "qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 %1, stop:%2 %1, stop:%3 %4, stop:1 %4)")
            .arg(AppStyle::accent())
            .arg(QString::number(ratio, 'f', 4))
            .arg(QString::number(nextStop, 'f', 4))
            .arg(AppStyle::surfaceMuted());
    }
    bar->setStyleSheet(QStringLiteral("background:%1;border-radius:2px;").arg(barBackground));
    layout->addWidget(bar);
}

void MessageBubbleWidget::setupActionChips(const QModelIndex& index, bool outgoing)
{
    const QString fcj = index.data(MessageListModel::FileCardJsonRole).toString().trimmed();
    const bool isFile = index.data(MessageListModel::FileMessageRole).toBool();
    const bool isResRef = index.data(MessageListModel::ResourceReferenceRole).toBool();
    const QString attachName = index.data(MessageListModel::AttachmentNameRole).toString();
    const QString localPath = index.data(MessageListModel::LocalFilePathRole).toString();
    const bool hasTransfer = !m_transferTaskId.trimmed().isEmpty();
    const bool transferCancelable = index.data(MessageListModel::TransferCancelableRole).toBool();
    const int delivState = index.data(MessageListModel::DeliveryStateRole).toInt();
    const bool fileReady = hasLocalFile(localPath)
        && (outgoing || delivState >= static_cast<int>(MessageDeliveryState::Received));

    auto* chipsLay = qobject_cast<QHBoxLayout*>(m_actionChipsWidget->layout());
    if (!chipsLay) return;

    const QString chipBg = AppStyle::accentSoft();
    const QString chipFg = AppStyle::accent();

    // 群文件卡片
    if (!fcj.isEmpty()) {
        const bool local = groupFileCardUsesLocalActions(index);
        m_actionChipsWidget->setVisible(true);

        if (local) {
            auto* openBtn = new QPushButton(QStringLiteral("打开文件"), m_actionChipsWidget);
            openBtn->setCursor(Qt::PointingHandCursor);
            openBtn->setStyleSheet(chipStyleSheet(chipBg, chipFg));
            connect(openBtn, &QPushButton::clicked, this, [this]() { emit messageFileOpenRequested(m_messageId); });
            chipsLay->addWidget(openBtn);

            auto* folderBtn = new QPushButton(QStringLiteral("打开文件夹"), m_actionChipsWidget);
            folderBtn->setCursor(Qt::PointingHandCursor);
            folderBtn->setStyleSheet(chipStyleSheet(chipBg, chipFg));
            connect(folderBtn, &QPushButton::clicked, this, [this]() { emit messageFileRevealRequested(m_messageId); });
            chipsLay->addWidget(folderBtn);

            // 预览
            const QJsonObject obj = QJsonDocument::fromJson(fcj.toUtf8()).object();
            const QString fn = obj.value(QStringLiteral("file_name")).toString();
            const bool imagePreviewSupported = !localPath.trimmed().isEmpty()
                && !QImageReader::imageFormat(localPath).isEmpty();
            if (imagePreviewSupported || FilePreviewWidget::isPreviewSupported(fn)) {
                auto* pvBtn = new QPushButton(QStringLiteral("预览"), m_actionChipsWidget);
                pvBtn->setCursor(Qt::PointingHandCursor);
                pvBtn->setStyleSheet(chipStyleSheet(chipBg, chipFg));
                connect(pvBtn, &QPushButton::clicked, this, [this]() { emit messageFilePreviewRequested(m_messageId); });
                chipsLay->addWidget(pvBtn);
            }
        } else {
            auto* dlBtn = new QPushButton(QStringLiteral("下载"), m_actionChipsWidget);
            dlBtn->setCursor(Qt::PointingHandCursor);
            dlBtn->setStyleSheet(chipStyleSheet(chipBg, chipFg));
            connect(dlBtn, &QPushButton::clicked, this, [this]() { emit messageFileDownloadRequested(m_messageId); });
            chipsLay->addWidget(dlBtn);
        }
        chipsLay->addStretch();
        return;
    }

    // 普通文件
    if (isFile && !attachName.trimmed().isEmpty()) {
        if (isImageAttachment(attachName, localPath)) {
            return;
        }
        if (fileReady) {
            m_actionChipsWidget->setVisible(true);
            auto* openBtn = new QPushButton(QStringLiteral("打开文件"), m_actionChipsWidget);
            openBtn->setCursor(Qt::PointingHandCursor);
            openBtn->setStyleSheet(chipStyleSheet(chipBg, chipFg));
            connect(openBtn, &QPushButton::clicked, this, [this]() { emit messageFileOpenRequested(m_messageId); });
            chipsLay->addWidget(openBtn);

            auto* folderBtn = new QPushButton(QStringLiteral("打开文件夹"), m_actionChipsWidget);
            folderBtn->setCursor(Qt::PointingHandCursor);
            folderBtn->setStyleSheet(chipStyleSheet(chipBg, chipFg));
            connect(folderBtn, &QPushButton::clicked, this, [this]() { emit messageFileRevealRequested(m_messageId); });
            chipsLay->addWidget(folderBtn);

            const bool imagePreviewSupported = !localPath.trimmed().isEmpty()
                && !QImageReader::imageFormat(localPath).isEmpty();
            if (imagePreviewSupported || FilePreviewWidget::isPreviewSupported(attachName)) {
                auto* pvBtn = new QPushButton(QStringLiteral("预览"), m_actionChipsWidget);
                pvBtn->setCursor(Qt::PointingHandCursor);
                pvBtn->setStyleSheet(chipStyleSheet(chipBg, chipFg));
                connect(pvBtn, &QPushButton::clicked, this, [this]() { emit messageFilePreviewRequested(m_messageId); });
                chipsLay->addWidget(pvBtn);
            }
            chipsLay->addStretch();
        }

        if (hasTransfer && transferCancelable) {
            m_actionChipsWidget->setVisible(true);
            auto* cancelBtn = new QPushButton(QStringLiteral("取消"), m_actionChipsWidget);
            cancelBtn->setCursor(Qt::PointingHandCursor);
            cancelBtn->setStyleSheet(chipStyleSheet(
                QStringLiteral("#FEE2E2"), QStringLiteral("#DC2626")));
            connect(cancelBtn, &QPushButton::clicked, this, [this]() {
                emit messageTransferCancelRequested(m_transferTaskId);
            });
            chipsLay->addWidget(cancelBtn);
            if (!fileReady) chipsLay->addStretch();
        }
    }
}

// ==========================================================================
// arrangeMessageAlignment — 根据 outgoing 重排布局
// ==========================================================================

void MessageBubbleWidget::arrangeMessageAlignment()
{
    if (!m_messageArea || !m_messageLayout) return;

    // 先移除所有项（不删除 widget）
    while (m_messageLayout->count() > 0) {
        m_messageLayout->takeAt(0);
    }

    if (m_outgoing) {
        // 发送方：[stretch] [body] [avatar_right]
        m_messageLayout->addStretch(1);
        m_messageLayout->addWidget(m_bodyWidget, 0, Qt::AlignTop | Qt::AlignVCenter);
        m_messageLayout->addWidget(m_avatarLabelRight, 0, Qt::AlignTop);
    } else {
        // 接收方：[avatar_left] [body] [stretch]
        m_messageLayout->addWidget(m_avatarLabel, 0, Qt::AlignTop);
        m_messageLayout->addWidget(m_bodyWidget, 0, Qt::AlignTop | Qt::AlignVCenter);
        m_messageLayout->addStretch(1);
    }
}

// ==========================================================================
// 主题与样式
// ==========================================================================

void MessageBubbleWidget::applyThemeStyleSheet()
{
    // 缓存不依赖 widget 状态的通用样式字符串
    static QString s_dateSepSheet;
    static QString s_quoteSheet;
    static QString s_quoteSenderSheet;
    static QString s_quoteBodySheet;
    static QString s_cachedTheme;
    const QString currentTheme = AppStyle::themeModeToString(AppStyle::currentThemeMode());
    if (s_cachedTheme != currentTheme) {
        s_cachedTheme = currentTheme;
        s_dateSepSheet = QStringLiteral(
            "QLabel#MsgDateSep { background: %1; color: %2; border-radius: 10px; "
            "padding: 4px 12px; font-size: 11px; }")
            .arg(AppStyle::surfaceMuted(), AppStyle::textSecondary());
        s_quoteSheet = QStringLiteral(
            "QFrame#MsgQuote { background: transparent; border-left: 3px solid %1; border-radius: 0px; padding-left: 4px; }")
            .arg(AppStyle::accent());
        s_quoteSenderSheet = QStringLiteral("color: %1;").arg(AppStyle::accent());
        s_quoteBodySheet = QStringLiteral("color: %1;").arg(AppStyle::textSecondary());
    }

    // 日期分隔
    if (m_dateSeparator) {
        m_dateSeparator->setStyleSheet(s_dateSepSheet);
    }

    // 气泡
    if (m_bubbleFrame && m_bubbleFrame->isVisible()
        && !m_bubbleFrame->styleSheet().contains(QStringLiteral("transparent"))) {
        m_bubbleFrame->setStyleSheet(textBubbleStyleSheet(m_outgoing));
    }

    // 引用块
    if (m_quoteFrame && m_quoteFrame->isVisible()) {
        m_quoteFrame->setStyleSheet(s_quoteSheet);
        m_quoteSenderLabel->setStyleSheet(s_quoteSenderSheet);
        m_quoteBodyLabel->setStyleSheet(s_quoteBodySheet);
    }

    // 撤回
    if (m_recalledLabel && m_recalledLabel->isVisible()) {
        m_recalledLabel->setStyleSheet(QStringLiteral(
            "QLabel#MsgRecalled { color: #808080; font-style: italic; }"));
    }

    // 正文样式
    if (m_bodyLabel && m_bodyLabel->isVisible()) {
        const QString fg = m_outgoing ? AppStyle::bubbleOutText() : AppStyle::bubbleInText();
        m_bodyLabel->setStyleSheet(QStringLiteral(
            "QLabel#MsgBodyLabel { background: transparent; border: none; color: %1; padding: 0; }").arg(fg));
    }
}

QColor MessageBubbleWidget::avatarFallbackColor(const QString& senderName)
{
    int h = 0;
    for (const QChar ch : senderName)
        h = (h * 31 + ch.unicode()) & 0x7FFF'FFFF;
    return kAvatarPalette[h % 6];
}

void MessageBubbleWidget::setSelected(bool selected)
{
    if (m_selected == selected) return;
    m_selected = selected;
    setProperty("selected", selected);
    style()->unpolish(this);
    style()->polish(this);
}

void MessageBubbleWidget::contextMenuEvent(QContextMenuEvent* event)
{
    emit contextMenuRequested(m_messageId, event->globalPos());
}

bool MessageBubbleWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_imagePreviewLabel
        && event->type() == QEvent::MouseButtonRelease
        && m_imagePreviewLabel
        && m_imagePreviewLabel->isVisible()
        && !m_messageId.trimmed().isEmpty()) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            emit messageFilePreviewRequested(m_messageId);
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}

QString MessageBubbleWidget::selectedText() const
{
    if (m_bodyLabel && m_bodyLabel->hasSelectedText())
        return m_bodyLabel->selectedText();
    return {};
}

bool MessageBubbleWidget::hasSelection() const
{
    return m_bodyLabel && m_bodyLabel->hasSelectedText();
}

void MessageBubbleWidget::clearSelection()
{
    // QLabel doesn't have a direct clearSelection, so we do nothing
    // The selection is lost when focus changes
}

bool MessageBubbleWidget::isFileCard() const
{
    return m_fileCardFrame && m_fileCardFrame->isVisible();
}

void MessageBubbleWidget::updateTransferProgress(int /*percent*/, const QString& statusText)
{
    if (m_transferStatusLabel) {
        m_transferStatusLabel->setText(statusText);
        m_transferStatusLabel->setVisible(!statusText.isEmpty());
    }
}

void MessageBubbleWidget::updateDeliveryState(int deliveryState, int groupReadCount, int activeMemberCount)
{
    if (!m_deliveryLabel) return;
    const QString text = deliveryIndicatorText(deliveryState, m_outgoing,
                                                groupReadCount, activeMemberCount);
    if (text.isEmpty()) {
        m_deliveryLabel->setVisible(false);
        return;
    }
    if (activeMemberCount > 0 && m_outgoing) {
        m_deliveryLabel->setText(QStringLiteral("<a href='#' style='color:%1;text-decoration:none'>%2</a>")
            .arg(AppStyle::textSecondary(), text.toHtmlEscaped()));
    } else {
        m_deliveryLabel->setText(text);
    }
    m_deliveryLabel->setVisible(true);
}

QString MessageBubbleDelegate_deliveryIndicatorTextForTesting(int state, bool outgoing,
                                                              int groupReadCount, int groupActiveMemberCount)
{
    return deliveryIndicatorText(state, outgoing, groupReadCount, groupActiveMemberCount);
}
