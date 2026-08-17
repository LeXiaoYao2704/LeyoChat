#include "ui/AzureDevOpsNotificationDialog.h"

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

QString normalizedBaseUrl(const AzureDevOpsConnectionSettings& settings)
{
    QString value = settings.baseUrl.trimmed();
    if (value.isEmpty()) {
        value = QStringLiteral("https://dev.azure.com");
    }
    while (value.endsWith('/')) {
        value.chop(1);
    }
    return value;
}

QString kindDefaultTitle(AzureDevOpsNotificationKind kind, const QString& project)
{
    switch (kind) {
    case AzureDevOpsNotificationKind::WorkItemUpdated:
        return QStringLiteral("%1 工作项通知").arg(project);
    case AzureDevOpsNotificationKind::PullRequestUpdated:
        return QStringLiteral("%1 PR 通知").arg(project);
    case AzureDevOpsNotificationKind::BuildCompleted:
    default:
        return QStringLiteral("%1 构建通知").arg(project);
    }
}

}  // namespace

AzureDevOpsNotificationDialog::AzureDevOpsNotificationDialog(
    const AzureDevOpsConnectionSettings& settings,
    QWidget* parent)
    : ElaDialog(parent)
    , m_settings(settings)
{
    LeyoDialog::applySecondaryDialogScaffold(this, QStringLiteral("azureDevOpsNotificationDialog"));
    setWindowTitle(QStringLiteral("发送 Azure DevOps 通知"));
    resize(520, 520);
    setModal(false);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);

    auto* card = new ElaFrame(this);
    card->setObjectName(QStringLiteral("azureDevOpsNotificationCard"));
    card->setStyleSheet(QStringLiteral("QFrame { background: %1; border-radius: 14px; }").arg(AppStyle::surface()));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);

    auto* title = new ElaText(QStringLiteral("发送 Azure DevOps 通知卡片"), card);
    title->setStyleSheet(QStringLiteral("font-size:20px; font-weight:700; color:%1;")
                             .arg(AppStyle::textPrimary()));

    auto* subtitle = new ElaText(
        QStringLiteral("这张卡片会按系统通知样式插入当前会话，后续接 webhook 时会复用同一套事件结构。"),
        card);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(
        QStringLiteral("font-size:13px; color:%1;").arg(AppStyle::textMuted()));

    auto* contentSection = LeyoDialog::createSectionFrame(card, QStringLiteral("azureDevOpsNotificationContentSection"));
    auto* contentSectionLayout = new QVBoxLayout(contentSection);
    contentSectionLayout->setContentsMargins(0, 0, 0, 0);
    contentSectionLayout->setSpacing(0);
    auto* contentArea = new QWidget(contentSection);
    contentArea->setObjectName(QStringLiteral("azureDevOpsNotificationContentArea"));
    auto* contentAreaLayout = new QVBoxLayout(contentArea);
    contentAreaLayout->setContentsMargins(0, 0, 0, 0);
    contentAreaLayout->setSpacing(16);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    m_kindCombo = new ElaComboBox(card);
    m_kindCombo->setObjectName(QStringLiteral("azureDevOpsNotificationKindCombo"));
    m_kindCombo->addItem(QStringLiteral("工作项更新"),
                         static_cast<int>(AzureDevOpsNotificationKind::WorkItemUpdated));
    m_kindCombo->addItem(QStringLiteral("PR 更新"),
                         static_cast<int>(AzureDevOpsNotificationKind::PullRequestUpdated));
    m_kindCombo->addItem(QStringLiteral("构建完成"),
                         static_cast<int>(AzureDevOpsNotificationKind::BuildCompleted));

    m_resourceIdEdit = new ElaLineEdit(card);
    m_resourceIdEdit->setObjectName(QStringLiteral("azureDevOpsNotificationResourceIdEdit"));
    m_resourceIdEdit->setPlaceholderText(QStringLiteral("例如 123、109、88"));

    m_titleEdit = new ElaLineEdit(card);
    m_titleEdit->setObjectName(QStringLiteral("azureDevOpsNotificationTitleEdit"));

    m_summaryEdit = new ElaTextEdit(card);
    m_summaryEdit->setObjectName(QStringLiteral("azureDevOpsNotificationSummaryEdit"));
    m_summaryEdit->setFixedHeight(90);

    m_statusEdit = new ElaLineEdit(card);
    m_statusEdit->setObjectName(QStringLiteral("azureDevOpsNotificationStatusEdit"));
    m_statusEdit->setPlaceholderText(QStringLiteral("例如 active、succeeded、failed"));

    m_actorEdit = new ElaLineEdit(card);
    m_actorEdit->setObjectName(QStringLiteral("azureDevOpsNotificationActorEdit"));
    m_actorEdit->setPlaceholderText(QStringLiteral("例如 CI Bot、张小乐"));

    m_urlEdit = new ElaLineEdit(card);
    m_urlEdit->setObjectName(QStringLiteral("azureDevOpsNotificationUrlEdit"));

    form->addRow(QStringLiteral("类型"), m_kindCombo);
    form->addRow(QStringLiteral("资源标识"), m_resourceIdEdit);
    form->addRow(QStringLiteral("标题"), m_titleEdit);
    form->addRow(QStringLiteral("摘要"), m_summaryEdit);
    form->addRow(QStringLiteral("状态"), m_statusEdit);
    form->addRow(QStringLiteral("触发人"), m_actorEdit);
    form->addRow(QStringLiteral("链接"), m_urlEdit);

    auto* context = new ElaText(
        QStringLiteral("组织：%1   项目：%2")
            .arg(settings.organization.trimmed().isEmpty() ? QStringLiteral("--")
                                                           : settings.organization.trimmed(),
                 settings.project.trimmed().isEmpty() ? QStringLiteral("--")
                                                      : settings.project.trimmed()),
        card);
    context->setStyleSheet(
        QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));

    auto* cancelButton = new ElaPushButton(QStringLiteral("取消"), card);
    cancelButton->setFlat(true);

    m_sendButton = new ElaPushButton(QStringLiteral("发送通知"), card);
    m_sendButton->setObjectName(QStringLiteral("azureDevOpsNotificationSendButton"));
    m_sendButton->setEnabled(false);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(cancelButton);
    buttonRow->addStretch();
    buttonRow->addWidget(m_sendButton);

    auto* actionSection = LeyoDialog::createSectionFrame(card, QStringLiteral("azureDevOpsNotificationActionSection"));
    auto* actionSectionLayout = new QVBoxLayout(actionSection);
    actionSectionLayout->setContentsMargins(0, 0, 0, 0);
    actionSectionLayout->setSpacing(0);
    auto* actionArea = new QWidget(actionSection);
    actionArea->setObjectName(QStringLiteral("azureDevOpsNotificationButtonArea"));
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
    m_statusEdit->setStyleSheet(inputStyle);
    m_actorEdit->setStyleSheet(inputStyle);
    m_urlEdit->setStyleSheet(inputStyle);

    cancelButton->setStyleSheet(QStringLiteral(
        "QPushButton { color:%1; border:none; background:transparent; font-size:13px; }"
        "QPushButton:hover { color:%2; }")
                                    .arg(AppStyle::textMuted(), AppStyle::accent()));

    const QString project = settings.project.trimmed().isEmpty()
        ? QStringLiteral("当前项目")
        : settings.project.trimmed();
    m_titleEdit->setText(kindDefaultTitle(AzureDevOpsNotificationKind::WorkItemUpdated, project));
    m_summaryEdit->setPlainText(QStringLiteral("这是一条用于验证通知卡片链路的 Azure DevOps 系统消息。"));
    m_statusEdit->setText(QStringLiteral("active"));
    m_actorEdit->setText(QStringLiteral("LeyoChat"));

    connect(cancelButton, &QAbstractButton::clicked, this, &QDialog::reject);
    connect(m_sendButton, &QAbstractButton::clicked, this, &QDialog::accept);
    connect(m_kindCombo, &QComboBox::currentIndexChanged, this, [this, project]() {
        m_titleEdit->setText(kindDefaultTitle(
            static_cast<AzureDevOpsNotificationKind>(m_kindCombo->currentData().toInt()), project));
        refreshPreview();
    });
    connect(m_resourceIdEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_titleEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_summaryEdit, &QTextEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_statusEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_actorEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_urlEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });

    refreshPreview();
}

