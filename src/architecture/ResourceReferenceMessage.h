#pragma once

#include <optional>

#include <QString>

#include "architecture/ResourceReference.h"
#include "domain/MessageEnvelope.h"
#include "domain/ResourceRefPayload.h"

struct ResourceReferenceMessagePayload {
    ResourceReference resource;
    QString summary;
};

MessageEnvelope buildResourceReferenceEnvelope(const QString& messageId,
                                               const QString& senderId,
                                               const QString& targetId,
                                               const QString& conversationId,
                                               const ResourceRefPayload& payload,
                                               qint64 createdAtMs);

MessageEnvelope buildResourceReferenceEnvelope(const QString& messageId,
                                               const QString& senderId,
                                               const QString& targetId,
                                               const QString& conversationId,
                                               const ResourceReferenceMessagePayload& payload,
                                               qint64 createdAtMs);

std::optional<ResourceReferenceMessagePayload> parseResourceReferenceEnvelope(
    const MessageEnvelope& envelope);
