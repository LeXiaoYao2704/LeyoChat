#include "ui/ReminderDialog.h"

#include "services/ReminderTimeOptions.h"
#include "ui/AppStyle.h"
#include "ui/LeyoDialog.h"

#include <ElaComboBox.h>
#include <ElaFrame.h>
#include <ElaLineEdit.h>
#include <ElaPushButton.h>
#include <ElaText.h>
#include <QAbstractButton>
#include <QDateTimeEdit>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

constexpr int kQuickThirtyMinutes = 0;
constexpr int kQuickOneHour = 1;
constexpr int kQuickTomorrowNine = 2;
constexpr int kQuickCustom = 3;

QString dateTimeEditStyle()
{
    return QStringLiteral(
               "QDateTimeEdit {"
               "  background:%1;"
               "  border:1.5px solid %2;"
               "  border-radius:10px;"
               "  padding:0 12px;"
               "  font-size:13px;"
               "  color:%3;"
               "}"
               "QDateTimeEdit:focus {"
               "  border:1.5px solid %4;"
               "  background:%5;"
               "}")
        .arg(AppStyle::surfaceAlt(),
             AppStyle::border(),
             AppStyle::textPrimary(),
             AppStyle::accent(),
             AppStyle::surface());
}

}  // namespace

ReminderDialog::ReminderDialog(QWidget* parent)
    : ElaDialog(parent)
{
    LeyoDialog::applySecondaryDialogScaffold(this, QStringLiteral("reminderDialog"));
    setWindowTitle(QStringLiteral("设置提醒"));
    setFixedWidth(480);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(0);

    auto* card = new ElaFrame(this);
    card->setObjectName(QStringLiteral("reminderDialogCard"));
    card->setStyleSheet(
        QStringLiteral(
            "QFrame#reminderDialogCard {"
            "  background:%1;"
            "  border:1px solid %2;"
            "  border-radius:12px;"
            "}")
            .arg(AppStyle::surface(), AppStyle::border()));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 24, 28, 24);
    cardLayout->setSpacing(0);

    auto* titleLabel = new ElaText(QStringLiteral("稍后提醒"), card);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        QStringLiteral("font-size:20px; font-weight:bold; color:%1;")
            .arg(AppStyle::textPrimary()));

    auto* subtitleLabel = new ElaText(QStringLiteral("为这条内容创建一个只在本机生效的提醒。"), card);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setStyleSheet(
        QStringLiteral("font-size:13px; color:%1;").arg(AppStyle::textMuted()));

    auto* contextCard = new ElaFrame(card);
    contextCard->setObjectName(QStringLiteral("reminderContextCard"));
    contextCard->setStyleSheet(
        QStringLiteral(
            "QFrame#reminderContextCard {"
            "  background:%1;"
            "  border:1px solid %2;"
            "  border-radius:10px;"
            "}")
            .arg(AppStyle::surfaceAlt(), AppStyle::border()));
    auto* contextLayout = new QVBoxLayout(contextCard);
    contextLayout->setContentsMargins(14, 12, 14, 12);
    contextLayout->setSpacing(6);

    m_contextTitle = new ElaText(QStringLiteral("当前内容"), contextCard);
    m_contextTitle->setObjectName(QStringLiteral("reminderContextTitle"));
    m_contextTitle->setStyleSheet(
        QStringLiteral("font-size:13px; font-weight:700; color:%1;")
            .arg(AppStyle::textPrimary()));
    m_contextPreview = new ElaText(QStringLiteral("将在指定时间提醒你回来处理。"), contextCard);
    m_contextPreview->setObjectName(QStringLiteral("reminderContextPreview"));
    m_contextPreview->setWordWrap(true);
    m_contextPreview->setStyleSheet(
        QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));
    contextLayout->addWidget(m_contextTitle);
    contextLayout->addWidget(m_contextPreview);

    auto* formSection = LeyoDialog::createSectionFrame(card, QStringLiteral("reminderFormSection"));
    auto* formLayout = new QVBoxLayout(formSection);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(10);

    auto* quickLabel = new ElaText(QStringLiteral("提醒时间"), formSection);
    quickLabel->setStyleSheet(
        QStringLiteral("font-size:12px; font-weight:700; color:%1;")
            .arg(AppStyle::textPrimary()));
    m_quickCombo = new ElaComboBox(formSection);
    m_quickCombo->setObjectName(QStringLiteral("reminderQuickCombo"));
    m_quickCombo->addItem(QStringLiteral("30 分钟后"), kQuickThirtyMinutes);
    m_quickCombo->addItem(QStringLiteral("1 小时后"), kQuickOneHour);
    m_quickCombo->addItem(QStringLiteral("明天 9:00"), kQuickTomorrowNine);
    m_quickCombo->addItem(QStringLiteral("自定义时间"), kQuickCustom);
    m_quickCombo->setCurrentIndex(kQuickOneHour);
    m_quickCombo->setFixedHeight(38);

    m_customDueEdit = new QDateTimeEdit(formSection);
    m_customDueEdit->setObjectName(QStringLiteral("reminderCustomDueEdit"));
    m_customDueEdit->setCalendarPopup(true);
    m_customDueEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_customDueEdit->setDateTime(QDateTime::currentDateTime().addSecs(60 * 60));
    m_customDueEdit->setFixedHeight(38);
    // Ela does not provide a date-time editor; keep the native editor inside the Ela-styled shell.
    m_customDueEdit->setStyleSheet(dateTimeEditStyle());

    auto* noteLabel = new ElaText(QStringLiteral("备注"), formSection);
    noteLabel->setStyleSheet(
        QStringLiteral("font-size:12px; font-weight:700; color:%1;")
            .arg(AppStyle::textPrimary()));
    m_noteEdit = new ElaLineEdit(formSection);
    m_noteEdit->setObjectName(QStringLiteral("reminderNoteEdit"));
    m_noteEdit->setPlaceholderText(QStringLiteral("可选，例如：确认后续方案"));
    m_noteEdit->setFixedHeight(38);

    m_validationLabel = new ElaText(formSection);
    m_validationLabel->setObjectName(QStringLiteral("reminderValidationLabel"));
    m_validationLabel->setWordWrap(true);
    m_validationLabel->setStyleSheet(
        QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::danger()));

    formLayout->addWidget(quickLabel);
    formLayout->addWidget(m_quickCombo);
    formLayout->addWidget(m_customDueEdit);
    formLayout->addSpacing(4);
    formLayout->addWidget(noteLabel);
    formLayout->addWidget(m_noteEdit);
    formLayout->addWidget(m_validationLabel);

    auto* cancelButton = new ElaPushButton(QStringLiteral("取消"), card);
    cancelButton->setFlat(true);
    cancelButton->setCursor(Qt::PointingHandCursor);

    m_okButton = new ElaPushButton(QStringLiteral("创建提醒"), card);
    m_okButton->setObjectName(QStringLiteral("reminderCreateButton"));
    m_okButton->setFixedHeight(40);
    m_okButton->setCursor(Qt::PointingHandCursor);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(8);
    buttonRow->addWidget(cancelButton);
    buttonRow->addStretch();
    buttonRow->addWidget(m_okButton);

    cardLayout->addWidget(titleLabel);
    cardLayout->addSpacing(4);
    cardLayout->addWidget(subtitleLabel);
    cardLayout->addSpacing(18);
    cardLayout->addWidget(contextCard);
    cardLayout->addSpacing(18);
    cardLayout->addWidget(formSection);
    cardLayout->addSpacing(14);
    cardLayout->addLayout(buttonRow);

    outer->addWidget(card);

    connect(cancelButton, &QAbstractButton::clicked, this, &QDialog::reject);
    connect(m_okButton, &QAbstractButton::clicked, this, &QDialog::accept);
    connect(m_quickCombo, &QComboBox::currentIndexChanged, this, [this]() {
        refreshDueTimeState();
    });
    connect(m_customDueEdit, &QDateTimeEdit::dateTimeChanged, this, [this]() {
        refreshDueTimeState();
    });

    refreshDueTimeState();
}

