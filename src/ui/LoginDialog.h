#pragma once

#include <ElaDialog.h>

class ElaLineEdit;
class ElaPushButton;
class ElaText;

class LoginDialog : public ElaDialog {
    Q_OBJECT

public:
    explicit LoginDialog(QWidget* parent = nullptr);

    QString displayName() const;
    QString employeeCode() const;

    bool hasStateForTesting(const QString& state) const;
    QString currentStateForTesting() const;
    void setStateForTesting(const QString& state);

private:
    void applyState(const QString& state);

    QString m_currentState;
    ElaLineEdit* m_displayNameEdit;
    ElaLineEdit* m_employeeCodeEdit;
    ElaPushButton* m_okButton;
    ElaText* m_stateHintLabel;
};
