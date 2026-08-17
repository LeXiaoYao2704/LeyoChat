#pragma once

#include "domain/MessageEnvelope.h"

#include <QString>

class PeerConnection;

bool sendDeliveryReceipt(PeerConnection* connection,
                         const QString& localClientId,
                         const MessageEnvelope& delivered);
