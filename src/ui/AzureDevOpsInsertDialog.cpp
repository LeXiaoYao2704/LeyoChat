#include "ui/AzureDevOpsInsertDialog.h"

#include "ui/AppStyle.h"
#include "ui/LeyoDialog.h"

#include <ElaComboBox.h>
#include <QFrame>
#include <ElaFrame.h>
#include <QHBoxLayout>
#include <ElaText.h>
#include <ElaLineEdit.h>
#include <ElaPushButton.h>
#include <QVBoxLayout>

namespace {

QString previewKindLabel(AzureDevOpsResourceKind kind)
{
    switch (kind) {
    case AzureDevOpsResourceKind::WorkItem:
        return QStringLiteral("工作项");
    case AzureDevOpsResourceKind::PullRequest:
        return QStringLiteral("合并请求");
    case AzureDevOpsResourceKind::Build:
        return QStringLiteral("构建结果");
    case AzureDevOpsResourceKind::Unknown:
    default:
        return QStringLiteral("未识别");
    }
}

}  // namespace

AzureDevOpsInsertDialog::AzureDevOpsInsertDialog(const AzureDevOpsConnectionSettings& settings,
                                                 QWidget* parent)
    : ElaDialog(parent)
    , m_settings(settings)
{
    LeyoDialog::applySecondaryDialogScaffold(this, QStringLiteral("azureDevOpsInsertDialog"));
    setWindowTitle(QStringLiteral("插入 Azure DevOps 卡片"));
    setFixedWidth(500);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(0);

    auto* card = new ElaFrame(this);
    card->setObjectName(QStringLiteral("azureDevOpsInsertDialogCard"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 28, 32, 28);
    cardLayout->setSpacing(0);

    auto* iconLabel = new ElaText(QStringLiteral("<span style='font-size:34px;'>ADO</span>"), card);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet(
        QStringLiteral("color:%1; font-weight:700;").arg(AppStyle::accent()));

    auto* titleLabel = new ElaText(QStringLiteral("插入 Azure DevOps 卡片"), card);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        QStringLiteral("font-size:20px; font-weight:bold; color:%1;")
            .arg(AppStyle::textPrimary()));

    auto* subtitleLabel = new ElaText(
        QStringLiteral("你可以直接粘贴链接，也可以按类型和编号手动插入工作项、PR 或构建卡片。"),
        card);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setStyleSheet(
        QStringLiteral("font-size:13px; color:%1;").arg(AppStyle::textMuted()));

    auto* contentSection = LeyoDialog::createSectionFrame(card, QStringLiteral("azureDevOpsInsertDialogContentSection"));
    auto* contentSectionLayout = new QVBoxLayout(contentSection);
    contentSectionLayout->setContentsMargins(0, 0, 0, 0);
    contentSectionLayout->setSpacing(0);

    auto* contentArea = new QWidget(contentSection);
    contentArea->setObjectName(QStringLiteral("azureDevOpsInsertContentArea"));
    auto* contentAreaLayout = new QVBoxLayout(contentArea);
    contentAreaLayout->setContentsMargins(0, 0, 0, 0);
    contentAreaLayout->setSpacing(18);

    auto* inputCard = new ElaFrame(contentArea);
    inputCard->setObjectName(QStringLiteral("azureDevOpsInputCard"));
    inputCard->setStyleSheet(
        QStringLiteral(
            "QFrame#azureDevOpsInputCard {"
            "  background:%1;"
            "  border:1px solid %2;"
            "  border-radius:12px;"
            "}")
            .arg(AppStyle::surfaceAlt(), AppStyle::border()));
    auto* inputLayout = new QVBoxLayout(inputCard);
    inputLayout->setContentsMargins(16, 14, 16, 14);
    inputLayout->setSpacing(10);

    auto* linkTitle = new ElaText(QStringLiteral("方式一：粘贴链接"), inputCard);
    linkTitle->setStyleSheet(
        QStringLiteral("font-size:12px; font-weight:700; color:%1;")
            .arg(AppStyle::textPrimary()));
    m_linkEdit = new ElaLineEdit(inputCard);
    m_linkEdit->setObjectName(QStringLiteral("azureDevOpsLinkEdit"));
    m_linkEdit->setPlaceholderText(
        QStringLiteral("例如：https://dev.azure.com/org/project/_workitems/edit/123"));
    m_linkEdit->setFixedHeight(42);

    auto* manualTitle = new ElaText(QStringLiteral("方式二：手动填写编号"), inputCard);
    manualTitle->setStyleSheet(
        QStringLiteral("font-size:12px; font-weight:700; color:%1;")
            .arg(AppStyle::textPrimary()));

    auto* manualRow = new QHBoxLayout;
    manualRow->setContentsMargins(0, 0, 0, 0);
    manualRow->setSpacing(10);
    m_typeCombo = new ElaComboBox(inputCard);
    m_typeCombo->setObjectName(QStringLiteral("azureDevOpsTypeCombo"));
    m_typeCombo->addItem(QStringLiteral("工作项"), static_cast<int>(AzureDevOpsResourceKind::WorkItem));
    m_typeCombo->addItem(QStringLiteral("合并请求"), static_cast<int>(AzureDevOpsResourceKind::PullRequest));
    m_typeCombo->addItem(QStringLiteral("构建结果"), static_cast<int>(AzureDevOpsResourceKind::Build));
    m_typeCombo->setFixedHeight(38);

    m_repositoryEdit = new ElaLineEdit(inputCard);
    m_repositoryEdit->setObjectName(QStringLiteral("azureDevOpsRepositoryEdit"));
    m_repositoryEdit->setPlaceholderText(QStringLiteral("仓库名（PR 必填）"));
    m_repositoryEdit->setFixedHeight(38);

    m_resourceIdEdit = new ElaLineEdit(inputCard);
    m_resourceIdEdit->setObjectName(QStringLiteral("azureDevOpsResourceIdEdit"));
    m_resourceIdEdit->setPlaceholderText(QStringLiteral("编号，例如 123"));
    m_resourceIdEdit->setFixedHeight(38);

    manualRow->addWidget(m_typeCombo, 2);
    manualRow->addWidget(m_repositoryEdit, 2);
    manualRow->addWidget(m_resourceIdEdit, 2);

    auto* contextLabel = new ElaText(
        QStringLiteral("组织：%1  项目：%2")
            .arg(settings.organization.trimmed().isEmpty() ? QStringLiteral("--")
                                                           : settings.organization.trimmed(),
                 settings.project.trimmed().isEmpty() ? QStringLiteral("--")
                                                      : settings.project.trimmed()),
        inputCard);
    contextLabel->setWordWrap(true);
    contextLabel->setStyleSheet(
        QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));

    const QString lineEditStyle =
        QStringLiteral(
            "QLineEdit {"
            "  background:%1;"
            "  border:1.5px solid %2;"
            "  border-radius:10px;"
            "  padding:0 14px;"
            "  font-size:14px;"
            "  color:%3;"
            "}"
            "QLineEdit:focus {"
            "  border:1.5px solid %4;"
            "  background:%5;"
            "}")
            .arg(AppStyle::surfaceAlt(),
                 AppStyle::border(),
                 AppStyle::textPrimary(),
                 AppStyle::accent(),
                 AppStyle::surface());
    m_linkEdit->setStyleSheet(lineEditStyle);
    m_repositoryEdit->setStyleSheet(lineEditStyle);
    m_resourceIdEdit->setStyleSheet(lineEditStyle);
    m_typeCombo->setStyleSheet(
        QStringLiteral(
            "QComboBox {"
            "  background:%1;"
            "  border:1.5px solid %2;"
            "  border-radius:10px;"
            "  padding:0 10px;"
            "  font-size:14px;"
            "  color:%3;"
            "}"
            "QComboBox:focus { border:1.5px solid %4; background:%5; }"
            "QComboBox::drop-down { border:none; width:24px; }")
            .arg(AppStyle::surfaceAlt(),
                 AppStyle::border(),
                 AppStyle::textPrimary(),
                 AppStyle::accent(),
                 AppStyle::surface()));

    inputLayout->addWidget(linkTitle);
    inputLayout->addWidget(m_linkEdit);
    inputLayout->addSpacing(6);
    inputLayout->addWidget(manualTitle);
    inputLayout->addLayout(manualRow);
    inputLayout->addWidget(contextLabel);

    m_hintLabel = new ElaText(QStringLiteral("等待输入 Azure DevOps 链接或编号"), contentArea);
    m_hintLabel->setObjectName(QStringLiteral("azureDevOpsHintLabel"));
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(
        QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));

    auto* previewCard = new ElaFrame(contentArea);
    previewCard->setObjectName(QStringLiteral("azureDevOpsPreviewCard"));
    previewCard->setStyleSheet(
        QStringLiteral(
            "QFrame#azureDevOpsPreviewCard {"
            "  background:%1;"
            "  border:1px solid %2;"
            "  border-radius:12px;"
            "}")
            .arg(AppStyle::surfaceAlt(), AppStyle::border()));
    auto* previewLayout = new QVBoxLayout(previewCard);
    previewLayout->setContentsMargins(16, 14, 16, 14);
    previewLayout->setSpacing(10);

    const auto addPreviewRow =
        [previewCard, previewLayout](const QString& labelText, const QString& objectName) {
            auto* row = new QHBoxLayout;
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(8);

            auto* label = new ElaText(labelText, previewCard);
            label->setStyleSheet(
                QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));
            auto* value = new ElaText(QStringLiteral("--"), previewCard);
            value->setObjectName(objectName);
            value->setStyleSheet(
                QStringLiteral("font-size:13px; font-weight:600; color:%1;")
                    .arg(AppStyle::textPrimary()));
            value->setWordWrap(true);

            row->addWidget(label);
            row->addWidget(value, 1);
            previewLayout->addLayout(row);
            return value;
        };
    m_previewKindValue =
        addPreviewRow(QStringLiteral("类型"), QStringLiteral("azureDevOpsPreviewKind"));
    m_previewProjectValue =
        addPreviewRow(QStringLiteral("项目"), QStringLiteral("azureDevOpsPreviewProject"));
    m_previewIdValue =
        addPreviewRow(QStringLiteral("标识"), QStringLiteral("azureDevOpsPreviewId"));

    auto* cancelButton = new ElaPushButton(QStringLiteral("取消"), card);
    cancelButton->setFlat(true);
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setStyleSheet(
        QStringLiteral(
            "QPushButton { color:%1; font-size:13px; border:none; background:transparent; }"
            "QPushButton:hover { color:%2; }")
            .arg(AppStyle::textMuted(), AppStyle::accent()));

    m_insertButton = new ElaPushButton(QStringLiteral("插入卡片"), card);
    m_insertButton->setObjectName(QStringLiteral("azureDevOpsInsertButton"));
    m_insertButton->setEnabled(false);
    m_insertButton->setFixedHeight(42);
    m_insertButton->setCursor(Qt::PointingHandCursor);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(8);
    buttonRow->addWidget(cancelButton);
    buttonRow->addStretch();
    buttonRow->addWidget(m_insertButton);

    auto* actionSection = LeyoDialog::createSectionFrame(card, QStringLiteral("azureDevOpsInsertDialogActionSection"));
    auto* actionSectionLayout = new QVBoxLayout(actionSection);
    actionSectionLayout->setContentsMargins(0, 0, 0, 0);
    actionSectionLayout->setSpacing(0);
    auto* buttonArea = new QWidget(actionSection);
    buttonArea->setObjectName(QStringLiteral("azureDevOpsInsertButtonArea"));
    auto* buttonAreaLayout = new QVBoxLayout(buttonArea);
    buttonAreaLayout->setContentsMargins(0, 0, 0, 0);
    buttonAreaLayout->setSpacing(0);
    buttonAreaLayout->addLayout(buttonRow);
    actionSectionLayout->addWidget(buttonArea);

    cardLayout->addWidget(iconLabel);
    cardLayout->addSpacing(8);
    cardLayout->addWidget(titleLabel);
    cardLayout->addSpacing(4);
    cardLayout->addWidget(subtitleLabel);
    cardLayout->addSpacing(22);
    contentAreaLayout->addWidget(inputCard);
    contentAreaLayout->addWidget(m_hintLabel);
    contentAreaLayout->addWidget(previewCard);
    contentSectionLayout->addWidget(contentArea);
    cardLayout->addWidget(contentSection);
    cardLayout->addSpacing(24);
    cardLayout->addWidget(actionSection);

    outer->addWidget(card);

    connect(cancelButton, &QAbstractButton::clicked, this, &QDialog::reject);
    connect(m_insertButton, &QAbstractButton::clicked, this, &QDialog::accept);
    connect(m_linkEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_typeCombo, &QComboBox::currentIndexChanged, this, [this]() {
        updateManualFieldVisibility();
        refreshPreview();
    });
    connect(m_repositoryEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_resourceIdEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(m_linkEdit, &QLineEdit::returnPressed, this, [this]() {
        if (m_insertButton->isEnabled()) {
            accept();
        }
    });
    connect(m_resourceIdEdit, &QLineEdit::returnPressed, this, [this]() {
        if (m_insertButton->isEnabled()) {
            accept();
        }
    });

    updateManualFieldVisibility();
    refreshPreview();
}

