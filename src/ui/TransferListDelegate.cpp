#include "ui/TransferListDelegate.h"

#include "ui/AppStyle.h"
#include "ui/TransferListModel.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>

TransferListDelegate::TransferListDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QSize TransferListDelegate::sizeHint(const QStyleOptionViewItem& option,
                                     const QModelIndex& /*index*/) const
{
    const int baseHeight = AppStyle::transferRowHeightForFont(option.font);
    return {0, option.rect.width() > 0 && option.rect.width() < 250 ? baseHeight + 18 : baseHeight};
}

void TransferListDelegate::paint(QPainter* painter,
                                 const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;

    // — 扁平行背景 —
    if (selected) {
        painter->fillRect(option.rect, QColor(AppStyle::selectedBg()));
    } else if (hovered) {
        painter->fillRect(option.rect, QColor(AppStyle::hoverBg()));
    }

    // 选中指示条
    if (selected) {
        const QRect railRect(option.rect.left() + 2,
                             option.rect.top() + 8,
                             3,
                             option.rect.height() - 16);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(AppStyle::accent()));
        painter->drawRoundedRect(railRect, 1.5, 1.5);
    }

    const QString fileName = index.data(Qt::DisplayRole).toString().trimmed();
    const QString statusLine = index.data(TransferListModel::StatusTextRole).toString().trimmed();
    const QString detailLine = index.data(TransferListModel::DetailTextRole).toString().trimmed();
    const QString fileBadge = index.data(TransferListModel::FileBadgeRole).toString().trimmed();

    const int leftPad = 14;
    const int rightPad = 14;
    QFont titleFont = AppStyle::titleFont(option.font);
    titleFont.setPointSizeF(qMax(11.0, titleFont.pointSizeF()));
    const QFontMetrics titleMetrics(titleFont);
    QFont metaFont = AppStyle::captionFont(option.font);
    const QFontMetrics metaMetrics(metaFont);
    const int iconSize = qMax(42, titleMetrics.height() + AppStyle::kSpace20);
    const QRect iconRect(option.rect.left() + leftPad,
                         option.rect.top() + (option.rect.height() - iconSize) / 2,
                         iconSize,
                         iconSize);

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(AppStyle::accentSoft()));
    painter->drawRoundedRect(iconRect, 8, 8);

    QFont iconFont = option.font;
    iconFont.setBold(true);
    iconFont.setPixelSize(qMax(11, metaMetrics.height() - 1));
    painter->setFont(iconFont);
    painter->setPen(QColor(AppStyle::accent()));
    painter->drawText(iconRect,
                      Qt::AlignCenter,
                      fileBadge.isEmpty() ? QStringLiteral("FILE") : fileBadge);

    const int textLeft = iconRect.right() + 10;
    const int statusChipWidth = qMax(72, metaMetrics.horizontalAdvance(statusLine) + 26);
    const bool compactLayout = option.rect.width() > 0 && option.rect.width() < 250;
    const int textWidth = compactLayout
                              ? (option.rect.width() - (textLeft - option.rect.left()) - rightPad)
                              : (option.rect.width() - (textLeft - option.rect.left()) - rightPad - statusChipWidth);
    const int detailHeight = detailLine.isEmpty() ? 0 : (metaMetrics.height() + AppStyle::kSpace4);
    const int chipHeight = qMax(22, metaMetrics.height() + 8);
    const int contentTop =
        option.rect.top()
        + qMax(AppStyle::kSpace10,
               (option.rect.height()
                - (titleMetrics.height() + AppStyle::kSpace4 + metaMetrics.height() + detailHeight
                   + (compactLayout ? chipHeight : 0)))
                     / 2);

    painter->setFont(titleFont);
    painter->setPen(QColor(AppStyle::textPrimary()));
    painter->drawText(QRect(textLeft, contentTop, textWidth, titleMetrics.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(titleFont).elidedText(fileName, Qt::ElideMiddle, textWidth));

    painter->setFont(metaFont);
    painter->setPen(QColor(AppStyle::textMuted()));
    painter->drawText(QRect(textLeft,
                            contentTop + titleMetrics.height() + AppStyle::kSpace4,
                            textWidth,
                            metaMetrics.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(metaFont).elidedText(detailLine, Qt::ElideRight, textWidth));

    if (!statusLine.isEmpty()) {
        painter->setPen(QColor(0x9A, 0xA1, 0xAE));
        painter->drawText(QRect(textLeft,
                                contentTop + titleMetrics.height() + metaMetrics.height() + AppStyle::kSpace8,
                                textWidth,
                                metaMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(metaFont).elidedText(statusLine, Qt::ElideRight, textWidth));
    }

    QColor chipBg(AppStyle::surfaceAlt());
    QColor chipFg(AppStyle::textSecondary());
    if (statusLine.contains(QStringLiteral("\u4E2D"))) {
        chipBg = QColor(AppStyle::accentSoft());
        chipFg = QColor(AppStyle::accent());
    } else if (statusLine.contains(QStringLiteral("\u5B8C\u6210"))
               || statusLine.contains(QStringLiteral("\u6210\u529F"))) {
        chipBg = QColor(0xE8, 0xF8, 0xEF);
        chipFg = QColor(0x1E, 0x8E, 0x6A);
    } else if (statusLine.contains(QStringLiteral("\u5931\u8D25"))
               || statusLine.contains(QStringLiteral("\u4E2D\u65AD"))) {
        chipBg = QColor(0xFE, 0xEC, 0xEA);
        chipFg = QColor(0xD9, 0x30, 0x25);
    }

    const QRect chipRect(compactLayout ? textLeft : (option.rect.right() - rightPad - statusChipWidth),
                         compactLayout ? (contentTop + titleMetrics.height() + metaMetrics.height() * 2 + AppStyle::kSpace10)
                                       : (option.rect.center().y() - chipHeight / 2),
                         compactLayout ? qMin(statusChipWidth, textWidth) : statusChipWidth,
                         chipHeight);
    painter->setPen(Qt::NoPen);
    painter->setBrush(chipBg);
    painter->drawRoundedRect(chipRect, chipHeight / 2, chipHeight / 2);
    painter->setFont(metaFont);
    painter->setPen(chipFg);
    painter->drawText(chipRect,
                      Qt::AlignCenter,
                      QFontMetrics(metaFont).elidedText(statusLine, Qt::ElideRight, chipRect.width() - 10));

    // --- 悬浮时绘制操作按钮 ---
    if (hovered) {
        const auto state =
            static_cast<FileTransferState>(index.data(TransferListModel::StateRole).toInt());
        const bool isCompleted = (state == FileTransferState::Completed);
        const bool showRetry = index.data(TransferListModel::RetryableRole).toBool();
        const int btnSize = 22;

        // × 关闭/取消/删除按钮（非已完成状态显示）
        if (!isCompleted) {
            const QRect closeRect = closeButtonRect(option);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0xF5, 0xF5, 0xF5));
            painter->drawRoundedRect(closeRect, btnSize / 2, btnSize / 2);
            // 绘制 ×
            painter->setPen(QPen(QColor(AppStyle::textMuted()), 1.5));
            const int m = 6;
            painter->drawLine(closeRect.left() + m, closeRect.top() + m,
                              closeRect.right() - m, closeRect.bottom() - m);
            painter->drawLine(closeRect.right() - m, closeRect.top() + m,
                              closeRect.left() + m, closeRect.bottom() - m);
        }

        // ↻ 重试按钮
        if (showRetry) {
            const QRect retryRect = retryButtonRect(option);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0xF5, 0xF5, 0xF5));
            painter->drawRoundedRect(retryRect, btnSize / 2, btnSize / 2);
            // 绘制 ↻ 箭头
            QFont arrowFont = option.font;
            arrowFont.setPixelSize(14);
            painter->setFont(arrowFont);
            painter->setPen(QColor(AppStyle::accent()));
            painter->drawText(retryRect, Qt::AlignCenter, QStringLiteral("\u21BB"));
        }
    }

    if (!selected) {
        painter->setPen(QColor(AppStyle::border()));
        painter->drawLine(option.rect.left() + leftPad + iconSize + 10,
                          option.rect.bottom(),
                          option.rect.right() - rightPad,
                          option.rect.bottom());
    }

    painter->restore();
}

QRect TransferListDelegate::closeButtonRect(const QStyleOptionViewItem& option)
{
    const int btnSize = 22;
    return QRect(option.rect.right() - 12 - btnSize,
                 option.rect.top() + 8,
                 btnSize, btnSize);
}

QRect TransferListDelegate::retryButtonRect(const QStyleOptionViewItem& option)
{
    const int btnSize = 22;
    const QRect closeR = closeButtonRect(option);
    return QRect(closeR.left() - 4 - btnSize,
                 closeR.top(),
                 btnSize, btnSize);
}

bool TransferListDelegate::editorEvent(QEvent* event,
                                       QAbstractItemModel* model,
                                       const QStyleOptionViewItem& option,
                                       const QModelIndex& index)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QPoint pos = mouseEvent->pos();

        const auto state =
            static_cast<FileTransferState>(index.data(TransferListModel::StateRole).toInt());
        const bool isCompleted = (state == FileTransferState::Completed);
        const bool showRetry = index.data(TransferListModel::RetryableRole).toBool();

        if (!isCompleted && closeButtonRect(option).contains(pos)) {
            emit closeClicked(index);
            return true;
        }
        if (showRetry && retryButtonRect(option).contains(pos)) {
            emit retryClicked(index);
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
