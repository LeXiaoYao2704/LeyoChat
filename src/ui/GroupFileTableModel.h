#pragma once
#include <QAbstractTableModel>
#include <QJsonArray>
#include <QJsonObject>

class GroupFileTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { FileName = 0, FileSize, Uploader, UploadDate, ColumnCount };

    explicit GroupFileTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const override;

    void setFiles(const QJsonArray& files);
    QJsonObject fileAt(int row) const;

    void setSearchFilter(const QString& text);
    void setSortColumn(int column, Qt::SortOrder order);
    void setFolderFilter(const QString& folderId);

private:
    void applyFilters();

    QJsonArray m_allFiles;
    QJsonArray m_filteredFiles;
    QString m_searchText;
    QString m_folderFilter;
    int m_sortColumn = UploadDate;
    Qt::SortOrder m_sortOrder = Qt::DescendingOrder;
};
