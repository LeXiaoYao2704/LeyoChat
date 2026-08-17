#pragma once

#include <string>

#include <QtGlobal>

struct Group {
    std::wstring groupId;
    std::wstring groupName;
    std::wstring ownerClientId;
    std::wstring announcement;
    int version = 0;
    qint64 createdAtMs = 0;
    qint64 updatedAtMs = 0;
    bool isActive = true;
};
