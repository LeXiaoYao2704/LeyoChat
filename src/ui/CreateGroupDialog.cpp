#include "ui/CreateGroupDialog.h"

#include "domain/PeerEndpoint.h"
#include "ui/AppStyle.h"
#include "ui/LeyoDialog.h"

#include <ElaCheckBox.h>
#include <ElaLineEdit.h>
#include <ElaPushButton.h>
#include <ElaScrollArea.h>
#include <ElaText.h>

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QString editStyle()
{
    return QStringLiteral(
        "QLineEdit {"
        "  background:%1;"
        "  border:1px solid %2;"
        "  border-radius:%3px;"
        "  padding:0 14px;"
        "  font-size:14px;"
        "  color:%4;"
        "}"
        "QLineEdit:focus {"
        "  background:%5;"
        "  border-color:%6;"
        "}")
        .arg(AppStyle::surfaceAlt(),
             AppStyle::border(),
             QString::number(AppStyle::kRadiusMd),
             AppStyle::textPrimary(),
             AppStyle::surface(),
             AppStyle::accent());
}

QString memberCheckStyle()
{
    return QStringLiteral(
        "QCheckBox {"
        "  font-size:14px;"
        "  color:%1;"
        "  padding:10px 12px;"
        "  spacing:10px;"
        "  border-radius:%2px;"
        "}"
        "QCheckBox:hover {"
        "  background:%3;"
        "}"
        "QCheckBox::indicator {"
        "  width:18px;"
        "  height:18px;"
        "  border:1px solid %4;"
        "  border-radius:5px;"
        "  background:%5;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background:%6;"
        "  border-color:%6;"
        "}")
        .arg(AppStyle::textPrimary(),
             QString::number(AppStyle::kRadiusMd),
             AppStyle::hoverBg(),
             AppStyle::borderStrong(),
             AppStyle::surface(),
             AppStyle::accent());
}

ElaText* makeSectionLabel(const QString& text, QWidget* parent)
{
    auto* label = new ElaText(text, parent);
    label->setStyleSheet(QStringLiteral(
                             "font-size:12px; color:%1; font-weight:700; letter-spacing:0.5px;")
                             .arg(AppStyle::textMuted()));
    return label;
}

} // namespace

