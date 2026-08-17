#include "GroupFileManagerDialog.h"
#include "GroupFileTableModel.h"
#include "GroupFileVersionDialog.h"
#include "FilePreviewWidget.h"
#ifdef LEYOCHAT_HAS_WEBENGINE
#include "OnlineEditorWidget.h"
#endif
#include "AppStyle.h"

#include <QButtonGroup>
#include <ElaComboBox.h>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <ElaText.h>
#include <ElaLineEdit.h>
#include <ElaMenu.h>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <ElaPushButton.h>
#include <QStandardPaths>
#include <QDir>
#include <QTableView>
#include <QVBoxLayout>
#include <QTimer>

GroupFileManagerDialog::GroupFileManagerDialog(const QString& groupId,
                                               const GroupFileServiceConfig& config,
                                               const QString& localClientId,
                                               const QString& localDisplayName,
                                               bool isAdmin,
                                               QWidget* parent)
    : ElaDialog(parent)
    , m_groupId(groupId)
    , m_config(config)
    , m_localClientId(localClientId)
    , m_localDisplayName(localDisplayName)
    , m_isAdmin(isAdmin)
{
    setWindowTitle(QStringLiteral("\u7fa4\u6587\u4ef6\u7ba1\u7406"));
    setMinimumSize(800, 500);
    resize(900, 600);
    setStyleSheet(QStringLiteral(
        "QDialog { background:%1; }"
    ).arg(AppStyle::windowBg()));
    m_nam = new QNetworkAccessManager(this);
    m_model = new GroupFileTableModel(this);
    setupUi();
    refreshFolderList();
    refreshFileList();
}

void GroupFileManagerDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(AppStyle::kSpace16, AppStyle::kSpace16,
                                   AppStyle::kSpace16, AppStyle::kSpace12);
    mainLayout->setSpacing(AppStyle::kSpace10);
    buildToolbar();
    buildFolderBar();
    buildFileTable();
    buildStatusBar();
}

void GroupFileManagerDialog::buildToolbar()
{
    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(AppStyle::kSpace8);

    m_uploadBtn = new ElaPushButton(QStringLiteral("\u2b06  \u4e0a\u4f20\u6587\u4ef6"), this);
    m_uploadBtn->setCursor(Qt::PointingHandCursor);
    m_uploadBtn->setFixedHeight(34);
    connect(m_uploadBtn, &QAbstractButton::clicked, this, &GroupFileManagerDialog::onUploadClicked);
    toolbar->addWidget(m_uploadBtn);

    m_createFolderBtn = new ElaPushButton(QStringLiteral("\u2795  \u65b0\u5efa\u6587\u4ef6\u5939"), this);
    m_createFolderBtn->setCursor(Qt::PointingHandCursor);
    m_createFolderBtn->setFixedHeight(34);
    m_createFolderBtn->setVisible(m_isAdmin);
    connect(m_createFolderBtn, &QAbstractButton::clicked, this, &GroupFileManagerDialog::onCreateFolderClicked);
    toolbar->addWidget(m_createFolderBtn);

    toolbar->addStretch();

    const QString inputStyle = QStringLiteral(
        "QLineEdit {"
        "  border:1px solid %1; border-radius:8px; padding:6px 10px;"
        "  background:%2; color:%3; font-size:12px;"
        "}"
        "QLineEdit:focus { border-color:%4; }"
    ).arg(AppStyle::border(), AppStyle::surface(),
          AppStyle::textPrimary(), AppStyle::accent());

    m_searchEdit = new ElaLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("\U0001f50d \u641c\u7d22\u6587\u4ef6\u540d..."));
    m_searchEdit->setFixedWidth(220);
    m_searchEdit->setFixedHeight(34);
    m_searchEdit->setStyleSheet(inputStyle);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &GroupFileManagerDialog::onSearchChanged);
    toolbar->addWidget(m_searchEdit);

    const QString comboStyle = QStringLiteral(
        "QComboBox {"
        "  border:1px solid %1; border-radius:8px; padding:6px 10px;"
        "  background:%2; color:%3; font-size:12px; min-width:90px;"
        "}"
        "QComboBox:hover { border-color:%4; }"
        "QComboBox::drop-down { border:none; width:20px; }"
        "QComboBox QAbstractItemView {"
        "  border:1px solid %1; border-radius:6px;"
        "  background:%2; color:%3; selection-background-color:%5;"
        "}"
    ).arg(AppStyle::border(), AppStyle::surface(),
          AppStyle::textPrimary(), AppStyle::accent(), AppStyle::hoverBg());

    m_sortCombo = new ElaComboBox(this);
    m_sortCombo->setFixedHeight(34);
    m_sortCombo->setStyleSheet(comboStyle);
    m_sortCombo->addItem(QStringLiteral("\u6700\u65b0\u66f4\u65b0"), GroupFileTableModel::UploadDate);
    m_sortCombo->addItem(QStringLiteral("\u6587\u4ef6\u540d"), GroupFileTableModel::FileName);
    m_sortCombo->addItem(QStringLiteral("\u5927\u5c0f"), GroupFileTableModel::FileSize);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GroupFileManagerDialog::onSortChanged);
    toolbar->addWidget(m_sortCombo);

    qobject_cast<QVBoxLayout*>(layout())->addLayout(toolbar);
}

