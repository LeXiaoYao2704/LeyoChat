#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "domain/MessageEnvelope.h"

class MessageCodec {
public:
    static std::string encode(const MessageEnvelope& envelope);
    static std::optional<MessageEnvelope> decode(std::string_view payload);
};
