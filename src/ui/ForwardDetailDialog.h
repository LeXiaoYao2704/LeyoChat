#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>

class QVBoxLayout;
class QScrollArea;

/// 合并转发卡片点击后展开的详情弹窗（钉钉风格：居中模态对话框，带关闭按钮）
class ForwardDetailDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ForwardDetailDialog(QWidget* parent = nullptr);

    /// 设置转发数据包 JSON 并构建内容
    void setPackage(const QJsonObject& package);

private:
    void buildContent(const QJsonArray& messages);

    QVBoxLayout* m_mainLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_contentWidget = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
};
