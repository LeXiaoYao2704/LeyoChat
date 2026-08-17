#include "GroupFileVersionDialog.h"
#include "AppStyle.h"
#include "LeyoDialog.h"

#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <ElaText.h>
#include <ElaPushButton.h>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QDateTime>
#include <QJsonObject>

GroupFileVersionDialog::GroupFileVersionDialog(const QString& fileName,
                                               const QJsonArray& versions,
                                               QWidget* parent)
    : ElaDialog(parent)
{
    LeyoDialog::applySecondaryDialogScaffold(this, QStringLiteral("groupFileVersionDialog"));
    setWindowTitle(QStringLiteral("%1 \u2014 \u7248\u672c\u5386\u53f2").arg(fileName));
    setMinimumSize(620, 340);
    setStyleSheet(QStringLiteral(
        "QDialog { background:%1; }"
    ).arg(AppStyle::windowBg()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(AppStyle::kSpace16, AppStyle::kSpace16,
                               AppStyle::kSpace16, AppStyle::kSpace12);
    layout->setSpacing(AppStyle::kSpace10);

    auto* toolbar = LeyoDialog::createSectionFrame(this, QStringLiteral("groupFileVersionToolbar"));
    toolbar->setStyleSheet(QStringLiteral(
        "QFrame#groupFileVersionToolbar {"
        "  background:%1;"
        "  border:1px solid %2;"
        "  border-radius:12px;"
        "}")
        .arg(AppStyle::surface(), AppStyle::border()));
    auto* toolbarLayout = new QVBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(AppStyle::kSpace16, AppStyle::kSpace14,
                                      AppStyle::kSpace16, AppStyle::kSpace14);
    toolbarLayout->setSpacing(AppStyle::kSpace6);

    auto* titleLabel = new ElaText(QStringLiteral("\U0001f4c4  %1").arg(fileName), toolbar);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { font-weight:600; font-size:14px; color:%1; }"
    ).arg(AppStyle::textPrimary()));
    toolbarLayout->addWidget(titleLabel);

    auto* summaryLabel = new ElaText(QStringLiteral("共 %1 个版本").arg(versions.size()), toolbar);
    summaryLabel->setStyleSheet(QStringLiteral(
        "QLabel { font-size:12px; color:%1; }"
    ).arg(AppStyle::textMuted()));
    toolbarLayout->addWidget(summaryLabel);
    layout->addWidget(toolbar);

    m_table = new QTableWidget(versions.size(), 4, this);
    m_table->setObjectName(QStringLiteral("groupFileVersionTable"));
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("\u7248\u672c\u53f7"),
        QStringLiteral("\u4e0a\u4f20\u8005"),
        QStringLiteral("\u65e5\u671f"),
        QStringLiteral("\u64cd\u4f5c")
    });
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(36);
    m_table->setStyleSheet(QStringLiteral(
        "QTableWidget {"
        "  background:%1; alternate-background-color:%2;"
        "  border:1px solid %3; border-radius:8px;"
        "  selection-background-color:%4; selection-color:%5; font-size:12px;"
        "}"
        "QTableWidget::item { padding:4px 8px; }"
        "QTableWidget::item:hover { background:%6; }"
        "QHeaderView::section {"
        "  background:%2; color:%7; font-size:11px; font-weight:600;"
        "  border:none; border-bottom:1px solid %3; padding:6px 8px;"
        "}"
    ).arg(AppStyle::surface(), AppStyle::surfaceAlt(), AppStyle::border(),
          AppStyle::selectedBg(), AppStyle::textPrimary(),
          AppStyle::hoverBg(), AppStyle::textSecondary()));

    const QString dlBtnStyle = QStringLiteral(
        "QPushButton {"
        "  border:none; border-radius:6px; padding:4px 12px;"
        "  background:%1; color:%2; font-size:11px; font-weight:600;"
        "}"
        "QPushButton:hover { background:%3; }"
    ).arg(AppStyle::accentSoft(), AppStyle::accent(), AppStyle::selectedBg());

    for (int i = 0; i < versions.size(); ++i) {
        const auto obj = versions[i].toObject();
        const int ver = obj["version_number"].toInt();
        const QString uploadedBy = obj["uploader_name"].toString();
        const qint64 dateMs = obj["uploaded_at_ms"].toInteger();
        const QString versionId = obj["version_id"].toString();
        const QString storagePath = obj["storage_path"].toString();

        auto* verItem = new QTableWidgetItem(QStringLiteral("v%1").arg(ver));
        verItem->setForeground(QColor(AppStyle::textPrimary()));
        m_table->setItem(i, 0, verItem);

        auto* uploaderItem = new QTableWidgetItem(uploadedBy);
        uploaderItem->setForeground(QColor(AppStyle::textSecondary()));
        m_table->setItem(i, 1, uploaderItem);

        auto* dateItem = new QTableWidgetItem(
            QDateTime::fromMSecsSinceEpoch(dateMs).toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        dateItem->setForeground(QColor(AppStyle::textMuted()));
        m_table->setItem(i, 2, dateItem);

        auto* downloadBtn = new ElaPushButton(QStringLiteral("\u2b07  \u4e0b\u8f7d"), m_table);
        downloadBtn->setCursor(Qt::PointingHandCursor);
        downloadBtn->setFixedHeight(28);
        downloadBtn->setStyleSheet(dlBtnStyle);
        connect(downloadBtn, &QAbstractButton::clicked, this, [this, versionId, storagePath]() {
            emit downloadVersionRequested(versionId, storagePath);
        });
        m_table->setCellWidget(i, 3, downloadBtn);
    }

    m_table->resizeColumnsToContents();
    layout->addWidget(m_table);

    auto* closeButton = new ElaPushButton(QStringLiteral("关闭"), this);
    closeButton->setObjectName(QStringLiteral("groupFileVersionCloseButton"));
    auto* actionRow = new QHBoxLayout;
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->addStretch();
    actionRow->addWidget(closeButton);
    connect(closeButton, &QAbstractButton::clicked, this, &QDialog::reject);
    layout->addLayout(actionRow);
}
