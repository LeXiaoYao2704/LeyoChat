#pragma once

#include <QWidget>

class QListView;
class QVBoxLayout;
class ChatComposerWidget;
class ChatHeaderWidget;

class ChatWorkspaceWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChatWorkspaceWidget(QWidget* parent = nullptr);

    void setHeaderWidget(ChatHeaderWidget* header);
    void setMessageView(QListView* view);
    void setComposerWidget(ChatComposerWidget* composer);

private:
    QVBoxLayout* m_layout = nullptr;
};
