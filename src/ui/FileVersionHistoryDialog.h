#pragma once
#include "integrations/RemoteFileServiceSettings.h"
#include <ElaDialog.h>

class QLabel;
class QTableWidget;

class FileVersionHistoryDialog : public ElaDialog {
    Q_OBJECT
public:
    explicit FileVersionHistoryDialog(const QString& fileId,
                                      const QString& fileName,
                                      const RemoteFileServiceConnectionSettings& settings,
                                      QWidget* parent = nullptr);

private:
    void loadVersionHistory();
    void downloadVersion(const QString& versionId, const QString& versionLabel);

    QString m_fileId;
    QString m_fileName;
    RemoteFileServiceConnectionSettings m_settings;

    QLabel*       m_statusLabel = nullptr;
    QTableWidget* m_table       = nullptr;
};
