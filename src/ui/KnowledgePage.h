#pragma once
#include "ElaScrollPage.h"
#include <QVector>

#include "integrations/KnowledgeServiceSettings.h"

class AiKnowledgePanel;
class KnowServiceClient;

class KnowledgePage : public ElaScrollPage {
    Q_OBJECT
public:
    explicit KnowledgePage(QWidget* parent = nullptr);

    void setAiKnowledgeServices(const QVector<KnowledgeServiceConfig>& configs,
                                const QString& preferredServiceId = QString());
    AiKnowledgePanel* aiKnowledgePanel() const { return m_aiKnowledgePanel; }
    void refreshTheme();

signals:
    void messageUrlOpenRequested(const QString& url);

private:
    AiKnowledgePanel* m_aiKnowledgePanel = nullptr;
    KnowServiceClient* m_aiKnowServiceClient = nullptr;
    QVector<KnowledgeServiceConfig> m_aiKnowledgeServiceConfigs;
};
