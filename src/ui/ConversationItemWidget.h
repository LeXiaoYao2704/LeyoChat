// ConversationItemWidget.h — 会话列表项 Widget（替代 ConversationListDelegate QPainter 绘制）
#pragma once

#include <QFrame>

class QLabel;
class QModelIndex;

class ConversationItemWidget : public QFrame {
    Q_OBJECT
    Q_PROPERTY(bool selected READ isSelected WRITE setSelected)
public:
    explicit ConversationItemWidget(QWidget* parent = nullptr);

    void populateFromIndex(const QModelIndex& index);

    void setConversationId(const QString& id);
    void setTitle(const QString& title);
    void setPreview(const QString& preview);
    void setTimeLabel(const QString& time);
    void setHasUnread(bool unread);
    void setIsPinned(bool pinned);
    void setIsStarred(bool starred);
    void setIsMuted(bool muted);
    void setHasMentionMe(bool mention);
    void setAvatarPath(const QString& path);
    void setDraftText(const QString& draft);
    void setIsOnline(bool online);
    void setSelected(bool selected);

    bool isSelected() const { return m_selected; }
    QString conversationId() const { return m_conversationId; }

    void applyThemeStyleSheet();

signals:
    void clicked(const QString& conversationId);
    void contextMenuRequested(const QString& conversationId, const QPoint& globalPos);
    void avatarHovered(const QString& conversationId, const QPoint& globalPos);
    void avatarHoverLeft();

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void buildAvatarPixmap(const QString& title, const QString& path, bool online);
    void setHovered(bool hovered);

    QString m_conversationId;
    bool m_selected = false;
    bool m_isOnline = true;

    QFrame* m_cardFrame = nullptr;
    QFrame* m_selectionBar = nullptr;
    QLabel* m_avatarLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_badgeLabel = nullptr;
    QLabel* m_timeLabel = nullptr;
    QLabel* m_draftTag = nullptr;
    QLabel* m_mentionTag = nullptr;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_unreadDot = nullptr;
};
