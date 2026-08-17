#pragma once
#include <ElaDialog.h>
#include <QJsonArray>
class QTableWidget;

class GroupFileVersionDialog : public ElaDialog {
    Q_OBJECT
public:
    GroupFileVersionDialog(const QString& fileName, const QJsonArray& versions, QWidget* parent = nullptr);

signals:
    void downloadVersionRequested(const QString& versionId, const QString& storagePath);

private:
    QTableWidget* m_table = nullptr;
};