QString AzureDevOpsInsertDialog::link() const
{
    return m_linkEdit ? m_linkEdit->text().trimmed() : QString();
}

void AzureDevOpsInsertDialog::setLinkText(const QString& link)
{
    if (!m_linkEdit) {
        return;
    }
    m_linkEdit->setText(link.trimmed());
    refreshPreview();
}

std::optional<AzureDevOpsResourceLocator> AzureDevOpsInsertDialog::parsedLocator() const
{
    return m_parsedLocator;
}

void AzureDevOpsInsertDialog::refreshPreview()
{
    const QString rawLink = link();
    m_parsedLocator = rawLink.isEmpty()
        ? buildManualLocator()
        : AzureDevOpsLinkParser::parse(rawLink);

    if (!m_parsedLocator.has_value()) {
        m_previewKindValue->setText(QStringLiteral("--"));
        m_previewProjectValue->setText(QStringLiteral("--"));
        m_previewIdValue->setText(QStringLiteral("--"));
        m_hintLabel->setText(rawLink.isEmpty()
                                 ? QStringLiteral("等待输入 Azure DevOps 链接或编号")
                                 : QStringLiteral("暂时无法识别这条链接，请检查是否为工作项、PR 或构建地址。"));
        m_insertButton->setEnabled(false);
        return;
    }

    m_previewKindValue->setText(previewKindLabel(m_parsedLocator->kind));
    m_previewProjectValue->setText(
        QStringLiteral("%1 / %2").arg(m_parsedLocator->organization, m_parsedLocator->project));
    m_previewIdValue->setText(m_parsedLocator->resourceId);
    m_hintLabel->setText(QStringLiteral("资源信息已识别，可以继续拉取摘要并插入卡片。"));
    m_insertButton->setEnabled(true);
}

