#pragma once

#include <ElaDialog.h>

#include "integrations/OutlookNotificationContracts.h"
#include "integrations/OutlookSettings.h"

class ElaComboBox;
class QKeyEvent;
class ElaLineEdit;
class ElaPushButton;
class QTextEdit;
class ElaTextEdit;

class OutlookNotificationDialog : public ElaDialog {
    Q_OBJECT

public:
    explicit OutlookNotificationDialog(const OutlookConnectionSettings& settings = {},
                                       QWidget* parent = nullptr);

    OutlookNotificationEvent event() const;

private:
    void refreshPreview();
    QString defaultUrl() const;
    void keyPressEvent(QKeyEvent* event) override;

    OutlookConnectionSettings m_settings;
    ElaComboBox* m_kindCombo = nullptr;
    ElaLineEdit* m_resourceIdEdit = nullptr;
    ElaLineEdit* m_titleEdit = nullptr;
    ElaTextEdit* m_summaryEdit = nullptr;
    ElaLineEdit* m_actorEdit = nullptr;
    ElaLineEdit* m_statusEdit = nullptr;
    ElaLineEdit* m_urlEdit = nullptr;
    ElaPushButton* m_sendButton = nullptr;
};
