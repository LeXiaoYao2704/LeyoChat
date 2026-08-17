#pragma once

#include <ElaDialog.h>

#include "integrations/AzureDevOpsNotificationContracts.h"
#include "integrations/AzureDevOpsSettings.h"

class ElaComboBox;
class QKeyEvent;
class ElaLineEdit;
class ElaPushButton;
class QTextEdit;
class ElaTextEdit;

class AzureDevOpsNotificationDialog : public ElaDialog {
    Q_OBJECT

public:
    explicit AzureDevOpsNotificationDialog(const AzureDevOpsConnectionSettings& settings = {},
                                           QWidget* parent = nullptr);

    AzureDevOpsNotificationEvent event() const;

private:
    void refreshPreview();
    QString kindUrl() const;
    void keyPressEvent(QKeyEvent* event) override;

    AzureDevOpsConnectionSettings m_settings;
    ElaComboBox* m_kindCombo = nullptr;
    ElaLineEdit* m_resourceIdEdit = nullptr;
    ElaLineEdit* m_titleEdit = nullptr;
    ElaTextEdit* m_summaryEdit = nullptr;
    ElaLineEdit* m_statusEdit = nullptr;
    ElaLineEdit* m_actorEdit = nullptr;
    ElaLineEdit* m_urlEdit = nullptr;
    ElaPushButton* m_sendButton = nullptr;
};
