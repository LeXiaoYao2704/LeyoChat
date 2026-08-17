#include "ui/CallOverlayWidget.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <ElaText.h>
#include <ElaPushButton.h>
#include <QTimer>

CallOverlayWidget::CallOverlayWidget(const QString& peerName, QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("callOverlayWidget"));
    setFixedHeight(44);
    setMinimumWidth(360);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(8);

    auto* titleLabel = new ElaText(QStringLiteral("通话中: %1").arg(peerName), this);
    m_durationLabel = new ElaText(QStringLiteral("00:00"), this);

    m_muteButton = new ElaPushButton(QStringLiteral("静音"), this);
    m_screenShareButton = new ElaPushButton(QStringLiteral("\u5171\u4eab\u684c\u9762"), this);
    m_screenShareButton->setVisible(false);
    m_remoteControlButton = new ElaPushButton(QStringLiteral("\u8fdc\u7a0b\u63a7\u5236"), this);
    m_remoteControlButton->setVisible(false);
    auto* hangupButton = new ElaPushButton(QStringLiteral("\u6302\u65ad"), this);

    layout->addWidget(titleLabel);
    layout->addWidget(m_durationLabel);
    layout->addStretch();
    layout->addWidget(m_muteButton);
    layout->addWidget(m_screenShareButton);
    layout->addWidget(m_remoteControlButton);
    layout->addWidget(hangupButton);

    connect(m_muteButton, &QAbstractButton::clicked, this, [this]() {
        m_muted = !m_muted;
        m_muteButton->setText(m_muted ? QStringLiteral("取消静音") : QStringLiteral("静音"));
        emit muteToggled(m_muted);
    });
    connect(hangupButton, &QAbstractButton::clicked, this, &CallOverlayWidget::hangupClicked);
    connect(m_screenShareButton, &QAbstractButton::clicked, this, &CallOverlayWidget::screenShareClicked);
    connect(m_remoteControlButton, &QAbstractButton::clicked, this, &CallOverlayWidget::remoteControlClicked);

    m_startedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_durationTimer = new QTimer(this);
    m_durationTimer->setInterval(1000);
    connect(m_durationTimer, &QTimer::timeout, this, &CallOverlayWidget::updateDuration);
    m_durationTimer->start();

    setStyleSheet(QStringLiteral(
        "QWidget#callOverlayWidget { background:#20242B; border:1px solid #3A4250; border-radius:10px; }"
        "QLabel { color:#F0F3F8; }"
        "QPushButton { border-radius:7px; padding:4px 10px; background:#2C3440; color:#F0F3F8; }"
        "QPushButton:hover { background:#3A4554; }"));
}

void CallOverlayWidget::setAudioMuted(bool muted)
{
    m_muted = muted;
    m_muteButton->setText(m_muted ? QStringLiteral("取消静音") : QStringLiteral("静音"));
}

void CallOverlayWidget::setScreenShareVisible(bool visible)
{
    m_screenShareButton->setVisible(visible);
}

void CallOverlayWidget::setRemoteControlVisible(bool visible)
{
    m_remoteControlButton->setVisible(visible);
}

void CallOverlayWidget::updateDuration()
{
    const qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - m_startedAtMs;
    const int secs = static_cast<int>(elapsedMs / 1000);
    m_durationLabel->setText(QStringLiteral("%1:%2")
                                 .arg(secs / 60, 2, 10, QLatin1Char('0'))
                                 .arg(secs % 60, 2, 10, QLatin1Char('0')));
}
