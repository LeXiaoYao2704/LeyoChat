#include "ui/MessageBubbleDelegate.h"

#include "domain/ChatMessage.h"
#include "app/ImageFileTypeHelpers.h"
#include "ui/AppStyle.h"
#include "ui/FilePreviewWidget.h"
#include "ui/MessageListModel.h"
#include "ui/MessageDeliveryPresentation.h"
#include "ui/MessageTextLinkifier.h"
#include "ui/StickerManager.h"
#include "ui/UiIcons.h"
#include "services/ResourceRefRouter.h"

#include <QFileInfo>
#include <QFile>
#include <QFontMetrics>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPixmapCache>
#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QTextCursor>
#include <QTextDocument>
#include <QRegularExpression>
#include <QUrl>

#include <memory>
#include <optional>

namespace {

QFont messageBodyFont(const QFont& base);
QFont messageMetaFont(const QFont& base);
bool isHtmlBody(const QString& body);
QString plainTextFromHtml(const QString& html);
int htmlTextHeight(const QString& html, int maxWidth, const QFont& baseFont);
int bubbleTextWidth(const QString& body, const QFontMetrics& metrics, int maxWidth);
bool isImageAttachment(const QString& attachmentName, const QString& localFilePath);
bool hasLocalAttachmentFile(const QString& localFilePath);
bool isAttachmentSummaryBody(const QString& body, bool isFile, bool isImageFile);
std::optional<ResourceRefPayload> parseResourceRefPayload(const QModelIndex& index);
QString displayBytesCompact(qint64 bytes);
qint64 transferDisplayBytesForState(FileTransferState state, qint64 bytesCompleted, qint64 fileSize);
int transferDisplayPercent(qint64 displayBytes, qint64 fileSize);
qreal transferDisplayRatio(qint64 displayBytes, qint64 fileSize);
QString transferStateSummary(FileTransferState state, qint64 bytesCompleted, qint64 fileSize,
                             qint64 speedBytesPerSec = 0);
int avatarDiameter();
int avatarGap();
int edgePadding();
int compactTransferCardHeight(const QFontMetrics& bodyMetrics, const QFontMetrics& metaMetrics);
int standardFileCardHeight(const QFontMetrics& bodyMetrics, const QFontMetrics& metaMetrics);
QSize inlinePreviewBubbleSize(const QString& localFilePath, int maxWidth, int maxHeight);
QPixmap loadAvatarPixmap(const QString& avatarPath, int targetSize);
QString avatarFallbackText(const QString& senderName);
void drawAvatar(QPainter* painter,
                const QRect& rect,
                const QString& avatarPath,
                const QString& senderName,
                const QColor& fallbackColor,
                const QColor& fallbackTextColor);

struct FileCardVisualPalette {
    QColor cardBg;
    QColor cardBorder;
    QColor iconBg;
    QColor titleColor;
    QColor metaColor;
    QColor chipBg;
    QColor chipText;
};

FileCardVisualPalette fileCardVisualPalette(bool outgoing, bool isImageFile)
{
    FileCardVisualPalette palette;
    const bool dark = AppStyle::isDarkTheme();
    // 文件卡背景使用半透明叠加，避免灰色与气泡底色冲突
    if (isImageFile) {
        palette.cardBg = QColor(AppStyle::accentSoft());
    } else if (outgoing) {
        palette.cardBg = dark ? QColor(255, 255, 255, 18) : QColor(255, 255, 255, 80);
    } else {
        palette.cardBg = dark ? QColor(255, 255, 255, 12) : QColor(255, 255, 255, 100);
    }
    palette.cardBorder = outgoing
                            ? (isImageFile ? QColor(AppStyle::borderStrong())
                                           : QColor(AppStyle::borderStrong()))
                            : QColor(AppStyle::border());
    palette.iconBg = outgoing
                        ? (isImageFile ? QColor(AppStyle::accentSoft())
                                       : QColor(AppStyle::selectedBg()))
                        : (dark ? QColor(AppStyle::accent()).lighter(140) : QColor(AppStyle::accent()).lighter(180));
    if (outgoing && !isImageFile) {
        palette.titleColor = QColor(AppStyle::textPrimary());
        palette.metaColor = QColor(AppStyle::textSecondary());
        palette.chipBg = dark ? QColor(255, 255, 255, 30) : QColor(255, 255, 255, 160);
        palette.chipText = QColor(AppStyle::accent());
    } else {
        palette.titleColor = outgoing ? QColor(AppStyle::bubbleOutText())
                                      : QColor(AppStyle::bubbleInText());
        palette.metaColor = outgoing
                                ? (isImageFile ? QColor(AppStyle::textSecondary())
                                               : QColor(AppStyle::textSecondary()))
                                : QColor(AppStyle::textSecondary());
        palette.chipBg = outgoing
                             ? (isImageFile ? QColor(AppStyle::accentSoft())
                                            : (dark ? QColor(255, 255, 255, 30) : QColor(255, 255, 255, 160)))
                             : (dark ? QColor(255, 255, 255, 25) : QColor(0, 0, 0, 12));
        palette.chipText = outgoing
                               ? QColor(AppStyle::accent())
                               : QColor(AppStyle::accent());
    }
    return palette;
}

int compactTransferCardHeight(const QFontMetrics& bodyMetrics, const QFontMetrics& metaMetrics)
{
    return qMax(90, bodyMetrics.height() + metaMetrics.height() * 2 + 44);
}

int standardFileCardHeight(const QFontMetrics& bodyMetrics, const QFontMetrics& metaMetrics)
{
    return qMax(88, bodyMetrics.height() + metaMetrics.height() * 2 + 30);
}

QString resolvedGroupFileCardLocalPath(const QModelIndex& index)
{
    const QString directPath = index.data(MessageListModel::LocalFilePathRole).toString().trimmed();
    if (!directPath.isEmpty()) {
        return directPath;
    }

    const QString fileCardJson = index.data(MessageListModel::FileCardJsonRole).toString().trimmed();
    if (fileCardJson.isEmpty()) {
        return {};
    }

    const QJsonObject cardObj = QJsonDocument::fromJson(fileCardJson.toUtf8()).object();
    const QString localPath = cardObj.value(QStringLiteral("local_path")).toString().trimmed();
    if (!localPath.isEmpty()) {
        return localPath;
    }

    if (index.data(MessageListModel::OutgoingRole).toBool()) {
        return cardObj.value(QStringLiteral("sender_file_path")).toString().trimmed();
    }

    return {};
}

bool groupFileCardUsesLocalActions(const QModelIndex& index)
{
    const QString localPath = resolvedGroupFileCardLocalPath(index);
    if (localPath.isEmpty()) {
        return false;
    }

    if (index.data(MessageListModel::OutgoingRole).toBool()) {
        return true;
    }

    return QFile::exists(localPath);
}

MessageBubbleDelegate::FileCardActionGeometry buildFileCardActionGeometry(
    const QStyleOptionViewItem& option,
    const QModelIndex& index)
{
    MessageBubbleDelegate::FileCardActionGeometry geometry;
    if (!index.isValid()) {
        return geometry;
    }

    const QString fileCardJson = index.data(MessageListModel::FileCardJsonRole).toString().trimmed();
    const bool isFile = index.data(MessageListModel::FileMessageRole).toBool();
    const bool isResourceReference = index.data(MessageListModel::ResourceReferenceRole).toBool();
    if (!fileCardJson.isEmpty()) {
        const bool outgoing = index.data(MessageListModel::OutgoingRole).toBool();
        const int maxBubbleWidth = option.rect.width() * AppStyle::kBubbleMaxWidthPct / 100;
        const QFont metaFont = messageMetaFont(option.font);
        const QFont bodyFont = messageBodyFont(option.font);
        const QFontMetrics metaMetrics(metaFont);
        const QFontMetrics bodyMetrics(bodyFont);
        const int cardBubbleWidth = qBound(280, qMin(maxBubbleWidth, 340), maxBubbleWidth);
        const int avatarInset = avatarDiameter() + avatarGap();
        const int bubbleX = outgoing
            ? option.rect.x() + option.rect.width() - edgePadding() - avatarInset - cardBubbleWidth
            : option.rect.x() + edgePadding() + avatarInset;
        int rowY = option.rect.y();
        if (index.data(MessageListModel::ShowDateSeparatorRole).toBool()) {
            rowY += messageMetaFont(option.font).pointSize() + 24;
        }
        const int nameY = rowY + 10;
        const int currentY = nameY + metaMetrics.height() + 6;
        const int cardH = standardFileCardHeight(bodyMetrics, metaMetrics);
        const QRect cardRect(bubbleX + 12, currentY + 8, cardBubbleWidth - 24, cardH);
        const int chipY = cardRect.bottom() - metaMetrics.height() - 12;
        geometry.hasActionChips = true;
        geometry.openFileChipRect = QRect(cardRect.x() + 12, chipY - 2, 84, metaMetrics.height() + 8);
        if (groupFileCardUsesLocalActions(index)) {
            geometry.openFolderChipRect =
                QRect(geometry.openFileChipRect.right() + 8, chipY - 2, 108, metaMetrics.height() + 8);
            const QJsonObject cardObj = QJsonDocument::fromJson(fileCardJson.toUtf8()).object();
            const QString cardFileName = cardObj.value(QStringLiteral("file_name")).toString();
            if (FilePreviewWidget::isPreviewSupported(cardFileName)) {
                geometry.hasPreviewChip = true;
                geometry.previewChipRect =
                    QRect(geometry.openFolderChipRect.right() + 8, chipY - 2, 60, metaMetrics.height() + 8);
            }
        }
        return geometry;
    }

    if (!isFile && !isResourceReference) {
        return geometry;
    }

    if (isResourceReference) {
        const auto payload = parseResourceRefPayload(index);
        if (!payload || payload->kind != QStringLiteral("shared_file")) {
            return geometry;
        }
        const bool outgoing = index.data(MessageListModel::OutgoingRole).toBool();
        const int rowX = option.rect.x();
        int rowY = option.rect.y();
        const int rowWidth = option.rect.width();
        const int maxBubbleWidth = rowWidth * AppStyle::kBubbleMaxWidthPct / 100;
        const int horizontalPadding = 12;
        const int verticalPadding = 8;
        const int rowRightPadding = 16;
        const QFont bodyFont = messageBodyFont(option.font);
        const QFont metaFont = messageMetaFont(option.font);
        const QFontMetrics bodyMetrics(bodyFont);
        const QFontMetrics metaMetrics(metaFont);
        if (index.data(MessageListModel::ShowDateSeparatorRole).toBool()) {
            rowY += messageMetaFont(option.font).pointSize() + 24;
        }
        const int bubbleWidth = qBound(280, qMin(maxBubbleWidth, 340), maxBubbleWidth);
        const int bubbleX = outgoing ? rowX + rowWidth - rowRightPadding - bubbleWidth
                                     : rowX + rowRightPadding;
        const int headerHeight = metaMetrics.height();
        const int currentY = rowY + 10 + headerHeight + 6;
        const int innerY = currentY + verticalPadding;
        const QRect cardRect(bubbleX + horizontalPadding,
                             innerY,
                             bubbleWidth - horizontalPadding * 2,
                             qMax(98, bodyMetrics.height() + metaMetrics.height() * 2 + 40));
        const int chipY = cardRect.bottom() - metaMetrics.height() - 12;
        geometry.hasResourceActionChips = true;
        geometry.resourceDownloadChipRect = QRect(cardRect.x() + 12, chipY - 2, 84, metaMetrics.height() + 8);
        geometry.resourceOpenChipRect = QRect(geometry.resourceDownloadChipRect.right() + 8, chipY - 2, 96, metaMetrics.height() + 8);
        return geometry;
    }

    const QString attachmentName = index.data(MessageListModel::AttachmentNameRole).toString();
    if (attachmentName.trimmed().isEmpty()) {
        return geometry;
    }

    const QString localFilePath = index.data(MessageListModel::LocalFilePathRole).toString();
    const QString body = index.data(MessageListModel::BodyRole).toString();
    const bool outgoing = index.data(MessageListModel::OutgoingRole).toBool();
    const int deliveryState = index.data(MessageListModel::DeliveryStateRole).toInt();
    const QString transferTaskId = index.data(MessageListModel::TransferTaskIdRole).toString();
    const bool transferCancelable = index.data(MessageListModel::TransferCancelableRole).toBool();
    const bool hasTransferState = !transferTaskId.trimmed().isEmpty();
    const auto transferState_ht = static_cast<FileTransferState>(
        index.data(MessageListModel::TransferStateRole).toInt());
    const bool hasLocalFile = hasLocalAttachmentFile(localFilePath);
    const bool fileReadyToOpen =
        hasLocalFile && (outgoing || deliveryState >= static_cast<int>(MessageDeliveryState::Received))
        && (!hasTransferState || transferState_ht == FileTransferState::Completed);
    if (!fileReadyToOpen && !hasTransferState) {
        return geometry;
    }
    const bool isImageFile = isImageAttachment(attachmentName, localFilePath);
    const bool compactTransferCard = hasTransferState && !isImageFile && !fileReadyToOpen;
    if (isImageFile) {
        return geometry;
    }

    const bool isFileSummaryBody = isAttachmentSummaryBody(body, isFile, isImageFile);
    const bool hasVisibleBody = !body.trimmed().isEmpty() && !isFileSummaryBody;
    const int rowX = option.rect.x();
    int rowY = option.rect.y();
    const int rowWidth = option.rect.width();
    if (index.data(MessageListModel::ShowDateSeparatorRole).toBool()) {
        rowY += messageMetaFont(option.font).pointSize() + 24;
    }
    const int maxBubbleWidth = rowWidth * AppStyle::kBubbleMaxWidthPct / 100;
    const int horizontalPadding = 12;
    const int rowSidePadding = edgePadding();
    const int verticalPadding = 8;
    const int avatarInset = avatarDiameter() + avatarGap();
    const QFont bodyFont = messageBodyFont(option.font);
    const QFont metaFont = messageMetaFont(option.font);
    const QFontMetrics bodyMetrics(bodyFont);
    const QFontMetrics metaMetrics(metaFont);
    const int textWidth = maxBubbleWidth - horizontalPadding * 2;
    int textHeight = 0;
    if (hasVisibleBody) {
        if (isHtmlBody(body)) {
            textHeight = htmlTextHeight(body, textWidth, bodyFont);
        } else {
            textHeight = bodyMetrics.boundingRect(QRect(0, 0, textWidth, 2000),
                                                  Qt::TextWordWrap,
                                                  body)
                             .height();
        }
    }
    int bubbleWidth = maxBubbleWidth;
    if (hasVisibleBody) {
        const QString widthText = isHtmlBody(body) ? plainTextFromHtml(body) : body;
        const int estimateWidth =
            bubbleTextWidth(widthText, bodyMetrics, textWidth) + horizontalPadding * 2;
        bubbleWidth = qMin(maxBubbleWidth, estimateWidth);
    }
    bubbleWidth = compactTransferCard
        ? qMax(bubbleWidth, qMin(maxBubbleWidth, 440))
        : qMax(bubbleWidth, qMin(maxBubbleWidth, 332));
    const int bubbleX = outgoing ? rowX + rowWidth - rowSidePadding - avatarInset - bubbleWidth
                                 : rowX + rowSidePadding + avatarInset;
    const int headerHeight = metaMetrics.height();
    int currentY = rowY + 10 + headerHeight + 6;
    const int bubbleContentHeight = (hasVisibleBody ? textHeight + 10 : 0) + 92;
    const int bubbleHeight = verticalPadding + bubbleContentHeight + verticalPadding + 2;
    int innerY = currentY + verticalPadding;
    // 引用回复块高度——与 paint() 保持一致
    const QString replyToMsgId_geo = index.data(MessageListModel::ReplyToMessageIdRole).toString();
    if (!replyToMsgId_geo.trimmed().isEmpty()) {
        const int quoteH = 6 + metaMetrics.height() + 2 + metaMetrics.height() + 6;
        innerY += quoteH + 4;
    }
    if (hasVisibleBody) {
        innerY += textHeight + 10;
    }

    const QRect cardRect(bubbleX + horizontalPadding,
                         innerY,
                         bubbleWidth - horizontalPadding * 2,
                         compactTransferCard
                             ? compactTransferCardHeight(bodyMetrics, metaMetrics)
                             : standardFileCardHeight(bodyMetrics, metaMetrics));
    if (compactTransferCard) {
        if (transferCancelable) {
            geometry.hasTransferCancelChip = true;
            geometry.transferCancelChipRect = QRect(cardRect.right() - 60,
                                                    cardRect.y() + 10,
                                                    48,
                                                    metaMetrics.height() + 10);
        }
        return geometry;
    }

    const int chipY = cardRect.bottom() - metaMetrics.height() - 12;
    geometry.hasActionChips = true;
    geometry.openFileChipRect = QRect(cardRect.x() + 12, chipY - 2, 84, metaMetrics.height() + 8);
    geometry.openFolderChipRect =
        QRect(geometry.openFileChipRect.right() + 8, chipY - 2, 108, metaMetrics.height() + 8);
    if (FilePreviewWidget::isPreviewSupported(attachmentName)) {
        geometry.hasPreviewChip = true;
        geometry.previewChipRect =
            QRect(geometry.openFolderChipRect.right() + 8, chipY - 2, 60, metaMetrics.height() + 8);
    }
    if (hasTransferState && transferCancelable) {
        geometry.hasTransferCancelChip = true;
        geometry.transferCancelChipRect = QRect(cardRect.right() - 74,
                                                cardRect.y() + 10,
                                                62,
                                                metaMetrics.height() + 8);
    }
    Q_UNUSED(bubbleHeight);
    return geometry;
}

struct MessageTextLayout {
    bool selectable = false;
    QRect textRect;
    std::shared_ptr<QTextDocument> document;
    QColor textColor;
};

QString deliveryStateText(int state)
{
    return messageDeliveryStateText(static_cast<MessageDeliveryState>(state));
}

QString deliveryIndicatorText(int state, bool outgoing,
                               int groupReadCount, int groupActiveMemberCount)
{
    return messageDeliveryIndicatorText(static_cast<MessageDeliveryState>(state),
                                         outgoing,
                                         groupReadCount,
                                         groupActiveMemberCount);
}

bool isHtmlBody(const QString& body)
{
    // 缓存 mightBeRichText 结果，避免 sizeHint+paint 每条消息重复调用
    static QHash<size_t, bool> sRichTextCache;
    const size_t key = qHash(body);
    auto it = sRichTextCache.constFind(key);
    if (it != sRichTextCache.constEnd()) {
        return it.value();
    }
    const bool result = Qt::mightBeRichText(body);
    if (sRichTextCache.size() > 2048) {
        sRichTextCache.clear();
    }
    sRichTextCache.insert(key, result);
    return result;
}

QString plainTextFromHtml(const QString& html)
{
    QTextDocument doc;
    doc.setHtml(html);
    return doc.toPlainText().trimmed();
}

int htmlTextHeight(const QString& html, int maxWidth, const QFont& baseFont)
{
    // 缓存 HTML 布局高度，避免在 sizeHint/paint 中反复创建 QTextDocument 解析 HTML
    // 使用 seed-hash 降低碰撞概率（原 XOR 方式碰撞率高）
    size_t cacheKey = qHash(html);
    cacheKey = cacheKey * 31 + static_cast<size_t>(maxWidth);
    cacheKey = cacheKey * 31 + qHash(baseFont.key());
    static QHash<std::size_t, int> sHtmlHeightCache;
    auto it = sHtmlHeightCache.constFind(cacheKey);
    if (it != sHtmlHeightCache.constEnd()) {
        return it.value();
    }

    QTextDocument doc;
    doc.setDefaultFont(baseFont);
    doc.setDefaultStyleSheet(QStringLiteral(
        "body{margin:0;color:%1;}"
        "p{margin:0 0 4px 0;}"
        "span,div{color:inherit;}"
        "a,a:link,a:visited,a *{color:%2 !important;text-decoration:underline;}"
        "*{background:transparent;}").arg(AppStyle::textPrimary(), AppStyle::accent()));
    doc.setTextWidth(maxWidth);
    doc.setHtml(html);
    // 高亮 @mention 以保持高度计算与渲染一致
    {
        static const QRegularExpression mentionRx(
            QStringLiteral("[@\uFF20]([^@\uFF20\\s]+)"));
        QTextCursor cursor(&doc);
        cursor.movePosition(QTextCursor::Start);
        while (!cursor.atEnd()) {
            cursor = doc.find(mentionRx, cursor);
            if (cursor.isNull()) break;
            QTextCharFormat fmt;
            fmt.setFontWeight(QFont::DemiBold);
            cursor.mergeCharFormat(fmt);
        }
    }
    const int height = static_cast<int>(doc.size().height());
    if (sHtmlHeightCache.size() > 2048) {
        sHtmlHeightCache.clear();
    }
    sHtmlHeightCache.insert(cacheKey, height);
    return height;
}

QFont messageBodyFont(const QFont& base)
{
    QFont font(base);
    if (font.pointSizeF() > 0.0) {
        font.setPointSizeF(qMax(10.5, font.pointSizeF()));
    } else if (font.pixelSize() > 0) {
        font.setPixelSize(qMax(14, font.pixelSize()));
    }
    return font;
}

QFont messageMetaFont(const QFont& base)
{
    return AppStyle::captionFont(base);
}

bool isEmojiOnlyBody(const QString& body)
{
    const QString trimmed = body.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > 6) {
        return false;
    }

