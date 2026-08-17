#pragma once

#include <QString>

#include "domain/ChatMessage.h"
#include "domain/MessageEnvelope.h"

struct DirectResourceReferenceDraft {
    QString targetClientId;
    QString conversationId;
    QString conversationTitle;
    QString messageId;
    QString preview;
    MessageEnvelope envelope;
    ChatMessage message;
};
