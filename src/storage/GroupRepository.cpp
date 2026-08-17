#include "storage/GroupRepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>

namespace {
Group groupFromQuery(const QSqlQuery& query) {
    return Group{
        query.value(0).toString().toStdWString(),
        query.value(1).toString().toStdWString(),
        query.value(2).toString().toStdWString(),
        query.value(3).toString().toStdWString(), // announcement
        query.value(4).toInt(),
        query.value(5).toLongLong(),
        query.value(6).toLongLong(),
        query.value(7).toInt() != 0
    };
}

GroupMember groupMemberFromQuery(const QSqlQuery& query) {
    return GroupMember{
        query.value(0).toString().toStdWString(),
        query.value(1).toString().toStdWString(),
        query.value(2).toString().toStdWString(),
        query.value(3).toLongLong(),
        query.value(4).toInt() != 0,
        query.value(5).toString().toStdWString()
    };
}
}

GroupRepository::GroupRepository(QString connectionName)
    : m_connectionName(connectionName) {}

bool GroupRepository::upsertGroup(const Group& group) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO groups
        (group_id, group_name, owner_client_id, announcement, version, created_at_ms, updated_at_ms, is_active)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(group_id) DO UPDATE SET
            group_name = excluded.group_name,
            owner_client_id = excluded.owner_client_id,
            announcement = excluded.announcement,
            version = excluded.version,
            created_at_ms = excluded.created_at_ms,
            updated_at_ms = excluded.updated_at_ms,
            is_active = excluded.is_active
    )"));
    query.addBindValue(QString::fromStdWString(group.groupId));
    query.addBindValue(QString::fromStdWString(group.groupName));
    query.addBindValue(QString::fromStdWString(group.ownerClientId));
    query.addBindValue(QString::fromStdWString(group.announcement));
    query.addBindValue(group.version);
    query.addBindValue(group.createdAtMs);
    query.addBindValue(group.updatedAtMs);
    query.addBindValue(group.isActive ? 1 : 0);
    return query.exec();
}

bool GroupRepository::appendEvent(const GroupEvent& event) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO group_events
        (event_id, group_id, event_type, operator_client_id, version, payload, created_at_ms)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )"));
    query.addBindValue(QString::fromStdWString(event.eventId));
    query.addBindValue(QString::fromStdWString(event.groupId));
    query.addBindValue(QString::fromStdWString(event.eventType));
    query.addBindValue(QString::fromStdWString(event.operatorClientId));
    query.addBindValue(event.version);
    query.addBindValue(QString::fromStdWString(event.payload));
    query.addBindValue(event.createdAtMs);
    return query.exec();
}

bool GroupRepository::replaceMembers(const std::wstring& groupId,
                                     const std::vector<GroupMember>& members) const {
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isValid()) {
        return false;
    }

    if (!database.transaction()) {
        return false;
    }

    QSqlQuery deleteQuery(database);
    deleteQuery.prepare(QStringLiteral("DELETE FROM group_members WHERE group_id = ?"));
    deleteQuery.addBindValue(QString::fromStdWString(groupId));
    if (!deleteQuery.exec()) {
        database.rollback();
        return false;
    }

    for (const GroupMember& member : members) {
        QSqlQuery insertQuery(database);
        insertQuery.prepare(QStringLiteral(R"(
            INSERT OR REPLACE INTO group_members
            (group_id, member_client_id, member_display_name_snapshot, joined_at_ms, is_active, role)
            VALUES (?, ?, ?, ?, ?, ?)
        )"));
        insertQuery.addBindValue(QString::fromStdWString(groupId));
        insertQuery.addBindValue(QString::fromStdWString(member.memberClientId));
        insertQuery.addBindValue(QString::fromStdWString(member.memberDisplayNameSnapshot));
        insertQuery.addBindValue(member.joinedAtMs);
        insertQuery.addBindValue(member.isActive ? 1 : 0);
        insertQuery.addBindValue(QString::fromStdWString(member.role));
        if (!insertQuery.exec()) {
            database.rollback();
            return false;
        }
    }

    return database.commit();
}

std::optional<Group> GroupRepository::findGroupById(const std::wstring& groupId) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT group_id, group_name, owner_client_id, announcement, version, created_at_ms, updated_at_ms, is_active
        FROM groups
        WHERE group_id = ?
        LIMIT 1
    )"));
    query.addBindValue(QString::fromStdWString(groupId));
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    const Group group = groupFromQuery(query);
    query.finish();
    return group;
}

std::vector<GroupMember> GroupRepository::loadMembers(const std::wstring& groupId) const {
    std::vector<GroupMember> members;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT group_id, member_client_id, member_display_name_snapshot, joined_at_ms, is_active, role
        FROM group_members
        WHERE group_id = ?
        ORDER BY joined_at_ms ASC, member_client_id ASC
    )"));
    query.addBindValue(QString::fromStdWString(groupId));
    if (!query.exec()) {
        return members;
    }

    while (query.next()) {
        members.push_back(groupMemberFromQuery(query));
    }

    return members;
}

int GroupRepository::countMembers(const std::wstring& groupId) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM group_members WHERE group_id = ? AND is_active = 1"));
    query.addBindValue(QString::fromStdWString(groupId));
    if (!query.exec() || !query.next()) return 0;
    return query.value(0).toInt();
}

std::vector<Group> GroupRepository::loadGroupsForMember(const std::wstring& memberClientId) const {
    std::vector<Group> groups;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT g.group_id, g.group_name, g.owner_client_id, g.announcement, g.version, g.created_at_ms, g.updated_at_ms, g.is_active
        FROM groups g
        INNER JOIN group_members gm ON gm.group_id = g.group_id
        WHERE gm.member_client_id = ?
          AND gm.is_active = 1
          AND g.is_active = 1
        ORDER BY g.updated_at_ms DESC, g.group_id ASC
    )"));
    query.addBindValue(QString::fromStdWString(memberClientId));
    if (!query.exec()) {
        return groups;
    }

    while (query.next()) {
        groups.push_back(groupFromQuery(query));
    }

    return groups;
}

bool GroupRepository::setAnnouncement(const QString& groupId, const QString& announcement) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(
        "UPDATE groups SET announcement = ? WHERE group_id = ?"));
    query.addBindValue(announcement);
    query.addBindValue(groupId);
    return query.exec();
}

bool GroupRepository::setMemberRole(const QString& groupId, const QString& memberClientId, const QString& role) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(
        "UPDATE group_members SET role = ? WHERE group_id = ? AND member_client_id = ? AND is_active = 1"));
    query.addBindValue(role);
    query.addBindValue(groupId);
    query.addBindValue(memberClientId);
    return query.exec() && query.numRowsAffected() > 0;
}

int GroupRepository::countMembersByRole(const std::wstring& groupId, const std::wstring& role) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM group_members WHERE group_id = ? AND role = ? AND is_active = 1"));
    query.addBindValue(QString::fromStdWString(groupId));
    query.addBindValue(QString::fromStdWString(role));
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}