    for (const QChar ch : trimmed) {
        if (ch.isSpace()) {
            return false;
        }
        if (ch.isLetterOrNumber()) {
            return false;
        }
        if (ch.unicode() < 0x2600 && ch.category() != QChar::Other_Surrogate) {
            return false;
        }
    }

    return true;
}

int bubbleTextWidth(const QString& body, const QFontMetrics& metrics, int maxWidth)
{
    if (body.trimmed().isEmpty()) {
        return qMin(maxWidth, 60);
    }

    if (isEmojiOnlyBody(body)) {
        return qBound(64, metrics.horizontalAdvance(body.trimmed()) + 26, qMin(maxWidth, 108));
    }

    const QRect bounds = metrics.boundingRect(QRect(0, 0, maxWidth, 2000),
                                              Qt::TextWordWrap,
                                              body);
    return qBound(60, bounds.width() + 28, maxWidth);
}

QString fileCacheFingerprint(const QString& localFilePath)
{
    if (localFilePath.trimmed().isEmpty()) {
        return {};
    }

    // 缓存文件指纹，避免在 sizeHint/paint 路径中反复调用 stat()
    static QHash<QString, QString> sFingerprintCache;
    auto it = sFingerprintCache.constFind(localFilePath);
    if (it != sFingerprintCache.constEnd()) {
        return it.value();
    }

    const QFileInfo info(localFilePath);
    if (!info.exists() || !info.isFile()) {
        return {};
    }

    const QString fp = QStringLiteral("%1|%2|%3")
        .arg(info.absoluteFilePath(),
             QString::number(info.size()),
             QString::number(info.lastModified().toMSecsSinceEpoch()));
    if (sFingerprintCache.size() > 1024) {
        sFingerprintCache.clear();
    }
    sFingerprintCache.insert(localFilePath, fp);
    return fp;
}

bool isImageAttachment(const QString& attachmentName, const QString& localFilePath)
{
    const QString fingerprint = fileCacheFingerprint(localFilePath);
    if (fingerprint.isEmpty()) {
        return isChatPreviewImageAttachmentName(attachmentName);
    }

    static QHash<QString, bool> imageTypeCache;
    const auto cached = imageTypeCache.constFind(fingerprint);
    if (cached != imageTypeCache.constEnd()) {
        return cached.value();
    }

    const bool isImage = !QImageReader::imageFormat(localFilePath).isEmpty();
    imageTypeCache.insert(fingerprint, isImage);
    // 缓存达上限时清除最早一半条目，而非全量清空（避免缓存效率归零）
    if (imageTypeCache.size() > 512) {
        auto keys = imageTypeCache.keys();
        for (int i = 0; i < keys.size() / 2; ++i) {
            imageTypeCache.remove(keys[i]);
        }
    }
    return isImage;
}

bool hasLocalAttachmentFile(const QString& localFilePath)
{
    const QFileInfo info(localFilePath);
    return info.exists() && info.isFile();
}

bool isAttachmentSummaryBody(const QString& body, bool isFile, bool isImageFile)
{
    if (!isFile) {
        return false;
    }

    const QString trimmed = body.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    if (trimmed.startsWith(QStringLiteral("[File]"), Qt::CaseInsensitive)) {
        return true;
    }
    if (isImageFile
        && (trimmed.startsWith(QStringLiteral("[Image]"), Qt::CaseInsensitive)
            || trimmed.startsWith(QStringLiteral("[图片]")))) {
        return true;
    }
    return false;
}

std::optional<ResourceRefPayload> parseResourceRefPayload(const QModelIndex& index)
{
    if (!index.isValid() || !index.data(MessageListModel::ResourceReferenceRole).toBool()) {
        return std::nullopt;
    }

    return ResourceRefRouter::parsePayload(index.data(MessageListModel::PayloadJsonRole).toByteArray());
}

QString resourceKindLabel(const QString& kind)
{
    const QString normalized = kind.trimmed().toLower();
    if (normalized == QStringLiteral("shared_file")) {
        return QStringLiteral("共享文件");
    }
    if (normalized == QStringLiteral("shared_doc")) {
        return QStringLiteral("共享文档");
    }
    if (normalized == QStringLiteral("connector")) {
        return QStringLiteral("连接器");
    }
    if (normalized == QStringLiteral("bot")) {
        return QStringLiteral("机器人");
    }
    if (normalized.isEmpty()) {
        return QStringLiteral("共享资源");
    }
    return kind.trimmed();
}

QPixmap loadInlinePreview(const QString& localFilePath, const QSize& targetSize)
{
    if (targetSize.width() <= 0 || targetSize.height() <= 0) {
        return {};
    }

    const QString fingerprint = fileCacheFingerprint(localFilePath);
    if (fingerprint.isEmpty()) {
        return {};
    }

    const QString cacheKey = QStringLiteral("bubble-preview|%1|%2x%3")
                                 .arg(fingerprint,
                                      QString::number(targetSize.width()),
                                      QString::number(targetSize.height()));
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached)) {
        return cached;
    }

    QImageReader reader(localFilePath);
    reader.setAutoTransform(true);
    // 在解码阶段就降采样，避免将全分辨率大图加载到内存后再缩放
    const QSize originalSize = reader.size();
    if (originalSize.isValid()
        && (originalSize.width() > targetSize.width() * 2
            || originalSize.height() > targetSize.height() * 2)) {
        reader.setScaledSize(originalSize.scaled(targetSize * 2,
                                                 Qt::KeepAspectRatio));
    }
    const QImage image = reader.read();
    if (image.isNull()) {
        return {};
    }

    const QPixmap scaledPreview = QPixmap::fromImage(image).scaled(
        targetSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    QPixmapCache::insert(cacheKey, scaledPreview);
    return scaledPreview;
}

QString displayBytesCompact(qint64 bytes)
{
    if (bytes < 0) {
        return QStringLiteral("--");
    }
    static const QStringList units = {QStringLiteral("B"),
                                      QStringLiteral("KB"),
                                      QStringLiteral("MB"),
                                      QStringLiteral("GB"),
                                      QStringLiteral("TB")};
    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < units.size() - 1) {
        value /= 1024.0;
        ++unitIndex;
    }
    const int precision = value >= 100.0 || unitIndex == 0 ? 0 : 1;
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', precision), units.at(unitIndex));
}

qint64 transferDisplayBytesForState(FileTransferState state, qint64 bytesCompleted, qint64 fileSize)
{
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

QString transferStateSummary(FileTransferState state, qint64 bytesCompleted, qint64 fileSize,
                             qint64 speedBytesPerSec)
{
    switch (state) {
    case FileTransferState::PendingOffer:
        return QStringLiteral("准备发送");
    case FileTransferState::WaitingAccept:
        return QStringLiteral("等待接收");
    case FileTransferState::ReadyToTransfer:
        return QStringLiteral("准备传输");
    case FileTransferState::Transferring:
        if (fileSize > 0) {
            QString text = QStringLiteral("%1 / %2")
                .arg(displayBytesCompact(bytesCompleted), displayBytesCompact(fileSize));
            if (speedBytesPerSec > 0) {
                text += QStringLiteral(" · %1/s").arg(displayBytesCompact(speedBytesPerSec));
                const qint64 remaining = fileSize - bytesCompleted;
                if (remaining > 0) {
                    const qint64 etaSec = remaining / speedBytesPerSec;
                    if (etaSec < 60) {
                        text += QStringLiteral(" · 约 %1s").arg(etaSec);
                    } else if (etaSec < 3600) {
                        text += QStringLiteral(" · 约 %1m%2s").arg(etaSec / 60).arg(etaSec % 60);
                    } else {
                        text += QStringLiteral(" · 约 %1h%2m").arg(etaSec / 3600).arg((etaSec % 3600) / 60);
                    }
                }
            }
            return text;
        }
        return QStringLiteral("正在传输");
    case FileTransferState::Paused:
        return QStringLiteral("已暂停");
    case FileTransferState::Interrupted:
        return QStringLiteral("传输中断");
    case FileTransferState::Completing:
        return QStringLiteral("正在完成");
    case FileTransferState::Completed:
        return QStringLiteral("已完成");
    case FileTransferState::Failed:
        return QStringLiteral("传输失败");
    case FileTransferState::Canceled:
        return QStringLiteral("已取消");
    }
    return QStringLiteral("准备发送");
}

int avatarDiameter()
{
    return 34;
}

int avatarGap()
{
    return 10;
}

int edgePadding()
{
    return 16;
}

constexpr int kImagePreviewMaxSize = 480;   // 纯图片气泡最大宽/高
constexpr int kImageThumbMaxWidth  = 280;   // 带文本时缩略图最大宽
constexpr int kImageThumbMaxHeight = 200;   // 带文本时缩略图最大高

QSize inlinePreviewBubbleSize(const QString& localFilePath, int maxWidth, int maxHeight)
{
    if (maxWidth <= 0 || maxHeight <= 0) {
        return {qMax(1, maxWidth), qMax(1, maxHeight)};
    }

    // 缓存原始图片尺寸，避免每次 sizeHint/paint 都访问磁盘
    static QHash<QString, QSize> sImageSizeCache;
    QSize imageSize;
    auto it = sImageSizeCache.constFind(localFilePath);
    if (it != sImageSizeCache.constEnd()) {
        imageSize = it.value();
    } else {
        QImageReader reader(localFilePath);
        reader.setAutoTransform(true);
        imageSize = reader.size();
        // 只缓存有效尺寸：文件部分下载时 size() 可能返回无效值，
        // 若此时缓存则后续调用永远使用错误 fallback，导致气泡高度与图片实际高度不符（空蓝区）
        if (imageSize.isValid() && imageSize.width() > 0 && imageSize.height() > 0) {
            sImageSizeCache.insert(localFilePath, imageSize);
        }
    }
    if (!imageSize.isValid() || imageSize.width() <= 0 || imageSize.height() <= 0) {
        return QSize(qMin(maxWidth, 240), qMin(maxHeight, 180));
    }

    if (imageSize.width() > maxWidth || imageSize.height() > maxHeight) {
        imageSize.scale(maxWidth, maxHeight, Qt::KeepAspectRatio);
    }
    imageSize.setWidth(qMax(72, imageSize.width()));
    imageSize.setHeight(qMax(56, imageSize.height()));
    return imageSize;
}

QPixmap loadAvatarPixmap(const QString& avatarPath, int targetSize)
{
    if (targetSize <= 0 || avatarPath.trimmed().isEmpty()) {
        return {};
    }
    const QString cacheKey = QStringLiteral("message-avatar|%1|%2").arg(avatarPath, QString::number(targetSize));
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached)) {
        return cached;
    }

    QImageReader reader(avatarPath);
    reader.setAutoTransform(true);
    // 在解码阶段就缩放到目标尺寸，避免加载完整分辨率图像
    const QSize originalSize = reader.size();
    if (originalSize.isValid()
        && (originalSize.width() > targetSize * 2
            || originalSize.height() > targetSize * 2)) {
        reader.setScaledSize(QSize(targetSize * 2, targetSize * 2));
    }
    const QImage image = reader.read();
    if (image.isNull()) {
        return {};
    }

    const QPixmap scaled = QPixmap::fromImage(image).scaled(targetSize,
                                                            targetSize,
                                                            Qt::KeepAspectRatioByExpanding,
                                                            Qt::SmoothTransformation);
    QPixmapCache::insert(cacheKey, scaled);
    return scaled;
}

QString avatarFallbackText(const QString& senderName)
{
    const QString trimmed = senderName.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("?");
    }
    return trimmed.left(1).toUpper();
}

void drawAvatar(QPainter* painter,
                const QRect& rect,
                const QString& avatarPath,
                const QString& senderName,
                const QColor& fallbackColor,
                const QColor& fallbackTextColor)
{
    if (!painter || !rect.isValid()) {
        return;
    }

    QPainterPath clipPath;
    clipPath.addEllipse(rect);
    const QPixmap avatar = loadAvatarPixmap(avatarPath, qMax(rect.width(), rect.height()));
    if (!avatar.isNull()) {
        painter->save();
        painter->setClipPath(clipPath);
        painter->drawPixmap(rect, avatar);
        painter->restore();
        return;
    }

    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(fallbackColor);
    painter->drawEllipse(rect);
    QFont avatarFont = painter->font();
    avatarFont.setBold(true);
    avatarFont.setPointSizeF(qMax(9.0, avatarFont.pointSizeF() - 0.5));
    painter->setFont(avatarFont);
    painter->setPen(fallbackTextColor);
    painter->drawText(rect, Qt::AlignCenter, avatarFallbackText(senderName));
    painter->restore();
}

