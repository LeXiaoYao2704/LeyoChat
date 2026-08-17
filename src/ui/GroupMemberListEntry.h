#pragma once

#include <QString>
#include <QVector>

struct GroupMemberListEntry {
    QString clientId;
    QString displayName;
    bool isOwner = false;
    bool isAdmin = false;
    bool isSelf = false;
    bool isOnline = false;
    QString avatarImagePath;
};

inline bool operator==(const GroupMemberListEntry& lhs, const GroupMemberListEntry& rhs)
{
    return lhs.clientId == rhs.clientId
        && lhs.displayName == rhs.displayName
        && lhs.isOwner == rhs.isOwner
        && lhs.isAdmin == rhs.isAdmin
        && lhs.isSelf == rhs.isSelf
        && lhs.isOnline == rhs.isOnline
        && lhs.avatarImagePath == rhs.avatarImagePath;
}

inline bool operator!=(const GroupMemberListEntry& lhs, const GroupMemberListEntry& rhs)
{
    return !(lhs == rhs);
}

using GroupMemberListEntries = QVector<GroupMemberListEntry>;