void GroupFileManagerDialog::buildFolderBar()
{
    m_folderBar = new QWidget(this);
    auto* folderLayout = new QHBoxLayout(m_folderBar);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderLayout->setSpacing(AppStyle::kSpace6);
    m_folderGroup = new QButtonGroup(this);
    m_folderGroup->setExclusive(true);
    auto* allBtn = new ElaPushButton(QStringLiteral("\u5168\u90e8"), m_folderBar);
    allBtn->setCheckable(true);
    allBtn->setChecked(true);
    allBtn->setCursor(Qt::PointingHandCursor);
    allBtn->setFixedHeight(30);
    allBtn->setProperty("folderId", QString());
    allBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  border:1px solid %1; border-radius:15px; padding:4px 14px;"
        "  background:%2; color:%3; font-size:12px;"
        "}"
        "QPushButton:hover { background:%4; }"
        "QPushButton:checked { background:%5; color:%6; border-color:transparent; font-weight:600; }"
    ).arg(AppStyle::border(), AppStyle::surface(), AppStyle::textSecondary(),
          AppStyle::hoverBg(), AppStyle::accentSoft(), AppStyle::accent()));
    m_folderGroup->addButton(allBtn);
    folderLayout->addWidget(allBtn);
    folderLayout->addStretch();
    connect(m_folderGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* btn) {
        onFolderSelected(btn->property("folderId").toString());
    });
    qobject_cast<QVBoxLayout*>(layout())->addWidget(m_folderBar);
}

