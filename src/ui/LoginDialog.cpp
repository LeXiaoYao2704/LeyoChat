#include "ui/LoginDialog.h"

#include "services/IdentityService.h"
#include "ui/AppStyle.h"

#include <ElaFrame.h>
#include <ElaLineEdit.h>
#include <ElaPushButton.h>
#include <ElaText.h>

#include <QColor>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

namespace {

QString colorWithAlpha(const QString& color, int alpha)
{
    QColor c(color);
    if (!c.isValid()) {
        c = QColor(QStringLiteral("#FFFFFF"));
    }
    c.setAlpha(alpha);
    return c.name(QColor::HexArgb);
}

QString firstRunDialogStyleSheet()
{
    return QStringLiteral(
        "ElaDialog#FirstRunEchoLoginDialog {"
        "  background: transparent;"
        "}"
        "ElaFrame#FirstRunEchoShell {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 22px;"
        "}"
        "ElaFrame#FirstRunEchoHeroCard {"
        "  background: %3;"
        "  border: 1px solid %4;"
        "  border-radius: 18px;"
        "}"
        "ElaFrame#FirstRunEchoIdentityCard {"
        "  background: %5;"
        "  border: 1px solid %6;"
        "  border-radius: 18px;"
        "}"
        "ElaText#FirstRunEchoKicker {"
        "  color: %7;"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "}"
        "ElaText#FirstRunEchoTitle {"
        "  color: %8;"
        "  font-size: 26px;"
        "  font-weight: 800;"
        "}"
        "ElaText#FirstRunEchoSubtitle, ElaText#FirstRunEchoHint, ElaText#loginStateHintLabel {"
        "  color: %9;"
        "  font-size: 12px;"
        "}"
        "ElaText#FirstRunEchoFieldLabel {"
        "  color: %8;"
        "  font-size: 13px;"
        "  font-weight: 700;"
        "}"
        "ElaText#FirstRunEchoBadge {"
        "  background: %10;"
        "  border: 1px solid %11;"
        "  border-radius: 16px;"
        "  color: %7;"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "  padding: 6px 10px;"
        "}"
        "QLineEdit {"
        "  background: %12;"
        "  border: 1.5px solid %13;"
        "  border-radius: 10px;"
        "  padding: 0 14px;"
        "  font-size: 14px;"
        "  color: %8;"
        "}"
        "QLineEdit:focus {"
        "  border: 1.5px solid %7;"
        "  background: %14;"
        "}"
        "QPushButton#FirstRunEchoCancelButton {"
        "  color: %9;"
        "  font-size: 13px;"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QPushButton#FirstRunEchoCancelButton:hover {"
        "  color: %7;"
        "}")
        .arg(colorWithAlpha(AppStyle::surface(), 218),
             colorWithAlpha(AppStyle::border(), 190),
             colorWithAlpha(AppStyle::accent(), 24),
             colorWithAlpha(AppStyle::accent(), 58),
             colorWithAlpha(AppStyle::surfaceAlt(), 232),
             colorWithAlpha(AppStyle::border(), 214),
             AppStyle::accent(),
             AppStyle::textPrimary(),
             AppStyle::textMuted(),
             colorWithAlpha(AppStyle::accent(), 28),
             colorWithAlpha(AppStyle::accent(), 72),
             colorWithAlpha(AppStyle::surfaceAlt(), 236),
             AppStyle::border(),
             colorWithAlpha(AppStyle::surface(), 246));
}

ElaText* createEchoText(const QString& objectName, const QString& text, QWidget* parent)
{
    auto* label = new ElaText(text, parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    return label;
}

} // namespace

LoginDialog::LoginDialog(QWidget* parent)
    : ElaDialog(parent),
      m_displayNameEdit(new ElaLineEdit(this)),
      m_employeeCodeEdit(new ElaLineEdit(this)),
      m_okButton(new ElaPushButton(QStringLiteral("进入"), this)),
      m_stateHintLabel(new ElaText(this))
{
    setObjectName(QStringLiteral("FirstRunEchoLoginDialog"));
    setWindowTitle(QStringLiteral("LeyoChat"));
    setFixedSize(720, 460);
    setStyleSheet(firstRunDialogStyleSheet());

    m_displayNameEdit->setPlaceholderText(QStringLiteral("请输入昵称"));
    m_displayNameEdit->setFixedHeight(44);

    m_employeeCodeEdit->setPlaceholderText(QStringLiteral("请输入工号"));
    m_employeeCodeEdit->setFixedHeight(44);

    m_okButton->setFixedHeight(44);
    m_okButton->setFixedWidth(128);
    m_okButton->setCursor(Qt::PointingHandCursor);

    auto* cancelBtn = new ElaPushButton(QStringLiteral("取消"), this);
    cancelBtn->setObjectName(QStringLiteral("FirstRunEchoCancelButton"));
    cancelBtn->setFlat(true);
    cancelBtn->setCursor(Qt::PointingHandCursor);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(cancelBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_okButton);

    auto* heroCard = new ElaFrame(this);
    heroCard->setObjectName(QStringLiteral("FirstRunEchoHeroCard"));
    auto* heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(26, 28, 26, 28);
    heroLayout->setSpacing(12);

    auto* badge = createEchoText(QStringLiteral("FirstRunEchoBadge"),
                                 QStringLiteral("首次初始化"),
                                 heroCard);
    badge->setFixedHeight(34);
    badge->setAlignment(Qt::AlignCenter);
    heroLayout->addWidget(badge, 0, Qt::AlignLeft);
    heroLayout->addSpacing(10);

    auto* heroTitle = createEchoText(QStringLiteral("FirstRunEchoTitle"),
                                     QStringLiteral("欢迎使用 LeyoChat"),
                                     heroCard);
    heroLayout->addWidget(heroTitle);

    auto* heroSubtitle = createEchoText(
        QStringLiteral("FirstRunEchoSubtitle"),
        QStringLiteral("先确认你的本机身份，随后进入会话、通知、通讯录和知识工作区。"),
        heroCard);
    heroLayout->addWidget(heroSubtitle);
    heroLayout->addStretch(1);

    auto* heroHint = createEchoText(
        QStringLiteral("FirstRunEchoHint"),
        QStringLiteral("此步骤只创建本地 profile，不改动消息、文件传输和网络发现逻辑。"),
        heroCard);
    heroLayout->addWidget(heroHint);

    auto* identityCard = new ElaFrame(this);
    identityCard->setObjectName(QStringLiteral("FirstRunEchoIdentityCard"));
    auto* identityLayout = new QVBoxLayout(identityCard);
    identityLayout->setContentsMargins(28, 30, 28, 26);
    identityLayout->setSpacing(0);

    auto* kickerLabel = createEchoText(QStringLiteral("FirstRunEchoKicker"),
                                       QStringLiteral("LEYOCHAT SETUP"),
                                       identityCard);
    identityLayout->addWidget(kickerLabel);
    identityLayout->addSpacing(8);

    auto* titleLabel = createEchoText(QStringLiteral("FirstRunEchoTitle"),
                                      QStringLiteral("设置你的聊天身份"),
                                      identityCard);
    identityLayout->addWidget(titleLabel);
    identityLayout->addSpacing(8);

    auto* subtitleLabel = createEchoText(
        QStringLiteral("FirstRunEchoSubtitle"),
        QStringLiteral("昵称会显示在会话和局域网发现里，工号用于保持原有身份校验。"),
        identityCard);
    identityLayout->addWidget(subtitleLabel);
    identityLayout->addSpacing(28);

    auto* displayNameLabel = createEchoText(QStringLiteral("FirstRunEchoFieldLabel"),
                                            QStringLiteral("昵称"),
                                            identityCard);
    identityLayout->addWidget(displayNameLabel);
    identityLayout->addSpacing(6);
    identityLayout->addWidget(m_displayNameEdit);
    identityLayout->addSpacing(16);

    auto* employeeCodeLabel = createEchoText(QStringLiteral("FirstRunEchoFieldLabel"),
                                             QStringLiteral("工号"),
                                             identityCard);
    identityLayout->addWidget(employeeCodeLabel);
    identityLayout->addSpacing(6);
    identityLayout->addWidget(m_employeeCodeEdit);
    identityLayout->addSpacing(18);

    m_stateHintLabel->setObjectName(QStringLiteral("loginStateHintLabel"));
    identityLayout->addWidget(m_stateHintLabel);
    identityLayout->addStretch(1);
    identityLayout->addLayout(btnRow);

    auto* shell = new ElaFrame(this);
    shell->setObjectName(QStringLiteral("FirstRunEchoShell"));
    auto* shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(20, 20, 20, 20);
    shellLayout->setSpacing(18);
    shellLayout->addWidget(heroCard, 0);
    shellLayout->addWidget(identityCard, 1);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 14, 14, 14);
    outer->addWidget(shell);