CreateGroupDialog::CreateGroupDialog(const QList<PeerEndpoint>& peers, QWidget* parent)
    : ElaDialog(parent)
    , m_nameEdit(new ElaLineEdit(this))
    , m_searchEdit(new ElaLineEdit(this))
    , m_selectionSummaryLabel(new ElaText(this))
    , m_visibleCountLabel(new ElaText(this))
    , m_emptyStateLabel(new ElaText(this))
    , m_okButton(new ElaPushButton(QStringLiteral("\u521B\u5EFA\u7FA4\u804A"), this))
    , m_membersWidget(new QWidget(this))
{
    LeyoDialog::applySecondaryDialogScaffold(this, QStringLiteral("createGroupDialog"));
    const QFont baseFont = font();
    const int fieldHeight = qMax(40, QFontMetrics(AppStyle::bodyFont(baseFont)).height() + 20);
    const int primaryButtonHeight = qMax(42, QFontMetrics(AppStyle::strongFont(baseFont)).height() + 20);
    const int scrollMinHeight = qMax(220, QFontMetrics(AppStyle::bodyFont(baseFont)).height() * 8 + 56);

    setWindowTitle(QStringLiteral("\u65B0\u5EFA\u7FA4\u804A"));
    setFixedWidth(460);

    m_nameEdit->setObjectName(QStringLiteral("createGroupNameEdit"));
    m_searchEdit->setObjectName(QStringLiteral("createGroupSearchEdit"));
    m_selectionSummaryLabel->setObjectName(QStringLiteral("createGroupSelectionSummaryLabel"));
    m_visibleCountLabel->setObjectName(QStringLiteral("createGroupVisibleCountLabel"));
    m_emptyStateLabel->setObjectName(QStringLiteral("createGroupEmptyStateLabel"));
    m_membersWidget->setObjectName(QStringLiteral("createGroupMembersWidget"));
    m_okButton->setObjectName(QStringLiteral("createGroupConfirmButton"));

    m_nameEdit->setPlaceholderText(QStringLiteral("\u8BF7\u8F93\u5165\u7FA4\u540D\u79F0"));
    m_nameEdit->setFixedHeight(fieldHeight);
    m_nameEdit->setStyleSheet(editStyle());

    m_searchEdit->setPlaceholderText(QStringLiteral("\u641C\u7D22\u8054\u7CFB\u4EBA"));
    m_searchEdit->setFixedHeight(fieldHeight);
    m_searchEdit->setStyleSheet(editStyle());

    m_selectionSummaryLabel->setStyleSheet(QStringLiteral(
        "background:%1; color:%2; border-radius:%3px; font-size:12px; font-weight:600; padding:5px 10px;")
                                               .arg(AppStyle::accentSoft(),
                                                    AppStyle::accent(),
                                                    QString::number(AppStyle::kRadiusMd)));
    m_visibleCountLabel->setStyleSheet(QStringLiteral(
        "font-size:12px; color:%1; font-weight:600;")
                                           .arg(AppStyle::textMuted()));
    m_emptyStateLabel->setAlignment(Qt::AlignCenter);
    m_emptyStateLabel->setWordWrap(true);
    m_emptyStateLabel->setStyleSheet(QStringLiteral(
                                         "font-size:13px; color:%1; padding:26px 18px;")
                                         .arg(AppStyle::textMuted()));
    m_emptyStateLabel->hide();

    m_okButton->setFixedHeight(primaryButtonHeight);
    m_okButton->setCursor(Qt::PointingHandCursor);

    auto* cancelBtn = new QPushButton(QStringLiteral("\u53D6\u6D88"), this);
    cancelBtn->setFlat(true);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setStyleSheet(QStringLiteral(
        "QPushButton { color:%1; font-size:13px; border:none; background:transparent; }"
        "QPushButton:hover { color:%2; }")
                                   .arg(AppStyle::textMuted(),
                                        AppStyle::accent()));

    auto* logo = new ElaText(QStringLiteral("\u25A6"), this);
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet(QStringLiteral(
                            "font-size:26px; color:%1; background:%2; border-radius:18px; padding:6px 10px;")
                            .arg(AppStyle::accent(),
                                 AppStyle::accentSoft()));

    auto* titleLabel = new ElaText(QStringLiteral("\u65B0\u5EFA\u7FA4\u804A"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QStringLiteral("font-size:22px; font-weight:700; color:%1;")
                                  .arg(AppStyle::textPrimary()));

    auto* subtitleLabel = new ElaText(
        QStringLiteral("\u4ECE\u8054\u7CFB\u4EBA\u4E2D\u9009\u62E9\u6210\u5458\uFF0C\u76F4\u63A5\u62C9\u8D77\u4E00\u4E2A\u5DE5\u4F5C\u7FA4\u7EC4"),
        this);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setStyleSheet(QStringLiteral("font-size:13px; color:%1;")
                                     .arg(AppStyle::textMuted()));

    auto* membersLayout = new QVBoxLayout(m_membersWidget);
    membersLayout->setContentsMargins(0, 0, 0, 0);
    membersLayout->setSpacing(4);

    for (const auto& peer : peers) {
        const QString displayName = QString::fromStdString(peer.displayName).trimmed();
        const QString clientId = QString::fromStdString(peer.clientId);
        const QString host = QString::fromStdString(peer.host);
        const QString label = displayName.isEmpty()
                                  ? clientId
                                  : QStringLiteral("%1  (%2)").arg(displayName, host.isEmpty() ? clientId : host);

        auto* checkbox = new ElaCheckBox(label, this);
        checkbox->setProperty("clientId", clientId);
        checkbox->setStyleSheet(memberCheckStyle());
        m_checkboxes.append(checkbox);
        membersLayout->addWidget(checkbox);

        connect(checkbox, &ElaCheckBox::toggled, this, [this]() {
            refreshSelectionSummary();
            refreshConfirmState();
        });
    }
    m_emptyStateLabel->setText(peers.isEmpty()
                                   ? QStringLiteral("\u8FD8\u6CA1\u6709\u53EF\u9080\u8BF7\u7684\u8054\u7CFB\u4EBA")
                                   : QStringLiteral("\u6CA1\u6709\u5339\u914D\u5230\u8054\u7CFB\u4EBA\uFF0C\u53EF\u4EE5\u6362\u4E2A\u5173\u952E\u5B57\u518D\u8BD5\u8BD5"));
    membersLayout->addWidget(m_emptyStateLabel);
    membersLayout->addStretch();

    auto* scrollArea = new ElaScrollArea(this);
    scrollArea->setWidget(m_membersWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setMinimumHeight(scrollMinHeight);
    scrollArea->setStyleSheet(QStringLiteral(
                                  "QScrollArea { background:transparent; }"
                                  "QScrollBar:vertical { width:8px; background:transparent; }"
                                  "QScrollBar::handle:vertical { background:%1; border-radius:4px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
                                  .arg(AppStyle::borderStrong()));

    auto* headerLayout = new QVBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);
    headerLayout->addWidget(logo, 0, Qt::AlignHCenter);
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subtitleLabel);

    auto* selectionRow = new QHBoxLayout;
    selectionRow->setContentsMargins(0, 0, 0, 0);
    selectionRow->setSpacing(8);
    selectionRow->addWidget(makeSectionLabel(QStringLiteral("\u6210\u5458"), this));
    selectionRow->addWidget(m_visibleCountLabel);
    selectionRow->addStretch();
    selectionRow->addWidget(m_selectionSummaryLabel);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(8);
    buttonRow->addWidget(cancelBtn);
    buttonRow->addStretch();
    buttonRow->addWidget(m_okButton);

    auto* formSection = LeyoDialog::createSectionFrame(this, QStringLiteral("createGroupDialogFormSection"));
    formSection->setStyleSheet(QStringLiteral(
        "QFrame#createGroupDialogFormSection {"
        "  background:%1;"
        "  border:1px solid %2;"
        "  border-radius:16px;"
        "}")
        .arg(AppStyle::surface(), AppStyle::border()));
    auto* formSectionLayout = new QVBoxLayout(formSection);
    formSectionLayout->setContentsMargins(0, 0, 0, 0);
    formSectionLayout->setSpacing(0);

    auto* formArea = new QWidget(formSection);
    formArea->setObjectName(QStringLiteral("createGroupFormArea"));
    auto* cardLayout = new QVBoxLayout(formArea);
    cardLayout->setContentsMargins(36, 34, 36, 30);
    cardLayout->setSpacing(0);
    cardLayout->addLayout(headerLayout);
    cardLayout->addSpacing(28);
    cardLayout->addWidget(makeSectionLabel(QStringLiteral("\u7FA4\u540D\u79F0"), this));
    cardLayout->addSpacing(6);
    cardLayout->addWidget(m_nameEdit);
    cardLayout->addSpacing(18);
    cardLayout->addWidget(makeSectionLabel(QStringLiteral("\u641C\u7D22"), this));
    cardLayout->addSpacing(6);
    cardLayout->addWidget(m_searchEdit);
    cardLayout->addSpacing(18);
    cardLayout->addLayout(selectionRow);
    cardLayout->addSpacing(10);
    cardLayout->addWidget(scrollArea);
    formSectionLayout->addWidget(formArea);

    auto* actionSection = LeyoDialog::createSectionFrame(this, QStringLiteral("createGroupDialogActionSection"));
    auto* actionSectionLayout = new QVBoxLayout(actionSection);
    actionSectionLayout->setContentsMargins(0, 0, 0, 0);
    actionSectionLayout->setSpacing(0);
    auto* buttonArea = new QWidget(actionSection);
    buttonArea->setObjectName(QStringLiteral("createGroupButtonArea"));
    auto* buttonAreaLayout = new QVBoxLayout(buttonArea);
    buttonAreaLayout->setContentsMargins(0, 0, 0, 0);
    buttonAreaLayout->setSpacing(0);
    buttonAreaLayout->addLayout(buttonRow);
    actionSectionLayout->addWidget(buttonArea);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(16);
    outer->addWidget(formSection);
    outer->addWidget(actionSection);

    connect(m_okButton, &QAbstractButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QAbstractButton::clicked, this, &QDialog::reject);
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this]() { refreshConfirmState(); });
    connect(m_nameEdit, &QLineEdit::returnPressed, m_okButton, &QAbstractButton::click);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        applySearchFilter(text);
    });

    refreshSelectionSummary();
    refreshConfirmState();
}

