#pragma once

#include <QString>

#include "domain/ChatMessage.h"

QString messageDeliveryStateText(MessageDeliveryState state);

QString messageDeliveryIndicatorText(MessageDeliveryState state,
                                     bool outgoing,
                                     int groupReadCount,
                                     int groupActiveMemberCount);