    connect(m_okButton, &QAbstractButton::clicked, this, [this]() {
        applyState(QStringLiteral("loading"));
        IdentityService identityService;
        const auto validation = identityService.validateInput(displayName(), employeeCode());
        if (!validation.isValid) {
            applyState(QStringLiteral("error"));
            QMessageBox::warning(this, QStringLiteral("LeyoChat"), validation.errorMessage);
            return;
        }
        accept();
    });
    connect(cancelBtn, &QAbstractButton::clicked, this, &QDialog::reject);
    connect(m_displayNameEdit, &QLineEdit::returnPressed, m_employeeCodeEdit, qOverload<>(&QWidget::setFocus));
    connect(m_employeeCodeEdit, &QLineEdit::returnPressed, m_okButton, &QAbstractButton::click);

    applyState(QStringLiteral("idle"));
}

QString LoginDialog::displayName() const
{
    return m_displayNameEdit->text().trimmed();
}

QString LoginDialog::employeeCode() const
{
    return m_employeeCodeEdit->text().trimmed();
}

bool LoginDialog::hasStateForTesting(const QString& state) const
{
    const QString normalized = state.trimmed().toLower();
    return normalized == QStringLiteral("idle")
        || normalized == QStringLiteral("loading")
        || normalized == QStringLiteral("error");
}

QString LoginDialog::currentStateForTesting() const
{
    return m_currentState;
}

void LoginDialog::setStateForTesting(const QString& state)
{
    applyState(state);
}

void LoginDialog::applyState(const QString& state)
{
    const QString normalized = state.trimmed().toLower();
    if (!hasStateForTesting(normalized)) {
        return;
    }

    m_currentState = normalized;
    if (normalized == QStringLiteral("loading")) {
        m_okButton->setEnabled(false);
        m_okButton->setText(QStringLiteral("登录中..."));
        m_displayNameEdit->setEnabled(false);
        m_employeeCodeEdit->setEnabled(false);
        m_stateHintLabel->setText(QStringLiteral("正在验证身份，请稍候..."));
        return;
    }

    m_okButton->setEnabled(true);
    m_okButton->setText(QStringLiteral("进入"));
    m_displayNameEdit->setEnabled(true);
    m_employeeCodeEdit->setEnabled(true);
    if (normalized == QStringLiteral("error")) {
        m_stateHintLabel->setText(QStringLiteral("登录失败，请检查输入后重试。"));
    } else {
        m_stateHintLabel->clear();
    }
}
