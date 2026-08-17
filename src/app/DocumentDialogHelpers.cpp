#include "app/DocumentDialogHelpers.h"

#include "ui/AppStyle.h"
#include "ui/MarkdownRenderer.h"

#include <ElaFrame.h>

#include <QDialog>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

void showDocumentDialog(QWidget* parent,
                        const QString& title,
                        const QString& subtitle,
                        const QString& content,
                        bool markdown)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.resize(760, 560);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background:%1; }"
        "QFrame#documentCard { background:%2; border:1px solid %3; border-radius:14px; }"
        "QLabel#documentTitle { color:%4; font-size:20px; font-weight:700; }"
        "QLabel#documentSubtitle { color:%5; font-size:12px; }"
        "QTextBrowser { background:%2; color:%4; border:1px solid %3; border-radius:10px; padding:8px; }"
        "QPushButton#documentPrimary { background:%6; color:#FFFFFF; border:none; border-radius:8px; padding:8px 18px; font-size:13px; font-weight:600; }"
        "QPushButton#documentPrimary:hover { background:%7; }")
        .arg(AppStyle::surface(),
             AppStyle::surfaceAlt(),
             AppStyle::border(),
             AppStyle::textPrimary(),
             AppStyle::textSecondary(),
             AppStyle::accent(),
             AppStyle::accentHover()));

    auto* rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(18, 18, 18, 18);

    auto* card = new ElaFrame(&dialog);
    card->setObjectName(QStringLiteral("documentCard"));
    rootLayout->addWidget(card);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    cardLayout->setSpacing(12);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("documentTitle"));
    auto* subtitleLabel = new QLabel(subtitle, card);
    subtitleLabel->setObjectName(QStringLiteral("documentSubtitle"));
    subtitleLabel->setWordWrap(true);
    auto* browser = new QTextBrowser(card);
    browser->setOpenExternalLinks(true);
    browser->setReadOnly(true);
    if (markdown) {
        browser->setHtml(MarkdownRenderer::renderMarkdownToHtml(
            content, {.emptyPlaceholder = QStringLiteral("暂无更新记录。")}));
    } else {
        browser->setPlainText(content.trimmed().isEmpty()
                                  ? QStringLiteral("暂无更新说明。")
                                  : content);
    }
    auto* closeButton = new QPushButton(QStringLiteral("关闭"), card);
    closeButton->setObjectName(QStringLiteral("documentPrimary"));

    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(subtitleLabel);
    cardLayout->addWidget(browser, 1);
    cardLayout->addWidget(closeButton, 0, Qt::AlignRight);

    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}
