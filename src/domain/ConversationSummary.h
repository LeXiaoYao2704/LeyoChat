#pragma once

#include <string>

#include <QtGlobal>

struct ConversationSummary {
    std::wstring conversationId;
    std::wstring title;
    std::wstring lastMessagePreview;
    qint64 lastMessageAtMs = 0;
    bool isPinned         = false;
    bool isStarred        = false;
    bool isMuted          = false;
    bool isDone           = false;
    bool isManuallyUnread = false;
    bool hasMentionMe     = false;  // 有未读的@我消息
};
