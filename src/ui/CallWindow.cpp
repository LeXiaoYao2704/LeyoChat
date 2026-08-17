#include "ui/CallWindow.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <ElaText.h>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QVBoxLayout>
#include <ElaToolButton.h>
#include <ElaPushButton.h>

CallWindow::CallWindow(Mode mode, const QString& peerName,
                       const QString& avatarPath, QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_mode(mode)
    , m_peerName(peerName)
{
    Q_UNUSED(avatarPath)
    setFixedSize(300, 440);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    buildUi();

    if (mode == Mode::Outgoing) {
        m_state = CallSession::State::OutgoingRing;
    } else {
        m_state = CallSession::State::IncomingRing;
    }
    applyState();
}

void CallWindow::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);  // 阴影留白
    root->setSpacing(0);

    // --- 最小化按钮 ---
    m_minimizeButton = new ElaToolButton(this);
    m_minimizeButton->setText(QStringLiteral("\u2014"));
    m_minimizeButton->setIsTransparent(true);
    m_minimizeButton->setFixedSize(24, 24);
    m_minimizeButton->setCursor(Qt::PointingHandCursor);
    connect(m_minimizeButton, &QAbstractButton::clicked, this, &QWidget::showMinimized);

    // --- 关闭按钮 ---
    m_closeButton = new ElaToolButton(this);
    m_closeButton->setText(QStringLiteral("\u2715"));
    m_closeButton->setIsTransparent(true);
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    connect(m_closeButton, &QAbstractButton::clicked, this, &QWidget::close);

    auto* topBar = new QHBoxLayout;
    topBar->setContentsMargins(8, 4, 4, 0);
    topBar->addStretch();
    topBar->addWidget(m_minimizeButton);
    topBar->addWidget(m_closeButton);
    root->addLayout(topBar);
    root->addSpacing(12);

    // --- 头像 ---
    m_avatarLabel = new ElaText(this);
    m_avatarLabel->setFixedSize(72, 72);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "stop:0 #4FC978, stop:1 #2E9E5A);"
        "border-radius: 36px; font-size: 26px; color: white; }"));
    const QString initial = m_peerName.isEmpty() ? QStringLiteral("?") : m_peerName.left(1);
    m_avatarLabel->setText(initial);
    root->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    root->addSpacing(14);

    // --- 名字 ---
    m_nameLabel = new ElaText(m_peerName, this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #F0F3F8; font-size: 16px; font-weight: bold; }"));
    root->addWidget(m_nameLabel);
    root->addSpacing(6);

    // --- 状态文字 + 呼吸指示点 ---
    auto* statusRow = new QHBoxLayout;
    statusRow->setSpacing(6);
    statusRow->addStretch();

    m_dotIndicator = new ElaText(QStringLiteral("\u25CF"), this);
    m_dotIndicator->setStyleSheet(QStringLiteral("QLabel { color: #4FC978; font-size: 10px; }"));
    m_dotIndicator->setVisible(false);
    statusRow->addWidget(m_dotIndicator);

    m_statusLabel = new ElaText(this);
    m_statusLabel->setStyleSheet(QStringLiteral("QLabel { color: #8892A4; font-size: 13px; }"));
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();
    root->addLayout(statusRow);
    root->addSpacing(4);

    // --- 通话计时 ---
    m_durationLabel = new ElaText(QStringLiteral("00:00"), this);
    m_durationLabel->setAlignment(Qt::AlignCenter);
    m_durationLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #4FC978; font-size: 22px; font-weight: bold; letter-spacing: 2px; }"));
    m_durationLabel->setVisible(false);
    root->addWidget(m_durationLabel);

    root->addStretch();

    // --- 功能按钮行 ---
    m_toolRow = new QWidget(this);
    auto* toolLayout = new QHBoxLayout(m_toolRow);
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->setSpacing(16);

    m_muteButton = new ElaToolButton(m_toolRow);
    m_muteButton->setText(QStringLiteral("\xF0\x9F\x94\x87 \u9759\u97f3"));
    m_muteButton->setIsTransparent(true);
    m_muteButton->setFixedHeight(36);
    m_muteButton->setCursor(Qt::PointingHandCursor);

    m_screenShareButton = new ElaToolButton(m_toolRow);
    m_screenShareButton->setText(QStringLiteral("\xF0\x9F\x96\xA5 \u5171\u4eab"));
    m_screenShareButton->setIsTransparent(true);
    m_screenShareButton->setFixedHeight(36);
    m_screenShareButton->setCursor(Qt::PointingHandCursor);

    m_remoteControlButton = new ElaToolButton(m_toolRow);
    m_remoteControlButton->setText(QStringLiteral("\xF0\x9F\x96\xB1 \u8bf7\u6c42\u63a7\u5236"));
    m_remoteControlButton->setIsTransparent(true);
    m_remoteControlButton->setFixedHeight(36);
    m_remoteControlButton->setCursor(Qt::PointingHandCursor);

    toolLayout->addStretch();
    toolLayout->addWidget(m_muteButton);
    toolLayout->addWidget(m_screenShareButton);
    toolLayout->addWidget(m_remoteControlButton);
    toolLayout->addStretch();

    m_toolRow->setVisible(false);
    root->addWidget(m_toolRow);
    root->addSpacing(16);

    // --- 主操作按钮行 ---
    auto* actionRow = new QHBoxLayout;
    actionRow->setSpacing(16);

    m_primaryButton = new ElaPushButton(this);
    m_primaryButton->setFixedSize(110, 42);
    m_primaryButton->setCursor(Qt::PointingHandCursor);

    m_answerButton = new ElaPushButton(QStringLiteral("\u63a5\u542c"), this);
    m_answerButton->setFixedSize(110, 42);
    m_answerButton->setCursor(Qt::PointingHandCursor);
    m_answerButton->setVisible(false);

    actionRow->addStretch();
    actionRow->addWidget(m_primaryButton);
    actionRow->addWidget(m_answerButton);
    actionRow->addStretch();
    root->addLayout(actionRow);
    root->addSpacing(12);

    // --- 信号连接 ---
    connect(m_muteButton, &QAbstractButton::clicked, this, [this]() {
        m_muted = !m_muted;
        m_muteButton->setText(m_muted ? QStringLiteral("\xF0\x9F\x94\x8A \u53d6\u6d88\u9759\u97f3")
                                      : QStringLiteral("\xF0\x9F\x94\x87 \u9759\u97f3"));
        emit muteToggled(m_muted);
    });
    connect(m_screenShareButton, &QAbstractButton::clicked, this, &CallWindow::screenShareClicked);
    connect(m_remoteControlButton, &QAbstractButton::clicked, this, &CallWindow::remoteControlClicked);
    connect(m_answerButton, &QAbstractButton::clicked, this, &CallWindow::answerClicked);

    connect(m_primaryButton, &QAbstractButton::clicked, this, [this]() {
        switch (m_state) {
        case CallSession::State::OutgoingRing:
            emit cancelClicked();
            break;
        case CallSession::State::IncomingRing:
            emit rejectClicked();
            break;
        case CallSession::State::Connecting:
        case CallSession::State::Active:
            emit hangupClicked();
            break;
        default:
            break;
        }
    });

    // --- 计时器 ---
    m_durationTimer = new QTimer(this);
    m_durationTimer->setInterval(1000);
    connect(m_durationTimer, &QTimer::timeout, this, &CallWindow::updateDuration);
}

