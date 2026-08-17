#pragma once
#include <ElaDialog.h>
#include <QJsonObject>
#include "integrations/RemoteFileServiceSettings.h"

class ElaComboBox;
class ElaLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class ElaPushButton;
class QTableView;
class ElaText;
class QButtonGroup;
class GroupFileTableModel;

class GroupFileManagerDialog : public ElaDialog {
    Q_OBJECT
public:
    GroupFileManagerDialog(const QString& groupId,
                          const GroupFileServiceConfig& config,
                          const QString& localClientId,
                          const QString& localDisplayName,
                          bool isAdmin,
                          QWidget* parent = nullptr);

signals:
    void groupFileReminderRequested(const QString& groupId,
                                    const QString& resourceId,
                                    const QString& fileName,
                                    const QString& previewSnapshot);

private slots:
    void refreshFileList();
    void refreshFolderList();
    void onUploadClicked();
    void onCreateFolderClicked();
    void onDeleteFile(const QString& fileId);
    void onMoveFile(const QString& fileId, const QString& folderId);
    void onShowVersionHistory(const QString& fileId, const QString& fileName);
    void onDownloadFile(const QString& fileId, const QString& fileName);
    void onPreviewFile(const QString& fileId, const QString& fileName);
    void downloadAndPreview(const QString& fileId, const QString& fileName);
#ifdef LEYOCHAT_HAS_WEBENGINE
    void onEditFile(const QString& fileId, const QString& fileName);
#endif
    void onFolderSelected(const QString& folderId);
    void onSearchChanged(const QString& text);
    void onSortChanged(int index);

private:
    void setupUi();
    void buildToolbar();
    void buildFolderBar();
    void buildFileTable();
    void buildStatusBar();
    QNetworkReply* apiGet(const QString& path);
    QNetworkReply* apiDelete(const QString& path);
    QNetworkReply* apiPost(const QString& path, const QJsonObject& body);
    QNetworkReply* apiPut(const QString& path, const QJsonObject& body);

    QString m_groupId;
    GroupFileServiceConfig m_config;
    QString m_localClientId;
    QString m_localDisplayName;
    bool m_isAdmin;

    QNetworkAccessManager* m_nam;
    GroupFileTableModel* m_model;

    ElaPushButton* m_uploadBtn;
    ElaPushButton* m_createFolderBtn;
    ElaLineEdit* m_searchEdit;
    ElaComboBox* m_sortCombo;
    QWidget* m_folderBar;
    QButtonGroup* m_folderGroup;
    QTableView* m_tableView;
    ElaText* m_statusLabel;
};
