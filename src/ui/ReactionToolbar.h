#pragma once

#include <QWidget>

class QTimer;

class ReactionToolbar : public QWidget {
    Q_OBJECT
public:
    explicit ReactionToolbar(QWidget* parent = nullptr);

    void showForMessage(const QString& messageId, const QRect& bubbleRect, bool isOutgoing);
    void scheduleHide();
    void cancelHide();
    QString currentMessageId() const { return m_messageId; }

signals:
    void reactionSelected(const QString& messageId, const QString& emoji);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString m_messageId;
    QTimer* m_hideTimer = nullptr;

    void setupUi();
    void updateStyleSheet();
};
