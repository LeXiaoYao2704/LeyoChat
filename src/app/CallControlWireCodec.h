#pragma once

#include <QString>

#include "domain/CallProtocol.h"
#include "domain/MessageEnvelope.h"

QString callControlTypeToWire(CallControlType type);
bool callControlTypeFromWire(const QString& text, CallControlType* type);
QString buildCallPayloadJson(const CallControlPayload& payload);
CallControlPayload callPayloadFromEnvelope(const MessageEnvelope& envelope);
