#include "KnowledgePage.h"

#include "app/AppSettings.h"
#include "integrations/KnowServiceClient.h"
#include "integrations/KnowledgeServiceSettings.h"
#include "ui/AiKnowledgePanel.h"

#include <QSettings>
#include <QAbstractScrollArea>
#include <QVBoxLayout>

namespace {

QString aiServiceDisplayName(const KnowledgeServiceConfig& config) {
    const QString displayName = config.displayName.trimmed();
    if (!displayName.isEmpty()) return displayName;
    const QString teamLabel = config.teamLabel.trimmed();
    if (!teamLabel.isEmpty()) return teamLabel;
    const QString localId = config.localId.trimmed();
    if (!localId.isEmpty()) return localId;
    return config.baseUrl.trimmed();
}

QStringList aiServiceDisplayNames(const QVector<KnowledgeServiceConfig>& configs,
                                  const QString& defaultServiceId, int* selectedIndex) {
    QStringList serviceNames;
    int resolvedSelectedIndex = 0;
    for (const KnowledgeServiceConfig& config : configs) {
        serviceNames.push_back(aiServiceDisplayName(config));
        if (!defaultServiceId.isEmpty() && config.localId == defaultServiceId)
            resolvedSelectedIndex = serviceNames.size() - 1;
    }
    if (selectedIndex) *selectedIndex = resolvedSelectedIndex;
    return serviceNames;
}

QStringList aiServiceLocalIds(const QVector<KnowledgeServiceConfig>& configs) {
    QStringList ids;
    for (const KnowledgeServiceConfig& config : configs) ids.push_back(config.localId);
    return ids;
}

const KnowledgeServiceConfig* aiServiceConfigForId(const QVector<KnowledgeServiceConfig>& configs,
                                                   const QString& serviceId) {
    const QString normalizedId = serviceId.trimmed();
    for (const KnowledgeServiceConfig& config : configs) {
        if (config.localId == normalizedId) return &config;
    }
    return nullptr;
}

} // namespace

KnowledgePage::KnowledgePage(QWidget* parent)
    : ElaScrollPage(parent)
{
    setTitleVisible(false);

    auto* contentWidget = new QWidget(this);
    contentWidget->setObjectName(QStringLiteral("knowledgePageRoot"));
    contentWidget->setAttribute(Qt::WA_StyledBackground, true);
    contentWidget->setAutoFillBackground(false);
    contentWidget->setStyleSheet(QStringLiteral("QWidget#knowledgePageRoot { background:transparent; }"));
    auto* rootLayout = new QVBoxLayout(contentWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_aiKnowledgePanel = new AiKnowledgePanel(contentWidget);
    rootLayout->addWidget(m_aiKnowledgePanel);
    addCentralWidget(contentWidget);

    m_aiKnowServiceClient = new HttpKnowServiceClient(this);

    // 加载知识服务配置
    {
        QSettings settings = AppSettings::createSettings();
        int selectedServiceIndex = 0;
        const QVector<KnowledgeServiceConfig> configs = KnowledgeServiceSettingsStore::load(&settings);
        const QString defaultServiceId = KnowledgeServiceSettingsStore::defaultServiceId(&settings);
        m_aiKnowledgeServiceConfigs = configs;
        m_aiKnowledgePanel->setAvailableServices(
            aiServiceDisplayNames(m_aiKnowledgeServiceConfigs, defaultServiceId, &selectedServiceIndex),
            aiServiceLocalIds(m_aiKnowledgeServiceConfigs),
            selectedServiceIndex);
    }

    connect(m_aiKnowledgePanel, &AiKnowledgePanel::querySubmitted, this, [this](const QString& queryText) {
        if (!m_aiKnowledgePanel || !m_aiKnowServiceClient) return;

        const QString normalizedQuery = queryText.trimmed();
        if (normalizedQuery.isEmpty()) {
            m_aiKnowledgePanel->showQueryError(QStringLiteral("\u8BF7\u8F93\u5165\u95EE\u9898\u540E\u518D\u53D1\u8D77\u67E5\u8BE2\u3002"));
            return;
        }

        const KnowledgeServiceConfig* config =
            aiServiceConfigForId(m_aiKnowledgeServiceConfigs, m_aiKnowledgePanel->selectedServiceId());
        if (!config) {
            m_aiKnowledgePanel->showQueryError(QStringLiteral("\u5F53\u524D\u672A\u627E\u5230\u53EF\u7528\u7684\u77E5\u8BC6\u670D\u52A1\u914D\u7F6E\u3002"));
            return;
        }

        m_aiKnowledgePanel->setQueryPending(true);
        const QString queryServiceId = m_aiKnowledgePanel->selectedServiceId();
        m_aiKnowServiceClient->query(*config, normalizedQuery,
            [this, queryServiceId](KnowServiceQueryResponse response) {
                if (!m_aiKnowledgePanel) return;
                if (queryServiceId != m_aiKnowledgePanel->selectedServiceId()) {
                    m_aiKnowledgePanel->setQueryPending(false);
                    return;
                }
                if (!response.errorMessage.trimmed().isEmpty()) {
                    m_aiKnowledgePanel->showQueryError(response.errorMessage);
                    return;
                }
                m_aiKnowledgePanel->showQueryResponse(response);
            });
    });

    connect(m_aiKnowledgePanel, &AiKnowledgePanel::sourceOpenRequested,
            this, &KnowledgePage::messageUrlOpenRequested);
}

void KnowledgePage::setAiKnowledgeServices(const QVector<KnowledgeServiceConfig>& configs,
                                           const QString& preferredServiceId)
{
    QString selectedServiceId = preferredServiceId.trimmed();
    if (selectedServiceId.isEmpty() && m_aiKnowledgePanel)
        selectedServiceId = m_aiKnowledgePanel->selectedServiceId();

    if (selectedServiceId.isEmpty()) {
        for (const KnowledgeServiceConfig& config : configs) {
            if (config.isDefault) { selectedServiceId = config.localId; break; }
        }
    }

    m_aiKnowledgeServiceConfigs = configs;
    if (!m_aiKnowledgePanel) return;

    int selectedServiceIndex = 0;
    m_aiKnowledgePanel->setAvailableServices(
        aiServiceDisplayNames(m_aiKnowledgeServiceConfigs, selectedServiceId, &selectedServiceIndex),
        aiServiceLocalIds(m_aiKnowledgeServiceConfigs),
        selectedServiceIndex);
}

void KnowledgePage::refreshTheme()
{
    setStyleSheet(QStringLiteral("KnowledgePage, QWidget#knowledgePageRoot { background:transparent; }"));
    for (auto* area : findChildren<QAbstractScrollArea*>()) {
        if (!area || !area->viewport()) continue;
        area->setAutoFillBackground(false);
        area->viewport()->setAutoFillBackground(false);
        QPalette palette = area->viewport()->palette();
        palette.setColor(QPalette::Base, Qt::transparent);
        palette.setColor(QPalette::Window, Qt::transparent);
        area->viewport()->setPalette(palette);
    }
    if (m_aiKnowledgePanel) {
        m_aiKnowledgePanel->refreshTheme();
    }
}