void ReminderDialog::setContextPreview(const QString& title, const QString& preview)
{
    if (m_contextTitle) {
        m_contextTitle->setText(title.trimmed().isEmpty() ? QStringLiteral("当前内容")
                                                          : title.trimmed());
    }
    if (m_contextPreview) {
        m_contextPreview->setText(preview.trimmed().isEmpty()
                                      ? QStringLiteral("将在指定时间提醒你回来处理。")
                                      : preview.trimmed());
    }
}

QDateTime ReminderDialog::selectedDueTime() const
{
    const QDateTime now = QDateTime::currentDateTime();
    if (!m_quickCombo) {
        return ReminderTimeOptions::oneHourLater(now);
    }

    switch (m_quickCombo->currentData().toInt()) {
    case kQuickThirtyMinutes:
        return ReminderTimeOptions::thirtyMinutesLater(now);
    case kQuickTomorrowNine:
        return ReminderTimeOptions::tomorrowAtNine(now);
    case kQuickCustom:
        return m_customDueEdit ? m_customDueEdit->dateTime() : QDateTime();
    case kQuickOneHour:
    default:
        return ReminderTimeOptions::oneHourLater(now);
    }
}

QString ReminderDialog::note() const
{
    return m_noteEdit ? m_noteEdit->text().trimmed() : QString();
}

bool ReminderDialog::okEnabledForTesting() const
{
    return m_okButton && m_okButton->isEnabled();
}

void ReminderDialog::setCustomDueTimeForTesting(const QDateTime& due)
{
    if (m_quickCombo) {
        m_quickCombo->setCurrentIndex(kQuickCustom);
    }
    if (m_customDueEdit) {
        m_customDueEdit->setDateTime(due);
    }
    refreshDueTimeState();
}

void ReminderDialog::selectQuickOptionForTesting(int index)
{
    if (!m_quickCombo) {
        return;
    }
    m_quickCombo->setCurrentIndex(index);
    refreshDueTimeState();
}

void ReminderDialog::refreshDueTimeState()
{
    const bool custom = customTimeSelected();
    if (m_customDueEdit) {
        m_customDueEdit->setVisible(custom);
    }

    const bool valid =
        ReminderTimeOptions::isValidDueTime(selectedDueTime(), QDateTime::currentDateTime());
    if (m_okButton) {
        m_okButton->setEnabled(valid);
    }
    if (m_validationLabel) {
        m_validationLabel->setText(valid ? QString() : QStringLiteral("请选择一个未来时间。"));
        m_validationLabel->setVisible(!valid);
    }
}

bool ReminderDialog::customTimeSelected() const
{
    return m_quickCombo && m_quickCombo->currentData().toInt() == kQuickCustom;
}
