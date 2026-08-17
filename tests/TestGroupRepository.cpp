// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增100行/修改0行/删除0行; 总行数250行
// @AI-LastModified: 2026-04-16 14:27:12

#include <QtTest/QTest>

#include "storage/DatabaseManager.h"
#include "storage/GroupRepository.h"

class TestGroupRepository : public QObject {
    Q_OBJECT

private slots:
    void createsGroupAndMembers() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-repo-test"));
        QVERIFY(databaseManager.open());

        GroupRepository repository(QStringLiteral("group-repo-test"));
        const Group group{
            L"group-001",
            L"研发一组",
            L"owner-001",
            L"",
            1,
            1712200000000,
            1712200000000,
            true
        };
        const std::vector<GroupMember> members = {
            GroupMember{L"group-001", L"owner-001", L"张三", 1712200000000, true},
            GroupMember{L"group-001", L"user-002", L"李四", 1712200001000, true}
        };

        QVERIFY(repository.upsertGroup(group));
        QVERIFY(repository.appendEvent(GroupEvent{
            L"event-setup-001",
            group.groupId,
            L"group_created",
            L"owner-001",
            1,
            L"{\"name\":\"研发一组\"}",
            1712200000500
        }));
        QVERIFY(repository.replaceMembers(group.groupId, members));

        const auto storedGroup = repository.findGroupById(group.groupId);
        QVERIFY(storedGroup.has_value());
        QCOMPARE(QString::fromStdWString(storedGroup->groupName), QStringLiteral("研发一组"));