void GroupFileManagerDialog::buildFileTable()
{
    m_tableView = new QTableView(this);
    m_tableView->setModel(m_model);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setShowGrid(false);
    m_tableView->setFrameShape(QFrame::NoFrame);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_tableView->horizontalHeader()->setMinimumSectionSize(80);
    m_tableView->horizontalHeader()->resizeSection(0, 280);
    m_tableView->verticalHeader()->hide();
    m_tableView->verticalHeader()->setDefaultSectionSize(38);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tableView->setStyleSheet(QStringLiteral(
        "QTableView {"
        "  background:%1; alternate-background-color:%2;"
        "  border:1px solid %3; border-radius:8px;"
        "  selection-background-color:%4; selection-color:%5;"
        "  font-size:12px;"
        "}"
        "QTableView::item { padding:4px 8px; }"
        "QTableView::item:hover { background:%6; }"
        "QHeaderView::section {"
        "  background:%2; color:%7; font-size:11px; font-weight:600;"
        "  border:none; border-bottom:1px solid %3; padding:6px 8px;"
        "}"
    ).arg(AppStyle::surface(), AppStyle::surfaceAlt(), AppStyle::border(),
          AppStyle::selectedBg(), AppStyle::textPrimary(),
          AppStyle::hoverBg(), AppStyle::textSecondary()));

    connect(m_tableView, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        if (!index.isValid()) return;
        const auto file = m_model->fileAt(index.row());
        if (file.isEmpty()) return;
        onPreviewFile(file[QStringLiteral("file_id")].toString(),
                      file[QStringLiteral("file_name")].toString());
    });

    connect(m_tableView, &QTableView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const auto index = m_tableView->indexAt(pos);
        if (!index.isValid()) return;
        const auto file = m_model->fileAt(index.row());
        if (file.isEmpty()) return;
        const QString fileId = file["file_id"].toString();
        const QString fileName = file["file_name"].toString();
        const QString uploadedById = file["uploaded_by_id"].toString();
        ElaMenu menu(this);
        menu.addAction(QStringLiteral("\u2b07  \u4e0b\u8f7d"), this, [this, fileId, fileName]() { onDownloadFile(fileId, fileName); });
        menu.addAction(QStringLiteral("👁  预览"), this, [this, fileId, fileName]() { onPreviewFile(fileId, fileName); });
        menu.addAction(QStringLiteral("\U0001f4cb  \u7248\u672c\u5386\u53f2"), this, [this, fileId, fileName]() { onShowVersionHistory(fileId, fileName); });
        menu.addAction(QStringLiteral("\u23f0  设置提醒"), this, [this, fileId, fileName]() {
            const QString trimmedFileId = fileId.trimmed();
            if (trimmedFileId.isEmpty()) {
                return;
            }
            const QString title = fileName.trimmed().isEmpty() ? trimmedFileId : fileName.trimmed();
            emit groupFileReminderRequested(
                m_groupId,
                trimmedFileId,
                fileName,
                QStringLiteral("群文件：%1").arg(title));
        });
#ifdef LEYOCHAT_HAS_WEBENGINE
        {
            const QString ext = QFileInfo(fileName).suffix().toLower();
            if (ext == QStringLiteral("xlsx") || ext == QStringLiteral("docx") || ext == QStringLiteral("pptx") ||
                ext == QStringLiteral("xls")  || ext == QStringLiteral("doc")  || ext == QStringLiteral("ppt") ||
                ext == QStringLiteral("xlsm") || ext == QStringLiteral("xlsb") || ext == QStringLiteral("docm") ||
                ext == QStringLiteral("pptm") || ext == QStringLiteral("odt")  || ext == QStringLiteral("ods") ||
                ext == QStringLiteral("odp")  || ext == QStringLiteral("csv")  || ext == QStringLiteral("rtf") ||
                ext == QStringLiteral("txt")) {
                menu.addAction(QStringLiteral("\u270f\ufe0f  \u5728\u7ebf\u7f16\u8f91"), this,
                    [this, fileId, fileName]() { onEditFile(fileId, fileName); });
            }
        }
#endif
        if (m_isAdmin || uploadedById == m_localClientId) {
            menu.addSeparator();
            menu.addAction(QStringLiteral("\U0001f5d1  \u5220\u9664"), this, [this, fileId]() { onDeleteFile(fileId); });
        }
        menu.exec(m_tableView->viewport()->mapToGlobal(pos));
    });
    qobject_cast<QVBoxLayout*>(layout())->addWidget(m_tableView, 1);
}

void GroupFileManagerDialog::buildStatusBar()
{
    m_statusLabel = new ElaText(this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { color:%1; font-size:11px; padding:2px 4px; }"
    ).arg(AppStyle::textMuted()));
    qobject_cast<QVBoxLayout*>(layout())->addWidget(m_statusLabel);
}

