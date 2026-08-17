#include "ContextPanel.h"

#include <ElaText.h>
#include <QVBoxLayout>

ContextPanel::ContextPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("contextPanel"));
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setMinimumWidth(0);
    setMaximumWidth(0);
    hide();

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(16, 16, 16, 16);
    m_layout->setSpacing(8);

    m_titleLabel = new ElaText(this);
    m_titleLabel->setObjectName(QStringLiteral("contextPanelTitle"));
    m_summaryLabel = new ElaText(this);
    m_summaryLabel->setObjectName(QStringLiteral("contextPanelSummary"));
    m_summaryLabel->setWordWrap(true);

    m_layout->addWidget(m_titleLabel);
    m_layout->addWidget(m_summaryLabel);
    m_layout->addStretch(1);

    refreshSummary();
}

void ContextPanel::showPrivateProfile(const QString& peerId)
{
    m_contextId = peerId.trimmed();
    m_sections.clear();
    m_pinnedOpen = false;
    refreshSummary();
}

void ContextPanel::showGroupContext(const QString& groupId)
{
    m_contextId = groupId.trimmed();
    m_sections = {
        QStringLiteral("announcements"),
        QStringLiteral("members"),
        QStringLiteral("files")
    };
    m_pinnedOpen = false;
    refreshSummary();
}

bool ContextPanel::isPinnedOpen() const
{
    return m_pinnedOpen;
}

bool ContextPanel::hasSection(const QString& key) const
{
    return m_sections.contains(key.trimmed());
}

void ContextPanel::refreshSummary()
{
    if (m_sections.isEmpty()) {
        m_titleLabel->setText(QStringLiteral("Private context"));
        m_summaryLabel->setText(m_contextId);
        return;
    }

    m_titleLabel->setText(QStringLiteral("Group context"));
    m_summaryLabel->setText(m_contextId + QStringLiteral(" | announcements, members, files"));
}
