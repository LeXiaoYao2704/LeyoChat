#include "CloseToTrayDialog.h"

#include "ui/AppStyle.h"

#include <ElaCheckBox.h>
#include <ElaPushButton.h>

#include <QAbstractButton>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

namespace {

void styleCloseDialogButton(ElaPushButton* button, bool primary)
{
    if (!button) return;
    button->setFixedSize(140, 40);
    button->setBorderRadius(9);

    if (primary) {
        button->setLightDefaultColor(QColor(255, 255, 255));
        button->setDarkDefaultColor(QColor(255, 255, 255));
        button->setLightHoverColor(QColor(245, 248, 252));
        button->setDarkHoverColor(QColor(245, 248, 252));
        button->setLightPressColor(QColor(232, 238, 246));
        button->setDarkPressColor(QColor(232, 238, 246));
        button->setLightTextColor(QColor(18, 24, 34));
        button->setDarkTextColor(QColor(18, 24, 34));
    } else {
        button->setLightDefaultColor(QColor(255, 255, 255));
        button->setDarkDefaultColor(QColor(255, 255, 255));
        button->setLightHoverColor(QColor(248, 249, 251));
        button->setDarkHoverColor(QColor(248, 249, 251));
        button->setLightPressColor(QColor(235, 239, 245));
        button->setDarkPressColor(QColor(235, 239, 245));
        button->setLightTextColor(QColor(18, 24, 34));
        button->setDarkTextColor(QColor(18, 24, 34));
    }
}

} // namespace

CloseToTrayDialog::CloseToTrayDialog(QWidget* parent)
    : ElaDialog(parent)
{
    setObjectName(QStringLiteral("closeToTrayDialog"));
    setWindowTitle(QStringLiteral("关闭 LeyoChat"));
    setWindowModality(Qt::WindowModal);
    setModal(true);
    setIsStayTop(false);
    setIsFixedSize(true);
    setAppBarHeight(46);
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    setFixedSize(560, 292);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(46, 28, 34, 32);
    root->setSpacing(0);

    auto* bodyRow = new QHBoxLayout();
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(20);

    auto* iconLabel = new QLabel(this);
    iconLabel->setFixedSize(46, 46);
    iconLabel->setStyleSheet(QStringLiteral("background:transparent;"));
    QPixmap icon(QStringLiteral(":/app/leyochat-icon.png"));
    if (!icon.isNull()) {
        iconLabel->setPixmap(icon.scaled(46, 46, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    bodyRow->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* textColumn = new QVBoxLayout();
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(7);

    auto* title = new QLabel(QStringLiteral("要把 LeyoChat 留在后台吗?"), this);
    title->setFixedHeight(34);
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    title->setStyleSheet(QStringLiteral("color:#050912; background:transparent; font-size:24px;"));
    QFont titleFont = title->font();
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    textColumn->addWidget(title);

    auto* hint = new QLabel(
        QStringLiteral("关闭主窗口后，LeyoChat 仍会留在任务栏托盘中接收新消息、文件和会话提醒。"),
        this);
    hint->setFixedHeight(42);
    hint->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#778193; background:transparent; font-size:14px; line-height:19px;"));
    textColumn->addWidget(hint);

    bodyRow->addLayout(textColumn, 1);
    root->addLayout(bodyRow);

    root->addSpacing(10);

    m_rememberCheck = new ElaCheckBox(QStringLiteral("不再提示，下次按这次选择执行"), this);
    m_rememberCheck->setStyleSheet(QStringLiteral(
        "QCheckBox{font-size:14px; color:#111827; background:transparent; spacing:10px;}"
        "QCheckBox::indicator{width:19px; height:19px;}"));
    root->addWidget(m_rememberCheck);

    root->addStretch();

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(18);
    buttonRow->addStretch(1);

    auto* exitButton = new ElaPushButton(QStringLiteral("退出 LeyoChat"), this);
    styleCloseDialogButton(exitButton, false);
    buttonRow->addWidget(exitButton);

    auto* backgroundButton = new ElaPushButton(QStringLiteral("后台运行"), this);
    styleCloseDialogButton(backgroundButton, true);
    buttonRow->addWidget(backgroundButton);
    root->addLayout(buttonRow);

    setStyleSheet(QStringLiteral(
        "ElaDialog#closeToTrayDialog{background:#FFFFFF; border-radius:10px;}"
        "ElaPushButton{border:1px solid #DDE2EA; font-size:16px; font-weight:500;}"
        "ElaPushButton:hover{border-color:#CBD5E1;}"));

    connect(this, &ElaDialog::closeButtonClicked, this, &QDialog::reject);
    connect(exitButton, &QAbstractButton::clicked, this, [this]() {
        m_selectedAction = ClientCloseAction::ExitApplication;
        accept();
    });
    connect(backgroundButton, &QAbstractButton::clicked, this, [this]() {
        m_selectedAction = ClientCloseAction::MinimizeToTray;
        accept();
    });
}

ClientCloseAction CloseToTrayDialog::selectedAction() const
{
    return m_selectedAction;
}

bool CloseToTrayDialog::dontAskAgain() const
{
    return m_rememberCheck && m_rememberCheck->isChecked();
}
