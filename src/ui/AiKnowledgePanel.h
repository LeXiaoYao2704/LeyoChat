#pragma once

#include "integrations/KnowServiceContracts.h"

#include <QStringList>
#include <QWidget>

class ElaComboBox;
class ElaText;
class QFrame;
class ElaFrame;
class QTextBrowser;
class ElaPushButton;
class ElaLineEdit;
class QVBoxLayout;

class AiKnowledgePanel : public QWidget {
    Q_OBJECT

public:
    explicit AiKnowledgePanel(QWidget* parent = nullptr);

    void setAvailableServices(const QStringList& serviceNames, const QStringList& serviceIds, int selectedIndex = 0);
    QString selectedServiceName() const;
    QString selectedServiceId() const;
    void setQueryPending(bool pending);
    void showQueryResponse(const KnowServiceQueryResponse& response);
    void showQueryError(const QString& message);
    void refreshTheme();

signals:
    void serviceChanged(const QString& serviceName);
    void querySubmitted(const QString& queryText);
    void sourceOpenRequested(const QString& url);

private:
    void switchToSearchMode();
    void switchToResultMode();
    void clearSourcesGrid();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

    // 居中搜索态
    QWidget* m_heroContainer = nullptr;
    ElaText* m_heroTitle = nullptr;
    ElaText* m_heroSubtitle = nullptr;
    ElaComboBox* m_serviceCombo = nullptr;
    ElaFrame* m_searchBarFrame = nullptr;
    ElaLineEdit* m_queryEdit = nullptr;
    ElaPushButton* m_submitButton = nullptr;

    // 结果态（顶部搜索栏 + 结果区）
    QWidget* m_resultContainer = nullptr;
    QWidget* m_resultToolbar = nullptr;
    ElaFrame* m_topSearchBarFrame = nullptr;
    ElaComboBox* m_topServiceCombo = nullptr;
    ElaLineEdit* m_topQueryEdit = nullptr;
    ElaPushButton* m_topSubmitButton = nullptr;
    ElaFrame* m_answerCard = nullptr;
    QTextBrowser* m_answerBody = nullptr;
    ElaFrame* m_sourcesCard = nullptr;
    ElaText* m_sourcesHeader = nullptr;
    QWidget* m_sourcesGrid = nullptr;
    bool m_sourcesExpanded = false;

    QString m_pendingQueryServiceId;
};
