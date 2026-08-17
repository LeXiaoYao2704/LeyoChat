#pragma once

#include <QWidget>
#include <QPointer>
#include <QVector>

class ElaLineEdit;
class ElaListWidget;
class ElaText;
class ElaIconButton;
class ElaFlowLayout;
class QListWidgetItem;
class QHBoxLayout;
class QVBoxLayout;
class QTimer;
class QPropertyAnimation;
class GlobalSearchHistory;

class GlobalSearchPanel : public QWidget {
    Q_OBJECT
public:
    explicit GlobalSearchPanel(GlobalSearchHistory* history, QWidget* parent = nullptr);

    void popup(const QPoint& globalPos, const QString& initialText = {});
    void dismiss();

    enum class Tab { All = 0, Contact, Group, ChatHistory, File, Department, _Count };
    Q_ENUM(Tab)

    struct ContactResult {
        QString clientId;
        QString displayName;
        QString detail;
        bool isOnline = false;
    };
    struct GroupResult {
        QString groupId;
        QString groupName;
        int memberCount = 0;
    };
    struct MessageResult {
        QString conversationId;
        QString messageId;
        QString senderName;
        QString bodyPreview;
        QString conversationTitle;
        qint64 createdAtMs = 0;
    };
    struct FileResult {
        QString taskId;
        QString fileName;
        QString peerName;
        qint64 createdAtMs = 0;
    };
    struct DepartmentResult {
        QString department;
        int memberCount = 0;
    };

    void setResults(const QVector<ContactResult>& contacts,
                    const QVector<GroupResult>& groups,
                    const QVector<MessageResult>& messages,
                    const QVector<FileResult>& files,
                    const QVector<DepartmentResult>& departments);

signals:
    void searchRequested(const QString& keyword, int tab);
    void contactActivated(const QString& clientId, const QString& displayName);
    void groupActivated(const QString& groupId, const QString& groupName);
    void messageActivated(const QString& conversationId, const QString& messageId);
    void fileActivated(const QString& taskId);
    void departmentActivated(const QString& department);
    void dismissed();

protected:
    bool event(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* e) override;

private:
    void buildUi();
    void switchTab(Tab tab);
    void refreshContent();
    void showEmptyState();
    void showSearchResults();
    void applyTheme();
    void onSearchTextChanged(const QString& text);
    void onItemClicked(QListWidgetItem* item);
    void animateIndicator(int tabIndex);
    void focusSearchEdit(bool selectText = false);
    QString highlightKeyword(const QString& text) const;

    GlobalSearchHistory* m_history = nullptr;
    ElaLineEdit* m_searchEdit = nullptr;
    QWidget* m_tabBar = nullptr;
    QVector<ElaText*> m_tabLabels;
    QWidget* m_tabIndicator = nullptr;
    QPropertyAnimation* m_indicatorAnim = nullptr;
    ElaListWidget* m_resultList = nullptr;
    QWidget* m_emptyStateWidget = nullptr;
    ElaFlowLayout* m_keywordFlow = nullptr;
    ElaFlowLayout* m_frequentFlow = nullptr;
    QTimer* m_debounceTimer = nullptr;
    Tab m_currentTab = Tab::All;
    QString m_currentKeyword;

    // 缓存搜索结果
    QVector<ContactResult> m_contacts;
    QVector<GroupResult> m_groups;
    QVector<MessageResult> m_messages;
    QVector<FileResult> m_files;
    QVector<DepartmentResult> m_departments;

    static constexpr int kPanelWidth = 600;
    static constexpr int kPanelMaxHeight = 450;
};