        const auto storedMembers = repository.loadMembers(group.groupId);
        QCOMPARE(storedMembers.size(), 2);
        QCOMPARE(QString::fromStdWString(storedMembers[1].memberClientId), QStringLiteral("user-002"));
    }

    void appendsGroupEventsWithoutReplacingExistingEvents() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-event-repo-test"));
        QVERIFY(databaseManager.open());

        GroupRepository repository(QStringLiteral("group-event-repo-test"));
        QVERIFY(repository.upsertGroup(Group{
            L"group-001",
            L"研发一组",
            L"owner-001",
            L"",
            1,
            1712200000000,
            1712200000000,
            true
        }));

        const GroupEvent event{
            L"event-001",
            L"group-001",
            L"group_created",
            L"owner-001",
            1,
            L"{\"name\":\"研发一组\"}",
            1712200000000
        };
        const GroupEvent duplicateVersionEvent{
            L"event-002",
            L"group-001",
            L"group_renamed",
            L"owner-001",
            1,
            L"{\"name\":\"研发一组-2\"}",
            1712200001000
        };

        QVERIFY(repository.appendEvent(event));
        QVERIFY(!repository.appendEvent(duplicateVersionEvent));
    }

    void rejectsOrphanGroupMembersAndEvents() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-fk-repo-test"));
        QVERIFY(databaseManager.open());

        GroupRepository repository(QStringLiteral("group-fk-repo-test"));

        QVERIFY(!repository.replaceMembers(L"missing-group", std::vector<GroupMember>{
            GroupMember{L"missing-group", L"user-001", L"王五", 1712200002000, true}
        }));

        QVERIFY(!repository.appendEvent(GroupEvent{
            L"event-orphan-001",
            L"missing-group",
            L"group_created",
            L"owner-001",
            1,
            L"{}",
            1712200002000
        }));
    }

    void loadGroupsForMemberIgnoresInactiveMembershipsAndGroups() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-active-membership-test"));
        QVERIFY(databaseManager.open());

        GroupRepository repository(QStringLiteral("group-active-membership-test"));

        QVERIFY(repository.upsertGroup(Group{
            L"group-active",
            L"active",
            L"owner-001",
            L"",
            2,
            1712200000000,
            1712200002000,
            true
        }));
        QVERIFY(repository.replaceMembers(L"group-active",
                                          std::vector<GroupMember>{
                                              GroupMember{L"group-active", L"owner-001", L"owner", 1712200000000, true},
                                              GroupMember{L"group-active", L"user-002", L"user-002", 1712200001000, true}
                                          }));

        QVERIFY(repository.upsertGroup(Group{
            L"group-left",
            L"left",
            L"owner-001",
            L"",
            3,
            1712200000000,
            1712200003000,
            true
        }));
        QVERIFY(repository.replaceMembers(L"group-left",
                                          std::vector<GroupMember>{
                                              GroupMember{L"group-left", L"owner-001", L"owner", 1712200000000, true},
                                              GroupMember{L"group-left", L"user-002", L"user-002", 1712200001000, false}
                                          }));

        QVERIFY(repository.upsertGroup(Group{
            L"group-disbanded",
            L"disbanded",
            L"owner-001",
            L"",
            4,
            1712200000000,
            1712200004000,
            false
        }));
        QVERIFY(repository.replaceMembers(L"group-disbanded",
                                          std::vector<GroupMember>{
                                              GroupMember{L"group-disbanded", L"owner-001", L"owner", 1712200000000, true},
                                              GroupMember{L"group-disbanded", L"user-002", L"user-002", 1712200001000, true}
                                          }));

        const auto groups = repository.loadGroupsForMember(L"user-002");
        QCOMPARE(groups.size(), 1);
        QCOMPARE(QString::fromStdWString(groups.front().groupId), QStringLiteral("group-active"));
    }

    void savesAndLoadsMemberRoleField() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-role-repo-test"));
        QVERIFY(databaseManager.open());

        GroupRepository repository(QStringLiteral("group-role-repo-test"));
        QVERIFY(repository.upsertGroup(Group{
            L"group-role",
            L"角色测试群",
            L"owner-001",
            L"",
            1,
            1712200000000,
            1712200000000,
            true
        }));
        QVERIFY(repository.replaceMembers(L"group-role",
                                          std::vector<GroupMember>{
                                              GroupMember{L"group-role", L"owner-001", L"张三", 1712200000000, true, L"owner"},
                                              GroupMember{L"group-role", L"user-002", L"李四", 1712200001000, true, L"admin"},
                                              GroupMember{L"group-role", L"user-003", L"王五", 1712200002000, true, L"member"}
                                          }));

        const auto members = repository.loadMembers(L"group-role");
        QCOMPARE(members.size(), 3);
        QCOMPARE(QString::fromStdWString(members[0].role), QStringLiteral("owner"));
        QCOMPARE(QString::fromStdWString(members[1].role), QStringLiteral("admin"));
        QCOMPARE(QString::fromStdWString(members[2].role), QStringLiteral("member"));
    }

    void setMemberRoleUpdatesActiveMembers() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-setrole-repo-test"));
        QVERIFY(databaseManager.open());

        GroupRepository repository(QStringLiteral("group-setrole-repo-test"));
        QVERIFY(repository.upsertGroup(Group{
            L"group-sr",
            L"设置角色测试",
            L"owner-001",
            L"",
            1,
            1712200000000,
            1712200000000,
            true
        }));
        QVERIFY(repository.replaceMembers(L"group-sr",
                                          std::vector<GroupMember>{
                                              GroupMember{L"group-sr", L"owner-001", L"owner", 1712200000000, true, L"owner"},
                                              GroupMember{L"group-sr", L"user-002", L"user2", 1712200001000, true, L"member"}
                                          }));

        QVERIFY(repository.setMemberRole(QStringLiteral("group-sr"),
                                         QStringLiteral("user-002"),
                                         QStringLiteral("admin")));

        const auto members = repository.loadMembers(L"group-sr");
        QCOMPARE(members.size(), 2);
        QCOMPARE(QString::fromStdWString(members[1].role), QStringLiteral("admin"));
    }

    void setMemberRoleFailsForInactiveMember() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-setrole-inactive-test"));
        QVERIFY(databaseManager.open());

        GroupRepository repository(QStringLiteral("group-setrole-inactive-test"));
        QVERIFY(repository.upsertGroup(Group{
            L"group-sri",
            L"inactive-role-test",
            L"owner-001",
            L"",
            1,
            1712200000000,
            1712200000000,
            true
        }));
        QVERIFY(repository.replaceMembers(L"group-sri",
                                          std::vector<GroupMember>{
                                              GroupMember{L"group-sri", L"owner-001", L"owner", 1712200000000, true, L"owner"},
                                              GroupMember{L"group-sri", L"user-002", L"user2", 1712200001000, false, L"member"}
                                          }));

        QVERIFY(!repository.setMemberRole(QStringLiteral("group-sri"),
                                          QStringLiteral("user-002"),
                                          QStringLiteral("admin")));
    }

    void countMembersByRoleReturnsCorrectCount() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-countrole-test"));
        QVERIFY(databaseManager.open());

        GroupRepository repository(QStringLiteral("group-countrole-test"));
        QVERIFY(repository.upsertGroup(Group{
            L"group-cr",
            L"count-role-test",
            L"owner-001",
            L"",
            1,
            1712200000000,
            1712200000000,
            true
        }));
        QVERIFY(repository.replaceMembers(L"group-cr",
                                          std::vector<GroupMember>{
                                              GroupMember{L"group-cr", L"owner-001", L"owner", 1712200000000, true, L"owner"},
                                              GroupMember{L"group-cr", L"user-002", L"admin1", 1712200001000, true, L"admin"},
                                              GroupMember{L"group-cr", L"user-003", L"admin2", 1712200002000, true, L"admin"},
                                              GroupMember{L"group-cr", L"user-004", L"member", 1712200003000, true, L"member"}
                                          }));

        QCOMPARE(repository.countMembersByRole(L"group-cr", L"admin"), 2);
        QCOMPARE(repository.countMembersByRole(L"group-cr", L"owner"), 1);
        QCOMPARE(repository.countMembersByRole(L"group-cr", L"member"), 1);
    }
};

QTEST_MAIN(TestGroupRepository)
#include "TestGroupRepository.moc"