QString CreateGroupDialog::groupName() const
{
    return m_nameEdit->text().trimmed();
}

void CreateGroupDialog::preSelectMembers(const QStringList& clientIds)
{
    for (ElaCheckBox* checkbox : m_checkboxes) {
        if (clientIds.contains(checkbox->property("clientId").toString())) {
            checkbox->setChecked(true);
        }
    }
    refreshSelectionSummary();
    refreshConfirmState();
}

QStringList CreateGroupDialog::selectedMemberIds() const
{
    QStringList ids;
    for (const ElaCheckBox* checkbox : m_checkboxes) {
        if (checkbox->isChecked()) {
            ids.append(checkbox->property("clientId").toString());
        }
    }
    return ids;
}

int CreateGroupDialog::selectedCount() const
{
    return selectedMemberIds().size();
}

int CreateGroupDialog::visibleMemberCount() const
{
    int visibleCount = 0;
    for (const ElaCheckBox* checkbox : m_checkboxes) {
        if (!checkbox->isHidden()) {
            ++visibleCount;
        }
    }
    return visibleCount;
}

QString CreateGroupDialog::selectionSummaryText() const
{
    return m_selectionSummaryLabel->text();
}

bool CreateGroupDialog::isConfirmEnabled() const
{
    return m_okButton != nullptr && m_okButton->isEnabled();
}

