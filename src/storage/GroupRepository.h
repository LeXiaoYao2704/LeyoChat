#pragma once

#include <optional>
#include <vector>

#include <QString>

#include "domain/Group.h"
#include "domain/GroupEvent.h"
#include "domain/GroupMember.h"

class GroupRepository {
public:
    explicit GroupRepository(QString connectionName);

    bool upsertGroup(const Group& group) const;
    bool appendEvent(const GroupEvent& event) const;
    bool replaceMembers(const std::wstring& groupId, const std::vector<GroupMember>& members) const;
    std::optional<Group> findGroupById(const std::wstring& groupId) const;
    std::vector<GroupMember> loadMembers(const std::wstring& groupId) const;
    int countMembers(const std::wstring& groupId) const;
    std::vector<Group> loadGroupsForMember(const std::wstring& memberClientId) const;
    bool setAnnouncement(const QString& groupId, const QString& announcement) const;
    bool setMemberRole(const QString& groupId, const QString& memberClientId, const QString& role) const;
    int countMembersByRole(const std::wstring& groupId, const std::wstring& role) const;

private:
    QString m_connectionName;
};
