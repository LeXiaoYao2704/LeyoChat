#include "ui/WorkbenchPage.h"

#include <QFrame>
#include <ElaFrame.h>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

QWidget* WorkbenchPage::createFeatureCard(const QString& key,
                                          const QString& title,
                                          const QString& description,
                                          QWidget* parent)
{
    auto* card = new ElaFrame(parent);
    card->setObjectName(QStringLiteral("workbenchCard_%1").arg(key));

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(6);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("workbenchCardTitle_%1").arg(key));

    auto* descLabel = new QLabel(description, card);
    descLabel->setObjectName(QStringLiteral("workbenchCardDesc_%1").arg(key));
    descLabel->setWordWrap(true);

    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(descLabel);

    m_featureCards.insert(key, card);
    return card;
}

WorkbenchPage::WorkbenchPage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("workbenchPage"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(QStringLiteral("工作台"), this);
    titleLabel->setObjectName(QStringLiteral("workbenchPageTitle"));

    auto* summaryLabel = new QLabel(QStringLiteral("企业入口将统一收敛到这里。"), this);
    summaryLabel->setObjectName(QStringLiteral("workbenchPageSummary"));
    summaryLabel->setWordWrap(true);

    auto* cardsGrid = new QGridLayout();
    cardsGrid->setHorizontalSpacing(12);
    cardsGrid->setVerticalSpacing(12);

    cardsGrid->addWidget(createFeatureCard(QStringLiteral("knowledge"),
                                           QStringLiteral("知识库"),
                                           QStringLiteral("沉淀问答与知识检索入口。"),
                                           this),
                         0,
                         0);
    cardsGrid->addWidget(createFeatureCard(QStringLiteral("devops"),
                                           QStringLiteral("Azure DevOps"),
                                           QStringLiteral("需求、缺陷与构建追踪入口。"),
                                           this),
                         0,
                         1);
    cardsGrid->addWidget(createFeatureCard(QStringLiteral("outlook"),
                                           QStringLiteral("Outlook"),
                                           QStringLiteral("邮件通知与会话上下文聚合入口。"),
                                           this),
                         1,
                         0);
    cardsGrid->addWidget(createFeatureCard(QStringLiteral("ferry"),
                                           QStringLiteral("摆渡"),
                                           QStringLiteral("跨域文件流转与留痕入口。"),
                                           this),
                         1,
                         1);

    layout->addWidget(titleLabel);
    layout->addWidget(summaryLabel);
    layout->addLayout(cardsGrid);
    layout->addStretch();
}

bool WorkbenchPage::hasCardForTesting(const QString& key) const
{
    return m_featureCards.contains(key) && m_featureCards.value(key) != nullptr;
}