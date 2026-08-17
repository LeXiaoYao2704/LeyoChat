#include <QtTest/QTest>

#include <cstdlib>
#include <QCoreApplication>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "storage/GroupRepository.h"
#include "services/IdentityService.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"
#include "storage/ProfileRepository.h"
#include "storage/ServiceBindingRepository.h"
#include "storage/ServiceRegistryRepository.h"
#include "storage/ServiceResourceRepository.h"

namespace {
bool tableExists(const QString& connectionName, const QString& tableName)
{
    QSqlQuery query(QSqlDatabase::database(connectionName, false));
    query.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ? LIMIT 1"));
    query.addBindValue(tableName);
    if (!query.exec()) {
        return false;
    }
    return query.next();
}

bool columnExists(const QString& connectionName, const QString& tableName, const QString& columnName)
{
    QSqlQuery query(QSqlDatabase::database(connectionName, false));
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(tableName))) {
        return false;
    }

    while (query.next()) {
        if (query.value(1).toString() == columnName) {
            return true;
        }
    }

    return false;
}

bool indexExists(const QString& connectionName, const QString& indexName)
{
    QSqlQuery query(QSqlDatabase::database(connectionName, false));
    query.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type = 'index' AND name = ? LIMIT 1"));
    query.addBindValue(indexName);
    if (!query.exec()) {
        return false;
    }
    return query.next();
}
}

