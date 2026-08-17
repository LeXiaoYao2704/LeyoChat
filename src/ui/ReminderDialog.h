#pragma once

#include <ElaDialog.h>

#include <QDateTime>

class ElaComboBox;
class ElaLineEdit;
class ElaPushButton;
class ElaText;
class QDateTimeEdit;

class ReminderDialog : public ElaDialog {
    Q_OBJECT

public:
    explicit ReminderDialog(QWidget* parent = nullptr);

    void setContextPreview(const QString& title, const QString& preview);
    QDateTime selectedDueTime() const;
    QString note() const;

    bool okEnabledForTesting() const;
    void setCustomDueTimeForTesting(const QDateTime& due);
    void selectQuickOptionForTesting(int index);

private:
    void refreshDueTimeState();
    bool customTimeSelected() const;

    ElaText* m_contextTitle = nullptr;
    ElaText* m_contextPreview = nullptr;
    ElaComboBox* m_quickCombo = nullptr;
    QDateTimeEdit* m_customDueEdit = nullptr;
    ElaLineEdit* m_noteEdit = nullptr;
    ElaText* m_validationLabel = nullptr;
    ElaPushButton* m_okButton = nullptr;
};
