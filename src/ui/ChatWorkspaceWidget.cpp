#include "ChatWorkspaceWidget.h"

#include "ChatComposerWidget.h"
#include "ChatHeaderWidget.h"

#include <QListView>
#include <QVBoxLayout>

namespace {
QVBoxLayout* ensureWorkspaceLayout(ChatWorkspaceWidget* widget, QVBoxLayout*& layout)
{
    if (!layout) {
        layout = new QVBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }

    return layout;
}
}

ChatWorkspaceWidget::ChatWorkspaceWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("chatWorkspaceWidget"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(false);
    // 背景色由全局样式表 AppStyle::stylesheet() 中
    // QWidget#chatWorkspaceWidget { background: chatStageBg } 设定，
    // 不在构造器内联样式里重复声明。
}

void ChatWorkspaceWidget::setHeaderWidget(ChatHeaderWidget* header)
{
    if (!header) {
        return;
    }

    ensureWorkspaceLayout(this, m_layout)->addWidget(header);
}

void ChatWorkspaceWidget::setMessageView(QListView* view)
{
    if (!view) {
        return;
    }

    ensureWorkspaceLayout(this, m_layout)->addWidget(view, 1);
}

void ChatWorkspaceWidget::setComposerWidget(ChatComposerWidget* composer)
{
    if (!composer) {
        return;
    }

    ensureWorkspaceLayout(this, m_layout)->addWidget(composer);
}
