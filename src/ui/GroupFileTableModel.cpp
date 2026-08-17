#include "ui/GroupFileTableModel.h"
#include <QDateTime>
#include <algorithm>
#include <vector>

static QString formatFileSize(qint64 bytes)
{
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024) return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 1);
}

GroupFileTableModel::GroupFileTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int GroupFileTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_filteredFiles.size();
}

int GroupFileTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant GroupFileTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredFiles.size())
        return {};

    if (role != Qt::DisplayRole)
        return {};

    const QJsonObject obj = m_filteredFiles[index.row()].toObject();
    switch (index.column()) {
    case FileName:
        return obj["file_name"].toString();
    case FileSize:
        return formatFileSize(obj["file_size"].toInteger());
    case Uploader:
        return obj["uploaded_by_name"].toString();
    case UploadDate:
        return QDateTime::fromMSecsSinceEpoch(obj["updated_at_ms"].toInteger())
            .toString("yyyy-MM-dd HH:mm");
    default:
        return {};
    }
}

QVariant GroupFileTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case FileName:   return QStringLiteral("文件名");
    case FileSize:   return QStringLiteral("大小");
    case Uploader:   return QStringLiteral("上传者");
    case UploadDate: return QStringLiteral("更新时间");
    default:         return {};
    }
}

void GroupFileTableModel::setFiles(const QJsonArray& files)
{
    beginResetModel();
    m_allFiles = files;
    applyFilters();
    endResetModel();
}

QJsonObject GroupFileTableModel::fileAt(int row) const
{
    if (row < 0 || row >= m_filteredFiles.size())
        return {};
    return m_filteredFiles[row].toObject();
}

void GroupFileTableModel::setSearchFilter(const QString& text)
{
    beginResetModel();
    m_searchText = text;
    applyFilters();
    endResetModel();
}

void GroupFileTableModel::setSortColumn(int column, Qt::SortOrder order)
{
    beginResetModel();
    m_sortColumn = column;
    m_sortOrder = order;
    applyFilters();
    endResetModel();
}

void GroupFileTableModel::setFolderFilter(const QString& folderId)
{
    beginResetModel();
    m_folderFilter = folderId;
    applyFilters();
    endResetModel();
}

void GroupFileTableModel::applyFilters()
{
    std::vector<QJsonValue> filtered;
    filtered.reserve(m_allFiles.size());

    for (const auto& val : m_allFiles) {
        const QJsonObject obj = val.toObject();

        if (!m_folderFilter.isEmpty()) {
            if (obj["folder_id"].toString() != m_folderFilter)
                continue;
        }

        if (!m_searchText.isEmpty()) {
            if (!obj["file_name"].toString().contains(m_searchText, Qt::CaseInsensitive))
                continue;
        }

        filtered.push_back(val);
    }

    std::sort(filtered.begin(), filtered.end(),
        [this](const QJsonValue& a, const QJsonValue& b) {
            const QJsonObject oa = a.toObject();
            const QJsonObject ob = b.toObject();
            bool lessThan = false;

            switch (m_sortColumn) {
            case FileName:
                lessThan = oa["file_name"].toString() < ob["file_name"].toString();
                break;
            case FileSize:
                lessThan = oa["file_size"].toInteger() < ob["file_size"].toInteger();
                break;
            case Uploader:
                lessThan = oa["uploaded_by_name"].toString() < ob["uploaded_by_name"].toString();
                break;
            case UploadDate:
            default:
                lessThan = oa["updated_at_ms"].toInteger() < ob["updated_at_ms"].toInteger();
                break;
            }

            return m_sortOrder == Qt::AscendingOrder ? lessThan : !lessThan;
        });

    m_filteredFiles = QJsonArray();
    for (const auto& val : filtered)
        m_filteredFiles.append(val);
}
