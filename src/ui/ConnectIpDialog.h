#pragma once

#include <ElaDialog.h>
#include <QVector>

class ElaLineEdit;
class QListWidget;
class ElaListWidget;
class ElaPushButton;

class ConnectIpDialog : public ElaDialog {
    Q_OBJECT

public:
    struct SearchResult {
        QString clientId;
        QString displayName;
        QString host;
        quint16 port = 0;
        bool isOnline = false;
    };

    explicit ConnectIpDialog(QWidget* parent = nullptr);

    /// 直连 IP 模式：返回用户输入的 IP
    QString host() const;
    quint16 port() const;

    /// 搜索选择模式：返回 true 表示用户选择了搜索结果
    bool hasSelectedResults() const;
    QVector<SearchResult> selectedResults() const;

    /// 设置已知联系人列表（供搜索用）
    void setKnownPeers(const QVector<SearchResult>& peers);

private:
    void performSearch();
    void updateAddButton();

    ElaLineEdit*   m_hostEdit = nullptr;
    ElaLineEdit*   m_portEdit = nullptr;
    ElaPushButton* m_searchBtn = nullptr;
    ElaListWidget* m_resultsList = nullptr;
    ElaPushButton* m_okButton = nullptr;
    QVector<SearchResult> m_knownPeers;
    QVector<SearchResult> m_searchResults;
};