QNetworkReply* GroupFileManagerDialog::apiGet(const QString& path)
{
    QNetworkRequest req(QUrl(m_config.baseUrl + path));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_config.bearerToken).toUtf8());
    return m_nam->get(req);
}

QNetworkReply* GroupFileManagerDialog::apiDelete(const QString& path)
{
    QNetworkRequest req(QUrl(m_config.baseUrl + path));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_config.bearerToken).toUtf8());
    return m_nam->deleteResource(req);
}

QNetworkReply* GroupFileManagerDialog::apiPost(const QString& path, const QJsonObject& body)
{
    QNetworkRequest req(QUrl(m_config.baseUrl + path));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_config.bearerToken).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return m_nam->post(req, QJsonDocument(body).toJson());
}

QNetworkReply* GroupFileManagerDialog::apiPut(const QString& path, const QJsonObject& body)
{
    QNetworkRequest req(QUrl(m_config.baseUrl + path));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_config.bearerToken).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return m_nam->put(req, QJsonDocument(body).toJson());
}

void GroupFileManagerDialog::refreshFileList()
{
    auto* reply = apiGet(QStringLiteral("/api/v1/files?workspaceId=%1").arg(m_groupId));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(QStringLiteral("\u52a0\u8f7d\u6587\u4ef6\u5217\u8868\u5931\u8d25: %1").arg(reply->errorString()));
            return;
        }
        const auto arr = QJsonDocument::fromJson(reply->readAll()).array();
        m_model->setFiles(arr);
        m_statusLabel->setText(QStringLiteral("\u5171 %1 \u4e2a\u6587\u4ef6").arg(arr.size()));
    });
}

void GroupFileManagerDialog::refreshFolderList()
{
    auto* reply = apiGet(QStringLiteral("/api/v1/folders?workspaceId=%1").arg(m_groupId));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const auto arr = QJsonDocument::fromJson(reply->readAll()).array();
        auto* folderLayout = qobject_cast<QHBoxLayout*>(m_folderBar->layout());
        while (m_folderGroup->buttons().size() > 1) {
            auto* btn = m_folderGroup->buttons().last();
            m_folderGroup->removeButton(btn);
            folderLayout->removeWidget(btn);
            delete btn;
        }
        auto* stretch = folderLayout->takeAt(folderLayout->count() - 1);
        delete stretch;
        for (const auto& val : arr) {
            const auto obj = val.toObject();
            auto* btn = new ElaPushButton(obj["folder_name"].toString(), m_folderBar);
            btn->setCheckable(true);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedHeight(30);
            btn->setProperty("folderId", obj["folder_id"].toString());
            btn->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  border:1px solid %1; border-radius:15px; padding:4px 14px;"
                "  background:%2; color:%3; font-size:12px;"
                "}"
                "QPushButton:hover { background:%4; }"
                "QPushButton:checked { background:%5; color:%6; border-color:transparent; font-weight:600; }"
            ).arg(AppStyle::border(), AppStyle::surface(), AppStyle::textSecondary(),
                  AppStyle::hoverBg(), AppStyle::accentSoft(), AppStyle::accent()));
            m_folderGroup->addButton(btn);
            folderLayout->addWidget(btn);
        }
        folderLayout->addStretch();
    });
}

