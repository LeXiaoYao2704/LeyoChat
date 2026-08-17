#include "ui/OutlookNotificationDialog.h"

#include "ui/AppStyle.h"
#include "ui/LeyoDialog.h"

#include <ElaComboBox.h>
#include <QFrame>
#include <ElaFrame.h>
#include <QFormLayout>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <ElaText.h>
#include <ElaLineEdit.h>
#include <ElaPushButton.h>
#include <QTextEdit>
#include <ElaTextEdit.h>
#include <QVBoxLayout>

namespace {

QString defaultTitle(OutlookNotificationKind kind, const QString& mailbox)
{
    const QString safeMailbox = mailbox.trimmed().isEmpty()
        ? QStringLiteral("Outlook")
        : mailbox.trimmed();
    switch (kind) {
    case OutlookNotificationKind::CalendarReminder:
        return QStringLiteral("%1 会议提醒").arg(safeMailbox);
    case OutlookNotificationKind::MailReceived:
    default:
        return QStringLiteral("%1 邮件提醒").arg(safeMailbox);
    }
}

}  // namespace

OutlookNotificationDialog::OutlookNotificationDialog(const OutlookConnectionSettings& settings,
                                                     QWidget* parent)
    : ElaDialog(parent)
    , m_settings(settings)
{
    LeyoDialog::applySecondaryDialogScaffold(this, QStringLiteral("outlookNotificationDialog"));
    setWindowTitle(QStringLiteral("发送 Outlook 通知"));
    resize(520, 500);
    setModal(false);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);

    auto* card = new ElaFrame(this);
    card->setObjectName(QStringLiteral("outlookNotificationCard"));
    card->setStyleSheet(QStringLiteral("QFrame { background: %1; border-radius: 14px; }").arg(AppStyle::surface()));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);

    auto* title = new ElaText(QStringLiteral("发送 Outlook 通知卡片"), card);
    title->setStyleSheet(QStringLiteral("font-size:20px; font-weight:700; color:%1;")
                             .arg(AppStyle::textPrimary()));

    auto* subtitle = new ElaText(
        QStringLiteral("这张卡片会按系统通知样式进入通知中心；后续接真实邮件和会议提醒时会复用同一套事件结构。"),
        card);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(
        QStringLiteral("font-size:13px; color:%1;").arg(AppStyle::textMuted()));

    auto* contentSection = LeyoDialog::createSectionFrame(card, QStringLiteral("outlookNotificationContentSection"));
    auto* contentSectionLayout = new QVBoxLayout(contentSection);
    contentSectionLayout->setContentsMargins(0, 0, 0, 0);
    contentSectionLayout->setSpacing(0);
    auto* contentArea = new QWidget(contentSection);
    contentArea->setObjectName(QStringLiteral("outlookNotificationContentArea"));
    auto* contentAreaLayout = new QVBoxLayout(contentArea);
    contentAreaLayout->setContentsMargins(0, 0, 0, 0);
    contentAreaLayout->setSpacing(16);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    m_kindCombo = new ElaComboBox(card);
    m_kindCombo->setObjectName(QStringLiteral("outlookNotificationKindCombo"));
    m_kindCombo->addItem(QStringLiteral("收到邮件"),
                         static_cast<int>(OutlookNotificationKind::MailReceived));
    m_kindCombo->addItem(QStringLiteral("会议提醒"),
                         static_cast<int>(OutlookNotificationKind::CalendarReminder));

    m_resourceIdEdit = new ElaLineEdit(card);
    m_resourceIdEdit->setObjectName(QStringLiteral("outlookNotificationResourceIdEdit"));
    m_resourceIdEdit->setPlaceholderText(QStringLiteral("例如：mail-42 / event-7"));

    m_titleEdit = new ElaLineEdit(card);
    m_titleEdit->setObjectName(QStringLiteral("outlookNotificationTitleEdit"));

    m_summaryEdit = new ElaTextEdit(card);
    m_summaryEdit->setObjectName(QStringLiteral("outlookNotificationSummaryEdit"));
    m_summaryEdit->setFixedHeight(90);

    m_actorEdit = new ElaLineEdit(card);
    m_actorEdit->setObjectName(QStringLiteral("outlookNotificationActorEdit"));
    m_actorEdit->setPlaceholderText(QStringLiteral("例如：王小明 / Outlook Calendar"));

    m_statusEdit = new ElaLineEdit(card);
    m_statusEdit->setObjectName(QStringLiteral("outlookNotificationStatusEdit"));
    m_statusEdit->setPlaceholderText(QStringLiteral("例如：unread / reminder"));

    m_urlEdit = new ElaLineEdit(card);
    m_urlEdit->setObjectName(QStringLiteral("outlookNotificationUrlEdit"));

    form->addRow(QStringLiteral("类型"), m_kindCombo);
    form->addRow(QStringLiteral("资源标识"), m_resourceIdEdit);
    form->addRow(QStringLiteral("标题"), m_titleEdit);
    form->addRow(QStringLiteral("摘要"), m_summaryEdit);
    form->addRow(QStringLiteral("触发方"), m_actorEdit);
    form->addRow(QStringLiteral("状态"), m_statusEdit);
    form->addRow(QStringLiteral("链接"), m_urlEdit);

    auto* context = new ElaText(
        QStringLiteral("账号：%1")
            .arg(settings.accountEmail.trimmed().isEmpty() ? QStringLiteral("--")
                                                           : settings.accountEmail.trimmed()),
        card);
    context->setStyleSheet(
        QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));

    auto* cancelButton = new ElaPushButton(QStringLiteral("取消"), card);
    cancelButton->setFlat(true);

    m_sendButton = new ElaPushButton(QStringLiteral("发送通知"), card);
    m_sendButton->setObjectName(QStringLiteral("outlookNotificationSendButton"));
    m_sendButton->setEnabled(false);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(cancelButton);
    buttonRow->addStretch();
    buttonRow->addWidget(m_sendButton);

    auto* actionSection = LeyoDialog::createSectionFrame(card, QStringLiteral("outlookNotificationActionSection"));
    auto* actionSectionLayout = new QVBoxLayout(actionSection);
    actionSectionLayout->setContentsMargins(0, 0, 0, 0);
    actionSectionLayout->setSpacing(0);
    auto* actionArea = new QWidget(actionSection);
    actionArea->setObjectName(QStringLiteral("outlookNotificationButtonArea"));
    auto* actionAreaLayout = new QVBoxLayout(actionArea);
    actionAreaLayout->setContentsMargins(0, 0, 0, 0);
    actionAreaLayout->setSpacing(0);
    actionAreaLayout->addLayout(buttonRow);
    actionSectionLayout->addWidget(actionArea);

    layout->addWidget(title);
    layout->addWidget(subtitle);
    contentAreaLayout->addLayout(form);
    contentAreaLayout->addWidget(context);
    contentSectionLayout->addWidget(contentArea);
    layout->addWidget(contentSection);
    layout->addWidget(actionSection);
    outer->addWidget(card);

    const QString inputStyle =
        QStringLiteral(
            "QLineEdit, QComboBox, QTextEdit {"
            "  background:%1;"
            "  border:1px solid %2;"
            "  border-radius:10px;"
            "  color:%3;"
            "  padding:8px 10px;"
            "}"
            "QLineEdit:focus, QComboBox:focus, QTextEdit:focus {"
            "  border:1px solid %4;"
            "  background:%5;"
            "}")
            .arg(AppStyle::surfaceAlt(),
                 AppStyle::border(),
                 AppStyle::textPrimary(),
                 AppStyle::accent(),
                 AppStyle::surface());

    m_kindCombo->setStyleSheet(inputStyle);
    m_resourceIdEdit->setStyleSheet(inputStyle);
    m_titleEdit->setStyleSheet(inputStyle);
    m_summaryEdit->setStyleSheet(inputStyle);
    m_actorEdit->setStyleSheet(inputStyle);
    m_statusEdit->setStyleSheet(inputStyle);
    m_urlEdit->setStyleSheet(inputStyle);

    cancelButton->setStyleSheet(QStringLiteral(
        "QPushButton { color:%1; border:none; background:transparent; font-size:13px; }"
        "QPushButton:hover { color:%2; }")
                                    .arg(AppStyle::textMuted(), AppStyle::accent()));

    const QString mailbox = settings.accountEmail.trimmed();
    m_titleEdit->setText(defaultTitle(OutlookNotificationKind::MailReceived, mailbox));
    m_summaryEdit->setPlainText(QStringLiteral("这是用于验证 Outlook 通知中心链路的一条只读提醒。"));
    m_actorEdit->setText(settings.displayName.trimmed().isEmpty()
                             ? QStringLiteral("Outlook")
                             : settings.displayName.trimmed());
    m_statusEdit->setText(QStringLiteral("unread"));

    connect(cancelButton, &QAbstractButton::clicked, this, &QDialog::reject);
    connect(m_sendButton, &QAbstractButton::clicked, this, &QDialog::accept);
    connect(m_kindCombo, &QComboBox::currentIndexChanged, this, [this, mailbox]() {
        m_titleEdit->setText(defaultTitle(
            static_cast<OutlookNotificationKind>(m_kindCombo->currentData().toInt()), mailbox));
        if (m_kindCombo->currentData().toInt()
            == static_cast<int>(OutlookNotificationKind::CalendarReminder)) {
            m_statusEdit->setText(QStringLiteral("reminder"));
        } else {
            m_statusEdit->setText(QStringLiteral("unread"));
        }
        refreshPreview();
    });
    connect(m_resourceIdEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_titleEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_summaryEdit, &QTextEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_actorEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_statusEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_urlEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });

    refreshPreview();
}

