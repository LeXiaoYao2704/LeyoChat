#include "ui/ConversationSidebarWidget.h"

ConversationSidebarWidget::ConversationSidebarWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("conversationSidebarWidget"));
    setAttribute(Qt::WA_StyledBackground, true);
    // 背景由全局样式表 AppStyle::stylesheet() 管理
}
