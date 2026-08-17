#pragma once

#include <string>

#include <QtGlobal>

struct GroupMember {
    std::wstring groupId;
    std::wstring memberClientId;
    std::wstring memberDisplayNameSnapshot;
    qint64 joinedAtMs = 0;
    bool isActive = true;
    std::wstring role;  // "owner", "admin", "member" (default)
};
