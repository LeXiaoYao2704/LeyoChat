#pragma once

#include <ElaDialog.h>
#include <QList>
#include <QString>

class ElaCheckBox;
class ElaPushButton;
class ElaText;
class QTableWidget;

/// 联系人/会话摘要信息，用于存储清理前的选择
struct ContactEntry {
    QString id;         // conversation_id 或目录名
    QString name;       // 显示名称
    qint64 sizeBytes;   // 该联系人下数据大小（估算）
    int itemCount;      // 项目数量
    bool selected{false};
};

/// 联系人选择对话框
/// 用于存储管理流程中先选择要管理哪些联系人的数据
class ContactSelectionDialog : public ElaDialog {
    Q_OBJECT
public:
    explicit ContactSelectionDialog(const QString& title,
                                    const QList<ContactEntry>& contacts,
                                    QWidget* parent = nullptr);

    /// 返回用户选中的联系人索引
    QList<int> selectedIndices() const;

signals:
    void selectionConfirmed(QList<int> indices);

private:
    void setupUi();
    void applyTableStyle();
    void updateSelectionSummary();
    void syncSelectAllState();

    static QString formatSize(qint64 bytes);

    QString title_;
    QList<ContactEntry> contacts_;

    QTableWidget* tableWidget_{nullptr};
    ElaCheckBox* selectAllCheck_{nullptr};
    ElaText* summaryLabel_{nullptr};
    ElaPushButton* confirmBtn_{nullptr};
    ElaPushButton* cancelBtn_{nullptr};
};