void CallWindow::updateState(CallSession::State state)
{
    m_state = state;
    applyState();
}

void CallWindow::setAudioMuted(bool muted)
{
    m_muted = muted;
    m_muteButton->setText(muted ? QStringLiteral("\xF0\x9F\x94\x8A \u53d6\u6d88\u9759\u97f3")
                                : QStringLiteral("\xF0\x9F\x94\x87 \u9759\u97f3"));
}

void CallWindow::setScreenSharing(bool sharing)
{
    m_screenShareButton->setText(sharing ? QStringLiteral("\xF0\x9F\x96\xA5 \u505c\u6b62\u5171\u4eab")
                                        : QStringLiteral("\xF0\x9F\x96\xA5 \u5171\u4eab"));
}

void CallWindow::setRemoteControlActive(bool active)
{
    m_remoteControlButton->setText(active ? QStringLiteral("\xF0\x9F\x96\xB1 \u505c\u6b62\u63a7\u5236")
                                         : QStringLiteral("\xF0\x9F\x96\xB1 \u8bf7\u6c42\u63a7\u5236"));
}

void CallWindow::applyState()
{
    auto applyCancelStyle = [](ElaPushButton* btn) {
        btn->setLightDefaultColor(QColor(0x4A, 0x52, 0x60));
        btn->setDarkDefaultColor(QColor(0x4A, 0x52, 0x60));
        btn->setLightHoverColor(QColor(0x5A, 0x62, 0x70));
        btn->setDarkHoverColor(QColor(0x5A, 0x62, 0x70));
        btn->setLightTextColor(QColor(0xE0, 0xE4, 0xEA));
        btn->setDarkTextColor(QColor(0xE0, 0xE4, 0xEA));
        btn->setBorderRadius(21);
    };
    auto applyHangupStyle = [](ElaPushButton* btn) {
        btn->setLightDefaultColor(QColor(0xF0, 0x50, 0x50));
        btn->setDarkDefaultColor(QColor(0xF0, 0x50, 0x50));
        btn->setLightHoverColor(QColor(0xE0, 0x3E, 0x3E));
        btn->setDarkHoverColor(QColor(0xE0, 0x3E, 0x3E));
        btn->setLightTextColor(Qt::white);
        btn->setDarkTextColor(Qt::white);
        btn->setBorderRadius(21);
    };

    switch (m_state) {
    case CallSession::State::OutgoingRing:
        m_statusLabel->setText(QStringLiteral("\u6b63\u5728\u547c\u53eb\uff0c\u7b49\u5f85\u63a5\u542c\u2026"));
        m_dotIndicator->setVisible(true);
        m_primaryButton->setText(QStringLiteral("\u53d6\u6d88"));
        applyCancelStyle(m_primaryButton);
        m_primaryButton->setVisible(true);
        m_answerButton->setVisible(false);
        m_toolRow->setVisible(false);
        m_durationLabel->setVisible(false);
        m_durationTimer->stop();
        break;

    case CallSession::State::IncomingRing:
        m_statusLabel->setText(QStringLiteral("%1 \u53d1\u8d77\u8bed\u97f3\u901a\u8bdd").arg(m_peerName));
        m_dotIndicator->setVisible(true);
        m_primaryButton->setText(QStringLiteral("\u62d2\u7edd"));
        applyHangupStyle(m_primaryButton);
        m_primaryButton->setVisible(true);
        m_answerButton->setVisible(true);
        m_toolRow->setVisible(false);
        m_durationLabel->setVisible(false);
        m_durationTimer->stop();
        break;

    case CallSession::State::Connecting:
        m_statusLabel->setText(QStringLiteral("\u6b63\u5728\u8fde\u63a5\u2026"));
        m_dotIndicator->setVisible(true);
        m_primaryButton->setText(QStringLiteral("\u6302\u65ad"));
        applyHangupStyle(m_primaryButton);
        m_primaryButton->setVisible(true);
        m_answerButton->setVisible(false);
        m_toolRow->setVisible(false);
        m_durationLabel->setVisible(false);
        m_durationTimer->stop();
        break;

    case CallSession::State::Active:
        m_statusLabel->setText(QStringLiteral("\u901a\u8bdd\u4e2d"));
        m_dotIndicator->setVisible(true);
        m_dotIndicator->setStyleSheet(QStringLiteral("QLabel { color: #4FC978; font-size: 10px; }"));
        m_primaryButton->setText(QStringLiteral("\u6302\u65ad"));
        applyHangupStyle(m_primaryButton);
        m_primaryButton->setVisible(true);
        m_answerButton->setVisible(false);
        m_toolRow->setVisible(true);
        m_durationLabel->setVisible(true);
        m_activeStartMs = QDateTime::currentMSecsSinceEpoch();
        m_durationLabel->setText(QStringLiteral("00:00"));
        m_durationTimer->start();
        break;

    case CallSession::State::Ended:
    case CallSession::State::Idle:
        m_dotIndicator->setVisible(false);
        m_durationTimer->stop();
        QTimer::singleShot(1000, this, &CallWindow::close);
        break;
    }
}

