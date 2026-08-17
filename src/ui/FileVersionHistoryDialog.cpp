#include "ui/FileVersionHistoryDialog.h"
#include "integrations/RemoteFileServiceAdapter.h"
#include "integrations/RemoteFileServiceContracts.h"

#include "ui/AppStyle.h"
#include "ui/LeyoDialog.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <ElaPushButton.h>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>

FileVersionHistoryDialog::FileVersionHistoryDialog(
    const QString& fileId,
    const QString& fileName,
    const RemoteFileServiceConnectionSettings& settings,
    QWidget* parent)
    : ElaDialog(parent)
    , m_fileId(fileId)
    , m_fileName(fileName)
    , m_settings(settings)
{
    LeyoDialog::applySecondaryDialogScaffold(this, QStringLiteral("fileVersionHistoryDialog"));
    setWindowTitle(QStringLiteral("版本历史 — %1").arg(fileName));
    setMinimumWidth(640);
    setMinimumHeight(360);
    setStyleSheet(QStringLiteral("QDialog { background:%1; }").arg(AppStyle::windowBg()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(AppStyle::kSpace16, AppStyle::kSpace16,
                               AppStyle::kSpace16, AppStyle::kSpace12);
    layout->setSpacing(AppStyle::kSpace12);

    auto* toolbar = LeyoDialog::createSectionFrame(this, QStringLiteral("fileVersionToolbar"));
    toolbar->setStyleSheet(QStringLiteral(
        "QFrame#fileVersionToolbar {"
        "  background:%1;"
        "  border:1px solid %2;"
        "  border-radius:12px;"
        "}")
        .arg(AppStyle::surface(), AppStyle::border()));
    auto* toolbarLayout = new QVBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(AppStyle::kSpace16, AppStyle::kSpace14,
                                      AppStyle::kSpace16, AppStyle::kSpace14);
    toolbarLayout->setSpacing(AppStyle::kSpace6);

    auto* titleLabel = new QLabel(QStringLiteral("<b>%1</b>").arg(fileName.toHtmlEscaped()), toolbar);
    titleLabel->setStyleSheet(QStringLiteral("color:%1; font-size:15px;").arg(AppStyle::textPrimary()));
    toolbarLayout->addWidget(titleLabel);

    m_statusLabel = new QLabel(QStringLiteral("正在加载版本历史…"), toolbar);
    m_statusLabel->setStyleSheet(QStringLiteral("color:%1; font-size:12px;").arg(AppStyle::textMuted()));
    toolbarLayout->addWidget(m_statusLabel);
    layout->addWidget(toolbar);

    auto* contentSection = LeyoDialog::createSectionFrame(this, QStringLiteral("fileVersionHistoryDialogContentSection"));
    auto* contentLayout = new QVBoxLayout(contentSection);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_table = new QTableWidget(0, 5, contentSection);
    m_table->setObjectName(QStringLiteral("fileVersionTable"));
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("版本"), QStringLiteral("上传人"), QStringLiteral("时间"),
         QStringLiteral("说明"), QStringLiteral("下载")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->hide();
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    contentLayout->addWidget(m_table);
    layout->addWidget(contentSection, 1);

    auto* closeButton = new ElaPushButton(QStringLiteral("关闭"), this);
    closeButton->setObjectName(QStringLiteral("fileVersionCloseButton"));
    auto* buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);
    QObject::connect(closeButton, &QAbstractButton::clicked, this, &QDialog::reject);
    layout->addLayout(buttonRow);

    loadVersionHistory();
}

void FileVersionHistoryDialog::loadVersionHistory()
{
    const QString fileId  = m_fileId;
    const RemoteFileServiceConnectionSettings settings = m_settings;

    QtConcurrent::run([fileId, settings]() -> QVector<RemoteFileVersion> {
        RemoteFileServiceAdapter adapter(settings);
        QString err;
        return adapter.getVersionHistory(fileId, &err);
    }).then(this, [this](QVector<RemoteFileVersion> versions) {
        if (versions.isEmpty()) {
            m_statusLabel->setText(QStringLiteral("暂无版本记录"));
            return;
        }
        m_statusLabel->setText(QStringLiteral("共 %1 个版本").arg(versions.size()));
        m_table->setRowCount(versions.size());
        for (int row = 0; row < versions.size(); ++row) {
            const RemoteFileVersion& v = versions[row];
            m_table->setItem(row, 0, new QTableWidgetItem(v.versionLabel));
            const QString uploader = v.uploaderName.trimmed().isEmpty() ? v.uploaderId : v.uploaderName;
            m_table->setItem(row, 1, new QTableWidgetItem(uploader));
            const QString dateStr = v.uploadedAtMs > 0
                ? QDateTime::fromMSecsSinceEpoch(v.uploadedAtMs).toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                : QStringLiteral("—");
            m_table->setItem(row, 2, new QTableWidgetItem(dateStr));
            m_table->setItem(row, 3, new QTableWidgetItem(v.changeNote));

            auto* btn = new ElaPushButton(QStringLiteral("下载"), this);
            const QString versionId    = v.versionId;
            const QString versionLabel = v.versionLabel;
            QObject::connect(btn, &QAbstractButton::clicked, this, [this, versionId, versionLabel]() {
                downloadVersion(versionId, versionLabel);
            });
            m_table->setCellWidget(row, 4, btn);
        }
    });
}

void FileVersionHistoryDialog::downloadVersion(const QString& versionId, const QString& versionLabel)
{
    const QString fileId   = m_fileId;
    const QString fileName = m_fileName;
    const RemoteFileServiceConnectionSettings settings = m_settings;
    const QString saveDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);

    m_statusLabel->setText(QStringLiteral("正在下载 %1…").arg(versionLabel));

    QtConcurrent::run([fileId, versionId, fileName, saveDir, settings]() -> std::pair<QString, QString> {
        RemoteFileServiceAdapter adapter(settings);
        QString err;
        const auto path = adapter.downloadVersion(fileId, versionId, fileName, saveDir, &err);
        return {path.value_or(QString{}), err};
    }).then(this, [this, versionLabel](std::pair<QString, QString> result) {
        if (!result.first.isEmpty()) {
            m_statusLabel->setText(QStringLiteral("%1 已下载").arg(versionLabel));
            QDesktopServices::openUrl(QUrl::fromLocalFile(result.first));
        } else {
            m_statusLabel->setText(QStringLiteral("下载失败：%1").arg(result.second));
        }
    });
}