void CreateGroupDialog::refreshSelectionSummary()
{
    QStringList names;
    for (const ElaCheckBox* checkbox : m_checkboxes) {
        if (checkbox->isChecked()) {
            names.append(checkbox->text().section(QStringLiteral("  ("), 0, 0));
        }
    }

    if (names.isEmpty()) {
        m_selectionSummaryLabel->setText(QStringLiteral("\u672A\u9009\u6210\u5458"));
    } else if (names.size() == 1) {
        m_selectionSummaryLabel->setText(QStringLiteral("\u5DF2\u9009 %1").arg(names.first()));
    } else if (names.size() == 2) {
        m_selectionSummaryLabel->setText(
            QStringLiteral("\u5DF2\u9009 %1\u3001%2").arg(names.at(0), names.at(1)));
    } else {
        m_selectionSummaryLabel->setText(
            QStringLiteral("\u5DF2\u9009 %1\u3001%2 \u7B49 %3 \u4EBA")
                .arg(names.at(0), names.at(1))
                .arg(names.size()));
    }
    m_visibleCountLabel->setText(QStringLiteral("\u53EF\u9080\u8BF7 %1 \u4EBA").arg(visibleMemberCount()));
}

void CreateGroupDialog::refreshConfirmState()
{
    m_okButton->setEnabled(!groupName().isEmpty() && selectedCount() > 0);
}

void CreateGroupDialog::applySearchFilter(const QString& text)
{
    const QString needle = text.trimmed();
    for (ElaCheckBox* checkbox : m_checkboxes) {
        const bool matches = needle.isEmpty()
                             || checkbox->text().contains(needle, Qt::CaseInsensitive)
                             || checkbox->property("clientId").toString().contains(needle, Qt::CaseInsensitive);
        checkbox->setVisible(matches);
    }
    m_visibleCountLabel->setText(QStringLiteral("\u53EF\u9080\u8BF7 %1 \u4EBA").arg(visibleMemberCount()));
    m_emptyStateLabel->setVisible(visibleMemberCount() == 0);
}
