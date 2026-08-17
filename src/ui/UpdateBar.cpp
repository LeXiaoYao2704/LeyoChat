#include "ui/UpdateBar.h"
#include <ElaPushButton.h>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QSizePolicy>

UpdateBar::UpdateBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(30);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setVisible(false);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 10, 0);
    layout->setSpacing(6);

    m_button = new ElaPushButton(this);
    m_button->setCursor(Qt::PointingHandCursor);
    m_button->setFixedHeight(26);
    m_button->setMinimumWidth(88);
    m_button->setBorderRadius(6);
    m_button->setLightDefaultColor(QColor(0x16, 0x5D, 0xFF));
    m_button->setDarkDefaultColor(QColor(0x16, 0x5D, 0xFF));
    m_button->setLightHoverColor(QColor(0x12, 0x52, 0xD9));
    m_button->setDarkHoverColor(QColor(0x12, 0x52, 0xD9));
    m_button->setLightPressColor(QColor(0x0E, 0x44, 0xBF));
    m_button->setDarkPressColor(QColor(0x0E, 0x44, 0xBF));
    m_button->setLightTextColor(Qt::white);
    m_button->setDarkTextColor(Qt::white);
    layout->addWidget(m_button);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedSize(92, 14);
    m_progressBar->setTextVisible(true);
    m_progressBar->setRange(0, 100);
    m_progressBar->setStyleSheet(QStringLiteral(
        "QProgressBar {"
        "  border: 1px solid #C9CDD4;"
        "  border-radius: 8px;"
        "  background: #F2F3F5;"
        "  font-size: 10px;"
        "  text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "  background: #165DFF;"
        "  border-radius: 7px;"
        "}"));
    m_progressBar->hide();
    layout->addWidget(m_progressBar);

    connect(m_button, &QPushButton::clicked, this, &UpdateBar::updateClicked);
}

void UpdateBar::showUpdate(const QString& version)
{
    m_pendingVersion = version;
    m_button->setText(QStringLiteral("\u2191 v%1").arg(version));
    m_button->show();
    m_button->setEnabled(true);
    m_progressBar->hide();
    adjustSize();
    show();
    raise();
}

void UpdateBar::showProgress(int percent)
{
    m_button->setText(QStringLiteral("\u2B07 \u4E0B\u8F7D"));
    m_button->setEnabled(false);
    m_progressBar->setValue(percent);
    m_progressBar->show();
    adjustSize();
    show();
    raise();
}

void UpdateBar::showReady()
{
    m_button->setText(QStringLiteral("\u2714 \u5B89\u88C5"));
    m_button->setEnabled(true);
    m_progressBar->hide();
    adjustSize();
    show();
    raise();
}

void UpdateBar::hideBar()
{
    hide();
}