MessageTextLayout buildMessageTextLayout(const QStyleOptionViewItem& option,
                                         const QModelIndex& index)
{
    MessageTextLayout layout;
    if (!index.isValid()) {
        return layout;
    }

    const bool outgoing = index.data(MessageListModel::OutgoingRole).toBool();
    const QString body = index.data(MessageListModel::BodyRole).toString();
    const bool isFile = index.data(MessageListModel::FileMessageRole).toBool();
    const bool isResourceReference = index.data(MessageListModel::ResourceReferenceRole).toBool();
    const QString attachmentName = index.data(MessageListModel::AttachmentNameRole).toString();
    const QString localFilePath = index.data(MessageListModel::LocalFilePathRole).toString();
    const bool isImageFile = isFile && isImageAttachment(attachmentName, localFilePath);
    const bool isFileSummaryBody = isAttachmentSummaryBody(body, isFile, isImageFile);
    const bool hasVisibleBody = !isResourceReference && !body.trimmed().isEmpty() && !isFileSummaryBody;
    if (!hasVisibleBody || isFile || isResourceReference) {
        return layout;
    }

    const int rowX = option.rect.x();
    const int rowY = option.rect.y();
    const int rowWidth = option.rect.width();
    const int maxBubbleWidth = rowWidth * AppStyle::kBubbleMaxWidthPct / 100;
    const int horizontalPadding = 12;
    const int verticalPadding = 8;
    const int rowSidePadding = edgePadding();
    const int avatarInset = avatarDiameter() + avatarGap();

    const QFont bodyFont = messageBodyFont(option.font);
    const QFont metaFont = messageMetaFont(option.font);
    const QFontMetrics bodyMetrics(bodyFont);
    const QFontMetrics metaMetrics(metaFont);

    const int textWidth = maxBubbleWidth - horizontalPadding * 2;
    const int textHeight = isHtmlBody(body)
                               ? htmlTextHeight(body, textWidth, bodyFont)
                               : bodyMetrics.boundingRect(QRect(0, 0, textWidth, 2000),
                                                          Qt::TextWordWrap,
                                                          body)
                                     .height();

    const QString widthText = isHtmlBody(body) ? plainTextFromHtml(body) : body;
    const int estimateWidth =
        bubbleTextWidth(widthText, bodyMetrics, textWidth) + horizontalPadding * 2;
    const int bubbleWidth = qMin(maxBubbleWidth, estimateWidth);
    const int bubbleX = outgoing ? rowX + rowWidth - rowSidePadding - avatarInset - bubbleWidth
                                 : rowX + rowSidePadding + avatarInset;
    const int headerHeight = metaMetrics.height();

    // Date separator offset: paint() 中 rowY 会加上 dateSepH，hitTest 必须匹配
    const bool showDateSep = index.data(MessageListModel::ShowDateSeparatorRole).toBool();
    const int dateSepH = showDateSep ? (metaFont.pointSize() + 24) : 0;

    // Reply quote block offset: paint() 中 innerY 会加上 quoteH + 4
    const QString replyToMsgId = index.data(MessageListModel::ReplyToMessageIdRole).toString();
    const int quoteBlockH = replyToMsgId.trimmed().isEmpty() ? 0
        : (6 + metaMetrics.height() + 2 + metaMetrics.height() + 6 + 4);

    const int contentTop = rowY + dateSepH + 10 + headerHeight + 6;

    layout.selectable = true;
    layout.textRect = QRect(bubbleX + horizontalPadding,
                            contentTop + verticalPadding + quoteBlockH,
                            bubbleWidth - horizontalPadding * 2,
                            textHeight);
    const bool softenOutgoingImageBubble = outgoing && isImageFile;
    layout.textColor = softenOutgoingImageBubble
                           ? QColor(AppStyle::textPrimary())
                           : (outgoing ? QColor(AppStyle::bubbleOutText())
                                       : QColor(AppStyle::bubbleInText()));

    // QTextDocument 缓存：避免每次 paint/editorEvent 都重新解析 HTML + 应用 mention 高亮。
    // 缓存 key = (messageId, body 哈希, textWidth, outgoing)，命中时直接复用 shared_ptr。
    const QString messageId = index.data(MessageListModel::MessageIdRole).toString();
    struct CachedDoc {
        std::shared_ptr<QTextDocument> document;
        QColor textColor;
    };
    static QHash<std::size_t, CachedDoc> sDocCache;
    const auto docCacheKey = qHash(messageId) ^ qHash(body) ^ static_cast<std::size_t>(textWidth)
                             ^ (outgoing ? std::size_t(0x9e3779b9) : std::size_t(0));
    auto cacheIt = sDocCache.constFind(docCacheKey);
    if (cacheIt != sDocCache.constEnd() && cacheIt->textColor == layout.textColor) {
        layout.document = cacheIt->document;
        return layout;
    }

    // 限制缓存大小，防止无限增长
    if (sDocCache.size() > 200) {
        sDocCache.clear();
    }

    layout.document = std::make_shared<QTextDocument>();
    layout.document->setDefaultFont(bodyFont);
    layout.document->setDefaultStyleSheet(QStringLiteral(
        "body{margin:0;color:%1;}"
        "p{margin:0 0 4px 0;}"
        "span,div{color:inherit;}"
        "a,a:link,a:visited,a *{color:%2 !important;text-decoration:underline;}"
        "*{background:transparent;}").arg(layout.textColor.name(), AppStyle::accent()));
    layout.document->setTextWidth(layout.textRect.width());
    if (isHtmlBody(body)) {
        layout.document->setHtml(body);
        MessageTextLinkifier::applyAutoLinksToDocument(layout.document.get(), AppStyle::accent());
    } else {
        layout.document->setHtml(
            MessageTextLinkifier::plainTextWithAutoLinksToHtml(body));
    }
    // 高亮 @mention 文本
    {
        static const QRegularExpression mentionRx(
            QStringLiteral("[@\uFF20]([^@\uFF20\\s]+)"));
        QTextCursor cursor(layout.document.get());
        cursor.movePosition(QTextCursor::Start);
        // 发送方和接收方气泡均为浅色，@提及统一用蓝色高亮
        const QColor mentionColor = QColor(AppStyle::accent());
        while (!cursor.atEnd()) {
            cursor = layout.document->find(mentionRx, cursor);
            if (cursor.isNull()) break;
            QTextCharFormat fmt;
            fmt.setForeground(mentionColor);
            fmt.setFontWeight(QFont::DemiBold);
            if (outgoing) {
                fmt.setFontUnderline(true);
            }
            cursor.mergeCharFormat(fmt);
        }
    }
    sDocCache.insert(docCacheKey, CachedDoc{layout.document, layout.textColor});
    return layout;
}

} // namespace

MessageBubbleDelegate::MessageBubbleDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QColor MessageBubbleDelegate::fileCardBackgroundColorForTesting(bool outgoing, bool isImageFile)
{
    return fileCardVisualPalette(outgoing, isImageFile).cardBg;
}

QColor MessageBubbleDelegate::fileCardTitleColorForTesting(bool outgoing, bool isImageFile)
{
    return fileCardVisualPalette(outgoing, isImageFile).titleColor;
}

MessageBubbleDelegate::FileCardActionGeometry MessageBubbleDelegate::fileCardActionGeometryForTesting(
    const QStyleOptionViewItem& option,
    const QModelIndex& index)
{
    return buildFileCardActionGeometry(option, index);
}

int MessageBubbleDelegate::bubbleMaxWidth(int viewWidth)
{
    const int width = viewWidth > 0 ? viewWidth : 600;
    return width * AppStyle::kBubbleMaxWidthPct / 100;
}