void GroupFileManagerDialog::onUploadClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("\u9009\u62e9\u8981\u4e0a\u4f20\u7684\u6587\u4ef6"));
    if (filePath.isEmpty()) return;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_statusLabel->setText(QStringLiteral("\u65e0\u6cd5\u6253\u5f00\u6587\u4ef6"));
        return;
    }
    const QFileInfo fi(filePath);
    const QString currentFolderId = m_folderGroup->checkedButton()
        ? m_folderGroup->checkedButton()->property("folderId").toString() : QString();
    QNetworkRequest req(QUrl(m_config.baseUrl +
        QStringLiteral("/api/v1/files/%1").arg(fi.fileName())));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_config.bearerToken).toUtf8());
    req.setRawHeader("X-Workspace-Id", m_groupId.toUtf8());
    req.setRawHeader("X-Uploader-Name", m_localDisplayName.toUtf8());
    req.setRawHeader("X-Client-Id", m_localClientId.toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    if (!currentFolderId.isEmpty()) {
        req.setRawHeader("X-Folder-Id", currentFolderId.toUtf8());
    }
    auto* reply = m_nam->put(req, file.readAll());
    m_statusLabel->setText(QStringLiteral("\u6b63\u5728\u4e0a\u4f20 %1...").arg(fi.fileName()));
    connect(reply, &QNetworkReply::finished, this, [this, reply, fi]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(QStringLiteral("\u4e0a\u4f20\u5931\u8d25: %1").arg(reply->errorString()));
            return;
        }
        m_statusLabel->setText(QStringLiteral("\u4e0a\u4f20\u6210\u529f: %1").arg(fi.fileName()));
        refreshFileList();
    });
}

void GroupFileManagerDialog::onCreateFolderClicked()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("\u65b0\u5efa\u6587\u4ef6\u5939"),
        QStringLiteral("\u6587\u4ef6\u5939\u540d\u79f0:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    QJsonObject body;
    body["workspaceId"] = m_groupId;
    body["folderName"] = name.trimmed();
    auto* reply = apiPost(QStringLiteral("/api/v1/folders"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(QStringLiteral("\u521b\u5efa\u6587\u4ef6\u5939\u5931\u8d25: %1").arg(reply->errorString()));
            return;
        }
        refreshFolderList();
    });
}

void GroupFileManagerDialog::onDeleteFile(const QString& fileId)
{
    const auto ret = QMessageBox::question(this, QStringLiteral("\u786e\u8ba4\u5220\u9664"),
        QStringLiteral("\u786e\u5b9a\u8981\u5220\u9664\u8fd9\u4e2a\u6587\u4ef6\u5417\uff1f\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002"));
    if (ret != QMessageBox::Yes) return;
    auto* reply = apiDelete(QStringLiteral("/api/v1/files/%1").arg(fileId));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(QStringLiteral("\u5220\u9664\u5931\u8d25: %1").arg(reply->errorString()));
            return;
        }
        refreshFileList();
    });
}

void GroupFileManagerDialog::onMoveFile(const QString& fileId, const QString& folderId)
{
    QJsonObject body;
    body["folderId"] = folderId;
    auto* reply = apiPut(QStringLiteral("/api/v1/files/%1/folder").arg(fileId), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(QStringLiteral("\u79fb\u52a8\u5931\u8d25: %1").arg(reply->errorString()));
            return;
        }
        refreshFileList();
    });
}

void GroupFileManagerDialog::onShowVersionHistory(const QString& fileId, const QString& fileName)
{
    auto* reply = apiGet(QStringLiteral("/api/v1/files/%1/versions").arg(fileId));
    connect(reply, &QNetworkReply::finished, this, [this, reply, fileName]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(QStringLiteral("\u83b7\u53d6\u7248\u672c\u5386\u53f2\u5931\u8d25"));
            return;
        }
        const auto versions = QJsonDocument::fromJson(reply->readAll()).array();
        auto* dlg = new GroupFileVersionDialog(fileName, versions, this);
        connect(dlg, &GroupFileVersionDialog::downloadVersionRequested,
                this, [this](const QString&, const QString& storagePath) {
            const QString savePath = QFileDialog::getSaveFileName(this, QStringLiteral("\u4fdd\u5b58\u6587\u4ef6"));
            if (savePath.isEmpty()) return;
            auto* downloadReply = apiGet(QStringLiteral("/api/v1/storage/%1").arg(storagePath));
            connect(downloadReply, &QNetworkReply::finished, this, [this, downloadReply, savePath]() {
                downloadReply->deleteLater();
                if (downloadReply->error() != QNetworkReply::NoError) {
                    m_statusLabel->setText(QStringLiteral("\u4e0b\u8f7d\u5931\u8d25"));
                    return;
                }
                QFile out(savePath);
                if (out.open(QIODevice::WriteOnly)) {
                    out.write(downloadReply->readAll());
                    m_statusLabel->setText(QStringLiteral("\u5df2\u4fdd\u5b58\u5230 %1").arg(savePath));
                }
            });
        });
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
}