class TestDatabaseManager : public QObject {
    Q_OBJECT

private slots:
    void createsStageTwoPersistenceTables() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("stage2-persistence.db"));
        const QString connectionName = QStringLiteral("stage2-persistence-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        QVERIFY(tableExists(connectionName, QStringLiteral("service_registry")));
        QVERIFY(tableExists(connectionName, QStringLiteral("workspace_service_bindings")));
        QVERIFY(tableExists(connectionName, QStringLiteral("group_service_bindings")));
        QVERIFY(tableExists(connectionName, QStringLiteral("service_resources")));
        QVERIFY(tableExists(connectionName, QStringLiteral("service_selection_state")));
    }

    void createsExtendedMessageColumnsForFreshDatabases() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("message-columns.db"));
        const QString connectionName = QStringLiteral("message-columns-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        QVERIFY(columnExists(connectionName, QStringLiteral("messages"), QStringLiteral("message_type")));
        QVERIFY(columnExists(connectionName, QStringLiteral("messages"), QStringLiteral("payload_json")));
        QVERIFY(indexExists(connectionName, QStringLiteral("idx_messages_conversation_received_at")));
        QVERIFY(indexExists(connectionName, QStringLiteral("idx_messages_conversation_created_at")));
        QVERIFY(indexExists(connectionName, QStringLiteral("idx_messages_conversation_sender")));
    }

    void createsReminderSchemaForFreshDatabases() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("reminders.db"));
        const QString connectionName = QStringLiteral("reminders-schema-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        QVERIFY(tableExists(connectionName, QStringLiteral("reminders")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("reminder_id")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("target_type")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("target_id")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("conversation_id")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("group_id")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("contact_id")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("resource_id")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("title_snapshot")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("preview_snapshot")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("due_at_ms")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("state")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("source_message_id")));
        QVERIFY(columnExists(connectionName, QStringLiteral("reminders"), QStringLiteral("payload_json")));
        QVERIFY(indexExists(connectionName, QStringLiteral("idx_reminders_state_due")));
        QVERIFY(indexExists(connectionName, QStringLiteral("idx_reminders_target")));
        QVERIFY(indexExists(connectionName, QStringLiteral("idx_reminders_conversation")));
    }

    void migratesLegacyMessageRowsWithSafeDefaults() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("legacy-messages.db"));
        const QString connectionName = QStringLiteral("legacy-messages-db");

        {
            auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());

            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral(R"(
                CREATE TABLE messages (
                    message_id TEXT PRIMARY KEY,
                    conversation_id TEXT NOT NULL,
                    sender_id TEXT NOT NULL,
                    body TEXT NOT NULL,
                    created_at_ms INTEGER NOT NULL,
                    delivery_state TEXT NOT NULL,
                    attachment_name TEXT NOT NULL DEFAULT '',
                    local_file_path TEXT NOT NULL DEFAULT ''
                )
            )")));
            QVERIFY(query.exec(QStringLiteral(
                "INSERT INTO messages "
                "(message_id, conversation_id, sender_id, body, created_at_ms, delivery_state, attachment_name, local_file_path) "
                "VALUES ('legacy-msg', 'conv-001', 'peer-001', 'legacy body', 123, 'received', '', '')")));

            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        QVERIFY(columnExists(connectionName, QStringLiteral("messages"), QStringLiteral("message_type")));
        QVERIFY(columnExists(connectionName, QStringLiteral("messages"), QStringLiteral("payload_json")));
        QVERIFY(indexExists(connectionName, QStringLiteral("idx_messages_conversation_received_at")));
        QVERIFY(indexExists(connectionName, QStringLiteral("idx_messages_conversation_created_at")));
        QVERIFY(indexExists(connectionName, QStringLiteral("idx_messages_conversation_sender")));

        QSqlQuery query(QSqlDatabase::database(connectionName, false));
        QVERIFY(query.exec(QStringLiteral(
            "SELECT message_type, payload_json FROM messages WHERE message_id = 'legacy-msg'")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("text"));
        QCOMPARE(query.value(1).toString(), QString());
    }

    void addsStageTwoPersistenceTablesWithoutBreakingExistingData() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("stage2-upgrade.db"));
        const QString connectionName = QStringLiteral("stage2-upgrade-db");

        {
            auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());

            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral(R"(
                CREATE TABLE profile (
                    client_id TEXT PRIMARY KEY,
                    display_name TEXT NOT NULL,
                    employee_code TEXT NOT NULL,
                    listen_port INTEGER NOT NULL
                )
            )")));
            QVERIFY(query.exec(QStringLiteral(R"(
                CREATE TABLE conversations (
                    conversation_id TEXT PRIMARY KEY,
                    title TEXT NOT NULL,
                    last_message_preview TEXT NOT NULL,
                    last_message_at_ms INTEGER NOT NULL
                )
            )")));
            QVERIFY(query.exec(QStringLiteral(
                "INSERT INTO profile (client_id, display_name, employee_code, listen_port) "
                "VALUES ('legacy-client', '测试用户', 'legacy001', 45454)")));
            QVERIFY(query.exec(QStringLiteral(
                "INSERT INTO conversations (conversation_id, title, last_message_preview, last_message_at_ms) "
                "VALUES ('legacy-peer', '张三', 'hello', 123)")));

            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        QVERIFY(tableExists(connectionName, QStringLiteral("profile")));
        QVERIFY(tableExists(connectionName, QStringLiteral("conversations")));
        QVERIFY(tableExists(connectionName, QStringLiteral("service_registry")));
        QVERIFY(tableExists(connectionName, QStringLiteral("workspace_service_bindings")));
        QVERIFY(tableExists(connectionName, QStringLiteral("group_service_bindings")));
        QVERIFY(tableExists(connectionName, QStringLiteral("service_resources")));
        QVERIFY(tableExists(connectionName, QStringLiteral("service_selection_state")));

        QSqlQuery profileQuery(QSqlDatabase::database(connectionName, false));
        QVERIFY(profileQuery.exec(QStringLiteral(
            "SELECT client_id, display_name, employee_code, listen_port FROM profile LIMIT 1")));
        QVERIFY(profileQuery.next());
        QCOMPARE(profileQuery.value(0).toString(), QStringLiteral("legacy-client"));
        QCOMPARE(profileQuery.value(1).toString(), QStringLiteral("测试用户"));

        QSqlQuery conversationQuery(QSqlDatabase::database(connectionName, false));
        QVERIFY(conversationQuery.exec(QStringLiteral(
            "SELECT conversation_id, title, last_message_preview, last_message_at_ms FROM conversations LIMIT 1")));
        QVERIFY(conversationQuery.next());
        QCOMPARE(conversationQuery.value(0).toString(), QStringLiteral("legacy-peer"));
        QCOMPARE(conversationQuery.value(1).toString(), QStringLiteral("张三"));
        QCOMPARE(conversationQuery.value(2).toString(), QStringLiteral("hello"));
        QCOMPARE(conversationQuery.value(3).toLongLong(), 123LL);
    }

    void serviceRegistryRepository_roundTripsDiscoveryData() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("service-registry.db"));
        const QString connectionName = QStringLiteral("service-registry-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ServiceRegistryRepository repository(connectionName);
        const QVector<ServiceRegistryEntry> registry{
            ServiceRegistryEntry{
                QStringLiteral("svc-001"),
                QStringLiteral("共享服务"),
                QStringLiteral("Leyo"),
                QStringLiteral("lan"),
                QStringLiteral("192.0.2.10"),
                static_cast<quint16>(9443),
                true,
                {
                    ServiceCapability{
                        QStringLiteral("shared_files"),
                        QStringLiteral("共享文件"),
                        QStringLiteral("1.0"),
                        true
                    }
                }
            },
            ServiceRegistryEntry{
                QStringLiteral("svc-002"),
                QStringLiteral("机器人服务"),
                QStringLiteral("Leyo"),
                QStringLiteral("lan"),
                QStringLiteral("192.0.2.11"),
                static_cast<quint16>(9444),
                false,
                {}
            }
        };

        QVERIFY(repository.replaceRegistry(registry, QStringLiteral("svc-001"), 1712500000000LL));

        const QVector<ServiceRegistryEntry> loadedRegistry = repository.loadRegistry();
        QCOMPARE(loadedRegistry.size(), 2);
        QCOMPARE(loadedRegistry.front().serviceId, QStringLiteral("svc-001"));
        QCOMPARE(loadedRegistry.front().capabilities.size(), 1);
        QCOMPARE(loadedRegistry.front().capabilities.front().capabilityName, QStringLiteral("共享文件"));

        const ServiceDiscoveryResult discoveryResult = repository.loadDiscoveryResult();
        QCOMPARE(discoveryResult.services.size(), 2);
        QCOMPARE(discoveryResult.defaultServiceId, QStringLiteral("svc-001"));
        QCOMPARE(discoveryResult.services.front().observedAtMs, 1712500000000LL);
    }

    void serviceBindingRepository_roundTripsBindingsAndSelection() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("service-bindings.db"));
        const QString connectionName = QStringLiteral("service-bindings-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ServiceBindingRepository repository(connectionName);

        const QVector<GroupServiceBindingSnapshot> groupBindings{
            GroupServiceBindingSnapshot{
                QStringLiteral("group-001"),
                QStringLiteral("设计群"),
                ServiceBinding{QStringLiteral("svc-001"), true, false, true},
                {},
                ServiceDiscoverySnapshot{QString(), QString(), QString(), QString(), 1712500001000LL, {}},
                ResourceReference{
                    QStringLiteral("svc-001"),
                    QStringLiteral("ws-001"),
                    QStringLiteral("resource-001"),
                    QStringLiteral("shared_group"),
                    QStringLiteral("设计群"),
                    QStringLiteral("v1"),
                    QStringLiteral("群资源"),
                    ResourceOrigin::Service
                },
                true
            }
        };
        const QVector<WorkspaceServiceBindingSnapshot> workspaceBindings{
            WorkspaceServiceBindingSnapshot{
                QStringLiteral("ws-001"),
                QStringLiteral("主工作区"),
                groupBindings
            }
        };

        QVERIFY(repository.replaceWorkspaceBindings(workspaceBindings));
        QVERIFY(repository.replaceGroupBindings(groupBindings));
        QVERIFY(repository.saveCurrentSelection(ServiceSelectionSnapshot{
            QStringLiteral("ws-001"),
            QStringLiteral("group-001"),
            QStringLiteral("svc-001"),
            QStringLiteral("共享服务"),
            QStringLiteral("group-binding"),
            {},
            ServiceDiscoverySnapshot{QString(), QString(), QString(), QString(), 1712500001000LL, {}},
            {},
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("resource-001"),
                QStringLiteral(),
                QString(),
                QString(),
                QString(),
                ResourceOrigin::Service
            },
            true
        }));

        const QVector<WorkspaceServiceBindingSnapshot> loadedWorkspaceBindings =
            repository.loadWorkspaceBindings();
        QCOMPARE(loadedWorkspaceBindings.size(), 1);
        QCOMPARE(loadedWorkspaceBindings.front().workspaceName, QStringLiteral("主工作区"));

        const QVector<GroupServiceBindingSnapshot> loadedGroupBindings = repository.loadGroupBindings();
        QCOMPARE(loadedGroupBindings.size(), 1);
        QCOMPARE(loadedGroupBindings.front().binding.boundServiceId, QStringLiteral("svc-001"));
        QCOMPARE(loadedGroupBindings.front().primaryResource.workspaceId, QStringLiteral("ws-001"));
        QCOMPARE(loadedGroupBindings.front().enabled, true);

        const ServiceSelectionSnapshot selection = repository.loadCurrentSelection();
        QCOMPARE(selection.workspaceId, QStringLiteral("ws-001"));
        QCOMPARE(selection.groupId, QStringLiteral("group-001"));
        QCOMPARE(selection.serviceId, QStringLiteral("svc-001"));
        QCOMPARE(selection.selectedResource.resourceId, QStringLiteral("resource-001"));
        QCOMPARE(selection.bound, true);
    }

    void serviceResourceRepository_roundTripsResources() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("service-resources.db"));
        const QString connectionName = QStringLiteral("service-resources-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ServiceResourceRepository repository(connectionName);
        const QVector<ResourceReference> resources{
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QStringLiteral("shared_file"),
                QStringLiteral("方案设计.md"),
                QStringLiteral("v2"),
                QStringLiteral("最新设计稿"),
                ResourceOrigin::Service
            },
            ResourceReference{
                QString(),
                QString(),
                QStringLiteral("local-001"),
                QStringLiteral("draft"),
                QStringLiteral("本地草稿"),
                QStringLiteral("v1"),
                QStringLiteral("本地资源"),
                ResourceOrigin::Local
            }
        };

        QVERIFY(repository.replaceResources(resources));

        const QVector<ResourceReference> loadedResources = repository.loadResources();
        QCOMPARE(loadedResources.size(), 2);
        QCOMPARE(loadedResources.front().resourceId, QStringLiteral("local-001"));
        QCOMPARE(loadedResources.back().title, QStringLiteral("方案设计.md"));
        QCOMPARE(loadedResources.back().origin, ResourceOrigin::Service);
    }

    void migratesLegacyProfileDatabaseToCanonicalFilename() {
        const QString canonicalConnectionName = QStringLiteral("leyochat-main-migrate-test");
        const QString legacyConnectionName = QStringLiteral("leyochat-legacy-migrate-test");

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString legacyPath = tempDir.filePath(QStringLiteral("leyochat.sqlite"));
        const QString canonicalPath = tempDir.filePath(QStringLiteral("leyochat.db"));

        {
            DatabaseManager bootstrapManager(canonicalPath, canonicalConnectionName);
            QVERIFY(bootstrapManager.open());
        }

        const Profile originalProfile{
            L"client-123",
            L"\u5F20\u4E50",
            L"testuser",
            L"",
            45454
        };

        {
            DatabaseManager legacyManager(legacyPath, legacyConnectionName);
            QVERIFY(legacyManager.open());

            ProfileRepository legacyRepository(legacyConnectionName);
            QVERIFY(legacyRepository.saveProfile(originalProfile));
        }

        {
            DatabaseManager canonicalManager(canonicalPath, canonicalConnectionName);
            QVERIFY(canonicalManager.open());

            ProfileRepository canonicalRepository(canonicalConnectionName);
            const auto loaded = canonicalRepository.loadProfile();
            QVERIFY(loaded);
            QCOMPARE(loaded->clientId, originalProfile.clientId);
            QCOMPARE(loaded->displayName, originalProfile.displayName);
            QCOMPARE(loaded->employeeCode, originalProfile.employeeCode);
            QCOMPARE(loaded->listenPort, originalProfile.listenPort);
        }

        {
            DatabaseManager reopenManager(canonicalPath, canonicalConnectionName);
            QVERIFY(reopenManager.open());

            ProfileRepository reopenRepository(canonicalConnectionName);
            const auto reopened = reopenRepository.loadProfile();
            QVERIFY(reopened);
            QCOMPARE(reopened->clientId, originalProfile.clientId);
            QCOMPARE(reopened->displayName, originalProfile.displayName);
            QCOMPARE(reopened->employeeCode, originalProfile.employeeCode);
            QCOMPARE(reopened->listenPort, originalProfile.listenPort);
        }
    }

    void profileRepositoryPersistsExtendedFields() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("profile-fields.db"));
        const QString connectionName = QStringLiteral("profile-fields-test");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ProfileRepository repository(connectionName);
        QVERIFY(repository.saveProfile(Profile{
            L"profile-client",
            L"\u5F20\u4E50",
            L"testuser",
            L"\u4FDD\u6301\u70ED\u7231",
            45454,
            L"\u5DE5\u4E1A\u8F6F\u4EF6\u4E2D\u5FC3",
            L"\u4EA7\u54C1\u7ECF\u7406",
            L"13800000000",
            L"\u7537",
            L"test.user@example.com"
        }));

        const auto loaded = repository.loadProfile();
        QVERIFY(loaded);
        QCOMPARE(loaded->department, std::wstring(L"\u5DE5\u4E1A\u8F6F\u4EF6\u4E2D\u5FC3"));
        QCOMPARE(loaded->jobTitle, std::wstring(L"\u4EA7\u54C1\u7ECF\u7406"));
        QCOMPARE(loaded->phoneNumber, std::wstring(L"13800000000"));
        QCOMPARE(loaded->gender, std::wstring(L"\u7537"));
        QCOMPARE(loaded->email, std::wstring(L"test.user@example.com"));
    }

    void keepsCanonicalProfileWhenLegacyDatabaseAlsoExists() {
        const QString canonicalConnectionName = QStringLiteral("leyochat-main-prefer-test");
        const QString legacyConnectionName = QStringLiteral("leyochat-legacy-prefer-test");
        const QString reopenConnectionName = QStringLiteral("leyochat-main-prefer-reopen-test");

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString legacyPath = tempDir.filePath(QStringLiteral("leyochat.sqlite"));
        const QString canonicalPath = tempDir.filePath(QStringLiteral("leyochat.db"));

        const Profile legacyProfile{
            L"legacy-client",
            L"\u65E7\u8D44\u6599",
            L"legacy001",
            L"",
            40001
        };
        const Profile canonicalProfile{
            L"canonical-client",
            L"\u65B0\u8D44\u6599",
            L"canonical001",
            L"",
            40002
        };

        {
            DatabaseManager legacyManager(legacyPath, legacyConnectionName);
            QVERIFY(legacyManager.open());

            ProfileRepository legacyRepository(legacyConnectionName);
            QVERIFY(legacyRepository.saveProfile(legacyProfile));
        }

        {
            DatabaseManager canonicalManager(canonicalPath, canonicalConnectionName);
            QVERIFY(canonicalManager.open());

            ProfileRepository canonicalRepository(canonicalConnectionName);
            QVERIFY(canonicalRepository.saveProfile(canonicalProfile));
        }

        {
            DatabaseManager reopenManager(canonicalPath, reopenConnectionName);
            QVERIFY(reopenManager.open());

            ProfileRepository reopenRepository(reopenConnectionName);
            const auto loaded = reopenRepository.loadProfile();
            QVERIFY(loaded);
            QVERIFY(loaded->clientId == canonicalProfile.clientId);
            QVERIFY(loaded->displayName == canonicalProfile.displayName);
            QVERIFY(loaded->employeeCode == canonicalProfile.employeeCode);
            QCOMPARE(loaded->listenPort, canonicalProfile.listenPort);
        }
    }

    void migratesLegacyAppDataRootDatabaseToCanonicalLocation() {
        const QString legacyConnectionName = QStringLiteral("leyochat-legacy-appdata-root-test");
        const QString canonicalConnectionName = QStringLiteral("leyochat-canonical-appdata-root-test");

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString roamingRoot = tempDir.filePath(QStringLiteral("Roaming"));
        const QString legacyRoot = roamingRoot + QStringLiteral("/LeyoChat");
        const QString canonicalRoot = roamingRoot + QStringLiteral("/LeyoChat/LeyoChat");
        QVERIFY(QDir().mkpath(legacyRoot));
        QVERIFY(QDir().mkpath(canonicalRoot));

        const QString legacyPath = legacyRoot + QStringLiteral("/leyochat.db");
        const QString canonicalPath = canonicalRoot + QStringLiteral("/leyochat.db");

        const Profile legacyProfile{
            L"legacy-appdata-client",
            L"\u65E7\u6839\u76EE\u5F55\u8D44\u6599",
            L"legacy-root-001",
            L"",
            40123
        };

        {
            DatabaseManager legacyManager(legacyPath, legacyConnectionName);
            QVERIFY(legacyManager.open());

            ProfileRepository legacyRepository(legacyConnectionName);
            QVERIFY(legacyRepository.saveProfile(legacyProfile));
        }

        {
            DatabaseManager canonicalManager(canonicalPath, canonicalConnectionName);
            QVERIFY(canonicalManager.open());

            ProfileRepository canonicalRepository(canonicalConnectionName);
            const auto loaded = canonicalRepository.loadProfile();
            QVERIFY(loaded);
            QCOMPARE(loaded->clientId, legacyProfile.clientId);
            QCOMPARE(loaded->displayName, legacyProfile.displayName);
            QCOMPARE(loaded->employeeCode, legacyProfile.employeeCode);
            QCOMPARE(loaded->listenPort, legacyProfile.listenPort);
        }
    }

    void migratesLegacyAppDataRootWhenCanonicalOnlyHasProfile() {
        const QString legacyConnectionName = QStringLiteral("leyochat-legacy-appdata-profile-only-test");
        const QString canonicalConnectionName = QStringLiteral("leyochat-canonical-appdata-profile-only-test");

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString roamingRoot = tempDir.filePath(QStringLiteral("Roaming"));
        const QString legacyRoot = roamingRoot + QStringLiteral("/LeyoChat");
        const QString canonicalRoot = roamingRoot + QStringLiteral("/LeyoChat/LeyoChat");
        QVERIFY(QDir().mkpath(legacyRoot));
        QVERIFY(QDir().mkpath(canonicalRoot));

        const QString legacyPath = legacyRoot + QStringLiteral("/leyochat.db");
        const QString canonicalPath = canonicalRoot + QStringLiteral("/leyochat.db");

        {
            DatabaseManager canonicalManager(canonicalPath, canonicalConnectionName);
            QVERIFY(canonicalManager.open());

            ProfileRepository canonicalProfiles(canonicalConnectionName);
            QVERIFY(canonicalProfiles.saveProfile(Profile{
                L"canonical-profile-only",
                L"\u65B0\u76EE\u5F55\u7A7A\u767D\u8D44\u6599",
                L"canonical-profile-only",
                L"",
                40125
            }));
        }

        {
            DatabaseManager legacyManager(legacyPath, legacyConnectionName);
            QVERIFY(legacyManager.open());

            ProfileRepository legacyProfiles(legacyConnectionName);
            QVERIFY(legacyProfiles.saveProfile(Profile{
                L"legacy-rich-client",
                L"\u65E7\u76EE\u5F55\u804A\u5929\u8BB0\u5F55",
                L"legacy-rich-001",
                L"",
                40124
            }));

            ConversationRepository legacyConversations(legacyConnectionName);
            QVERIFY(legacyConversations.upsertConversation(ConversationSummary{
                L"legacy-rich-client|peer-a",
                L"\u5F20\u4E09",
                L"legacy hello",
                456
            }));
            QVERIFY(legacyConversations.appendMessage(ChatMessage{
                L"legacy-message-001",
                L"legacy-rich-client|peer-a",
                L"peer-a",
                L"legacy hello",
                456,
                MessageDeliveryState::Received,
                {},
                {}
            }));
        }

        {
            DatabaseManager canonicalManager(canonicalPath, canonicalConnectionName);
            QVERIFY(canonicalManager.open());

            ProfileRepository canonicalProfiles(canonicalConnectionName);
            const auto profile = canonicalProfiles.loadProfile();
            QVERIFY(profile);
            QCOMPARE(profile->clientId, std::wstring(L"legacy-rich-client"));

            ConversationRepository canonicalConversations(canonicalConnectionName);
            const auto summaries = canonicalConversations.loadConversationSummaries();
            QCOMPARE(summaries.size(), 1);
            QCOMPARE(summaries.front().conversationId, std::wstring(L"legacy-rich-client|peer-a"));

            const auto messages = canonicalConversations.loadMessages(L"legacy-rich-client|peer-a");
            QCOMPARE(messages.size(), 1);
            QCOMPARE(messages.front().messageId, std::wstring(L"legacy-message-001"));
        }
    }

    void conversationRepositoryUpsertsConversationInsideStableTarget() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("conversation-summary.db"));
        const QString connectionName = QStringLiteral("conversation-summary-target");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.upsertConversation(ConversationSummary{
            L"client-a|client-b",
            L"client-b",
            L"hello",
            123
        }));

        const auto summaries = repository.loadConversationSummaries();
        QCOMPARE(summaries.size(), 1);
        QCOMPARE(summaries.front().title, std::wstring(L"client-b"));
        QCOMPARE(summaries.front().lastMessagePreview, std::wstring(L"hello"));
    }

    void opensDatabaseWithChatServiceIdentifiersInsideStableTarget() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("chat-service.db"));
        DatabaseManager manager(databasePath, QStringLiteral("chat-service-db"));
        QVERIFY(manager.open());
    }

    void keepsCanonicalGroupDataWhenLegacyDatabaseAlsoExists() {
        const QString canonicalConnectionName = QStringLiteral("leyochat-main-group-migrate-test");
        const QString legacyConnectionName = QStringLiteral("leyochat-legacy-group-migrate-test");
        const QString reopenConnectionName = QStringLiteral("leyochat-main-group-reopen-test");

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString legacyPath = tempDir.filePath(QStringLiteral("leyochat.sqlite"));
        const QString canonicalPath = tempDir.filePath(QStringLiteral("leyochat.db"));

        {
            DatabaseManager canonicalManager(canonicalPath, canonicalConnectionName);
            QVERIFY(canonicalManager.open());

            GroupRepository canonicalRepository(canonicalConnectionName);
            QVERIFY(canonicalRepository.upsertGroup(Group{
                L"group-canonical",
                L"研发保留组",
                L"owner-canonical",
                L"",
                1,
                1712200000000,
                1712200000000,
                true
            }));
        }

        {
            DatabaseManager legacyManager(legacyPath, legacyConnectionName);
            QVERIFY(legacyManager.open());

            ProfileRepository legacyRepository(legacyConnectionName);
            QVERIFY(legacyRepository.saveProfile(Profile{
                L"legacy-client",
                L"\u65E7\u8D44\u6599",
                L"legacy001",
                L"",
                40001
            }));
        }

        {
            DatabaseManager reopenManager(canonicalPath, reopenConnectionName);
            QVERIFY(reopenManager.open());

            GroupRepository reopenRepository(reopenConnectionName);
            const auto storedGroup = reopenRepository.findGroupById(L"group-canonical");
            QVERIFY(storedGroup.has_value());
            QCOMPARE(storedGroup->groupName, std::wstring(L"研发保留组"));

            ProfileRepository reopenProfileRepository(reopenConnectionName);
            const auto profile = reopenProfileRepository.loadProfile();
            QVERIFY(!profile);
        }
    }

    void createsGroupSchemaForRepository() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("group-schema.db"));
        const QString connectionName = QStringLiteral("group-schema-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        GroupRepository repository(connectionName);
        QVERIFY(repository.upsertGroup(Group{
            L"group-schema-001",
            L"研发二组",
            L"owner-002",
            L"",
            2,
            1712200002000,
            1712200003000,
            true
        }));
        QVERIFY(repository.replaceMembers(L"group-schema-001", std::vector<GroupMember>{
            GroupMember{L"group-schema-001", L"owner-002", L"王五", 1712200002000, true}
        }));

        const auto storedGroup = repository.findGroupById(L"group-schema-001");
        QVERIFY(storedGroup.has_value());
        QCOMPARE(storedGroup->ownerClientId, std::wstring(L"owner-002"));

        const auto groups = repository.loadGroupsForMember(L"owner-002");
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups.front().groupName, std::wstring(L"研发二组"));
    }

    void upsertGroup_keepsExistingMembersIntact() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("group-upsert-members.db"));
        const QString connectionName = QStringLiteral("group-upsert-members-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        GroupRepository repository(connectionName);

        Group group{
            L"group-upsert-001",
            L"\u516C\u544A\u6D4B\u8BD5\u7FA4",
            L"owner-001",
            L"",
            1,
            1712200010000,
            1712200010000,
            true
        };
        QVERIFY(repository.upsertGroup(group));
        QVERIFY(repository.replaceMembers(L"group-upsert-001", std::vector<GroupMember>{
            GroupMember{L"group-upsert-001", L"owner-001", L"\u5F20\u4E09", 1712200010000, true},
            GroupMember{L"group-upsert-001", L"user-002", L"\u674E\u56DB", 1712200010001, true},
        }));

        group.announcement = L"\u4ECA\u665A\u516B\u70B9\u540C\u6B65\u8FDB\u5EA6";
        group.updatedAtMs = 1712200015000;
        group.version = 2;
        QVERIFY(repository.upsertGroup(group));

        const auto storedGroup = repository.findGroupById(L"group-upsert-001");
        QVERIFY(storedGroup.has_value());
        QCOMPARE(storedGroup->announcement, std::wstring(L"\u4ECA\u665A\u516B\u70B9\u540C\u6B65\u8FDB\u5EA6"));

        const auto members = repository.loadMembers(L"group-upsert-001");
        QCOMPARE(members.size(), 2);
        QCOMPARE(QString::fromStdWString(members[0].memberClientId), QStringLiteral("owner-001"));
        QCOMPARE(QString::fromStdWString(members[1].memberClientId), QStringLiteral("user-002"));
    }

    void loadsPendingOutgoingMessagesForConversation() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("pending-outgoing.db"));
        const QString connectionName = QStringLiteral("pending-outgoing-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.appendMessage(ChatMessage{
            L"msg-pending",
            L"client-a|client-b",
            L"client-a",
            L"queued",
            100,
            MessageDeliveryState::Pending,
            {},
            {}
        }));
        QVERIFY(repository.appendMessage(ChatMessage{
            L"msg-sent",
            L"client-a|client-b",
            L"client-a",
            L"sent",
            101,
            MessageDeliveryState::Sent,
            {},
            {}
        }));
        QVERIFY(repository.appendMessage(ChatMessage{
            L"msg-inbound",
            L"client-a|client-b",
            L"client-b",
            L"inbound",
            102,
            MessageDeliveryState::Received,
            {},
            {}
        }));

        const auto pending = repository.loadPendingOutgoingMessages(L"client-a|client-b", L"client-a");
        QCOMPARE(pending.size(), 1);
        QCOMPARE(pending.front().messageId, std::wstring(L"msg-pending"));
        QCOMPARE(pending.front().deliveryState, MessageDeliveryState::Pending);
    }

    void persistsFileMessageMetadata() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("file-message-metadata.db"));
        const QString connectionName = QStringLiteral("file-message-metadata-db");

        {
            DatabaseManager manager(databasePath, connectionName);
            QVERIFY(manager.open());

            {
                ConversationRepository repository(connectionName);
                QVERIFY(repository.appendMessage(ChatMessage{
                    L"msg-file",
                    L"client-a|client-b",
                    L"client-a",
                    L"[File] report.txt",
                    200,
                    MessageDeliveryState::Sent,
                    L"report.txt",
                    L"C:/temp/report.txt"
                }));

                {
                    const auto messages = repository.loadMessages(L"client-a|client-b");
                    QCOMPARE(messages.size(), 1);
                    QCOMPARE(messages.front().messageId, std::wstring(L"msg-file"));
                    QCOMPARE(messages.front().attachmentName, std::wstring(L"report.txt"));
                    QCOMPARE(messages.front().localFilePath, std::wstring(L"C:/temp/report.txt"));
                }
            }
        }
    }

    void conversationRepositoryRoundTripsExtendedMessageFields() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("message-extended-fields.db"));
        const QString connectionName = QStringLiteral("message-extended-fields-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.appendMessage(ChatMessage{
            L"msg-resource",
            L"client-a|client-b",
            L"client-a",
            L"[共享资源] 设计文档",
            400,
            MessageDeliveryState::Sent,
            {},
            {},
            L"resource_ref",
            L"{\"resource_id\":\"res-001\",\"kind\":\"shared_file\"}"
        }));

        const auto messages = repository.loadMessages(L"client-a|client-b");
        QCOMPARE(messages.size(), 1);
        QCOMPARE(messages.front().messageType, std::wstring(L"resource_ref"));
        QCOMPARE(messages.front().payloadJson,
                 std::wstring(L"{\"resource_id\":\"res-001\",\"kind\":\"shared_file\"}"));
    }

    void updatesMessageBodyForTransferStatus() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("file-message-body.db"));
        const QString connectionName = QStringLiteral("file-message-body-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.appendMessage(ChatMessage{
            L"msg-transfer",
            L"client-a|client-b",
            L"client-a",
            L"[File] report.zip (等待对方接受)",
            300,
            MessageDeliveryState::Pending,
            L"report.zip",
            L"C:/temp/report.zip"
        }));

        QVERIFY(repository.updateMessageBody(QStringLiteral("msg-transfer"),
                                             QStringLiteral("[File] report.zip (发送中)")));

        const auto messages = repository.loadMessages(L"client-a|client-b");
        QCOMPARE(messages.size(), 1);
        QCOMPARE(messages.front().body, std::wstring(L"[File] report.zip (发送中)"));
    }

    void remapsLegacyDirectConversationIdsToCanonicalConversation() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("legacy-direct-remap.db"));
        const QString connectionName = QStringLiteral("legacy-direct-remap-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.upsertConversation(ConversationSummary{
            L"peer-legacy",
            L"张三",
            L"hello",
            123
        }));
        QVERIFY(repository.appendMessage(ChatMessage{
            L"msg-legacy",
            L"peer-legacy",
            L"peer-legacy",
            L"hello",
            123,
            MessageDeliveryState::Received,
            {},
            {}
        }));

        QVERIFY(repository.remapConversationId(QStringLiteral("peer-legacy"),
                                               QStringLiteral("local-001|peer-legacy")));
        QVERIFY(repository.upsertConversation(ConversationSummary{
            L"local-001|peer-legacy",
            L"张三",
            L"hello",
            123
        }));
        QVERIFY(repository.deleteConversation(QStringLiteral("peer-legacy")));

        const auto messages = repository.loadMessages(L"local-001|peer-legacy");
        QCOMPARE(messages.size(), 1);
        QCOMPARE(messages.front().conversationId, std::wstring(L"local-001|peer-legacy"));

        const auto summaries = repository.loadConversationSummaries();
        QCOMPARE(summaries.size(), 1);
        QCOMPARE(summaries.front().conversationId, std::wstring(L"local-001|peer-legacy"));
    }

    void deletesKnownPeerFromRepository() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("known-peer-delete.db"));
        const QString connectionName = QStringLiteral("known-peer-delete-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.saveKnownPeer(PeerEndpoint{
            std::string("peer-001"),
            std::string("张三"),
            std::string("192.0.2.10"),
            45454,
            true
        }));

        QCOMPARE(repository.loadKnownPeers().size(), 1);
        QVERIFY(repository.deleteKnownPeer(QStringLiteral("peer-001")));
        QVERIFY(repository.loadKnownPeers().empty());
    }

    void upsertResource_insertsAndReturnsTrue()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString databasePath = tempDir.filePath(QStringLiteral("upsert-insert.db"));
        const QString connectionName = QStringLiteral("upsert-insert-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ServiceResourceRepository repo(connectionName);
        ResourceReference ref;
        ref.serviceId    = QStringLiteral("remote-file-service");
        ref.workspaceId  = QStringLiteral("group-a");
        ref.resourceId   = QStringLiteral("file-001");
        ref.resourceKind = QStringLiteral("shared_file");
        ref.title        = QStringLiteral("Design Doc");
        ref.version      = QStringLiteral("v1");
        ref.origin       = ResourceOrigin::Service;

        QVERIFY(repo.upsertResource(ref));

        const auto resources = repo.loadResources();
        QCOMPARE(resources.size(), 1);
        QCOMPARE(resources.first().resourceId, QStringLiteral("file-001"));
        QCOMPARE(resources.first().title, QStringLiteral("Design Doc"));
        QCOMPARE(resources.first().origin, ResourceOrigin::Service);
    }

    void upsertResource_conflictUpdatesTitleAndVersion()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString databasePath = tempDir.filePath(QStringLiteral("upsert-conflict.db"));
        const QString connectionName = QStringLiteral("upsert-conflict-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ServiceResourceRepository repo(connectionName);
        ResourceReference ref;
        ref.serviceId    = QStringLiteral("remote-file-service");
        ref.workspaceId  = QStringLiteral("group-a");
        ref.resourceId   = QStringLiteral("file-001");
        ref.resourceKind = QStringLiteral("shared_file");
        ref.title        = QStringLiteral("Old Title");
        ref.version      = QStringLiteral("v1");
        ref.origin       = ResourceOrigin::Service;
        QVERIFY(repo.upsertResource(ref));

        ref.title   = QStringLiteral("New Title");
        ref.version = QStringLiteral("v2");
        QVERIFY(repo.upsertResource(ref));

        const auto resources = repo.loadResources();
        QCOMPARE(resources.size(), 1);             // no duplicate row
        QCOMPARE(resources.first().title, QStringLiteral("New Title"));
        QCOMPARE(resources.first().version, QStringLiteral("v2"));
    }
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    TestDatabaseManager testCase;
    const int result = QTest::qExec(&testCase, argc, argv);
    std::_Exit(result);
}

#include "TestDatabaseManager.moc"
