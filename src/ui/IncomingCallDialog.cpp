#include "ui/IncomingCallDialog.h"

#include "ui/AppStyle.h"
#include "ui/LeyoDialog.h"

#include <QHBoxLayout>
#include <ElaText.h>
#include <ElaPushButton.h>
#include <QTimer>
#include <QVBoxLayout>

IncomingCallDialog::IncomingCallDialog(const QString& callerName,
                                       const QString& callId,
                                       QWidget* parent)
    : ElaDialog(parent)
    , m_callId(callId)
{
    LeyoDialog::applySecondaryDialogScaffold(this, QStringLiteral("incomingCallDialog"));
    setWindowTitle(QStringLiteral("来电"));
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Dialog);
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedSize(320, 140);

    setStyleSheet(QStringLiteral(
        "ElaDialog#secondaryPageScaffold_incomingCallDialog { background:transparent; }"
        "QFrame#incomingCallCard {"
        "  background:%1;"
        "  border:1px solid %2;"
        "  border-radius:14px;"
        "}"
        "QLabel { color:%3; }"
        "QPushButton { border-radius:18px; padding:6px 20px; font-size:14px; font-weight:600; }"
    ).arg(AppStyle::surface(), AppStyle::borderStrong(), AppStyle::textPrimary()));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    auto* card = LeyoDialog::createSectionFrame(this, QStringLiteral("incomingCallCard"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 20, 24, 20);
    cardLayout->setSpacing(16);

    auto* statusSection = LeyoDialog::createSectionFrame(card, QStringLiteral("incomingCallStatusSection"));
    auto* statusLayout = new QVBoxLayout(statusSection);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(6);

    auto* title = new ElaText(QStringLiteral("\xF0\x9F\x93\x9E  %1 \u53D1\u8D77\u8BED\u97F3\u901A\u8BDD").arg(callerName), statusSection);
    title->setAlignment(Qt::AlignCenter);
    QFont f = title->font();
    f.setPointSize(13);
    title->setFont(f);
    statusLayout->addWidget(title);

    auto* hint = new ElaText(QStringLiteral("30 \u79D2\u5185\u672A\u5904\u7406\u5C06\u81EA\u52A8\u62D2\u7EDD"), statusSection);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet(QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));
    statusLayout->addWidget(hint);

    auto* actionSection = LeyoDialog::createSectionFrame(card, QStringLiteral("incomingCallActionSection"));
    auto* buttonLayout = new QHBoxLayout(actionSection);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(16);
    auto* rejectButton = new ElaPushButton(QStringLiteral("拒绝"), actionSection);
    rejectButton->setObjectName(QStringLiteral("incomingCallRejectButton"));
    rejectButton->setFixedSize(100, 36);
    rejectButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; color:white; border:none; }"
        "QPushButton:hover { background:%2; }")
        .arg(AppStyle::danger(), AppStyle::danger()));
    auto* answerButton = new ElaPushButton(QStringLiteral("接听"), actionSection);
    answerButton->setObjectName(QStringLiteral("incomingCallAnswerButton"));
    answerButton->setFixedSize(100, 36);
    answerButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; color:white; border:none; }"
        "QPushButton:hover { background:%2; }")
        .arg(AppStyle::success(), AppStyle::success()));
    buttonLayout->addStretch();
    buttonLayout->addWidget(rejectButton);
    buttonLayout->addWidget(answerButton);
    buttonLayout->addStretch();
    cardLayout->addWidget(statusSection);
    cardLayout->addWidget(actionSection);
    rootLayout->addWidget(card);

    connect(answerButton, &QAbstractButton::clicked, this, [this]() {
        emit answered(m_callId);
        accept();
    });
    connect(rejectButton, &QAbstractButton::clicked, this, [this]() {
        emit rejected(m_callId);
        reject();
    });

    m_autoRejectTimer = new QTimer(this);
    m_autoRejectTimer->setSingleShot(true);
    m_autoRejectTimer->setInterval(30000);
    connect(m_autoRejectTimer, &QTimer::timeout, this, [this]() {
        emit rejected(m_callId);
        reject();
    });
    m_autoRejectTimer->start();
}
