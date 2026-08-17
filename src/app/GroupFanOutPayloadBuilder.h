#pragma once

#include <vector>

#include <QByteArray>
#include <QString>

#include "domain/MessageEnvelope.h"
#include "services/ChatService.h"

struct GroupFanOutPayload {
    QString targetId;
    QByteArray blob;
    QString messageId;
};

std::vector<ChatService::PendingGroupFanOutEnvelope> buildPendingGroupFanOut(
    const std::vector<MessageEnvelope>& envelopes,
    std::vector<GroupFanOutPayload>* payloads);