AzureDevOpsNotificationEvent AzureDevOpsNotificationDialog::event() const
{
    AzureDevOpsNotificationEvent event;
    event.kind = static_cast<AzureDevOpsNotificationKind>(m_kindCombo->currentData().toInt());
    event.serviceId = QStringLiteral("local-azure-devops");
    event.workspaceId = QStringLiteral("local-devops");
    event.resourceId = m_resourceIdEdit->text().trimmed();
    event.title = m_titleEdit->text().trimmed();
    event.summary = m_summaryEdit->toPlainText().trimmed();
    event.status = m_statusEdit->text().trimmed();
    event.webUrl = m_urlEdit->text().trimmed();
    event.actor = m_actorEdit->text().trimmed();
    return event;
}

void AzureDevOpsNotificationDialog::refreshPreview()
{
    if (m_urlEdit->text().trimmed().isEmpty() && !m_resourceIdEdit->text().trimmed().isEmpty()) {
        m_urlEdit->setText(kindUrl());
    }

    const bool ready = !m_resourceIdEdit->text().trimmed().isEmpty()
        && !m_titleEdit->text().trimmed().isEmpty()
        && !m_summaryEdit->toPlainText().trimmed().isEmpty();
    m_sendButton->setEnabled(ready);
}

QString AzureDevOpsNotificationDialog::kindUrl() const
{
    const QString org = m_settings.organization.trimmed();
    const QString project = m_settings.project.trimmed();
    const QString id = m_resourceIdEdit->text().trimmed();
    if (org.isEmpty() || project.isEmpty() || id.isEmpty()) {
        return {};
    }

    const QString root = normalizedBaseUrl(m_settings);
    const auto kind = static_cast<AzureDevOpsNotificationKind>(m_kindCombo->currentData().toInt());
    switch (kind) {
    case AzureDevOpsNotificationKind::WorkItemUpdated:
        return QStringLiteral("%1/%2/%3/_workitems/edit/%4").arg(root, org, project, id);
    case AzureDevOpsNotificationKind::PullRequestUpdated:
        // Note: 仓库名与项目名相同时有效；如需支持不同仓库名，请在 m_settings 中添加 repository 字段
        return QStringLiteral("%1/%2/%3/_git/%4/pullrequest/%5").arg(root, org, project, project, id);
    case AzureDevOpsNotificationKind::BuildCompleted:
    default:
        return QStringLiteral("%1/%2/%3/_build/results?buildId=%4").arg(root, org, project, id);
    }
}

void AzureDevOpsNotificationDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    QDialog::keyPressEvent(event);
}
