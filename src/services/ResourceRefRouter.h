#pragma once

#include <optional>

#include <QByteArray>
#include <QString>

#include "domain/MessageEnvelope.h"
#include "domain/ResourceRefPayload.h"

class ResourceRefRouter {
public:
    static bool isResourceReferenceEnvelope(const MessageEnvelope& envelope);
    static QByteArray serializePayload(const ResourceRefPayload& payload);
    static std::optional<ResourceRefPayload> parsePayload(const QByteArray& payloadJson);
    static std::optional<ResourceRefPayload> parseEnvelope(const MessageEnvelope& envelope);
    static QString previewLabel(const MessageEnvelope& envelope);
};