std::optional<AzureDevOpsResourceLocator> AzureDevOpsInsertDialog::buildManualLocator() const
{
    if (!m_typeCombo || !m_resourceIdEdit) {
        return std::nullopt;
    }

    const QString organization = m_settings.organization.trimmed();
    const QString project = m_settings.project.trimmed();
    const QString resourceId = m_resourceIdEdit->text().trimmed();
    if (organization.isEmpty() || project.isEmpty() || resourceId.isEmpty()) {
        return std::nullopt;
    }

    AzureDevOpsResourceLocator locator;
    locator.kind = static_cast<AzureDevOpsResourceKind>(m_typeCombo->currentData().toInt());
    locator.organization = organization;
    locator.project = project;
    locator.resourceId = resourceId;

    const QString baseUrl = m_settings.baseUrl.trimmed().isEmpty()
        ? QStringLiteral("https://dev.azure.com")
        : m_settings.baseUrl.trimmed();

    switch (locator.kind) {
    case AzureDevOpsResourceKind::WorkItem:
        locator.webUrl = QStringLiteral("%1/%2/%3/_workitems/edit/%4")
                             .arg(baseUrl, organization, project, resourceId);
        break;
    case AzureDevOpsResourceKind::PullRequest:
        locator.repository = m_repositoryEdit ? m_repositoryEdit->text().trimmed() : QString();
        if (locator.repository.isEmpty()) {
            return std::nullopt;
        }
        locator.webUrl = QStringLiteral("%1/%2/%3/_git/%4/pullrequest/%5")
                             .arg(baseUrl, organization, project, locator.repository, resourceId);
        break;
    case AzureDevOpsResourceKind::Build:
        locator.webUrl = QStringLiteral("%1/%2/%3/_build/results?buildId=%4")
                             .arg(baseUrl, organization, project, resourceId);
        break;
    case AzureDevOpsResourceKind::Unknown:
    default:
        return std::nullopt;
    }

    return locator.isValid() ? std::optional<AzureDevOpsResourceLocator>(locator) : std::nullopt;
}

void AzureDevOpsInsertDialog::updateManualFieldVisibility()
{
    if (!m_typeCombo || !m_repositoryEdit) {
        return;
    }

    const bool isPullRequest =
        static_cast<AzureDevOpsResourceKind>(m_typeCombo->currentData().toInt())
        == AzureDevOpsResourceKind::PullRequest;
    m_repositoryEdit->setVisible(isPullRequest);
}