QSize MessageBubbleDelegate::sizeHint(const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const
{
    if (!index.isValid()) {
        const QFontMetrics fm(messageBodyFont(option.font));
        return {option.rect.width(), qMax(56, fm.height() + 44)};
    }

    // 缓存 sizeHint 结果：避免在滚动时对同一消息反复执行 ~120 行计算
    static QHash<std::size_t, QSize> sSizeHintCache;
    const QString msgId = index.data(MessageListModel::MessageIdRole).toString();
    const int viewWidth = option.rect.width();
    // 视图未布局时 width 为 0，不缓存也不计算（返回占位高度）
    if (viewWidth <= 0) {
        return {1, 60};
    }
    // 将传输状态和投递状态纳入 key，确保状态变化时缓存自动失效
    const int transferHint = index.data(MessageListModel::TransferStateRole).toInt();
    const int deliveryHint = index.data(MessageListModel::DeliveryStateRole).toInt();
    const QString reactionsJson_sz = index.data(MessageListModel::ReactionsJsonRole).toString();
    const bool hasReactions_sz = !reactionsJson_sz.isEmpty();
    // 图片文件可能在消息首次布局时尚未落盘，随后在同一路径变为可读。
    // 此时消息 id、宽度及传输/投递状态都可能不变，不能复用占位图的旧行高。
    // 图片尺寸本身由 inlinePreviewBubbleSize 的内部缓存负责，跳过外层行高缓存即可。
    const bool imageMessageForSizing =
        index.data(MessageListModel::FileMessageRole).toBool()
        && isImageAttachment(
            index.data(MessageListModel::AttachmentNameRole).toString(),
            index.data(MessageListModel::LocalFilePathRole).toString());
    const bool allowSizeHintCache = !imageMessageForSizing;
    const auto cacheKey = qHash(msgId) ^ static_cast<std::size_t>(viewWidth)
                        ^ (static_cast<std::size_t>(transferHint) << 8)
                        ^ (static_cast<std::size_t>(deliveryHint) << 16)
                        ^ (hasReactions_sz ? std::size_t{1} << 32 : std::size_t{0});
    if (!msgId.isEmpty() && allowSizeHintCache) {
        auto it = sSizeHintCache.constFind(cacheKey);
        if (it != sSizeHintCache.constEnd()) {
            return it.value();
        }
    }

    // Reaction pills 额外高度（pill 行高 22px + 间距 6px）
    const int reactionExtraHeight = hasReactions_sz ? 28 : 0;

    const auto cacheAndReturn = [&](const QSize& sz) -> QSize {
        const QSize finalSz(sz.width(), sz.height() + reactionExtraHeight);
        // 仅缓存高度合理的结果；异常小值（如布局期间 viewWidth 瞬时为边界值）不写入缓存，
        // 避免后续帧持续使用错误高度导致消息重叠。
        if (!msgId.isEmpty() && allowSizeHintCache && finalSz.height() >= 40) {
            sSizeHintCache.insert(cacheKey, finalSz);
        }
        return finalSz;
    };

    const bool isRecalled = index.data(MessageListModel::RecalledRole).toBool();
    const bool showDateSeparator = index.data(MessageListModel::ShowDateSeparatorRole).toBool();
    const int dateSeparatorHeight = showDateSeparator ? (messageMetaFont(option.font).pointSize() + 24) : 0;
    if (isRecalled) {
        const QFontMetrics metaFm(messageMetaFont(option.font));
        const int totalHeight = 10 + metaFm.height() + 6 + metaFm.height() + 10 + metaFm.height() + 14;
        return cacheAndReturn({option.rect.width(), qMax(totalHeight, metaFm.height() * 3 + 40) + dateSeparatorHeight});
    }

    // System notification message: centered, compact
    const QString messageType_sz = index.data(MessageListModel::MessageTypeRole).toString();
    if (messageType_sz == QStringLiteral("system")) {
        const QFont sysFont = messageMetaFont(option.font);
        const QFontMetrics sysFm(sysFont);
        const QString sysBody = index.data(MessageListModel::BodyRole).toString();
        const int textW = option.rect.width() - 80;
        const QRect textBounds = sysFm.boundingRect(QRect(0, 0, textW, 2000), Qt::TextWordWrap, sysBody);
        const int totalHeight = 8 + sysFm.height() + 4 + textBounds.height() + 8;
        return cacheAndReturn({option.rect.width(), qMax(totalHeight, sysFm.height() * 2 + 24) + dateSeparatorHeight});
    }

    // 贴纸消息：固定高度
    if (messageType_sz == QStringLiteral("sticker")) {
        const QFontMetrics metaFmSt(messageMetaFont(option.font));
        // header(name+time) + gap + sticker(120) + gap + delivery + bottom
        const int totalH = 6 + metaFmSt.height() + 4 + 120 + 2 + metaFmSt.height() + 4;
        return cacheAndReturn({option.rect.width(), totalH + dateSeparatorHeight});
    }

    // 通话记录消息：居中系统消息样式
    if (messageType_sz == QStringLiteral("call_record")) {
        const QFont sysFont = messageMetaFont(option.font);
        const QFontMetrics sysFm(sysFont);
        const int totalHeight = 8 + sysFm.height() + 8;
        return cacheAndReturn({option.rect.width(), qMax(totalHeight, 32) + dateSeparatorHeight});
    }

    // 合并转发卡片：固定高度卡片
    if (messageType_sz == QStringLiteral("forward_package")) {
        const QFont metaFontFp = messageMetaFont(option.font);
        const QFontMetrics metaFmFp(metaFontFp);
        const QFont bodyFontFp = messageBodyFont(option.font);
        const QFontMetrics bodyFmFp(bodyFontFp);
        // header + card(title + 3 preview lines + separator + count) + delivery
        const int cardH = 8 + bodyFmFp.height() + 4 + metaFmFp.height() * 3 + 12 + 1 + 4 + metaFmFp.height() + 8;
        const int totalH = 6 + metaFmFp.height() + 4 + cardH + 4 + metaFmFp.height() + 6;
        return cacheAndReturn({option.rect.width(), totalH + dateSeparatorHeight});
    }

    // 群文件卡片（FileService/P2P Offer-only）
    const QString fileCardJson_sz = index.data(MessageListModel::FileCardJsonRole).toString();
    if (!fileCardJson_sz.trimmed().isEmpty()) {
        const QFont bodyFontSz = messageBodyFont(option.font);
        const QFont metaFontSz = messageMetaFont(option.font);
        const QFontMetrics bodyFmSz(bodyFontSz);
        const QFontMetrics metaFmSz(metaFontSz);
        const int cardH = standardFileCardHeight(bodyFmSz, metaFmSz);
        const int totalHeight = 10 + metaFmSz.height() + 6 + cardH + 8 + metaFmSz.height() + 10;
        return cacheAndReturn({option.rect.width(),
                qMax(totalHeight, cardH + metaFmSz.height() * 2 + 38) + dateSeparatorHeight});
    }

    const QString body = index.data(MessageListModel::BodyRole).toString();
    const bool isFile = index.data(MessageListModel::FileMessageRole).toBool();
    const bool isResourceReference = index.data(MessageListModel::ResourceReferenceRole).toBool();
    const QString attachmentName = index.data(MessageListModel::AttachmentNameRole).toString();
    const QString localFilePath = index.data(MessageListModel::LocalFilePathRole).toString();
    const bool isImageFile = isFile && isImageAttachment(attachmentName, localFilePath);
    const bool isFileSummaryBody = isAttachmentSummaryBody(body, isFile, isImageFile);
    const bool hasVisibleBody = !isResourceReference && !body.trimmed().isEmpty() && !isFileSummaryBody;
    const bool isEmojiOnly = !isFile && isEmojiOnlyBody(body);
    const bool hasLocalPreview = isImageFile && hasLocalAttachmentFile(localFilePath);
    const bool hasTransferState =
        !index.data(MessageListModel::TransferTaskIdRole).toString().trimmed().isEmpty();
    const auto transferState_sz = static_cast<FileTransferState>(
        index.data(MessageListModel::TransferStateRole).toInt());
    const bool outgoing_sz = index.data(MessageListModel::OutgoingRole).toBool();
    const int delivState_sz = index.data(MessageListModel::DeliveryStateRole).toInt();
    const bool hasLocalFile_sz = hasLocalAttachmentFile(localFilePath);
    const bool fileReadyToOpen_sz =
        hasLocalFile_sz && (outgoing_sz || delivState_sz >= static_cast<int>(MessageDeliveryState::Received))
        && (!hasTransferState || transferState_sz == FileTransferState::Completed);
    const bool compactTransferCard = hasTransferState && !isImageFile && !fileReadyToOpen_sz;
    const bool pureImageBubble = isImageFile && hasLocalPreview
        && (!hasTransferState || transferState_sz == FileTransferState::Completed || fileReadyToOpen_sz);
    const bool hasAvatar = !index.data(MessageListModel::SenderAvatarPathRole).toString().trimmed().isEmpty();
    const int maxBubbleWidth = bubbleMaxWidth(option.rect.width()) - 24;

    const QFont bodyFont = messageBodyFont(option.font);
    const QFont metaFont = messageMetaFont(option.font);
    const QFontMetrics fm(bodyFont);
    const QFontMetrics metaMetrics(metaFont);

    int textHeight = 0;
    if (hasVisibleBody) {
        if (isEmojiOnly) {
            textHeight = qMax(fm.height(), 26);
        } else if (isHtmlBody(body)) {
            textHeight = htmlTextHeight(body, maxBubbleWidth - 24, bodyFont);
        } else {
            const QRect textBounds = fm.boundingRect(QRect(0, 0, maxBubbleWidth, 2000),
                                                     Qt::TextWordWrap,
                                                     body);
            textHeight = textBounds.height();
        }
    }

    // Reply quote block height
    const QString replyToMsgId_sz = index.data(MessageListModel::ReplyToMessageIdRole).toString();
    int quoteBlockHeight = 0;
    if (!replyToMsgId_sz.trimmed().isEmpty()) {
        const QFont quoteFont = messageMetaFont(option.font);
        const QFontMetrics quoteFm(quoteFont);
        // "回复 张三:" line + body preview line + padding
        quoteBlockHeight = 6 + quoteFm.height() + 2 + quoteFm.height() + 6 + 4;
    }

    if (isFile) {
        const int fileCardHeight = pureImageBubble
                                       ? inlinePreviewBubbleSize(localFilePath,
                                                                 qMin(maxBubbleWidth, kImagePreviewMaxSize),
                                                                 kImagePreviewMaxSize).height()
                                       : compactTransferCard
                                             ? compactTransferCardHeight(fm, metaMetrics)
                                       : (isImageFile
                                              ? (hasLocalPreview
                                                     ? inlinePreviewBubbleSize(localFilePath,
                                                                               qMin(maxBubbleWidth, kImageThumbMaxWidth),
                                                                               kImageThumbMaxHeight).height() + 16
                                                     : 96)
                                              : standardFileCardHeight(fm, metaMetrics));
        const int bubbleContentHeight = (hasVisibleBody ? textHeight + 10 : 0) + fileCardHeight + quoteBlockHeight;
        const int totalHeight = pureImageBubble
            ? 6 + metaMetrics.height() + 4 + bubbleContentHeight + 2 + metaMetrics.height() + 4
            : 10 + metaMetrics.height() + 6 + bubbleContentHeight + 8
                + metaMetrics.height() + 10;
        const int minFileHeight = pureImageBubble
            ? totalHeight
            : qMax(totalHeight, fileCardHeight + metaMetrics.height() * 2 + 38);
        return cacheAndReturn({option.rect.width(), minFileHeight + dateSeparatorHeight});
    }

    if (isResourceReference) {
        const int resourceCardHeight = qMax(98, fm.height() + metaMetrics.height() * 2 + 40);
        const int totalHeight = 10 + metaMetrics.height() + 6 + resourceCardHeight + 8
                              + metaMetrics.height() + 10;
        return cacheAndReturn({option.rect.width(),
                qMax(totalHeight, resourceCardHeight + metaMetrics.height() * 2 + 38) + dateSeparatorHeight});
    }

    const int totalHeight = 10 + metaMetrics.height() + 6 + quoteBlockHeight + textHeight + 10 + metaMetrics.height() + 14;
    const int minimumHeight = isEmojiOnly
                                  ? (metaMetrics.height() * 2 + 66)
                                  : (metaMetrics.height() * 2 + fm.height() + 30);
    return cacheAndReturn({option.rect.width(),
            qMax(qMax(totalHeight, minimumHeight), hasAvatar ? avatarDiameter() + 28 : minimumHeight) + dateSeparatorHeight});
}

QString MessageBubbleDelegate::selectedText() const
{
    return m_selectionState.text;
}

bool MessageBubbleDelegate::hasSelection() const
{
    return m_selectionState.index.isValid()
        && m_selectionState.anchor >= 0
        && m_selectionState.cursor >= 0
        && m_selectionState.anchor != m_selectionState.cursor
        && !m_selectionState.text.isEmpty();
}

void MessageBubbleDelegate::clearSelection()
{
    m_selectionState = {};
}


bool MessageBubbleDelegate::isPointInPureImageBubble(const QModelIndex& index,
                                                     const QPoint& viewportPos) const
{
    if (!index.isValid()) {
        return false;
    }

    const bool isFile = index.data(MessageListModel::FileMessageRole).toBool();
    const QString attachmentName = index.data(MessageListModel::AttachmentNameRole).toString();
    const QString localFilePath = index.data(MessageListModel::LocalFilePathRole).toString();
    const bool isImage = isFile && isImageAttachment(attachmentName, localFilePath);
    const bool hasLocal = isImage && hasLocalAttachmentFile(localFilePath);
    const bool outgoing = index.data(MessageListModel::OutgoingRole).toBool();
    const int deliveryState = index.data(MessageListModel::DeliveryStateRole).toInt();
    const QString transferTaskId = index.data(MessageListModel::TransferTaskIdRole).toString();
    const bool hasTransfer = !transferTaskId.trimmed().isEmpty();
    const auto transferState = static_cast<FileTransferState>(
        index.data(MessageListModel::TransferStateRole).toInt());
    const bool fileReady = hasLocal
        && (outgoing || deliveryState >= static_cast<int>(MessageDeliveryState::Received));
    const bool pureImage = isImage && hasLocal
        && (!hasTransfer || transferState == FileTransferState::Completed || fileReady);
    if (!pureImage) {
        return false;
    }

    const QPersistentModelIndex pIdx(index);
    auto it = m_pureImageBubbleRects.constFind(pIdx);
    if (it == m_pureImageBubbleRects.constEnd()) {
        return false;
    }
    return it.value().contains(viewportPos);
}
bool MessageBubbleDelegate::helpEvent(QHelpEvent* event,
                                      QAbstractItemView* view,
                                      const QStyleOptionViewItem& option,
                                      const QModelIndex& index)
{
    if (event && event->type() == QEvent::ToolTip && index.isValid()) {
        const QPersistentModelIndex pIdx(index);
        if (m_reactionPillRects.contains(pIdx)) {
            const auto& pills = m_reactionPillRects.value(pIdx);
            const QPoint pos = event->pos();
            for (const auto& pill : pills) {
                if (pill.first.contains(pos)) {
                    // 解析 reactions JSON 获取该 emoji 的参与者列表
                    const QString reactionsJson = index.data(MessageListModel::ReactionsJsonRole).toString();
                    const QJsonObject reactions = QJsonDocument::fromJson(reactionsJson.toUtf8()).object();
                    const QJsonArray reactors = reactions.value(pill.second).toArray();
                    QStringList names;
                    names.reserve(reactors.size());
                    for (const auto& r : reactors) {
                        const QString clientId = r.toString();
                        if (m_nameResolver) {
                            names << m_nameResolver(clientId);
                        } else {
                            names << clientId;
                        }
                    }
                    QToolTip::showText(event->globalPos(), names.join(QStringLiteral("\n")), view);
                    return true;
                }
            }
        }
    }
    QToolTip::hideText();
    return true;
}

bool MessageBubbleDelegate::editorEvent(QEvent* event,
                                        QAbstractItemModel* model,
                                        const QStyleOptionViewItem& option,
                                        const QModelIndex& index)
{
    Q_UNUSED(model);

    if (event && event->type() == QEvent::MouseButtonRelease && index.isValid()) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            // 头像点击检测
            const QPersistentModelIndex pIdx(index);
            if (m_avatarRects.contains(pIdx)) {
                const QRect avatarRect = m_avatarRects.value(pIdx);
                if (avatarRect.contains(mouseEvent->pos())) {
                    const bool isOutgoing = index.data(MessageListModel::OutgoingRole).toBool();
                    if (!isOutgoing) {
                        const QString senderId = index.data(MessageListModel::SenderIdRole).toString();
                        if (!senderId.trimmed().isEmpty()) {
                            const QPoint globalPos = static_cast<const QWidget*>(option.widget)
                                ->mapToGlobal(mouseEvent->pos());
                            emit avatarClicked(senderId, globalPos);
                            return true;
                        }
                    }
                }
            }
            // Check click on group read receipt indicator
            const QPersistentModelIndex persistentIndex(index);
            // 合并转发卡片点击检测
            if (m_forwardCardRects.contains(pIdx)) {
                const QRect fwdRect = m_forwardCardRects.value(pIdx);
                if (fwdRect.contains(mouseEvent->pos())) {
                    const QString msgId_fwd = index.data(MessageListModel::MessageIdRole).toString();
                    emit forwardCardClicked(msgId_fwd);
                    return true;
                }
            }
            // 回复引用块点击检测 → 跳转到原消息
            if (m_replyQuoteRects.contains(pIdx)) {
                const QRect quoteRect = m_replyQuoteRects.value(pIdx);
                if (quoteRect.contains(mouseEvent->pos())) {
                    const QString replyToId = index.data(MessageListModel::ReplyToMessageIdRole).toString();
                    if (!replyToId.trimmed().isEmpty()) {
                        emit replyQuoteClicked(replyToId);
                        return true;
                    }
                }
            }
            // Reaction pill 点击检测
            if (m_reactionPillRects.contains(pIdx)) {
                const auto& pills = m_reactionPillRects.value(pIdx);
                for (const auto& pill : pills) {
                    if (pill.first.contains(mouseEvent->pos())) {
                        const QString msgId_rx = index.data(MessageListModel::MessageIdRole).toString();
                        emit const_cast<MessageBubbleDelegate*>(this)->reactionToggled(msgId_rx, pill.second);
                        return true;
                    }
                }
            }
            if (m_deliveryTextRects.contains(persistentIndex)) {
                const QRect deliveryRect = m_deliveryTextRects.value(persistentIndex);
                if (deliveryRect.contains(mouseEvent->pos())) {
                    const int groupActiveMemberCount = index.data(MessageListModel::GroupActiveMemberCountRole).toInt();
                    if (groupActiveMemberCount > 0) {
                        const QString messageId = index.data(MessageListModel::MessageIdRole).toString();
                        emit readReceiptDetailRequested(messageId);
                        return true;
                    }
                }
            }
            // 群文件卡片: 下载/打开 按钮点击
            const QString fileCardJson_ev = index.data(MessageListModel::FileCardJsonRole).toString();
            if (!fileCardJson_ev.trimmed().isEmpty()) {
                const auto geometry = buildFileCardActionGeometry(option, index);
                if (geometry.hasActionChips && geometry.openFileChipRect.contains(mouseEvent->pos())) {
                    const QString msgId_ev = index.data(MessageListModel::MessageIdRole).toString();
                    if (groupFileCardUsesLocalActions(index)) {
                        emit messageFileOpenRequested(msgId_ev);
                    } else {
                        emit messageFileDownloadRequested(msgId_ev);
                    }
                    return true;
                }
                if (geometry.hasActionChips && geometry.openFolderChipRect.contains(mouseEvent->pos())) {
                    const QString msgId_ev = index.data(MessageListModel::MessageIdRole).toString();
                    if (groupFileCardUsesLocalActions(index)) {
                        emit messageFileRevealRequested(msgId_ev);
                    }
                    return true;
                }
                if (geometry.hasPreviewChip && geometry.previewChipRect.contains(mouseEvent->pos())) {
                    const QString msgId_ev = index.data(MessageListModel::MessageIdRole).toString();
                    emit messageFilePreviewRequested(msgId_ev);
                    return true;
                }
            }
            // 纯图片气泡：点击图片区域打开图片查看器
            {
                const bool isFile_ev = index.data(MessageListModel::FileMessageRole).toBool();
                const QString attachName_ev = index.data(MessageListModel::AttachmentNameRole).toString();
                const QString localPath_ev = index.data(MessageListModel::LocalFilePathRole).toString();
                const bool isImage_ev = isFile_ev && isImageAttachment(attachName_ev, localPath_ev);
                const bool hasLocal_ev = isImage_ev && hasLocalAttachmentFile(localPath_ev);
                const bool outgoing_ev = index.data(MessageListModel::OutgoingRole).toBool();
                const int delivState_ev = index.data(MessageListModel::DeliveryStateRole).toInt();
                const QString taskId_ev = index.data(MessageListModel::TransferTaskIdRole).toString();
                const bool hasTransfer_ev = !taskId_ev.trimmed().isEmpty();
                const auto txState_ev = static_cast<FileTransferState>(
                    index.data(MessageListModel::TransferStateRole).toInt());
                const bool fileReady_ev = hasLocal_ev
                    && (outgoing_ev || delivState_ev >= static_cast<int>(MessageDeliveryState::Received));
                const bool pureImage_ev = isImage_ev && hasLocal_ev
                    && (!hasTransfer_ev || txState_ev == FileTransferState::Completed || fileReady_ev);
                if (pureImage_ev) {
                    const QPersistentModelIndex pIdx_img(index);
                    if (m_pureImageBubbleRects.contains(pIdx_img)
                        && m_pureImageBubbleRects.value(pIdx_img).contains(mouseEvent->pos())) {
                        const QString msgId_ev = index.data(MessageListModel::MessageIdRole).toString();
                        emit messageFilePreviewRequested(msgId_ev);
                        return true;
                    }
                }
            }
            const auto geometry = buildFileCardActionGeometry(option, index);
            const QString messageId = index.data(MessageListModel::MessageIdRole).toString();
            const QString transferTaskId = index.data(MessageListModel::TransferTaskIdRole).toString();
            if (geometry.hasActionChips && !messageId.trimmed().isEmpty()) {
                if (geometry.openFileChipRect.contains(mouseEvent->pos())) {
                    emit messageFileOpenRequested(messageId);
                    return true;
                }
                if (geometry.openFolderChipRect.contains(mouseEvent->pos())) {
                    emit messageFileRevealRequested(messageId);
                    return true;
                }
                if (geometry.hasPreviewChip && geometry.previewChipRect.contains(mouseEvent->pos())) {
                    emit messageFilePreviewRequested(messageId);
                    return true;
                }
            }
            if (geometry.hasResourceActionChips && !messageId.trimmed().isEmpty()) {
                if (geometry.resourceDownloadChipRect.contains(mouseEvent->pos())) {
                    emit messageFileDownloadRequested(messageId);
                    return true;
                }
                if (geometry.resourceOpenChipRect.contains(mouseEvent->pos())) {
                    emit messageFileVersionHistoryRequested(messageId);
                    return true;
                }
            }
            if (geometry.hasTransferCancelChip && !transferTaskId.trimmed().isEmpty()
                && geometry.transferCancelChipRect.contains(mouseEvent->pos())) {
                emit messageTransferCancelRequested(transferTaskId);
                return true;
            }
        }
    }

    const auto requestViewportUpdate = [&option]() {
        if (auto* view = qobject_cast<QAbstractItemView*>(const_cast<QWidget*>(option.widget))) {
            view->viewport()->update();
        }
    };

    const auto updateSelectionFromPoint = [this, &option, &index](const QPoint& point, bool resetAnchor) {
        const MessageTextLayout layout = buildMessageTextLayout(option, index);
        if (!layout.selectable) {
            return false;
        }

        const QRect interactiveRect = layout.textRect.adjusted(-12, -28, 12, 8);
        const bool canClampToEdge = !resetAnchor && m_selectionState.dragging
                                    && m_selectionState.index == index;
        if (!interactiveRect.contains(point) && !canClampToEdge) {
            return false;
        }

        QPoint clampedPoint = point;
        if (!layout.textRect.contains(point)) {
            const int clampedX = qBound(layout.textRect.left(), point.x(), layout.textRect.right() - 1);
            int clampedY = qBound(layout.textRect.top(), point.y(), layout.textRect.bottom() - 1);
            if (point.y() < layout.textRect.top() || point.y() >= layout.textRect.bottom()) {
                clampedY = layout.textRect.top() + qMax(0, layout.textRect.height() / 2);
            }
            clampedPoint = QPoint(clampedX, clampedY);
        }

        const QPointF documentPoint = clampedPoint - layout.textRect.topLeft();
        const int hit = layout.document->documentLayout()->hitTest(documentPoint, Qt::FuzzyHit);
        if (hit < 0) {
            return false;
        }

        m_selectionState.index = index;
        if (resetAnchor || m_selectionState.anchor < 0) {
            m_selectionState.anchor = hit;
        }
        m_selectionState.cursor = hit;

        QTextCursor cursor(layout.document.get());
        cursor.setPosition(qMin(m_selectionState.anchor, m_selectionState.cursor));
        cursor.setPosition(qMax(m_selectionState.anchor, m_selectionState.cursor),
                           QTextCursor::KeepAnchor);
        m_selectionState.text = cursor.selectedText().replace(QChar::ParagraphSeparator,
                                                              QLatin1Char('\n'));
        return true;
    };

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            return false;
        }
        if (!updateSelectionFromPoint(mouseEvent->pos(), true)) {
            clearSelection();
            requestViewportUpdate();
            return false;
        }
        m_selectionState.dragging = true;
        requestViewportUpdate();
        return true;
    }
    case QEvent::MouseMove: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        auto* view = qobject_cast<QAbstractItemView*>(const_cast<QWidget*>(option.widget));
        bool handCursor = false;
        // 检测超链接悬停
        {
            const MessageTextLayout layout = buildMessageTextLayout(option, index);
            if (view && layout.document && layout.textRect.contains(mouseEvent->pos())) {
                const QPointF docPoint = mouseEvent->pos() - layout.textRect.topLeft();
                const int pos = layout.document->documentLayout()->hitTest(docPoint, Qt::ExactHit);
                if (pos >= 0) {
                    QTextCursor tc(layout.document.get());
                    tc.setPosition(pos);
                    if (!tc.charFormat().anchorHref().isEmpty())
                        handCursor = true;
                }
            }
        }
        // 检测文件卡片按钮悬停
        if (!handCursor && view) {
            // 检测头像悬停（仅对方的头像，非自己）
            const bool isOutgoing = index.data(MessageListModel::OutgoingRole).toBool();
            if (!isOutgoing) {
                const QPersistentModelIndex pIdx(index);
                if (m_avatarRects.contains(pIdx)) {
                    const QRect avatarRect = m_avatarRects.value(pIdx);
                    if (avatarRect.contains(mouseEvent->pos())) {
                        handCursor = true;
                    }
                }
            }
            const auto geometry = buildFileCardActionGeometry(option, index);
            if (geometry.hasActionChips) {
                const QPoint pos = mouseEvent->pos();
                if (geometry.openFileChipRect.contains(pos)
                    || geometry.openFolderChipRect.contains(pos)
                    || (geometry.hasPreviewChip && geometry.previewChipRect.contains(pos))) {
                    handCursor = true;
                }
            }
            if (geometry.hasResourceActionChips) {
                const QPoint pos = mouseEvent->pos();
                if (geometry.resourceDownloadChipRect.contains(pos)
                    || geometry.resourceOpenChipRect.contains(pos)) {
                    handCursor = true;
                }
            }
            if (geometry.hasTransferCancelChip
                && geometry.transferCancelChipRect.contains(mouseEvent->pos())) {
                handCursor = true;
            }
            // 合并转发卡片：悬停时显示手型光标
            if (!handCursor) {
                const QPersistentModelIndex pIdx_fwd(index);
                if (m_forwardCardRects.contains(pIdx_fwd)
                    && m_forwardCardRects.value(pIdx_fwd).contains(mouseEvent->pos())) {
                    handCursor = true;
                }
            }
            // 回复引用块：悬停时显示手型光标
            if (!handCursor) {
                const QPersistentModelIndex pIdx_rq(index);
                if (m_replyQuoteRects.contains(pIdx_rq)
                    && m_replyQuoteRects.value(pIdx_rq).contains(mouseEvent->pos())) {
                    handCursor = true;
                }
            }
            // 纯图片气泡：悬停时显示手型光标（仅在图片区域内）
            if (!handCursor) {
                const bool isFile_hv = index.data(MessageListModel::FileMessageRole).toBool();
                const QString attachName_hv = index.data(MessageListModel::AttachmentNameRole).toString();
                const QString localPath_hv = index.data(MessageListModel::LocalFilePathRole).toString();
                const bool isImage_hv = isFile_hv && isImageAttachment(attachName_hv, localPath_hv);
                const bool hasLocal_hv = isImage_hv && hasLocalAttachmentFile(localPath_hv);
                const bool outgoing_hv = index.data(MessageListModel::OutgoingRole).toBool();
                const int delivState_hv = index.data(MessageListModel::DeliveryStateRole).toInt();
                const QString taskId_hv = index.data(MessageListModel::TransferTaskIdRole).toString();
                const bool hasTransfer_hv = !taskId_hv.trimmed().isEmpty();
                const auto txState_hv = static_cast<FileTransferState>(
                    index.data(MessageListModel::TransferStateRole).toInt());
                const bool fileReady_hv = hasLocal_hv
                    && (outgoing_hv || delivState_hv >= static_cast<int>(MessageDeliveryState::Received));
                if (isImage_hv && hasLocal_hv
                    && (!hasTransfer_hv || txState_hv == FileTransferState::Completed || fileReady_hv)) {
                    const QPersistentModelIndex pIdx_hv(index);
                    if (m_pureImageBubbleRects.contains(pIdx_hv)
                        && m_pureImageBubbleRects.value(pIdx_hv).contains(mouseEvent->pos())) {
                        handCursor = true;
                    }
                }
            }
        }
        if (view) {
            if (handCursor)
                view->viewport()->setCursor(Qt::PointingHandCursor);
            else
                view->viewport()->unsetCursor();
            // 触发重绘以更新 chip 悬停高亮
            view->viewport()->update();
        }
        if (!m_selectionState.dragging || m_selectionState.index != index) {
            return false;
        }
        if (updateSelectionFromPoint(mouseEvent->pos(), false)) {
            requestViewportUpdate();
            return true;
        }
        return false;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const bool wasDragging = m_selectionState.dragging && m_selectionState.index == index;
        if (wasDragging) {
            updateSelectionFromPoint(mouseEvent->pos(), false);
        }

        // 检测是否命中超链接（无论是否正在拖选都检测）
        const bool wasSelection = wasDragging
            && m_selectionState.anchor >= 0 && m_selectionState.cursor >= 0
            && m_selectionState.anchor != m_selectionState.cursor;
        if (!wasSelection && mouseEvent->button() == Qt::LeftButton) {
            const MessageTextLayout layout = buildMessageTextLayout(option, index);
            if (layout.document && layout.textRect.contains(mouseEvent->pos())) {
                const QPointF docPoint = mouseEvent->pos() - layout.textRect.topLeft();
                const int pos = layout.document->documentLayout()->hitTest(docPoint, Qt::ExactHit);
                if (pos >= 0) {
                    QTextCursor tc(layout.document.get());
                    tc.setPosition(pos);
                    const QString anchor = tc.charFormat().anchorHref();
                    if (!anchor.isEmpty()) {
                        const QUrl url = QUrl::fromUserInput(anchor);
                        if (url.isValid()) {
                            emit linkClicked(url);
                            m_selectionState.dragging = false;
                            requestViewportUpdate();
                            return true;
                        }
                    }
                }
            }
        }

        if (!wasDragging) {
            return false;
        }

        m_selectionState.dragging = false;
        requestViewportUpdate();
        return true;
    }
    default:
        break;
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void MessageBubbleDelegate::paint(QPainter* painter,
                                  const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    // 行背景透明，让 QListView 的 transparent 背景穿透
    painter->fillRect(option.rect, Qt::transparent);

    // 获取鼠标在视口中的位置用于 chip 悬停高亮
    QPoint chipHoverPos(-1, -1);
    if (auto* view = qobject_cast<const QAbstractItemView*>(option.widget)) {
        chipHoverPos = view->viewport()->mapFromGlobal(QCursor::pos());
    }

    if (!index.isValid()) {
        painter->restore();
        return;
    }

    const bool outgoing = index.data(MessageListModel::OutgoingRole).toBool();
    const QString body = index.data(MessageListModel::BodyRole).toString();
    const QString sender = index.data(MessageListModel::SenderNameRole).toString();
    const QString timeLabel = index.data(MessageListModel::TimeLabelRole).toString();
    const QString avatarPath = index.data(MessageListModel::SenderAvatarPathRole).toString();
    const bool isFile = index.data(MessageListModel::FileMessageRole).toBool();
    const bool isResourceReference = index.data(MessageListModel::ResourceReferenceRole).toBool();
    const auto resourcePayload = parseResourceRefPayload(index);
    const QString attachmentName = index.data(MessageListModel::AttachmentNameRole).toString();
    const QString localFilePath = index.data(MessageListModel::LocalFilePathRole).toString();
    const bool isImageFile = isFile && isImageAttachment(attachmentName, localFilePath);
    const int deliveryState = index.data(MessageListModel::DeliveryStateRole).toInt();
    const QString transferTaskId = index.data(MessageListModel::TransferTaskIdRole).toString();
    const bool hasTransferState = !transferTaskId.trimmed().isEmpty();
    const FileTransferState transferState =
        static_cast<FileTransferState>(index.data(MessageListModel::TransferStateRole).toInt());
    const qint64 transferBytesCompleted =
        index.data(MessageListModel::TransferBytesCompletedRole).toLongLong();
    const qint64 transferFileSize = index.data(MessageListModel::TransferFileSizeRole).toLongLong();
    const qint64 transferSpeed = index.data(MessageListModel::TransferSpeedRole).toLongLong();
    const bool isFileSummaryBody = isAttachmentSummaryBody(body, isFile, isImageFile);
    const bool hasVisibleBody = !isResourceReference && !body.trimmed().isEmpty() && !isFileSummaryBody;
    const bool isEmojiOnly = !isFile && isEmojiOnlyBody(body);
    const bool hasLocalPreview = isImageFile && hasLocalAttachmentFile(localFilePath);
    const bool fileCompleteAndLocal =
        hasLocalAttachmentFile(localFilePath)
        && (outgoing || deliveryState >= static_cast<int>(MessageDeliveryState::Received))
        && (!hasTransferState || transferState == FileTransferState::Completed);
    const bool compactTransferCard = hasTransferState && !isImageFile && !fileCompleteAndLocal;
    const bool pureImageBubble = isImageFile && hasLocalPreview
                                 && (!hasTransferState || transferState == FileTransferState::Completed
                                     || fileCompleteAndLocal);
    const bool isRecalled = index.data(MessageListModel::RecalledRole).toBool();
    const bool isEdited = index.data(MessageListModel::EditedRole).toBool();

    int rowX = option.rect.x();
    int rowY = option.rect.y();
    const int rowWidth = option.rect.width();

    // ── 多选模式勾选框 ──
    bool multiSelectMode = false;
    bool multiSelected = false;
    if (auto* view = qobject_cast<const QAbstractItemView*>(option.widget)) {
        const QWidget* topLevelWindow = view->window();
        if (topLevelWindow
            && topLevelWindow->property("messageMultiSelectMode").toBool()) {
            multiSelectMode = true;
            const QString msgId = index.data(MessageListModel::MessageIdRole).toString();
            const auto selectedIds =
                topLevelWindow->property("multiSelectedMessageIds").value<QSet<QString>>();
            multiSelected = selectedIds.contains(msgId);
        }
    }
    const int checkboxAreaWidth = multiSelectMode ? 36 : 0;
    if (multiSelectMode) {
        // 绘制勾选框
        const int cbSize = 20;
        const int cbX = rowX + 8;
        const int cbY = option.rect.y() + (option.rect.height() - cbSize) / 2;
        painter->setPen(QPen(QColor(AppStyle::border()), 1.5));
        if (multiSelected) {
            painter->setBrush(QColor(AppStyle::accent()));
            painter->setPen(Qt::NoPen);
        } else {
            painter->setBrush(Qt::NoBrush);
        }
        painter->drawRoundedRect(QRect(cbX, cbY, cbSize, cbSize), 4, 4);
        if (multiSelected) {
            // 绘制勾号
            painter->setPen(QPen(Qt::white, 2));
            painter->drawLine(cbX + 4, cbY + cbSize / 2, cbX + cbSize / 2 - 1, cbY + cbSize - 5);
            painter->drawLine(cbX + cbSize / 2 - 1, cbY + cbSize - 5, cbX + cbSize - 4, cbY + 5);
        }
        rowX += checkboxAreaWidth;
    }

    // 闪烁高亮：点击引用块跳转到原消息时短暂高亮整行
    if (auto* view = qobject_cast<const QAbstractItemView*>(option.widget)) {
        const QString flashId = view->property("flashHighlightMessageId").toString();
        if (!flashId.isEmpty()) {
            const QString msgId = index.data(MessageListModel::MessageIdRole).toString();
            if (msgId == flashId) {
                QColor hlColor(AppStyle::accent());
                hlColor.setAlpha(30);
                painter->fillRect(option.rect, hlColor);
            }
        }
    }

    // Date separator
    const bool showDateSeparator = index.data(MessageListModel::ShowDateSeparatorRole).toBool();
    if (showDateSeparator) {
        const QString dateLabel = index.data(MessageListModel::DateLabelRole).toString();
        const QFont dateSepFont = messageMetaFont(option.font);
        const QFontMetrics dateSepFm(dateSepFont);
        const int dateSepH = dateSepFont.pointSize() + 24;
        const int labelW = dateSepFm.horizontalAdvance(dateLabel) + 24;
        const int labelX = rowX + (rowWidth - labelW) / 2;
        const int labelY = rowY + 6;
        const int labelH = dateSepFm.height() + 8;

        // Draw pill background
        const QColor pillBg(AppStyle::surfaceMuted());
        const QColor pillText(AppStyle::textSecondary());
        QPainterPath pillPath;
        pillPath.addRoundedRect(QRectF(labelX, labelY, labelW, labelH), labelH / 2.0, labelH / 2.0);
        painter->fillPath(pillPath, pillBg);
        painter->setFont(dateSepFont);
        painter->setPen(pillText);
        painter->drawText(QRect(labelX, labelY, labelW, labelH),
                          Qt::AlignCenter, dateLabel);

        rowY += dateSepH;
    }

    const int maxBubbleWidth = bubbleMaxWidth(rowWidth) - 24;
    const int horizontalPadding = 12;
    const int verticalPadding = 8;
    const int rowSidePadding = edgePadding();
    const int avatarInset = avatarDiameter() + avatarGap();

    const QFont bodyFont = messageBodyFont(option.font);
    const QFont metaFont = messageMetaFont(option.font);
    const QFontMetrics bodyMetrics(bodyFont);
    const QFontMetrics metaMetrics(metaFont);

    // System notification message: centered label
    const QString messageType_pt = index.data(MessageListModel::MessageTypeRole).toString();
    if (messageType_pt == QStringLiteral("system")) {
        const QFont sysFont = messageMetaFont(option.font);
        const QFontMetrics sysFm(sysFont);
        painter->setFont(sysFont);

        // Time label at top center
        painter->setPen(QColor(AppStyle::textSecondary()));
        const int timeH = sysFm.height();
        painter->drawText(QRect(rowX, rowY + 8, rowWidth, timeH),
                          Qt::AlignCenter, timeLabel);

        // Body text centered
        const QColor sysColor(0x3A, 0xA5, 0x5F);  // green tint for system messages
        painter->setPen(sysColor);
        const int textW = rowWidth - 80;
        const int textLeft = rowX + 40;
        const int textTop = rowY + 8 + timeH + 4;
        const int textAreaH = option.rect.height() - 8 - timeH - 4 - 8;
        painter->drawText(QRect(textLeft, textTop, textW, textAreaH),
                          Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                          body);

        painter->restore();
        return;
    }

    // 通话记录消息：居中显示带电话图标
    if (messageType_pt == QStringLiteral("call_record")) {
        const QFont sysFont = messageMetaFont(option.font);
        const QFontMetrics sysFm(sysFont);
        painter->setFont(sysFont);

        // 解析通话记录
        const QJsonObject callObj = QJsonDocument::fromJson(body.toUtf8()).object();
        const QString result = callObj.value(QStringLiteral("result")).toString();
        const qint64 durationMs = static_cast<qint64>(callObj.value(QStringLiteral("durationMs")).toDouble());

        QString displayText;
        if (result == QStringLiteral("completed")) {
            const int totalSec = static_cast<int>(durationMs / 1000);
            const int min = totalSec / 60;
            const int sec = totalSec % 60;
            displayText = QStringLiteral("\xF0\x9F\x93\x9E \u8bed\u97f3\u901a\u8bdd  %1:%2")
                              .arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
        } else if (result == QStringLiteral("rejected")) {
            displayText = QStringLiteral("\xF0\x9F\x93\x9E \u5bf9\u65b9\u5df2\u62d2\u7edd");
        } else if (result == QStringLiteral("no_answer")) {
            displayText = QStringLiteral("\xF0\x9F\x93\x9E \u65e0\u4eba\u63a5\u542c");
        } else if (result == QStringLiteral("busy")) {
            displayText = QStringLiteral("\xF0\x9F\x93\x9E \u5bf9\u65b9\u5fd9\u7ebf");
        } else if (result == QStringLiteral("cancelled")) {
            displayText = QStringLiteral("\xF0\x9F\x93\x9E \u5df2\u53d6\u6d88");
        } else {
            displayText = QStringLiteral("\xF0\x9F\x93\x9E \u901a\u8bdd\u7ed3\u675f");
        }

        const QColor callColor(0x70, 0xC0, 0xA0);
        painter->setPen(callColor);
        painter->drawText(QRect(rowX, rowY + 8, rowWidth, sysFm.height()),
                          Qt::AlignCenter, displayText);

        painter->restore();
        return;
    }

    // Recalled message: short-circuit normal rendering
    if (isRecalled) {
        const QString recallText = QStringLiteral("\u6B64\u6D88\u606F\u5DF2\u88AB\u64A4\u56DE");
        const int recallTextW = metaMetrics.horizontalAdvance(recallText);
        const QString headerText = QStringLiteral("%1  %2").arg(sender, timeLabel);
        const int headerTextW = metaMetrics.horizontalAdvance(headerText) + 8;
        const int recallBubbleWidth = qBound(120,
            qMax(recallTextW + horizontalPadding * 2 + 20, headerTextW), maxBubbleWidth);
        const int recallBubbleX = outgoing ? rowX + rowWidth - rowSidePadding - avatarInset - recallBubbleWidth
                                           : rowX + rowSidePadding + avatarInset;
        const int headerH = metaMetrics.height();

        // Draw sender + time header
        painter->setFont(metaFont);
        painter->setPen(QColor(AppStyle::textSecondary()));
        painter->drawText(QRect(recallBubbleX, rowY + 10, recallBubbleWidth, headerH),
                          outgoing ? (Qt::AlignRight | Qt::AlignVCenter)
                                   : (Qt::AlignLeft | Qt::AlignVCenter),
                          headerText);

        const int recallBubbleTop = rowY + 10 + headerH + 6;
        const int recallBubbleH = verticalPadding + metaMetrics.height() + verticalPadding + 2;

        const QColor recallBubbleColor = outgoing
            ? QColor(AppStyle::bubbleOut())
            : QColor(AppStyle::bubbleIn());
        const QColor recallBubbleBorder = outgoing
            ? QColor(AppStyle::bubbleOutBorder())
            : QColor(AppStyle::bubbleInBorder());

        QPainterPath recallPath;
        recallPath.addRoundedRect(QRectF(recallBubbleX, recallBubbleTop, recallBubbleWidth, recallBubbleH),
                                  AppStyle::kBubbleRadius, AppStyle::kBubbleRadius);
        painter->fillPath(recallPath, recallBubbleColor);
        painter->setPen(QPen(recallBubbleBorder, 1.0));
        painter->drawPath(recallPath);
        painter->setPen(Qt::NoPen);

        QFont recallFont = metaFont;
        recallFont.setItalic(true);
        painter->setFont(recallFont);
        painter->setPen(QColor(128, 128, 128));
        painter->drawText(QRect(recallBubbleX + horizontalPadding,
                                recallBubbleTop + verticalPadding,
                                recallBubbleWidth - horizontalPadding * 2,
                                metaMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          recallText);

        painter->restore();
        return;
    }

    // ── 群文件卡片 (FileService / P2P Offer-only) ─────────────────────────
    const QString fileCardJson_paint = index.data(MessageListModel::FileCardJsonRole).toString();
    if (!fileCardJson_paint.trimmed().isEmpty()) {
        const QJsonObject cardObj = QJsonDocument::fromJson(fileCardJson_paint.toUtf8()).object();
        const QString cardFileName = cardObj.value(QStringLiteral("file_name")).toString();
        const qint64  cardFileSize = cardObj.value(QStringLiteral("file_size")).toInteger();
        const QString cardChannel  = cardObj.value(QStringLiteral("channel")).toString();
        const QString localPath    = resolvedGroupFileCardLocalPath(index);
        const bool fileDownloaded  = groupFileCardUsesLocalActions(index);

        const int cardBubbleWidth = qBound(280, qMin(maxBubbleWidth, 340), maxBubbleWidth);
        const int bubbleX = outgoing ? rowX + rowWidth - rowSidePadding - avatarInset - cardBubbleWidth
                                     : rowX + rowSidePadding + avatarInset;
        const int headerHeight = metaMetrics.height();
        const bool hasAvatar = !avatarPath.isEmpty() || !sender.isEmpty();

        // Avatar — 使用与普通消息相同的 6 色调色板 + 缓存矩形以支持点击/悬停
        if (hasAvatar) {
            static const QColor kAvatarPalette[] = {
                QColor(0x52, 0x73, 0xE8), QColor(0x2F, 0xA4, 0x84),
                QColor(0xD9, 0x96, 0x3A), QColor(0x7B, 0x68, 0xE6),
                QColor(0xD8, 0x5A, 0x9A), QColor(0x32, 0x96, 0xC4),
            };
            int h = 0;
            for (const QChar ch : sender)
                h = (h * 31 + ch.unicode()) & 0x7FFF'FFFF;
            const int avatarSize = avatarDiameter();
            const int avatarX = outgoing ? bubbleX + cardBubbleWidth + avatarGap()
                                         : rowX + rowSidePadding;
            const int avatarY = rowY + 10 + headerHeight + 6;
            const QRect avatarRect(avatarX, avatarY, avatarSize, avatarSize);
            drawAvatar(painter, avatarRect, avatarPath, sender,
                       kAvatarPalette[h % 6], Qt::white);
            m_avatarRects[QPersistentModelIndex(index)] = avatarRect;
        }

        // Header (sender name + time)
        painter->setFont(metaFont);
        painter->setPen(QColor(AppStyle::textSecondary()));
        const int nameY = rowY + 10;
        if (outgoing) {
            painter->drawText(QRect(bubbleX, nameY, cardBubbleWidth - 4, headerHeight),
                              Qt::AlignRight | Qt::AlignVCenter,
                              timeLabel + QStringLiteral("  ") + sender);
        } else {
            painter->drawText(QRect(bubbleX + 4, nameY, cardBubbleWidth, headerHeight),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              sender + QStringLiteral("  ") + timeLabel);
        }

        // Bubble
        const int currentY = nameY + headerHeight + 6;
        const int cardH = standardFileCardHeight(bodyMetrics, metaMetrics);
        const int bubbleH = verticalPadding + cardH + verticalPadding;
        const QRectF bubbleRect(bubbleX, currentY, cardBubbleWidth, bubbleH);
        const QColor bubbleColor = outgoing ? QColor(AppStyle::bubbleOut())
                                            : QColor(AppStyle::bubbleIn());
        const QColor borderColor = outgoing ? QColor(AppStyle::bubbleOutBorder())
                                            : QColor(AppStyle::bubbleInBorder());
        QPainterPath bubblePath;
        bubblePath.addRoundedRect(bubbleRect, AppStyle::kBubbleRadius, AppStyle::kBubbleRadius);
        painter->fillPath(bubblePath, bubbleColor);
        painter->setPen(QPen(borderColor, 1.0));
        painter->drawPath(bubblePath);
        painter->setPen(Qt::NoPen);

        // File card content
        const FileCardVisualPalette palette = fileCardVisualPalette(outgoing, false);
        const QRect cardRect(bubbleX + horizontalPadding,
                             currentY + verticalPadding,
                             cardBubbleWidth - horizontalPadding * 2,
                             cardH);
        QPainterPath cardPath;
        cardPath.addRoundedRect(QRectF(cardRect), 8, 8);
        painter->fillPath(cardPath, palette.cardBg);
        painter->setPen(QPen(palette.cardBorder, 1.0));
        painter->drawPath(cardPath);
        painter->setPen(Qt::NoPen);

        // Icon
        const int iconSize = 40;
        const int iconX = cardRect.x() + 12;
        const int iconY = cardRect.y() + 12;
        QPainterPath iconPath;
        iconPath.addRoundedRect(QRectF(iconX, iconY, iconSize, iconSize), 8, 8);
        painter->fillPath(iconPath, palette.iconBg);
        painter->setFont(bodyFont);
        painter->setPen(palette.titleColor);
        painter->drawText(QRect(iconX, iconY, iconSize, iconSize),
                          Qt::AlignCenter, QStringLiteral("\U0001F4C1"));

        // File name
        const int titleLeft = iconX + iconSize + 10;
        const int titleWidth = cardRect.right() - 12 - titleLeft;
        painter->setFont(bodyFont);
        painter->setPen(palette.titleColor);
        const QString elidedName = bodyMetrics.elidedText(cardFileName, Qt::ElideMiddle, titleWidth);
        painter->drawText(QRect(titleLeft, iconY, titleWidth, bodyMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          elidedName);

        // File size
        painter->setFont(metaFont);
        painter->setPen(palette.metaColor);
        const QString sizeText = cardFileSize > 1048576
            ? QStringLiteral("%1 MB").arg(static_cast<double>(cardFileSize) / 1048576.0, 0, 'f', 1)
            : QStringLiteral("%1 KB").arg(static_cast<double>(cardFileSize) / 1024.0, 0, 'f', 0);
        painter->drawText(QRect(titleLeft, iconY + bodyMetrics.height() + 2, titleWidth, metaMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          sizeText + QStringLiteral("  ·  ") + cardChannel);

        // Action chips: 下载 / 打开文件 + 打开文件夹
        const int chipY = cardRect.bottom() - metaMetrics.height() - 14;
        const QRect chipRect(cardRect.x() + 12, chipY, 84, metaMetrics.height() + 8);
        QPainterPath chipPath;
        chipPath.addRoundedRect(QRectF(chipRect), chipRect.height() / 2.0, chipRect.height() / 2.0);
        painter->fillPath(chipPath, chipRect.contains(chipHoverPos) ? palette.chipBg.darker(115) : palette.chipBg);
        painter->setFont(metaFont);
        painter->setPen(palette.chipText);
        const QString chipLabel = fileDownloaded ? QStringLiteral("\u6253\u5F00\u6587\u4EF6") : QStringLiteral("\u4E0B\u8F7D");
        painter->drawText(chipRect, Qt::AlignCenter, chipLabel);

        if (fileDownloaded) {
            const QRect chip2Rect(chipRect.right() + 8, chipY, 108, metaMetrics.height() + 8);
            QPainterPath chip2Path;
            chip2Path.addRoundedRect(QRectF(chip2Rect), chip2Rect.height() / 2.0, chip2Rect.height() / 2.0);
            painter->fillPath(chip2Path, chip2Rect.contains(chipHoverPos) ? palette.chipBg.darker(115) : palette.chipBg);
            painter->setPen(palette.chipText);
            painter->drawText(chip2Rect, Qt::AlignCenter, QStringLiteral("\u6253\u5F00\u6587\u4EF6\u5939"));

            // 预览按钮
            if (FilePreviewWidget::isPreviewSupported(cardFileName)) {
                const QRect previewRect(chip2Rect.right() + 8, chipY, 60, metaMetrics.height() + 8);
                QPainterPath previewPath;
                previewPath.addRoundedRect(QRectF(previewRect), previewRect.height() / 2.0, previewRect.height() / 2.0);
                painter->fillPath(previewPath, previewRect.contains(chipHoverPos) ? palette.chipBg.darker(115) : palette.chipBg);
                painter->setPen(palette.chipText);
                painter->drawText(previewRect, Qt::AlignCenter, QStringLiteral("预览"));
            }
        }

        // Delivery indicator
        const int groupReadCount_fc = index.data(MessageListModel::GroupReadCountRole).toInt();
        const int groupActiveMemberCount_fc = index.data(MessageListModel::GroupActiveMemberCountRole).toInt();
        const QString deliveryText_fc = deliveryIndicatorText(deliveryState, outgoing,
                                                              groupReadCount_fc, groupActiveMemberCount_fc);
        if (!deliveryText_fc.isEmpty()) {
            painter->setFont(metaFont);
            painter->setPen(QColor(AppStyle::textSecondary()));
            painter->drawText(QRect(bubbleX, currentY + bubbleH + 2, cardBubbleWidth, metaMetrics.height()),
                              outgoing ? Qt::AlignRight : Qt::AlignLeft,
                              deliveryText_fc);
        }

        painter->restore();
        return;
    }

    // ─── 贴纸消息渲染 ───
    if (messageType_pt == QStringLiteral("sticker")) {
        const QString payloadJson = index.data(MessageListModel::PayloadJsonRole).toString();
        const QJsonObject stickerObj = QJsonDocument::fromJson(payloadJson.toUtf8()).object();
        const QString sPackId = stickerObj.value(QStringLiteral("pack_id")).toString();
        const QString sStickerId = stickerObj.value(QStringLiteral("sticker_id")).toString();

        constexpr int stickerSize = 120;
        const int stickerBubbleW = stickerSize;
        const int bubbleX = outgoing ? rowX + rowWidth - rowSidePadding - avatarInset - stickerBubbleW
                                     : rowX + rowSidePadding + avatarInset;
        const int headerH = metaMetrics.height();

        // 头像
        const int avatarX = outgoing ? rowX + rowWidth - rowSidePadding - avatarDiameter()
                                     : rowX + rowSidePadding;
        const int avatarY = rowY + 6;
        const QRect avatarRect(avatarX, avatarY, avatarDiameter(), avatarDiameter());
        {
            static const QColor kAvatarPalette[] = {
                QColor(0x52, 0x73, 0xE8), QColor(0x2F, 0xA4, 0x84),
                QColor(0xD9, 0x96, 0x3A), QColor(0x7B, 0x68, 0xE6),
                QColor(0xD8, 0x5A, 0x9A), QColor(0x32, 0x96, 0xC4),
            };
            int h = 0;
            for (const QChar ch : sender)
                h = (h * 31 + ch.unicode()) & 0x7FFF'FFFF;
            drawAvatar(painter, avatarRect, avatarPath, sender,
                       kAvatarPalette[h % 6], Qt::white);
        }
        m_avatarRects[QPersistentModelIndex(index)] = avatarRect;

        // 名称 + 时间
        painter->setFont(metaFont);
        painter->setPen(QColor(AppStyle::textSecondary()));
        const QString headerText = QStringLiteral("%1  %2").arg(sender, timeLabel);
        const int headerDrawW = qMax(stickerBubbleW,
                                     metaMetrics.horizontalAdvance(headerText) + 4);
        const int headerDrawX = outgoing
            ? bubbleX + stickerBubbleW - headerDrawW
            : bubbleX;
        painter->drawText(QRect(headerDrawX, rowY + 6, headerDrawW, headerH),
                          outgoing ? (Qt::AlignRight | Qt::AlignVCenter)
                                   : (Qt::AlignLeft | Qt::AlignVCenter),
                          headerText);

        const int innerY = rowY + 6 + headerH + 4;
        const QRect stickerRect(bubbleX, innerY, stickerSize, stickerSize);

        // 尝试使用 QMovie 播放 GIF 动画
        QString stickerPath = StickerManager::instance().stickerFilePath(sPackId, sStickerId);
        if (stickerPath.isEmpty()) {
            // 尝试从 payloadJson 中的 base64 缓存
            const QString base64 = stickerObj.value(QStringLiteral("gif_base64")).toString();
            if (!base64.isEmpty()) {
                const QByteArray gifData = QByteArray::fromBase64(base64.toLatin1());
                stickerPath = StickerManager::instance().cacheReceivedSticker(sPackId, sStickerId, gifData);
            }
        }

        if (!stickerPath.isEmpty()) {
            const bool isAnimated = stickerPath.endsWith(QStringLiteral(".gif"), Qt::CaseInsensitive);

            if (isAnimated) {
                // GIF：使用 QMovie 播放动画
                const QPersistentModelIndex pIdx(index);
                auto& movie = m_stickerMovies[pIdx];
                if (!movie || movie->fileName() != stickerPath) {
                    movie = std::make_shared<QMovie>(stickerPath);
                    movie->setCacheMode(QMovie::CacheAll);
                    if (auto* view = qobject_cast<const QAbstractItemView*>(option.widget)) {
                        auto* viewport = view->viewport();
                        QObject::connect(movie.get(), &QMovie::frameChanged, viewport,
                                         [viewport]() { viewport->update(); });
                    }
                    movie->start();
                }
                const QPixmap frame = movie->currentPixmap();
                if (!frame.isNull()) {
                    const QPixmap scaled = frame.scaled(stickerSize, stickerSize,
                                                        Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    const int dx = stickerRect.x() + (stickerSize - scaled.width()) / 2;
                    const int dy = stickerRect.y() + (stickerSize - scaled.height()) / 2;
                    painter->drawPixmap(dx, dy, scaled);
                }
            } else {
                // PNG/JPG/WebP：静态图片直接绘制
                const QPixmap pm = StickerManager::instance().stickerThumbnail(
                    sPackId, sStickerId, stickerSize);
                if (!pm.isNull()) {
                    const int dx = stickerRect.x() + (stickerSize - pm.width()) / 2;
                    const int dy = stickerRect.y() + (stickerSize - pm.height()) / 2;
                    painter->drawPixmap(dx, dy, pm);
                }
            }
        } else {
            // 降级：显示 emoji 文本
            const QString emoji = stickerObj.value(QStringLiteral("emoji")).toString();
            QFont emojiFont = bodyFont;
            emojiFont.setPixelSize(64);
            painter->setFont(emojiFont);
            painter->setPen(QColor(AppStyle::textPrimary()));
            painter->drawText(stickerRect, Qt::AlignCenter, emoji.isEmpty() ? QStringLiteral("🎭") : emoji);
        }

        // 送达指示
        const int groupReadCount_st = index.data(MessageListModel::GroupReadCountRole).toInt();
        const int groupActiveMemberCount_st = index.data(MessageListModel::GroupActiveMemberCountRole).toInt();
        const QString deliveryText_st = deliveryIndicatorText(deliveryState, outgoing,
                                                              groupReadCount_st, groupActiveMemberCount_st);
        if (!deliveryText_st.isEmpty()) {
            painter->setFont(metaFont);
            painter->setPen(QColor(AppStyle::textSecondary()));
            painter->drawText(QRect(bubbleX, innerY + stickerSize + 2, stickerBubbleW, metaMetrics.height()),
                              outgoing ? Qt::AlignRight : Qt::AlignLeft,
                              deliveryText_st);
        }

        painter->restore();
        return;
    }

    // ─── 合并转发卡片渲染 ───
    if (messageType_pt == QStringLiteral("forward_package")) {
        const QString payloadJson = index.data(MessageListModel::PayloadJsonRole).toString();
        const QJsonObject pkgObj = QJsonDocument::fromJson(payloadJson.toUtf8()).object();
        const QJsonArray msgs = pkgObj.value(QStringLiteral("messages")).toArray();
        const int sourceCount = pkgObj.value(QStringLiteral("count")).toInt(msgs.size());

        constexpr int cardWidth = 240;
        const int bubbleX = outgoing ? rowX + rowWidth - rowSidePadding - avatarInset - cardWidth
                                     : rowX + rowSidePadding + avatarInset;

        // 头像
        const int avatarX = outgoing ? rowX + rowWidth - rowSidePadding - avatarDiameter()
                                     : rowX + rowSidePadding;
        const int avatarY = rowY + 6;
        const QRect avatarRect(avatarX, avatarY, avatarDiameter(), avatarDiameter());
        {
            static const QColor kAvatarPalette[] = {
                QColor(0x52, 0x73, 0xE8), QColor(0x2F, 0xA4, 0x84),
                QColor(0xD9, 0x96, 0x3A), QColor(0x7B, 0x68, 0xE6),
                QColor(0xD8, 0x5A, 0x9A), QColor(0x32, 0x96, 0xC4),
            };
            int h = 0;
            for (const QChar ch : sender)
                h = (h * 31 + ch.unicode()) & 0x7FFF'FFFF;
            drawAvatar(painter, avatarRect, avatarPath, sender,
                       kAvatarPalette[h % 6], Qt::white);
        }
        m_avatarRects[QPersistentModelIndex(index)] = avatarRect;

        // 名称 + 时间
        painter->setFont(metaFont);
        painter->setPen(QColor(AppStyle::textSecondary()));
        const QString headerText = QStringLiteral("%1  %2").arg(sender, timeLabel);
        painter->drawText(QRect(bubbleX, rowY + 6, cardWidth, metaMetrics.height()),
                          outgoing ? (Qt::AlignRight | Qt::AlignVCenter)
                                   : (Qt::AlignLeft | Qt::AlignVCenter),
                          headerText);

        const int cardY = rowY + 6 + metaMetrics.height() + 4;
        const QFont bodyFontFp = messageBodyFont(option.font);
        const QFontMetrics bodyFmFp(bodyFontFp);
        const int cardH = 8 + bodyFmFp.height() + 4 + metaMetrics.height() * 3 + 12 + 1 + 4 + metaMetrics.height() + 8;
        const QRect cardRect(bubbleX, cardY, cardWidth, cardH);

        // 卡片背景
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(AppStyle::surfaceAlt()));
        painter->drawRoundedRect(cardRect, 8, 8);

        // 卡片边框
        painter->setPen(QPen(QColor(AppStyle::border()), 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(cardRect, 8, 8);

        int cy = cardY + 8;
        // 标题
        painter->setFont(bodyFontFp);
        painter->setPen(QColor(AppStyle::textPrimary()));
        painter->drawText(QRect(bubbleX + 10, cy, cardWidth - 20, bodyFmFp.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("\U0001F4CB \u804a\u5929\u8bb0\u5f55"));
        cy += bodyFmFp.height() + 4;

        // 预览行（最多3行）
        painter->setFont(metaFont);
        painter->setPen(QColor(AppStyle::textSecondary()));
        const int previewLines = qMin(3, static_cast<int>(msgs.size()));
        for (int i = 0; i < previewLines; ++i) {
            const QJsonObject m = msgs[i].toObject();
            const QString lineSender = m.value(QStringLiteral("sender")).toString();
            const QString msgType = m.value(QStringLiteral("type")).toString();
            QString lineBody;
            if (msgType == QStringLiteral("text")) {
                lineBody = m.value(QStringLiteral("text")).toString();
            } else if (msgType == QStringLiteral("image")) {
                lineBody = QStringLiteral("[图片]");
            } else {
                lineBody = QStringLiteral("[%1]").arg(
                    m.value(QStringLiteral("fileName")).toString());
            }
            const QString lineText = QStringLiteral("%1: %2").arg(lineSender, lineBody);
            const QString elidedLine = metaMetrics.elidedText(lineText, Qt::ElideRight, cardWidth - 20);
            painter->drawText(QRect(bubbleX + 10, cy, cardWidth - 20, metaMetrics.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, elidedLine);
            cy += metaMetrics.height() + 4;
        }

        // 分割线
        cy += 2;
        painter->setPen(QPen(QColor(AppStyle::border()), 1));
        painter->drawLine(bubbleX + 10, cy, bubbleX + cardWidth - 10, cy);
        cy += 5;

        // 计数
        painter->setFont(metaFont);
        painter->setPen(QColor(AppStyle::textMuted()));
        painter->drawText(QRect(bubbleX + 10, cy, cardWidth - 20, metaMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("\u67e5\u770b %1 \u6761\u804a\u5929\u8bb0\u5f55").arg(sourceCount));

        // 送达指示
        const int groupReadCount_fp = index.data(MessageListModel::GroupReadCountRole).toInt();
        const int groupActiveMemberCount_fp = index.data(MessageListModel::GroupActiveMemberCountRole).toInt();
        const QString deliveryText_fp = deliveryIndicatorText(deliveryState, outgoing,
                                                              groupReadCount_fp, groupActiveMemberCount_fp);
        if (!deliveryText_fp.isEmpty()) {
            painter->setFont(metaFont);
            painter->setPen(QColor(AppStyle::textSecondary()));
            painter->drawText(QRect(bubbleX, cardY + cardH + 2, cardWidth, metaMetrics.height()),
                              outgoing ? Qt::AlignRight : Qt::AlignLeft,
                              deliveryText_fp);
        }

        // 保存卡片区域用于点击检测
        m_forwardCardRects[QPersistentModelIndex(index)] = cardRect;

        painter->restore();
        return;
    }

    const int textWidth = maxBubbleWidth - horizontalPadding * 2;
    int textHeight = 0;
    if (hasVisibleBody) {
        if (isEmojiOnly) {
            textHeight = qMax(bodyMetrics.height(), 26);
        } else if (isHtmlBody(body)) {
            textHeight = htmlTextHeight(body, textWidth, bodyFont);
        } else {
            const QRect textBounds = bodyMetrics.boundingRect(QRect(0, 0, textWidth, 2000),
                                                              Qt::TextWordWrap,
                                                              body);
            textHeight = textBounds.height();
        }
    }

    int bubbleWidth = maxBubbleWidth;
    if (hasVisibleBody) {
        const QString widthText = isHtmlBody(body) ? plainTextFromHtml(body) : body;
        const int estimateWidth = bubbleTextWidth(widthText, bodyMetrics, textWidth) + horizontalPadding * 2;
        bubbleWidth = qMin(maxBubbleWidth, estimateWidth);
    }
    if (isFile) {
        if (pureImageBubble) {
            const QSize previewBubbleSize =
                inlinePreviewBubbleSize(localFilePath, qMin(maxBubbleWidth, kImagePreviewMaxSize), kImagePreviewMaxSize);
            bubbleWidth = qBound(72,
                                 previewBubbleSize.width(),
                                 qMin(maxBubbleWidth, kImagePreviewMaxSize));
        } else if (isImageFile && hasLocalPreview) {
            const QSize previewBubbleSize =
                inlinePreviewBubbleSize(localFilePath, qMin(maxBubbleWidth - horizontalPadding * 2, kImageThumbMaxWidth), kImageThumbMaxHeight);
            bubbleWidth = qBound(160,
                                 previewBubbleSize.width() + horizontalPadding * 2,
                                 qMin(maxBubbleWidth, kImageThumbMaxWidth + 80));
        } else if (compactTransferCard) {
            bubbleWidth = qMax(bubbleWidth, qMin(maxBubbleWidth, 440));
        } else {
            bubbleWidth = qMax(bubbleWidth, qMin(maxBubbleWidth, isImageFile ? 248 : 332));
        }
    }
    if (isResourceReference) {
        bubbleWidth = qBound(280, qMin(maxBubbleWidth, 340), maxBubbleWidth);
    }

    // 确保气泡宽度至少能完整显示"名称  时间"头部，避免时间被截断
    {
        const int headerMinW = metaMetrics.horizontalAdvance(
            QStringLiteral("%1  %2").arg(sender, timeLabel)) + 12;
        bubbleWidth = qMax(bubbleWidth, qMin(maxBubbleWidth, headerMinW));
    }

    // Reply quote block height for paint layout
    const QString replyToMsgId_paint = index.data(MessageListModel::ReplyToMessageIdRole).toString();
    const int quoteBlockHeight_paint = replyToMsgId_paint.trimmed().isEmpty() ? 0
        : (6 + metaMetrics.height() + 2 + metaMetrics.height() + 6 + 4);

    int bubbleContentHeight = textHeight + quoteBlockHeight_paint;
    if (isFile) {
        bubbleContentHeight = (hasVisibleBody ? textHeight + 10 : 0)
                            + (pureImageBubble
                                   ? inlinePreviewBubbleSize(localFilePath,
                                                             qMin(maxBubbleWidth, kImagePreviewMaxSize),
                                                             kImagePreviewMaxSize).height()
                                   : compactTransferCard
                                         ? compactTransferCardHeight(bodyMetrics, metaMetrics)
                                   : (isImageFile
                                   ? (hasLocalPreview
                                          ? inlinePreviewBubbleSize(localFilePath,
                                                                    qMin(maxBubbleWidth - horizontalPadding * 2, kImageThumbMaxWidth),
                                                                    kImageThumbMaxHeight).height() + 16
                                          : 96)
                                   : standardFileCardHeight(bodyMetrics, metaMetrics)));
    }
    if (isResourceReference) {
        bubbleContentHeight = qMax(98, bodyMetrics.height() + metaMetrics.height() * 2 + 40);
    }
    const int bubbleHeight = pureImageBubble
                                 ? bubbleContentHeight + 2
                                 : (verticalPadding + bubbleContentHeight + verticalPadding + 2);

    const int bubbleX = outgoing ? rowX + rowWidth - rowSidePadding - avatarInset - bubbleWidth
                                 : rowX + rowSidePadding + avatarInset;

    const int headerHeight = metaMetrics.height();
    const int footerHeight = outgoing ? metaMetrics.height() : 0;
    int currentY = rowY + (pureImageBubble ? 6 : 10);

    const QRect avatarRect(outgoing ? option.rect.right() - rowSidePadding - avatarDiameter() + 1
                                    : rowX + rowSidePadding,
                           currentY + headerHeight + (pureImageBubble ? 2 : 4),
                           avatarDiameter(),
                           avatarDiameter());
    // 缓存头像矩形以供 editorEvent 点击检测
    m_avatarRects[QPersistentModelIndex(index)] = avatarRect;
    // 使用与会话列表和联系人列表相同的头像颜色算法
    {
        static const QColor kAvatarPalette[] = {
            QColor(0x52, 0x73, 0xE8),
            QColor(0x2F, 0xA4, 0x84),
            QColor(0xD9, 0x96, 0x3A),
            QColor(0x7B, 0x68, 0xE6),
            QColor(0xD8, 0x5A, 0x9A),
            QColor(0x32, 0x96, 0xC4),
        };
        int h = 0;
        for (const QChar ch : sender) {
            h = (h * 31 + ch.unicode()) & 0x7FFF'FFFF;
        }
        const QColor fallbackBg = kAvatarPalette[h % 6];
        drawAvatar(painter,
                   avatarRect,
                   avatarPath,
                   sender,
                   fallbackBg,
                   Qt::white);
    }

    painter->setFont(metaFont);
    painter->setPen(QColor(AppStyle::textSecondary()));
    const QString headerText = QStringLiteral("%1  %2").arg(sender, timeLabel);
    painter->drawText(QRect(bubbleX, currentY, bubbleWidth, headerHeight),
                      outgoing ? (Qt::AlignRight | Qt::AlignVCenter)
                               : (Qt::AlignLeft | Qt::AlignVCenter),
                      headerText);
    currentY += headerHeight + (pureImageBubble ? 4 : 6);

    const bool usePhotoBubble = isImageFile;
    const QColor bubbleColor = usePhotoBubble
                                   ? QColor(AppStyle::accentSoft())
                                   : (outgoing ? QColor(AppStyle::bubbleOut())
                                               : QColor(AppStyle::bubbleIn()));
    const QColor bubbleBorder = usePhotoBubble
                                    ? QColor(AppStyle::selectedBg())
                                    : (outgoing ? QColor(AppStyle::bubbleOutBorder())
                                                : QColor(AppStyle::bubbleInBorder()));

    QPainterPath bubblePath;
    bubblePath.addRoundedRect(QRectF(bubbleX, currentY, bubbleWidth, bubbleHeight),
                              AppStyle::kBubbleRadius,
                              AppStyle::kBubbleRadius);
    // 缓存纯图片气泡矩形以供 editorEvent 点击区域检测
    if (pureImageBubble) {
        m_pureImageBubbleRects[QPersistentModelIndex(index)] =
            QRect(bubbleX, currentY, bubbleWidth, bubbleHeight);
    } else {
        m_pureImageBubbleRects.remove(QPersistentModelIndex(index));
    }
    if (!pureImageBubble) {
        painter->fillPath(bubblePath, bubbleColor);
        painter->setPen(QPen(bubbleBorder, 1.0));
        painter->drawPath(bubblePath);
        painter->setPen(Qt::NoPen);
    }

    int innerY = pureImageBubble ? currentY : currentY + verticalPadding;

    // Reply quote block rendering
    const QString replyToMsgId_pt = index.data(MessageListModel::ReplyToMessageIdRole).toString();
    if (!replyToMsgId_pt.trimmed().isEmpty()) {
        const QString replyToSenderName = index.data(MessageListModel::ReplyToSenderNameRole).toString();
        const QString replyToBodyText = index.data(MessageListModel::ReplyToBodyRole).toString();
        const QFont quoteFont = messageMetaFont(option.font);
        const QFontMetrics quoteFm(quoteFont);

        const int quoteLeft = bubbleX + horizontalPadding;
        const int quoteWidth = bubbleWidth - horizontalPadding * 2;
        const int quotePadLeft = 8;  // left bar width + gap
        const int quoteH = 6 + quoteFm.height() + 2 + quoteFm.height() + 6;

        // Background
        const QColor quoteBg(AppStyle::surfaceMuted());
        QPainterPath quotePath;
        quotePath.addRoundedRect(QRectF(quoteLeft, innerY, quoteWidth, quoteH), 6, 6);
        painter->fillPath(quotePath, quoteBg);

        // Left blue bar
        const QColor quoteBar(AppStyle::accent());
        painter->fillRect(QRect(quoteLeft, innerY + 4, 3, quoteH - 8), quoteBar);

        // Sender name
        painter->setFont(quoteFont);
        painter->setPen(QColor(AppStyle::accent()));
        const QString senderLine = QStringLiteral("\u56DE\u590D %1").arg(replyToSenderName);
        painter->drawText(QRect(quoteLeft + quotePadLeft, innerY + 6, quoteWidth - quotePadLeft - 6, quoteFm.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          quoteFm.elidedText(senderLine, Qt::ElideRight, quoteWidth - quotePadLeft - 6));

        // Body preview — 对文件/图片类型显示前缀图标
        painter->setPen(QColor(AppStyle::textSecondary()));
        QString previewText = replyToBodyText.trimmed();
        if (previewText.isEmpty()) {
            previewText = QStringLiteral("[\u6D88\u606F]");
        } else if (previewText.startsWith(QStringLiteral("[\u56FE\u7247]"))
                   || previewText.startsWith(QStringLiteral("[\u6587\u4EF6]"))) {
            // 已有类型前缀，保持原样
        }
        painter->drawText(QRect(quoteLeft + quotePadLeft, innerY + 6 + quoteFm.height() + 2,
                                quoteWidth - quotePadLeft - 6, quoteFm.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          quoteFm.elidedText(previewText, Qt::ElideRight, quoteWidth - quotePadLeft - 6));

        // 存储引用块区域用于点击跳转
        m_replyQuoteRects[QPersistentModelIndex(index)] = QRect(quoteLeft, innerY, quoteWidth, quoteH);

        innerY += quoteH + 4;
    }

    if (hasVisibleBody) {
        MessageTextLayout textLayout = buildMessageTextLayout(option, index);
        const bool hasSelection = textLayout.selectable
            && m_selectionState.index == index
            && m_selectionState.anchor >= 0
            && m_selectionState.cursor >= 0
            && m_selectionState.anchor != m_selectionState.cursor;
        // 若需要应用选择高亮，必须深拷贝文档，否则会污染缓存的 shared_ptr
        std::shared_ptr<QTextDocument> drawDoc = textLayout.document;
        if (hasSelection && drawDoc) {
            auto freshDoc = std::make_shared<QTextDocument>();
            freshDoc->setDefaultFont(drawDoc->defaultFont());
            freshDoc->setDefaultStyleSheet(drawDoc->defaultStyleSheet());
            freshDoc->setTextWidth(drawDoc->textWidth());
            freshDoc->setHtml(drawDoc->toHtml());
            QTextCursor selectionCursor(freshDoc.get());
            selectionCursor.setPosition(qMin(m_selectionState.anchor, m_selectionState.cursor));
            selectionCursor.setPosition(qMax(m_selectionState.anchor, m_selectionState.cursor),
                                        QTextCursor::KeepAnchor);
            QTextCharFormat selectionFormat;
            selectionFormat.setBackground(QColor(51, 120, 240, 100));
            selectionFormat.setForeground(Qt::white);
            selectionCursor.mergeCharFormat(selectionFormat);
            drawDoc = freshDoc;
        }
        painter->save();
        painter->translate(bubbleX + horizontalPadding, innerY);
        if (drawDoc) {
            drawDoc->drawContents(painter);
        }
        painter->restore();
        innerY += textHeight + 10;
    }

    if (isFile && !attachmentName.isEmpty()) {
        const QSize previewSize = isImageFile && hasLocalPreview
            ? inlinePreviewBubbleSize(localFilePath, qMin(maxBubbleWidth, kImageThumbMaxWidth), kImageThumbMaxHeight)
            : QSize();
        const QRect cardRect(bubbleX + horizontalPadding,
                             innerY,
                             bubbleWidth - horizontalPadding * 2,
                             isImageFile ? (hasLocalPreview ? previewSize.height() + 16 : 96)
                                         : standardFileCardHeight(bodyMetrics, metaMetrics));
        const FileCardVisualPalette palette = fileCardVisualPalette(outgoing, isImageFile);

        if (isImageFile) {
            if (!pureImageBubble) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(palette.cardBg);
                painter->drawRoundedRect(cardRect, 12, 12);
            }
            const QRect previewRect = pureImageBubble
                ? QRect(bubbleX,
                        innerY,
                        bubbleWidth,
                        inlinePreviewBubbleSize(localFilePath, qMin(maxBubbleWidth, kImagePreviewMaxSize), kImagePreviewMaxSize).height())
                : QRect(cardRect.x() + 8,
                        cardRect.y() + 8,
                        hasLocalPreview ? previewSize.width() : (cardRect.width() - 16),
                        hasLocalPreview ? previewSize.height() : (cardRect.height() - 16));
            const QPixmap previewPixmap = loadInlinePreview(localFilePath, previewRect.size());
            painter->setPen(Qt::NoPen);
            const qreal previewRadius = pureImageBubble ? AppStyle::kBubbleRadius : 12.0;
            if (!pureImageBubble) {
                painter->setBrush(QColor(AppStyle::accentSoft()));
                painter->drawRoundedRect(previewRect, previewRadius, previewRadius);
            }
            if (!previewPixmap.isNull()) {
                const QPoint previewTopLeft(previewRect.x() + (previewRect.width() - previewPixmap.width()) / 2,
                                            previewRect.y() + (previewRect.height() - previewPixmap.height()) / 2);
                QPainterPath previewClip;
                previewClip.addRoundedRect(previewRect, previewRadius, previewRadius);
                painter->save();
                painter->setClipPath(previewClip);
                painter->drawPixmap(previewTopLeft, previewPixmap);
                painter->restore();
            } else {
                // 图片加载失败时绘制占位背景（含纯图片气泡场景）
                painter->setBrush(QColor(AppStyle::accentSoft()));
                painter->drawRoundedRect(previewRect, previewRadius, previewRadius);

                const QRect iconRect(previewRect.x() + 16, previewRect.y() + 14, 24, 24);
                QFont iconFont = bodyFont;
                iconFont.setBold(true);
                painter->setFont(iconFont);
                painter->setPen(QColor(AppStyle::accent()));
                painter->drawText(iconRect, Qt::AlignCenter, UiIcons::actionFiles());

                painter->setPen(QColor(AppStyle::textPrimary()));
                painter->setFont(metaFont);
                painter->drawText(QRect(previewRect.x() + 14,
                                        previewRect.y() + 44,
                                        previewRect.width() - 28,
                                        metaMetrics.height() + 4),
                                  Qt::AlignLeft | Qt::AlignVCenter,
                                  QFontMetrics(metaFont).elidedText(attachmentName, Qt::ElideMiddle, previewRect.width() - 28));
                painter->setPen(QColor(AppStyle::textMuted()));
                painter->drawText(QRect(previewRect.x() + 14,
                                        previewRect.bottom() - metaMetrics.height() - 10,
                                        previewRect.width() - 28,
                                        metaMetrics.height() + 2),
                                  Qt::AlignLeft | Qt::AlignVCenter,
                                  QStringLiteral("图片预览暂不可用"));
            }

        } else {
            painter->setPen(Qt::NoPen);
            painter->setBrush(palette.cardBg);
            painter->drawRoundedRect(cardRect, 12, 12);

            const auto actionGeometry = buildFileCardActionGeometry(option, index);
            const int fileIconSize = compactTransferCard ? 40 : 44;
            const QRect iconRect(cardRect.x() + 12, cardRect.y() + 12, fileIconSize, fileIconSize);
            painter->setPen(Qt::NoPen);
            painter->setBrush(palette.iconBg);
            painter->drawRoundedRect(iconRect, 12, 12);

            QFont iconFont = bodyFont;
            iconFont.setBold(true);
            painter->setFont(iconFont);
            painter->setPen(outgoing ? QColor(AppStyle::bubbleOutText()) : QColor(AppStyle::accent()));
            painter->drawText(iconRect, Qt::AlignCenter, UiIcons::actionFiles());

            const int titleX = iconRect.right() + 12;
            int contentRight = cardRect.right() - 14;
            if (actionGeometry.hasTransferCancelChip) {
                contentRight = actionGeometry.transferCancelChipRect.left() - 10;
            }
            const int titleWidth = qMax(96, contentRight - titleX);

            QFont fileTitleFont = bodyFont;
            fileTitleFont.setBold(true);
            painter->setFont(fileTitleFont);
            painter->setPen(palette.titleColor);
            painter->drawText(QRect(titleX, cardRect.y() + 12, titleWidth, bodyMetrics.height() + 2),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              QFontMetrics(fileTitleFont).elidedText(attachmentName, Qt::ElideMiddle, titleWidth));

            painter->setFont(metaFont);
            painter->setPen(palette.metaColor);
            const bool hasLocalFile = hasLocalAttachmentFile(localFilePath);
            const bool fileReadyToOpen =
                hasLocalFile && (outgoing || deliveryState >= static_cast<int>(MessageDeliveryState::Received));

            if (compactTransferCard) {
                const qint64 displayTransferBytes =
                    transferDisplayBytesForState(transferState, transferBytesCompleted, transferFileSize);
                const qreal ratio = transferDisplayRatio(displayTransferBytes, transferFileSize);
                const int percent = transferDisplayPercent(displayTransferBytes, transferFileSize);
                QString progressSummary = transferStateSummary(transferState,
                                                               displayTransferBytes,
                                                               transferFileSize,
                                                               transferSpeed);
                if (transferFileSize > 0) {
                    progressSummary = QStringLiteral("%1  ·  %2%")
                                          .arg(progressSummary,
                                               QString::number(percent));
                    progressSummary.replace(QStringLiteral("路"), QStringLiteral("·"));
                }
                const int statusTop = cardRect.y() + 12 + bodyMetrics.height() + 4;
                painter->drawText(QRect(titleX,
                                        statusTop,
                                        titleWidth,
                                        metaMetrics.height()),
                                  Qt::AlignLeft | Qt::AlignVCenter,
                                  QFontMetrics(metaFont).elidedText(progressSummary,
                                                                    Qt::ElideRight,
                                                                    titleWidth));

                const QRect progressTrack(titleX,
                                          statusTop + metaMetrics.height() + 8,
                                          qMax(80, cardRect.right() - titleX - 12),
                                          6);
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor(AppStyle::selectedBg()));
                painter->drawRoundedRect(progressTrack, 3, 3);
                painter->setBrush(QColor(AppStyle::accent()));
                painter->drawRoundedRect(QRectF(progressTrack.x(),
                                                progressTrack.y(),
                                                progressTrack.width() * ratio,
                                                progressTrack.height()),
                                         3,
                                         3);

                if (actionGeometry.hasTransferCancelChip) {
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(QColor(AppStyle::hoverBg()));
                    painter->drawRoundedRect(actionGeometry.transferCancelChipRect, 10, 10);
                    painter->setPen(QColor(AppStyle::danger()));
                    painter->drawText(actionGeometry.transferCancelChipRect,
                                      Qt::AlignCenter,
                                      QStringLiteral("\u53D6\u6D88"));
                }
            } else {
                const int chipY = cardRect.bottom() - metaMetrics.height() - 12;
                const QRect primaryChip(cardRect.x() + 12, chipY - 2, 84, metaMetrics.height() + 8);
                const QRect secondaryChip(primaryChip.right() + 8, chipY - 2, 108, metaMetrics.height() + 8);

                painter->setPen(Qt::NoPen);
                painter->setBrush(primaryChip.contains(chipHoverPos) ? palette.chipBg.darker(115) : palette.chipBg);
                painter->drawRoundedRect(primaryChip, 9, 9);
                painter->setBrush(secondaryChip.contains(chipHoverPos) ? palette.chipBg.darker(115) : palette.chipBg);
                painter->drawRoundedRect(secondaryChip, 9, 9);

            painter->setFont(metaFont);
                painter->setPen(palette.chipText);
            const QString primaryChipText = fileReadyToOpen
                                                ? QStringLiteral("\u6253\u5F00\u6587\u4EF6")
                                                : QStringLiteral("\u6587\u4EF6");
            QString secondaryChipText;
            if (fileReadyToOpen) {
                secondaryChipText = QStringLiteral("\u6253\u5F00\u6587\u4EF6\u5939");
            } else if (deliveryState == static_cast<int>(MessageDeliveryState::Failed)) {
                secondaryChipText = QStringLiteral("\u4F20\u8F93\u5931\u8D25");
            } else if (outgoing) {
                secondaryChipText = QStringLiteral("\u7B49\u5F85\u63A5\u6536");
            } else {
                secondaryChipText = QStringLiteral("\u51C6\u5907\u63A5\u6536");
            }
            painter->drawText(primaryChip, Qt::AlignCenter, primaryChipText);
            painter->drawText(secondaryChip, Qt::AlignCenter, secondaryChipText);

            if (fileReadyToOpen && FilePreviewWidget::isPreviewSupported(attachmentName)) {
                const QRect previewChip(secondaryChip.right() + 8, chipY - 2, 60, metaMetrics.height() + 8);
                painter->setPen(Qt::NoPen);
                painter->setBrush(previewChip.contains(chipHoverPos) ? palette.chipBg.darker(115) : palette.chipBg);
                painter->drawRoundedRect(previewChip, 9, 9);
                painter->setPen(palette.chipText);
                painter->drawText(previewChip, Qt::AlignCenter, QStringLiteral("预览"));
            }

            /*

                    QStringLiteral("%1 · %2")
                        .arg(QStringLiteral("%1%").arg(percent),
                             transferStateSummary(transferState, transferBytesCompleted, transferFileSize));
                painter->drawText(QRect(progressTrack.x(),
                                        progressTrack.bottom() + 6,
                                        cardRect.width() - 24,
                                        metaMetrics.height() + 2),
                                  Qt::AlignLeft | Qt::AlignVCenter,
                                  QFontMetrics(metaFont).elidedText(progressText,
                                                                    Qt::ElideRight,
                                                                    cardRect.width() - 24));

                if (transferCancelable) {
                    const auto geometry = buildFileCardActionGeometry(option, index);
                    if (geometry.hasTransferCancelChip) {
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(QColor(AppStyle::hoverBg()));
                        painter->drawRoundedRect(geometry.transferCancelChipRect, 9, 9);
                        painter->setPen(QColor(AppStyle::danger()));
                        painter->drawText(geometry.transferCancelChipRect,
                                          Qt::AlignCenter,
                                          QStringLiteral("取消"));
                    }
                }
            */
            }
        }
    } else if (isResourceReference) {
        const QRect cardRect(bubbleX + horizontalPadding,
                             innerY,
                             bubbleWidth - horizontalPadding * 2,
                             qMax(98, bodyMetrics.height() + metaMetrics.height() * 2 + 40));
        const FileCardVisualPalette palette = fileCardVisualPalette(outgoing, false);
        const QString title = resourcePayload.has_value() && !resourcePayload->title.trimmed().isEmpty()
            ? resourcePayload->title.trimmed()
            : body.trimmed();
        const QString subtitle = resourcePayload.has_value() && !resourcePayload->subtitle.trimmed().isEmpty()
            ? resourcePayload->subtitle.trimmed()
            : resourceKindLabel(resourcePayload.has_value() ? resourcePayload->kind : QString());
        const bool isSharedFile = resourcePayload.has_value()
            && resourcePayload->kind == QStringLiteral("shared_file");
        const QString primaryChipText = isSharedFile
            ? QStringLiteral("下载")
            : resourceKindLabel(resourcePayload.has_value() ? resourcePayload->kind : QString());
        const QString secondaryChipText = isSharedFile
            ? QStringLiteral("历史版本")
            : (resourcePayload.has_value() && !resourcePayload->status.trimmed().isEmpty()
                ? resourcePayload->status.trimmed()
                : QStringLiteral("已同步"));

        painter->setPen(Qt::NoPen);
        painter->setBrush(palette.cardBg);
        painter->drawRoundedRect(cardRect, 12, 12);

        const QRect iconRect(cardRect.x() + 12, cardRect.y() + 12, 44, 44);
        painter->setPen(Qt::NoPen);
        painter->setBrush(palette.iconBg);
        painter->drawRoundedRect(iconRect, 12, 12);

        QFont iconFont = bodyFont;
        iconFont.setBold(true);
        painter->setFont(iconFont);
        painter->setPen(outgoing ? QColor(AppStyle::bubbleOutText()) : QColor(AppStyle::accent()));
        painter->drawText(iconRect, Qt::AlignCenter, UiIcons::actionFiles());

        const int titleX = iconRect.right() + 12;
        const int titleWidth = cardRect.right() - titleX - 14;
        QFont titleFont = bodyFont;
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(palette.titleColor);
        painter->drawText(QRect(titleX, cardRect.y() + 12, titleWidth, bodyMetrics.height() + 2),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(titleFont).elidedText(title, Qt::ElideRight, titleWidth));

        painter->setFont(metaFont);
        painter->setPen(palette.metaColor);
        painter->drawText(QRect(titleX,
                                cardRect.y() + 12 + bodyMetrics.height() + 6,
                                titleWidth,
                                metaMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(metaFont).elidedText(subtitle, Qt::ElideRight, titleWidth));

        const int chipY = cardRect.bottom() - metaMetrics.height() - 12;
        const QRect primaryChip(cardRect.x() + 12, chipY - 2, 84, metaMetrics.height() + 8);
        const QRect secondaryChip(primaryChip.right() + 8, chipY - 2, 96, metaMetrics.height() + 8);
        painter->setPen(Qt::NoPen);
        painter->setBrush(palette.chipBg);
        painter->drawRoundedRect(primaryChip, 9, 9);
        painter->drawRoundedRect(secondaryChip, 9, 9);

        painter->setFont(metaFont);
        painter->setPen(palette.chipText);
        painter->drawText(primaryChip, Qt::AlignCenter, primaryChipText);
        painter->drawText(secondaryChip, Qt::AlignCenter, secondaryChipText);
    }

    currentY += bubbleHeight + (pureImageBubble ? 2 : 6);

    // === Reaction Pills ===
    {
        const QString reactionsJson = index.data(MessageListModel::ReactionsJsonRole).toString();
        if (!reactionsJson.isEmpty()) {
            const QJsonObject reactions = QJsonDocument::fromJson(reactionsJson.toUtf8()).object();
            if (!reactions.isEmpty()) {
                static const QStringList kEmojiOrder = {
                    QStringLiteral("\xF0\x9F\x91\x8D"), QStringLiteral("\xE2\x9D\xA4\xEF\xB8\x8F"),
                    QStringLiteral("\xF0\x9F\x98\x82"), QStringLiteral("\xF0\x9F\x98\xAE"),
                    QStringLiteral("\xF0\x9F\x8E\x89"), QStringLiteral("\xF0\x9F\x91\x80"),
                    QStringLiteral("\xF0\x9F\x91\x8C")
                };
                const int pillHeight = 22;
                const int pillSpacing = 4;
                const int pillPadding = 6;
                const int pillRadius = 10;
                int pillX = outgoing ? (bubbleX + bubbleWidth) : bubbleX;
                // 从气泡左侧开始向右排列
                pillX = bubbleX;

                QVector<QPair<QRect, QString>> pillRects;
                QFont pillFont = metaFont;
                pillFont.setPixelSize(11);
                const QFontMetrics pillFm(pillFont);

                for (const QString& emoji : kEmojiOrder) {
                    if (!reactions.contains(emoji)) continue;
                    const QJsonArray reactors = reactions.value(emoji).toArray();
                    if (reactors.isEmpty()) continue;

                    const QString pillText = QStringLiteral("%1 %2").arg(emoji).arg(reactors.size());
                    const int pillTextWidth = pillFm.horizontalAdvance(pillText);
                    const int pillWidth = pillTextWidth + pillPadding * 2;
                    const QRect pillRect(pillX, currentY, pillWidth, pillHeight);

                    // 判断自己是否参与
                    bool selfParticipated = false;
                    for (const auto& r : reactors) {
                        if (r.toString() == m_localClientId) { selfParticipated = true; break; }
                    }

                    // 绘制 pill 背景
                    painter->save();
                    painter->setRenderHint(QPainter::Antialiasing);
                    painter->setPen(selfParticipated
                        ? QPen(QColor(AppStyle::accent()), 1.5)
                        : QPen(QColor(AppStyle::border()), 1));
                    painter->setBrush(QColor(AppStyle::surfaceAlt()));
                    painter->drawRoundedRect(pillRect, pillRadius, pillRadius);

                    // 绘制文字
                    painter->setPen(QColor(AppStyle::textSecondary()));
                    painter->setFont(pillFont);
                    painter->drawText(pillRect, Qt::AlignCenter, pillText);
                    painter->restore();

                    pillRects.append({pillRect, emoji});
                    pillX += pillWidth + pillSpacing;
                }

                m_reactionPillRects[QPersistentModelIndex(index)] = pillRects;
                currentY += pillHeight + 6;
            } else {
                m_reactionPillRects.remove(QPersistentModelIndex(index));
            }
        } else {
            m_reactionPillRects.remove(QPersistentModelIndex(index));
        }
    }

    if (outgoing) {
        painter->setFont(metaFont);
        painter->setPen(QColor(AppStyle::textSecondary()));
        const int groupReadCount   = index.data(MessageListModel::GroupReadCountRole).toInt();
        const int groupActiveMemberCount = index.data(MessageListModel::GroupActiveMemberCountRole).toInt();
        const QString deliveryText = deliveryIndicatorText(deliveryState, outgoing,
                                                           groupReadCount, groupActiveMemberCount);
        const QRect footerRect(bubbleX, currentY, bubbleWidth, footerHeight);
        // Cache rect for click detection on group messages (any delivery state)
        if (groupActiveMemberCount > 0) {
            const int textW = metaMetrics.horizontalAdvance(deliveryText);
            const QRect clickableRect(footerRect.right() - textW - 4,
                                      footerRect.y(),
                                      textW + 8,
                                      footerRect.height());
            m_deliveryTextRects[QPersistentModelIndex(index)] = clickableRect;
            // Draw with clickable style (underline hint)
            painter->setPen(QColor(AppStyle::accent()));
        } else {
            m_deliveryTextRects.remove(QPersistentModelIndex(index));
        }
        if (isEdited) {
            const QString editedLabel = QStringLiteral("(\u5DF2\u7F16\u8F91)  ") + deliveryText;
            painter->drawText(footerRect,
                              Qt::AlignRight | Qt::AlignVCenter,
                              editedLabel);
        } else {
            painter->drawText(footerRect,
                              Qt::AlignRight | Qt::AlignVCenter,
                              deliveryText);
        }
    } else if (isEdited) {
        painter->setFont(metaFont);
        QColor editedColor = QColor(AppStyle::textSecondary());
        editedColor.setAlpha(180);
        painter->setPen(editedColor);
        painter->drawText(QRect(bubbleX, currentY, bubbleWidth, metaMetrics.height()),
                          Qt::AlignRight | Qt::AlignVCenter,
                          QStringLiteral("(\u5DF2\u7F16\u8F91)"));
    }

    painter->restore();
}

QString MessageBubbleDelegate::deliveryIndicatorTextForTesting(int state, bool outgoing,
                                                               int groupReadCount,
                                                               int groupActiveMemberCount)
{
    return deliveryIndicatorText(state, outgoing, groupReadCount, groupActiveMemberCount);
}
