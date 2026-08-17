#include "ui/ReactionToolbar.h"

#include "ui/AppStyle.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>

static const QStringList kReactionEmojis = {
    QStringLiteral("\xF0\x9F\x91\x8D"),   // 👍
    QStringLiteral("\xE2\x9D\xA4\xEF\xB8\x8F"), // ❤️
    QStringLiteral("\xF0\x9F\x98\x82"),   // 😂
    QStringLiteral("\xF0\x9F\x98\xAE"),   // 😮
    QStringLiteral("\xF0\x9F\x8E\x89"),   // 🎉
    QStringLiteral("\xF0\x9F\x91\x80")    // 👀
};

ReactionToolbar::ReactionToolbar(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setupUi();

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(400);
    connect(m_hideTimer, &QTimer::timeout, this, &QWidget::hide);
}

void ReactionToolbar::setupUi()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);

    for (const QString& emoji : kReactionEmojis) {
        auto* btn = new QPushButton(emoji, this);
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFlat(true);
        connect(btn, &QPushButton::clicked, this, [this, emoji]() {
            emit reactionSelected(m_messageId, emoji);
            hide();
        });
        layout->addWidget(btn);
    }

    setFixedHeight(34);
    adjustSize();
    updateStyleSheet();
    hide();
}

void ReactionToolbar::updateStyleSheet()
{
    setStyleSheet(QStringLiteral(
        "ReactionToolbar {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  font-size: 16px;"
        "  border-radius: 4px;"
        "  padding: 0px;"
        "}"
        "QPushButton:hover {"
        "  background: %3;"
        "}")
        .arg(AppStyle::surface(), AppStyle::border(), AppStyle::hoverBg()));
}

void ReactionToolbar::showForMessage(const QString& messageId, const QRect& bubbleRect, bool isOutgoing)
{
    m_messageId = messageId;
    m_hideTimer->stop();
    updateStyleSheet();

    // 定位：气泡右上角偏移
    const int toolbarWidth = sizeHint().width();
    int x = isOutgoing
        ? (bubbleRect.right() - toolbarWidth)
        : bubbleRect.left();
    const int y = bubbleRect.top() - height() - 2;

    // 确保不超出父 widget 边界
    if (parentWidget()) {
        x = qBound(0, x, parentWidget()->width() - toolbarWidth);
    }

    move(x, qMax(0, y));
    show();
    raise();
}

void ReactionToolbar::scheduleHide()
{
    m_hideTimer->start();
}

void ReactionToolbar::cancelHide()
{
    m_hideTimer->stop();
}

void ReactionToolbar::enterEvent(QEnterEvent* event)
{
    m_hideTimer->stop();
    QWidget::enterEvent(event);
}

void ReactionToolbar::leaveEvent(QEvent* event)
{
    m_hideTimer->start();
    QWidget::leaveEvent(event);
}
