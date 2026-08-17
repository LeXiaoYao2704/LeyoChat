#pragma once

#include "integrations/KnowledgeServiceSettings.h"

#include <QString>
#include <QVector>
#include <QWidget>

class KnowServiceClient;
class ElaCheckBox;
class QFormLayout;
class QFrame;
class ElaText;
class ElaLineEdit;
class QListWidget;
class ElaListWidget;
class ElaPushButton;

class KnowledgeServiceSettingsWidget : public QWidget {
    Q_OBJECT

public:
    explicit KnowledgeServiceSettingsWidget(QWidget* parent = nullptr);

    void setConfigs(const QVector<KnowledgeServiceConfig>& configs);
    QVector<KnowledgeServiceConfig> configs();
    bool validate(QString* errorMessage = nullptr);
    void setConnectionClient(KnowServiceClient* client);
    QString exportToJson();
    bool importFromJson(const QString& json, QString* errorMessage = nullptr);
    bool moveService(int fromIndex, int toIndex);

private:
    void addService();
    void duplicateCurrentService();
    void removeCurrentService();
    void testCurrentConnection();
    void handleCurrentRowChanged(int currentRow);
    void syncCurrentFieldsToModel();
    void loadCurrentModelToFields();
    void refreshServiceList();
    void syncConfigsFromListOrder();
    void setEditorsEnabled(bool enabled);
    void refreshStyles();
    void setStatusText(const QString& message);
    QString nextGeneratedLocalId() const;
    QString nextDuplicateDisplayName(const QString& sourceName) const;
    QString serviceDisplayLabel(const KnowledgeServiceConfig& config, int index) const;
    int indexOfLocalId(const QString& localId) const;
    KnowServiceClient* ensureConnectionClient();

    ElaListWidget* m_serviceList = nullptr;
    ElaPushButton* m_addButton = nullptr;
    ElaPushButton* m_duplicateButton = nullptr;
    ElaPushButton* m_removeButton = nullptr;
    ElaPushButton* m_importButton = nullptr;
    ElaPushButton* m_exportButton = nullptr;
    ElaPushButton* m_testConnectionButton = nullptr;
    ElaLineEdit* m_displayNameEdit = nullptr;
    ElaLineEdit* m_baseUrlEdit = nullptr;
    ElaLineEdit* m_teamLabelEdit = nullptr;
    ElaLineEdit* m_accessTokenEdit = nullptr;
    ElaLineEdit* m_serviceInstanceIdEdit = nullptr;
    ElaLineEdit* m_knowledgeBaseIdEdit = nullptr;
    ElaLineEdit* m_apiVersionEdit = nullptr;
    ElaLineEdit* m_capabilitiesEdit = nullptr;
    ElaCheckBox* m_defaultCheck = nullptr;
    ElaText* m_statusLabel = nullptr;
    QFrame* m_listCard = nullptr;
    QFrame* m_editorCard = nullptr;
    ElaText* m_listTitle = nullptr;
    ElaText* m_listHint = nullptr;
    ElaText* m_editorTitle = nullptr;
    ElaText* m_editorHint = nullptr;
    QFormLayout* m_formLayout = nullptr;
    QVector<KnowledgeServiceConfig> m_configs;
    KnowServiceClient* m_connectionClient = nullptr;
    bool m_ownsConnectionClient = false;
    int m_currentRow = -1;
    bool m_updatingUi = false;
    bool m_reorderingFromUi = false;
};