void CallWindow::updateDuration()
{
    if (m_activeStartMs <= 0) return;
    const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_activeStartMs;
    const int totalSec = static_cast<int>(elapsed / 1000);
    const int min = totalSec / 60;
    const int sec = totalSec % 60;
    m_durationLabel->setText(QStringLiteral("%1:%2")
                                 .arg(min, 2, 10, QChar('0'))
                                 .arg(sec, 2, 10, QChar('0')));
}

void CallWindow::closeEvent(QCloseEvent* event)
{
    switch (m_state) {
    case CallSession::State::OutgoingRing:
        emit cancelClicked();
        break;
    case CallSession::State::IncomingRing:
        emit rejectClicked();
        break;
    case CallSession::State::Connecting:
    case CallSession::State::Active:
        emit hangupClicked();
        break;
    default:
        break;
    }
    event->accept();
}

void CallWindow::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 阴影区域：外层 12px 边距留给阴影
    const QRectF cardRect(12, 12, width() - 24, height() - 24);
    const qreal radius = 16.0;

    // 绘制多层阴影
    for (int i = 4; i >= 1; --i) {
        QPainterPath shadowPath;
        shadowPath.addRoundedRect(cardRect.adjusted(-i, -i, i, i), radius + i, radius + i);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 12 * i));
        painter.drawPath(shadowPath);
    }

    // 绘制圆角卡片背景
    QPainterPath cardPath;
    cardPath.addRoundedRect(cardRect, radius, radius);
    painter.setPen(QPen(QColor(0x3A, 0x42, 0x50), 1));
    painter.setBrush(QColor(0x1E, 0x21, 0x28));
    painter.drawPath(cardPath);
}

void CallWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void CallWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragStart);
        event->accept();
    }
}

void CallWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    }
}
