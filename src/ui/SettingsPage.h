#pragma once

#include "ClientPreferences.h"
#include "domain/Profile.h"
#include "integrations/OutlookSettings.h"
#include "integrations/KnowledgeServiceSettings.h"
#include "StorageCleanupDialog.h"
#include "ElaScrollPage.h"

#include <QPixmap>

class ElaCheckBox;
class ElaComboBox;
class ElaLineEdit;
class ElaListView;
class ElaPlainTextEdit;
class ElaPushButton;
class ElaScrollArea;
class ElaSpinBox;
class ElaText;
class ElaToggleSwitch;
class KnowledgeServiceSettingsWidget;
class QKeySequenceEdit;
class QLabel;
class QEvent;

class SettingsPage : public ElaScrollPage
{
    Q_OBJECT

public:
    explicit SettingsPage(const Profile& profile,
                          const ClientPreferences& preferences,
                          const QString& dataRoot,
                          bool setupMode,
                          QWidget* parent = nullptr);

    const Profile& profile() const;
    void setProfile(const Profile& profile);
    const ClientPreferences& preferences() const;
    void setClientPreferences(const ClientPreferences& preferences);
    void setSetupMode(bool setupMode);

    // 集成服务 API（供 LeyoApplication 连接）
    OutlookConnectionSettings collectOutlookSettings() const;
    KnowledgeServiceSettingsWidget* knowledgeServiceWidget() const { return knowledgeServiceWidget_; }

    // 供外部填充 Outlook 认证后结果
    void setOutlookAuthResult(const QString& email, const QString& displayName);
    void setOutlookTestStatus(const QString& text);
    // 设置头像图片（从 LeyoApplication 传入）
    void setAvatarPixmap(const QPixmap& pixmap);
    // 供外部更新"检查更新"状态
    void setCheckUpdateStatus(const QString& text, const QString& color);
    void setCheckUpdateButtonEnabled(bool enabled);

    // 存储管理
    void setCleanupStatus(const QString& text);

Q_SIGNALS:
    void settingsSaved(const Profile& profile, const ClientPreferences& preferences, bool wasInitialSetup);

    // 集成服务信号
    void outlookTestConnectionRequested();

    // 高级设置信号
    void dataExportRequested();
    void dataImportRequested();
    void checkUpdateRequested();

    // 存储管理信号
    void storageCategoryManageRequested(StorageCategory category, int ageIndex);

    // 关于信号
    void showAboutDialogRequested();
    void showReleaseNotesRequested();
    void showRuntimeArchitectureRequested();
    void exportDiagnosticsRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    void applyThemeStyles();
    void refreshProfilePreview();
    void refreshPreferenceControls();
    void refreshSetupMode();
    void previewSystemPreferences();
    void persistSystemPreferences();
    Profile collectProfile() const;
    ClientPreferences collectPreferences() const;

    QString dataRoot_;
    Profile profile_;
    ClientPreferences preferences_;
    bool setupMode_{false};
    bool updatingControls_{false};
    bool applyingThemeStyles_{false};

    // Hero 卡片
    QLabel* avatarPreviewLabel_{nullptr};
    QPixmap avatarPixmap_;
    ElaText* accountSummaryLabel_{nullptr};
    ElaText* saveStatusLabel_{nullptr};

    // 用户信息
    ElaLineEdit* displayNameEdit_{nullptr};
    ElaLineEdit* titleEdit_{nullptr};
    ElaLineEdit* departmentEdit_{nullptr};
    ElaLineEdit* emailEdit_{nullptr};
    ElaLineEdit* phoneEdit_{nullptr};
    ElaPlainTextEdit* signatureEdit_{nullptr};

    // 系统设置
    ElaComboBox* themeModeCombo_{nullptr};
    ElaComboBox* navigationModeCombo_{nullptr};
    ElaComboBox* stackSwitchModeCombo_{nullptr};
    ElaComboBox* windowPaintModeCombo_{nullptr};
    ElaComboBox* windowDisplayModeCombo_{nullptr};
    ElaToggleSwitch* userCardSwitch_{nullptr};

    // 集成服务 - Outlook
    ElaCheckBox* outlookEnabledCheck_{nullptr};
    ElaLineEdit* outlookServerUrlEdit_{nullptr};
    ElaLineEdit* outlookUsernameEdit_{nullptr};
    ElaLineEdit* outlookPasswordEdit_{nullptr};
    ElaLineEdit* outlookEmailEdit_{nullptr};
    ElaLineEdit* outlookDisplayNameEdit_{nullptr};
    ElaCheckBox* outlookNotifyEnabledCheck_{nullptr};
    ElaSpinBox* outlookPollIntervalSpin_{nullptr};
    ElaText* outlookAuthStatusLabel_{nullptr};

    // 知识服务
    KnowledgeServiceSettingsWidget* knowledgeServiceWidget_{nullptr};

    // 高级设置
    ElaToggleSwitch* trayPopupSwitch_{nullptr};
    QKeySequenceEdit* hotkeyEdit_{nullptr};
    ElaText* hotkeyTestResult_{nullptr};
    ElaLineEdit* updateServerEdit_{nullptr};
    ElaLineEdit* messageServerEdit_{nullptr};
    ElaLineEdit* messageServerTokenEdit_{nullptr};
    ElaLineEdit* messageServerWorkspaceEdit_{nullptr};
    ElaCheckBox* autoCheckUpdateBox_{nullptr};
    ElaSpinBox* checkIntervalSpin_{nullptr};

    // 存储管理
    ElaLineEdit* incomingFilesPathEdit_{nullptr};
    ElaComboBox* cleanupAgeSpin_{nullptr};
    ElaText* cleanupStatusLabel_{nullptr};

    // 关于
    ElaText* versionLabel_{nullptr};
    ElaPushButton* checkUpdateButton_{nullptr};
    ElaText* checkUpdateStatusLabel_{nullptr};

    ElaPushButton* saveButton_{nullptr};

    // 旧集成设置快照（用于保存时合并轮询状态）
    OutlookConnectionSettings initialOutlookSettings_;

    // 左侧导航
    ElaListView* navList_{nullptr};
    ElaScrollArea* contentScrollArea_{nullptr};
    QVector<QWidget*> sectionAnchors_;
};