OutlookNotificationEvent OutlookNotificationDialog::event() const
{
    OutlookNotificationEvent event;
    event.kind = static_cast<OutlookNotificationKind>(m_kindCombo->currentData().toInt());
    event.serviceId = QStringLiteral("local-outlook");
    event.workspaceId = QStringLiteral("local-outlook");
    event.resourceId = m_resourceIdEdit->text().trimmed();
    event.title = m_titleEdit->text().trimmed();
    event.summary = m_summaryEdit->toPlainText().trimmed();
    event.status = m_statusEdit->text().trimmed();
    event.webUrl = m_urlEdit->text().trimmed();
    event.actor = m_actorEdit->text().trimmed();
    return event;
}

void OutlookNotificationDialog::refreshPreview()
{
    if (m_urlEdit->text().trimmed().isEmpty() && !m_resourceIdEdit->text().trimmed().isEmpty()) {
        m_urlEdit->setText(defaultUrl());
    }

    const bool ready = !m_resourceIdEdit->text().trimmed().isEmpty()
        && !m_titleEdit->text().trimmed().isEmpty()
        && !m_summaryEdit->toPlainText().trimmed().isEmpty();
    m_sendButton->setEnabled(ready);
}

QString OutlookNotificationDialog::defaultUrl() const
{
    if (m_resourceIdEdit->text().trimmed().isEmpty()) {
        return {};
    }

    if (m_kindCombo->currentData().toInt()
        == static_cast<int>(OutlookNotificationKind::CalendarReminder)) {
        return QStringLiteral("https://outlook.office.com/calendar/");
    }
    return QStringLiteral("https://outlook.office.com/mail/");
}

void OutlookNotificationDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    QDialog::keyPressEvent(event);
}
