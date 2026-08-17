#pragma once

#include <ElaDialog.h>

#include "integrations/AzureDevOpsLinkParser.h"
#include "integrations/AzureDevOpsSettings.h"

class ElaComboBox;
class ElaText;
class ElaLineEdit;
class ElaPushButton;

class AzureDevOpsInsertDialog : public ElaDialog {
    Q_OBJECT

public:
    explicit AzureDevOpsInsertDialog(const AzureDevOpsConnectionSettings& settings = {},
                                     QWidget* parent = nullptr);

    QString link() const;
    void setLinkText(const QString& link);
    std::optional<AzureDevOpsResourceLocator> parsedLocator() const;

private:
    void refreshPreview();
    std::optional<AzureDevOpsResourceLocator> buildManualLocator() const;
    void updateManualFieldVisibility();

    AzureDevOpsConnectionSettings m_settings;
    ElaLineEdit* m_linkEdit = nullptr;
    ElaComboBox* m_typeCombo = nullptr;
    ElaLineEdit* m_repositoryEdit = nullptr;
    ElaLineEdit* m_resourceIdEdit = nullptr;
    ElaText* m_hintLabel = nullptr;
    ElaText* m_previewKindValue = nullptr;
    ElaText* m_previewProjectValue = nullptr;
    ElaText* m_previewIdValue = nullptr;
    ElaPushButton* m_insertButton = nullptr;
    std::optional<AzureDevOpsResourceLocator> m_parsedLocator;
};