void GroupFileManagerDialog::onDownloadFile(const QString& fileId, const QString& fileName)
{
    const QString savePath = QFileDialog::getSaveFileName(this, QStringLiteral("\u4fdd\u5b58\u6587\u4ef6"), fileName);
    if (savePath.isEmpty()) return;
    auto* reply = apiGet(QStringLiteral("/api/v1/files/%1/download").arg(fileId));
    m_statusLabel->setText(QStringLiteral("\u6b63\u5728\u4e0b\u8f7d %1...").arg(fileName));
    connect(reply, &QNetworkReply::finished, this, [this, reply, savePath, fileName]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(QStringLiteral("\u4e0b\u8f7d\u5931\u8d25: %1").arg(reply->errorString()));
            return;
        }
        QFile out(savePath);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(reply->readAll());
            m_statusLabel->setText(QStringLiteral("\u5df2\u4fdd\u5b58: %1").arg(fileName));
        }
    });
}

#ifdef LEYOCHAT_HAS_WEBENGINE
void GroupFileManagerDialog::onEditFile(const QString& fileId, const QString& fileName)
{
    QJsonObject body;
    body[QStringLiteral("fileId")] = fileId;
    body[QStringLiteral("clientId")] = m_localClientId;
    body[QStringLiteral("displayName")] = m_localDisplayName;

    auto* reply = apiPost(QStringLiteral("/api/v1/wopi-tokens"), body);
    m_statusLabel->setText(QStringLiteral("正在获取编辑令牌..."));
    connect(reply, &QNetworkReply::finished, this, [this, reply, fileId, fileName]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(QStringLiteral("无法获取编辑令牌: %1").arg(reply->errorString()));
            return;
        }
        const auto resp = QJsonDocument::fromJson(reply->readAll()).object();
        const QString accessToken = resp[QStringLiteral("access_token")].toString();
        if (accessToken.isEmpty()) {
            m_statusLabel->setText(QStringLiteral("编辑令牌为空"));
            return;
        }

        const QString editorUrl = m_config.baseUrl
            + QStringLiteral("/editor/") + fileId
            + QStringLiteral("?access_token=") + accessToken;

        auto* editor = new OnlineEditorWidget(editorUrl, fileName, this);
        editor->setWindowFlag(Qt::Window);
        connect(editor, &OnlineEditorWidget::editorClosed,
                this, &GroupFileManagerDialog::refreshFileList);
        editor->show();
        editor->raise();
        editor->activateWindow();
        m_statusLabel->setText(QStringLiteral("已打开编辑器: %1").arg(fileName));
    });
}
#endif

void GroupFileManagerDialog::onFolderSelected(const QString& folderId)
{
    m_model->setFolderFilter(folderId);
}

void GroupFileManagerDialog::onSearchChanged(const QString& text)
{
    m_model->setSearchFilter(text);
}

void GroupFileManagerDialog::onSortChanged(int index)
{
    const int col = m_sortCombo->itemData(index).toInt();
    m_model->setSortColumn(col, col == GroupFileTableModel::UploadDate
        ? Qt::DescendingOrder : Qt::AscendingOrder);
}

