#pragma once

#include <string>

#include <QtGlobal>

struct GroupEvent {
    std::wstring eventId;
    std::wstring groupId;
    std::wstring eventType;
    std::wstring operatorClientId;
    int version = 0;
    std::wstring payload;
    qint64 createdAtMs = 0;
};
