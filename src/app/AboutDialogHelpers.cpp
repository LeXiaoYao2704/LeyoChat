#include "app/AboutDialogHelpers.h"

#include "app/ApplicationInfo.h"
#include "app/DocumentDialogHelpers.h"
#include "ui/AppStyle.h"

#include <ElaFrame.h>

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

void showCurrentReleaseNotesDialog(QWidget* parent,
                                   const QString& title,
                                   const QString& subtitle)
{
    showDocumentDialog(parent,
                       title,
                       subtitle,
                       ApplicationInfo::releaseNotesText(),
                       false);
}

void showAboutDialogWindow(QWidget* parent, const QString& appDisplayName)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("关于 %1").arg(appDisplayName));
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.resize(560, 360);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background:%1; }"
        "QFrame#aboutCard { background:%2; border:1px solid %3; border-radius:14px; }"
        "QLabel#aboutBadge { background:%4; color:%5; border-radius:10px; padding:3px 10px; font-size:11px; font-weight:600; }"
        "QLabel#aboutTitle { color:%6; font-size:22px; font-weight:700; }"
        "QLabel#aboutMeta { color:%7; font-size:13px; }"
        "QLabel#aboutBody { color:%6; font-size:13px; line-height:1.5; }"
        "QPushButton#aboutPrimary { background:%8; color:#FFFFFF; border:none; border-radius:8px; padding:8px 18px; font-size:13px; font-weight:600; }"
        "QPushButton#aboutPrimary:hover { background:%9; }"
        "QPushButton#aboutGhost { background:%2; color:%6; border:1px solid %3; border-radius:8px; padding:8px 18px; font-size:13px; }"
        "QPushButton#aboutGhost:hover { background:%10; }")
        .arg(AppStyle::surface(),
             AppStyle::surfaceAlt(),
             AppStyle::border(),
             AppStyle::hoverBg(),
             AppStyle::accent(),
             AppStyle::textPrimary(),
             AppStyle::textSecondary(),
             AppStyle::accent(),
             AppStyle::accentHover(),
             AppStyle::surfaceAlt()));

    auto* rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(18, 18, 18, 18);

    auto* card = new ElaFrame(&dialog);
    card->setObjectName(QStringLiteral("aboutCard"));
    rootLayout->addWidget(card);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 22, 22, 22);
    cardLayout->setSpacing(12);

    auto* badge = new QLabel(QStringLiteral("Windows 桌面版"), card);
    badge->setObjectName(QStringLiteral("aboutBadge"));
    badge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    auto* titleLabel = new QLabel(appDisplayName, card);
    titleLabel->setObjectName(QStringLiteral("aboutTitle"));
    auto* metaLabel = new QLabel(QStringLiteral("版本 %1 · %2")
                                     .arg(ApplicationInfo::currentVersion(),
                                          ApplicationInfo::companyName()),
                                 card);
    metaLabel->setObjectName(QStringLiteral("aboutMeta"));
    auto* bodyLabel = new QLabel(
        QStringLiteral("LeyoChat 用来承载局域网沟通、群协作和后续扩展能力。"
                       "程序内可以随时查看本版更新，方便定位问题与确认修复。"),
        card);
    bodyLabel->setObjectName(QStringLiteral("aboutBody"));
    bodyLabel->setWordWrap(true);

    auto* copyrightLabel = new QLabel(
        QStringLiteral("Copyright (C) 2026 LeXiaoYao2704"), card);
    copyrightLabel->setObjectName(QStringLiteral("aboutMeta"));

    auto* notesButton = new QPushButton(QStringLiteral("本版更新"), card);
    notesButton->setObjectName(QStringLiteral("aboutPrimary"));
    auto* closeButton = new QPushButton(QStringLiteral("关闭"), card);
    closeButton->setObjectName(QStringLiteral("aboutGhost"));

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 6, 0, 0);
    buttonRow->setSpacing(10);
    buttonRow->addWidget(notesButton);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);

    cardLayout->addWidget(badge, 0, Qt::AlignLeft);
    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(metaLabel);
    cardLayout->addWidget(bodyLabel);
    cardLayout->addWidget(copyrightLabel);
    cardLayout->addStretch();
    cardLayout->addLayout(buttonRow);

    QObject::connect(notesButton, &QPushButton::clicked, &dialog, [&]() {
        showCurrentReleaseNotesDialog(&dialog,
                                      QStringLiteral("%1 本版更新").arg(appDisplayName),
                                      QStringLiteral("当前版本 %1")
                                          .arg(ApplicationInfo::currentVersion()));
    });
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}