void GroupFileManagerDialog::onPreviewFile(const QString& fileId, const QString& fileName)
{
    const auto type = FilePreviewWidget::detectPreviewType(fileName);

    if (type == FilePreviewWidget::Unsupported) {
        QMessageBox::information(this, QStringLiteral("预览"),
            QStringLiteral("该文件不支持预览: %1").arg(fileName));
        return;
    }

    // Office 格式且服务在线 → ONLYOFFICE 只读模式
#ifdef LEYOCHAT_HAS_WEBENGINE
    if (type == FilePreviewWidget::Office && m_config.enabled) {
        QJsonObject body;
        body[QStringLiteral("fileId")] = fileId;
        body[QStringLiteral("clientId")] = m_localClientId;
        body[QStringLiteral("displayName")] = m_localDisplayName;
        auto* reply = apiPost(QStringLiteral("/api/v1/wopi-tokens"), body);
        m_statusLabel->setText(QStringLiteral("正在获取预览令牌..."));
        // 为 WOPI 令牌请求设置 10 秒超时，避免网络不稳定导致长期等待
        auto* timeoutTimer = new QTimer(this);
        timeoutTimer->setInterval(10000); // 10 秒
        timeoutTimer->setSingleShot(true);
        connect(timeoutTimer, &QTimer::timeout, this, [this, reply, fileId, fileName]() {
            if (reply->isRunning()) {
                reply->abort();
                m_statusLabel->setText(QStringLiteral("令牌获取超时，尝试下载预览"));
                downloadAndPreview(fileId, fileName);
            }
        });
        timeoutTimer->start();

        connect(reply, &QNetworkReply::finished, this, [this, reply, fileId, fileName, timeoutTimer]() {
            timeoutTimer->stop();
            timeoutTimer->deleteLater();
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                m_statusLabel->setText(QStringLiteral("获取令牌失败，尝试下载预览"));
                downloadAndPreview(fileId, fileName);
                return;
            }
            const auto resp = QJsonDocument::fromJson(reply->readAll()).object();
            const QString token = resp[QStringLiteral("access_token")].toString();
            if (token.isEmpty()) {
                downloadAndPreview(fileId, fileName);
                return;
            }
            const QString url = m_config.baseUrl
                + QStringLiteral("/editor/") + fileId
                + QStringLiteral("?access_token=") + token
                + QStringLiteral("&mode=view");
            auto* preview = FilePreviewWidget::fromOnlyOffice(url, fileName, this);
            connect(preview, &FilePreviewWidget::editRequested, this, [this, preview, fileId, fileName]() {
                preview->close();
                onEditFile(fileId, fileName);
            });
            preview->show();
            preview->raise();
            preview->activateWindow();
            m_statusLabel->setText(QStringLiteral("已打开预览: %1").arg(fileName));
        });
        return;
    }
#endif

    // 其他格式 → 下载后预览
    downloadAndPreview(fileId, fileName);
}

void GroupFileManagerDialog::downloadAndPreview(const QString& fileId, const QString& fileName)
{
    m_statusLabel->setText(QStringLiteral("正在下载 %1 ...").arg(fileName));
    const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                           + QStringLiteral("/leyochat-preview");
    QDir().mkpath(tmpDir);
    const QString tmpPath = tmpDir + QStringLiteral("/") + fileName;

    auto* reply = apiGet(QStringLiteral("/api/v1/files/") + fileId + QStringLiteral("/download"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, tmpPath, fileName]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(QStringLiteral("下载失败: %1").arg(reply->errorString()));
            return;
        }
        QFile file(tmpPath);
        if (!file.open(QIODevice::WriteOnly)) {
            m_statusLabel->setText(QStringLiteral("无法写入临时文件"));
            return;
        }
        file.write(reply->readAll());
        file.close();

        auto* preview = FilePreviewWidget::fromLocalFile(tmpPath, fileName, this);
        preview->show();
        preview->raise();
        preview->activateWindow();
        m_statusLabel->setText(QStringLiteral("已打开预览: %1").arg(fileName));
    });
}
