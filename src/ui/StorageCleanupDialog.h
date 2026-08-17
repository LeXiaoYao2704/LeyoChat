#pragma once

#include <ElaDialog.h>
#include <QDateTime>
#include <QList>
#include <QString>

class ElaCheckBox;
class ElaPushButton;
class ElaText;
class QTableWidget;

/// 存储清理的数据分类
enum class StorageCategory {
    Logs,
    Messages,
    Files,
    Images
};

/// 单条可清理项的描述
struct CleanupItem {
    QString name;       // 显示名称 (如文件名或会话标题)
    QString path;       // 磁盘路径 (空表示数据库记录)
    qint64 sizeBytes;   // 大小(字节)
    QDateTime lastModified;
    bool selected{false};
};

/// 分类存储管理对话框
/// 仿微信风格：显示该分类下所有项目列表，用户可勾选后批量删除
class StorageCleanupDialog : public ElaDialog {
    Q_OBJECT
public:
    explicit StorageCleanupDialog(StorageCategory category,
                                  const QList<CleanupItem>& items,
                                  QWidget* parent = nullptr);

    /// 返回用户勾选的项的索引
    QList<int> selectedIndices() const;

Q_SIGNALS:
    /// 用户确认删除所选项
    void deleteRequested(StorageCategory category, QList<int> indices);

private:
    void setupUi();
    void applyTableStyle();
    void updateSelectionSummary();
    void syncSelectAllState();
    QString categoryTitle() const;
    static QString formatSize(qint64 bytes);

    StorageCategory category_;
    QList<CleanupItem> items_;

    QTableWidget* tableWidget_{nullptr};
    ElaCheckBox* selectAllCheck_{nullptr};
    ElaText* summaryLabel_{nullptr};
    ElaPushButton* deleteBtn_{nullptr};
    ElaPushButton* cancelBtn_{nullptr};
};
