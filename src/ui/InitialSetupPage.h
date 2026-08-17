#pragma once

#include <QPoint>
#include <QWidget>

#include "ClientPreferences.h"
#include "domain/Profile.h"

class ElaComboBox;
class ElaLineEdit;
class ElaPushButton;
class ElaToolButton;
class ElaText;
class QEvent;
class QFrame;
class ElaFrame;
class QStackedWidget;
class ElaStackedWidget;

class InitialSetupPage : public QWidget
{
    Q_OBJECT

public:
    explicit InitialSetupPage(const Profile& profile,
                              const ClientPreferences& preferences,
                              const QString& dataRoot,
                              QWidget* parent = nullptr);

    const Profile& profile() const;
    const ClientPreferences& preferences() const;
    void setProfile(const Profile& profile);
    void setClientPreferences(const ClientPreferences& preferences);

    int currentStep() const;
    int stepCount() const;
    bool hasPreviousStep() const;
    bool hasNextStep() const;

    void goToPreviousStep();
    void goToNextStep();

Q_SIGNALS:
    void setupCompleted(const Profile& profile, const ClientPreferences& preferences);

protected:
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyThemeStyles();
    void refreshStepContent();
    void refreshAvatarPreview();
    void refreshPreferenceControls();
    void refreshWelcomePage();
    void previewPreferences();
    Profile collectProfile() const;
    ClientPreferences collectPreferences() const;

    QString dataRoot_;
    Profile profile_;
    ClientPreferences preferences_;
    int currentStepIndex_{0};
    bool updatingControls_{false};
    bool applyingThemeStyles_{false};
    bool draggingWindow_{false};
    QPoint windowDragOffset_;
    QWidget* headerBar_{nullptr};
    ElaText* stepCounterLabel_{nullptr};
    ElaText* titleLabel_{nullptr};
    ElaText* descriptionLabel_{nullptr};
    ElaText* statusLabel_{nullptr};
    ElaFrame* wizardCard_{nullptr};
    ElaStackedWidget* stepStack_{nullptr};
    ElaToolButton* previousStepButton_{nullptr};
    ElaToolButton* nextStepButton_{nullptr};
    ElaToolButton* closeButton_{nullptr};
    ElaText* avatarPreviewLabel_{nullptr};
    ElaText* avatarMetaLabel_{nullptr};
    ElaPushButton* chooseAvatarButton_{nullptr};
    ElaPushButton* resetAvatarButton_{nullptr};
    ElaLineEdit* displayNameEdit_{nullptr};
    ElaLineEdit* titleEdit_{nullptr};
    ElaLineEdit* departmentEdit_{nullptr};
    ElaComboBox* themeModeCombo_{nullptr};
    ElaComboBox* stackSwitchModeCombo_{nullptr};
    ElaComboBox* windowPaintModeCombo_{nullptr};
    ElaComboBox* windowDisplayModeCombo_{nullptr};
    ElaText* welcomeLogoLabel_{nullptr};
    ElaText* welcomeSummaryLabel_{nullptr};
    ElaPushButton* startButton_{nullptr};
};
