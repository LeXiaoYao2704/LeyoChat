#include "storage/DatabaseManager.h"

#include <array>
#include <filesystem>
#include <optional>

#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

namespace {
struct DatabaseContentSummary {
    bool hasProfile = false;
    bool hasNonProfileData = false;

    [[nodiscard]] bool hasAnyData() const {
        return hasProfile || hasNonProfileData;
    }
};

QSqlDatabase acquireConnection(const QString& connectionName, const QString& databasePath) {
    QSqlDatabase database = QSqlDatabase::contains(connectionName)
                                ? QSqlDatabase::database(connectionName, false)
                                : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    if (!database.isValid()) {
        return QSqlDatabase();
    }

    if (database.databaseName() != databasePath) {
        if (database.isOpen()) {
            database.close();
        }
        database.setDatabaseName(databasePath);
    }

    if (!database.isOpen() && !database.open()) {
        return QSqlDatabase();
    }

    return database;
}

bool enableForeignKeys(const QString& connectionName) {
    QSqlQuery query(QSqlDatabase::database(connectionName, false));
    query.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    // 限制锁等待时间，防止主线程因 SQLite 文件锁竞争冻结过久。
    // 100ms 在高并发写入场景（群消息/多 peer 重连补发）下过短，
    // 会频繁触发 SQLITE_BUSY → appendMessage 返回 false → 丢消息。
    // 3000ms 足以等待绝大多数 WAL 写入完成，同时不至于冻结 UI 过长。
    query.exec(QStringLiteral("PRAGMA busy_timeout = 3000"));
    // 禁用自动 WAL checkpoint：checkpoint 需要独占锁，在主线程执行会阻塞 UI。
    // WAL checkpoint 由后台连接关闭时自动触发（sqlite3_close 默认做 passive checkpoint）。
    query.exec(QStringLiteral("PRAGMA wal_autocheckpoint = 0"));
    return query.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
}

std::wstring legacyPathForDatabase(const QString& databasePath) {
    std::wstring legacyPath = databasePath.toStdWString();
    legacyPath.resize(legacyPath.size() - 2);
    legacyPath += L"sqlite";
    return legacyPath;
}

bool tableExists(const QString& connectionName, const QString& tableName) {
    QSqlQuery query(QSqlDatabase::database(connectionName, false));
    query.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ? LIMIT 1"));
    query.addBindValue(tableName);
    if (!query.exec()) {
        query.finish();
        return false;
    }

    const bool exists = query.next();
    query.finish();
    return exists;
}

bool tableHasRows(const QString& connectionName, const QString& tableName) {
    if (!tableExists(connectionName, tableName)) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName, false));
    const QString statement = QStringLiteral("SELECT 1 FROM %1 LIMIT 1").arg(tableName);
    if (!query.exec(statement)) {
        query.finish();
        return false;
    }

    const bool hasRows = query.next();
    query.finish();
    return hasRows;
}

DatabaseContentSummary summarizeDatabaseContents(const QString& connectionName) {
    DatabaseContentSummary summary;
    summary.hasProfile = tableHasRows(connectionName, QStringLiteral("profile"));

    static const std::array<const char*, 10> nonProfileTables = {
        "conversations",
        "messages",
        "groups",
        "group_members",
        "group_events",
        "file_tasks",
        "file_chunks",
        "remote_chat_cursors",
        "remote_chat_device_cursors",
        "remote_message_event_cursors",
    };

    for (const char* tableName : nonProfileTables) {
        if (tableHasRows(connectionName, QString::fromLatin1(tableName))) {
            summary.hasNonProfileData = true;
            return summary;
        }
    }

    summary.hasNonProfileData = tableHasRows(connectionName, QStringLiteral("known_peers"));
    return summary;
}

std::optional<DatabaseContentSummary> summarizeDatabaseFile(const QString& databasePath) {
    if (!QFile::exists(databasePath)) {
        return std::nullopt;
    }

    const QString connectionName = QStringLiteral("leyochat-db-summary-%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    std::optional<DatabaseContentSummary> summary;
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        if (!database.isValid()) {
            return std::nullopt;
        }

        database.setDatabaseName(databasePath);
        if (!database.open()) {
            database.close();
            return std::nullopt;
        }

        summary = summarizeDatabaseContents(connectionName);
        database.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
    return summary;
}

std::array<QString, 3> legacyDatabaseCandidates(const QString& databasePath) {
    const QString siblingSqlitePath = QString::fromStdWString(legacyPathForDatabase(databasePath));

    std::filesystem::path canonicalPath(databasePath.toStdWString());
    std::filesystem::path legacyRootDbPath;
    std::filesystem::path legacyRootSqlitePath;

    const std::filesystem::path canonicalFileName = canonicalPath.filename();
    const std::filesystem::path canonicalParent = canonicalPath.parent_path();
    const std::filesystem::path canonicalGrandparent = canonicalParent.parent_path();

    if (!canonicalGrandparent.empty()
        && canonicalGrandparent.filename() == std::filesystem::path(L"LeyoChat")
        && !canonicalParent.filename().empty()) {
        const std::filesystem::path roamingRoot = canonicalGrandparent.parent_path();
        const std::filesystem::path legacyDir = roamingRoot / canonicalParent.filename();
        legacyRootDbPath = legacyDir / canonicalFileName;

        std::wstring sqliteName = canonicalFileName.wstring();
        if (sqliteName.size() >= 2) {
            sqliteName.resize(sqliteName.size() - 2);
            sqliteName += L"sqlite";
            legacyRootSqlitePath = legacyDir / sqliteName;
        }
    }

    return {
        siblingSqlitePath,
        legacyRootDbPath.empty() ? QString() : QString::fromStdWString(legacyRootDbPath.wstring()),
        legacyRootSqlitePath.empty() ? QString() : QString::fromStdWString(legacyRootSqlitePath.wstring())
    };
}

}

DatabaseManager::DatabaseManager(QString databasePath, QString connectionName)
    : m_databasePath(databasePath),
      m_connectionName(connectionName) {}

DatabaseManager::~DatabaseManager() {
    discardConnection();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DatabaseManager::ensureConnection() {
    const QSqlDatabase database = acquireConnection(m_connectionName, m_databasePath);
    if (!database.isValid()) {
        return false;
    }

    if (!enableForeignKeys(m_connectionName)) {
        return false;
    }

    return true;
}

bool DatabaseManager::open() {
    if (!ensureConnection()) {
        return false;
    }

    if (!runMigrations()) {
        return false;
    }

    if (!shouldMigrateLegacyDatabase()) {
        return true;
    }

    discardConnection();
    {
        std::error_code errorCode;
        std::filesystem::remove(std::filesystem::path(m_databasePath.toStdWString()), errorCode);
    }
    {
        std::error_code errorCode;
        const bool copied = std::filesystem::copy_file(
            std::filesystem::path(m_cachedLegacyPath),
            std::filesystem::path(m_databasePath.toStdWString()),
            std::filesystem::copy_options::overwrite_existing,
            errorCode);
        if (!copied || errorCode) {
            return false;
        }
    }
    if (!QFile::exists(m_databasePath)) {
        return false;
    }

    if (!ensureConnection()) {
        return false;
    }
    return runMigrations();
}

bool DatabaseManager::runMigrations() {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));

    const bool profileOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS profile (
            client_id TEXT PRIMARY KEY,
            display_name TEXT NOT NULL,
            employee_code TEXT NOT NULL,
            listen_port INTEGER NOT NULL,
            signature TEXT NOT NULL DEFAULT '',
            department TEXT NOT NULL DEFAULT '',
            job_title TEXT NOT NULL DEFAULT '',
            phone_number TEXT NOT NULL DEFAULT '',
            gender TEXT NOT NULL DEFAULT '',
            email TEXT NOT NULL DEFAULT ''
        )
    )"));
    if (!profileOk) {
        query.finish();
        return false;
    }

    const bool conversationsOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS conversations (
            conversation_id TEXT PRIMARY KEY,
            title TEXT NOT NULL,
            last_message_preview TEXT NOT NULL,
            last_message_at_ms INTEGER NOT NULL
        )
    )"));
    if (!conversationsOk) {
        query.finish();
        return false;
    }

    const bool messagesOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS messages (
            message_id TEXT PRIMARY KEY,
            conversation_id TEXT NOT NULL,
            sender_id TEXT NOT NULL,
            body TEXT NOT NULL,
            created_at_ms INTEGER NOT NULL,
            delivery_state TEXT NOT NULL,
            attachment_name TEXT NOT NULL DEFAULT '',
            local_file_path TEXT NOT NULL DEFAULT '',
            message_type TEXT NOT NULL DEFAULT 'text',
            payload_json TEXT NOT NULL DEFAULT ''
        )
    )"));
    if (!messagesOk) {
        query.finish();
        return false;
    }

    const bool groupsOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS groups (
            group_id TEXT PRIMARY KEY,
            group_name TEXT NOT NULL,
            owner_client_id TEXT NOT NULL,
            version INTEGER NOT NULL,
            created_at_ms INTEGER NOT NULL,
            updated_at_ms INTEGER NOT NULL,
            is_active INTEGER NOT NULL
        )
    )"));
    if (!groupsOk) {
        query.finish();
        return false;
    }

    const bool groupMembersOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS group_members (
            group_id TEXT NOT NULL,
            member_client_id TEXT NOT NULL,
            member_display_name_snapshot TEXT NOT NULL,
            joined_at_ms INTEGER NOT NULL,
            is_active INTEGER NOT NULL,
            PRIMARY KEY (group_id, member_client_id),
            FOREIGN KEY (group_id) REFERENCES groups(group_id) ON DELETE CASCADE
        )
    )"));
    if (!groupMembersOk) {
        query.finish();
        return false;
    }

    const bool groupEventsOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS group_events (
            event_id TEXT PRIMARY KEY,
            group_id TEXT NOT NULL,
            event_type TEXT NOT NULL,
            operator_client_id TEXT NOT NULL,
            version INTEGER NOT NULL,
            payload TEXT NOT NULL,
            created_at_ms INTEGER NOT NULL,
            UNIQUE (group_id, version),
            FOREIGN KEY (group_id) REFERENCES groups(group_id) ON DELETE CASCADE
        )
    )"));
    if (!groupEventsOk) {
        query.finish();
        return false;
    }

    const bool fileTasksOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS file_tasks (
            task_id TEXT PRIMARY KEY,
            conversation_id TEXT NOT NULL,
            peer_client_id TEXT NOT NULL,
            group_id TEXT NOT NULL DEFAULT '',
            direction TEXT NOT NULL,
            state TEXT NOT NULL,
            file_name TEXT NOT NULL,
            file_hash TEXT NOT NULL,
            source_path TEXT NOT NULL,
            target_path TEXT NOT NULL,
            temp_path TEXT NOT NULL,
            error_code TEXT NOT NULL DEFAULT '',
            error_text TEXT NOT NULL DEFAULT '',
            file_size INTEGER NOT NULL,
            chunk_size INTEGER NOT NULL,
            chunk_count INTEGER NOT NULL,
            bytes_completed INTEGER NOT NULL,
            last_chunk_index INTEGER NOT NULL,
            created_at_ms INTEGER NOT NULL,
            updated_at_ms INTEGER NOT NULL
        )
    )"));
    if (!fileTasksOk) {
        query.finish();
        return false;
    }

    const bool fileTaskPeerIndexOk = query.exec(QStringLiteral(R"(
        CREATE INDEX IF NOT EXISTS idx_file_tasks_peer
        ON file_tasks(peer_client_id)
    )"));
    if (!fileTaskPeerIndexOk) {
        query.finish();
        return false;
    }

    const bool fileTaskConversationIndexOk = query.exec(QStringLiteral(R"(
        CREATE INDEX IF NOT EXISTS idx_file_tasks_conversation
        ON file_tasks(conversation_id)
    )"));
    if (!fileTaskConversationIndexOk) {
        query.finish();
        return false;
    }

    const bool fileTaskStateIndexOk = query.exec(QStringLiteral(R"(
        CREATE INDEX IF NOT EXISTS idx_file_tasks_state
        ON file_tasks(state)
    )"));
    if (!fileTaskStateIndexOk) {
        query.finish();
        return false;
    }

    const bool fileChunksOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS file_chunks (
            task_id TEXT NOT NULL,
            chunk_index INTEGER NOT NULL,
            size INTEGER NOT NULL,
            updated_at_ms INTEGER NOT NULL,
            PRIMARY KEY (task_id, chunk_index)
        )
    )"));
    if (!fileChunksOk) {
        query.finish();
        return false;
    }

    const bool knownPeersOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS known_peers (
            client_id TEXT PRIMARY KEY,
            display_name TEXT NOT NULL DEFAULT '',
            host TEXT NOT NULL DEFAULT '',
            port INTEGER NOT NULL DEFAULT 0,
            last_seen_at_ms INTEGER NOT NULL DEFAULT 0
        )
    )"));
    if (!knownPeersOk) {
        query.finish();
        return false;
    }

    const bool serviceRegistryOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS service_registry (
            service_id TEXT PRIMARY KEY,
            service_name TEXT NOT NULL DEFAULT '',
            organization_name TEXT NOT NULL DEFAULT '',
            environment_name TEXT NOT NULL DEFAULT '',
            host TEXT NOT NULL DEFAULT '',
            port INTEGER NOT NULL DEFAULT 0,
            tls_enabled INTEGER NOT NULL DEFAULT 0,
            manifest_version TEXT NOT NULL DEFAULT '',
            is_default INTEGER NOT NULL DEFAULT 0,
            observed_at_ms INTEGER NOT NULL DEFAULT 0,
            raw_capabilities_json TEXT NOT NULL DEFAULT '[]'
        )
    )"));
    if (!serviceRegistryOk) {
        query.finish();
        return false;
    }

    const bool workspaceBindingsOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS workspace_service_bindings (
            workspace_id TEXT PRIMARY KEY,
            workspace_name TEXT NOT NULL DEFAULT '',
            bound_service_id TEXT NOT NULL DEFAULT '',
            shared_files_enabled INTEGER NOT NULL DEFAULT 0,
            shared_editing_enabled INTEGER NOT NULL DEFAULT 0,
            connectors_enabled INTEGER NOT NULL DEFAULT 0,
            updated_at_ms INTEGER NOT NULL DEFAULT 0
        )
    )"));
    if (!workspaceBindingsOk) {
        query.finish();
        return false;
    }

    const bool groupBindingsOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS group_service_bindings (
            group_id TEXT PRIMARY KEY,
            workspace_id TEXT NOT NULL DEFAULT '',
            group_name_snapshot TEXT NOT NULL DEFAULT '',
            bound_service_id TEXT NOT NULL DEFAULT '',
            shared_files_enabled INTEGER NOT NULL DEFAULT 0,
            shared_editing_enabled INTEGER NOT NULL DEFAULT 0,
            connectors_enabled INTEGER NOT NULL DEFAULT 0,
            primary_resource_id TEXT NOT NULL DEFAULT '',
            primary_resource_kind TEXT NOT NULL DEFAULT '',
            enabled INTEGER NOT NULL DEFAULT 0,
            updated_at_ms INTEGER NOT NULL DEFAULT 0
        )
    )"));
    if (!groupBindingsOk) {
        query.finish();
        return false;
    }

    const bool serviceResourcesOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS service_resources (
            resource_id TEXT PRIMARY KEY,
            service_id TEXT NOT NULL DEFAULT '',
            workspace_id TEXT NOT NULL DEFAULT '',
            resource_kind TEXT NOT NULL DEFAULT '',
            title TEXT NOT NULL DEFAULT '',
            version TEXT NOT NULL DEFAULT '',
            summary TEXT NOT NULL DEFAULT '',
            origin TEXT NOT NULL DEFAULT 'local',
            raw_payload_json TEXT NOT NULL DEFAULT '{}',
            updated_at_ms INTEGER NOT NULL DEFAULT 0
        )
    )"));
    if (!serviceResourcesOk) {
        query.finish();
        return false;
    }

    const bool serviceSelectionOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS service_selection_state (
            selection_key TEXT PRIMARY KEY,
            workspace_id TEXT NOT NULL DEFAULT '',
            group_id TEXT NOT NULL DEFAULT '',
            service_id TEXT NOT NULL DEFAULT '',
            service_name TEXT NOT NULL DEFAULT '',
            selection_source TEXT NOT NULL DEFAULT '',
            selected_resource_id TEXT NOT NULL DEFAULT '',
            bound INTEGER NOT NULL DEFAULT 0,
            updated_at_ms INTEGER NOT NULL DEFAULT 0
        )
    )"));
    if (!serviceSelectionOk) {
        query.finish();
        return false;
    }

    const bool readReceiptsOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS message_read_receipts (
            message_id  TEXT NOT NULL,
            reader_id   TEXT NOT NULL,
            read_at_ms  INTEGER NOT NULL,
            PRIMARY KEY (message_id, reader_id)
        )
    )"));
    if (!readReceiptsOk) {
        query.finish();
        return false;
    }

    const bool pendingDeliveryReceiptsOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS pending_delivery_receipts (
            message_id      TEXT PRIMARY KEY,
            sender_id       TEXT NOT NULL,
            target_id       TEXT NOT NULL,
            conversation_id TEXT NOT NULL DEFAULT '',
            received_at_ms  INTEGER NOT NULL
        )
    )"));
    if (!pendingDeliveryReceiptsOk) {
        query.finish();
        return false;
    }

    const bool remoteChatCursorsOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS remote_chat_cursors (
            conversation_id TEXT PRIMARY KEY,
            last_received_seq INTEGER NOT NULL DEFAULT 0,
            updated_at_ms INTEGER NOT NULL
        )
    )"));
    if (!remoteChatCursorsOk) {
        query.finish();
        return false;
    }

    const bool remoteChatDeviceCursorsOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS remote_chat_device_cursors (
            conversation_id TEXT NOT NULL,
            device_id TEXT NOT NULL,
            last_received_seq INTEGER NOT NULL DEFAULT 0,
            updated_at_ms INTEGER NOT NULL,
            PRIMARY KEY (conversation_id, device_id)
        )
    )"));
    if (!remoteChatDeviceCursorsOk) {
        query.finish();
        return false;
    }

    const bool remoteMessageEventCursorsOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS remote_message_event_cursors (
            workspace_id TEXT NOT NULL,
            device_id TEXT NOT NULL,
            last_event_id INTEGER NOT NULL DEFAULT 0,
            updated_at_ms INTEGER NOT NULL,
            PRIMARY KEY (workspace_id, device_id)
        )
    )"));
    if (!remoteMessageEventCursorsOk) {
        query.finish();
        return false;
    }

    const bool remoteMessageIdMapOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS remote_message_id_map (
            server_message_id TEXT PRIMARY KEY,
            local_message_id TEXT NOT NULL,
            updated_at_ms INTEGER NOT NULL
        )
    )"));
    if (!remoteMessageIdMapOk) {
        query.finish();
        return false;
    }

    const bool pendingRemoteReadAcksOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS pending_remote_read_acks (
            server_message_id TEXT PRIMARY KEY,
            conversation_id TEXT NOT NULL,
            read_seq INTEGER NOT NULL DEFAULT 0,
            created_at_ms INTEGER NOT NULL,
            updated_at_ms INTEGER NOT NULL
        )
    )"));
    if (!pendingRemoteReadAcksOk) {
        query.finish();
        return false;
    }

    const bool pendingRemoteDeliveryAcksOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS pending_remote_delivery_acks (
            server_message_id TEXT PRIMARY KEY,
            conversation_id TEXT NOT NULL,
            received_seq INTEGER NOT NULL DEFAULT 0,
            created_at_ms INTEGER NOT NULL,
            updated_at_ms INTEGER NOT NULL
        )
    )"));
    if (!pendingRemoteDeliveryAcksOk) {
        query.finish();
        return false;
    }

    const bool remoteSessionPresenceOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS remote_session_presence (
            workspace_id TEXT NOT NULL,
            client_id TEXT NOT NULL,
            device_id TEXT NOT NULL,
            session_id TEXT NOT NULL,
            is_online INTEGER NOT NULL DEFAULT 0,
            connected_at_ms INTEGER NOT NULL DEFAULT 0,
            last_seen_at_ms INTEGER NOT NULL DEFAULT 0,
            last_event_id INTEGER NOT NULL DEFAULT 0,
            updated_at_ms INTEGER NOT NULL,
            PRIMARY KEY (workspace_id, client_id, device_id)
        )
    )"));
    if (!remoteSessionPresenceOk) {
        query.finish();
        return false;
    }

    const bool remindersOk = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS reminders (
            reminder_id TEXT PRIMARY KEY,
            target_type TEXT NOT NULL,
            target_id TEXT NOT NULL DEFAULT '',
            conversation_id TEXT NOT NULL DEFAULT '',
            group_id TEXT NOT NULL DEFAULT '',
            contact_id TEXT NOT NULL DEFAULT '',
            resource_id TEXT NOT NULL DEFAULT '',
            title_snapshot TEXT NOT NULL DEFAULT '',
            preview_snapshot TEXT NOT NULL DEFAULT '',
            note TEXT NOT NULL DEFAULT '',
            due_at_ms INTEGER NOT NULL,
            created_at_ms INTEGER NOT NULL,
            updated_at_ms INTEGER NOT NULL,
            fired_at_ms INTEGER NOT NULL DEFAULT 0,
            completed_at_ms INTEGER NOT NULL DEFAULT 0,
            state TEXT NOT NULL DEFAULT 'scheduled',
            source_message_id TEXT NOT NULL DEFAULT '',
            payload_json TEXT NOT NULL DEFAULT ''
        )
    )"));
    if (!remindersOk) {
        query.finish();
        return false;
    }

    const bool remindersStateDueIndexOk = query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_reminders_state_due "
        "ON reminders(state, due_at_ms)"));
    if (!remindersStateDueIndexOk) {
        query.finish();
        return false;
    }

    const bool remindersTargetIndexOk = query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_reminders_target "
        "ON reminders(target_type, target_id)"));
    if (!remindersTargetIndexOk) {
        query.finish();
        return false;
    }

    const bool remindersConversationIndexOk = query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_reminders_conversation "
        "ON reminders(conversation_id, due_at_ms)"));
    if (!remindersConversationIndexOk) {
        query.finish();
        return false;
    }

    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_mrr_message_id "
        "ON message_read_receipts(message_id)"));
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_remote_message_id_map_local "
        "ON remote_message_id_map(local_message_id)"));
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_pending_remote_read_acks_created "
        "ON pending_remote_read_acks(created_at_ms, server_message_id)"));
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_pending_remote_delivery_acks_created "
        "ON pending_remote_delivery_acks(created_at_ms, server_message_id)"));
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_remote_session_presence_online "
        "ON remote_session_presence(workspace_id, is_online, client_id)"));

    // 迁移：为 profile 表补资料列（兼容旧数据库）
    query.exec(QStringLiteral("ALTER TABLE profile ADD COLUMN signature TEXT NOT NULL DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE profile ADD COLUMN department TEXT NOT NULL DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE profile ADD COLUMN job_title TEXT NOT NULL DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE profile ADD COLUMN phone_number TEXT NOT NULL DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE profile ADD COLUMN gender TEXT NOT NULL DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE profile ADD COLUMN email TEXT NOT NULL DEFAULT ''"));

    // 迁移：为 conversations 表补 flag 列（兼容旧数据库）
    query.exec(QStringLiteral("ALTER TABLE conversations ADD COLUMN is_pinned INTEGER NOT NULL DEFAULT 0"));
    query.exec(QStringLiteral("ALTER TABLE conversations ADD COLUMN is_starred INTEGER NOT NULL DEFAULT 0"));
    query.exec(QStringLiteral("ALTER TABLE conversations ADD COLUMN is_muted INTEGER NOT NULL DEFAULT 0"));
    query.exec(QStringLiteral("ALTER TABLE conversations ADD COLUMN is_done INTEGER NOT NULL DEFAULT 0"));
    query.exec(QStringLiteral("ALTER TABLE conversations ADD COLUMN is_manually_unread INTEGER NOT NULL DEFAULT 0"));

    // 迁移：为 groups 表补 announcement 列（兼容旧数据库）
    query.exec(QStringLiteral("ALTER TABLE groups ADD COLUMN announcement TEXT NOT NULL DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN message_type TEXT NOT NULL DEFAULT 'text'"));
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN payload_json TEXT NOT NULL DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN received_at_ms INTEGER NOT NULL DEFAULT 0"));
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_messages_conversation_received_at "
        "ON messages(conversation_id, received_at_ms DESC)"));
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_messages_conversation_created_at "
        "ON messages(conversation_id, created_at_ms DESC)"));
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_messages_conversation_sender "
        "ON messages(conversation_id, sender_id)"));
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_conversations_pinned_last_message "
        "ON conversations(is_pinned, last_message_at_ms DESC)"));

    // 迁移：为 messages 表补消息变更列（撤回/编辑，兼容旧数据库）
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN is_recalled INTEGER NOT NULL DEFAULT 0"));
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN recalled_at_ms INTEGER NOT NULL DEFAULT 0"));
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN edited_at_ms INTEGER NOT NULL DEFAULT 0"));
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN last_mutation_at_ms INTEGER NOT NULL DEFAULT 0"));
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN last_editor_id TEXT NOT NULL DEFAULT ''"));

    // 迁移：为 messages 表补消息回复列（兼容旧数据库）
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN reply_to_message_id TEXT NOT NULL DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN reply_to_sender_id TEXT NOT NULL DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN reply_to_body TEXT NOT NULL DEFAULT ''"));

    // 迁移：为 messages 表补群文件卡片列（兼容旧数据库）
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN file_card_json TEXT NOT NULL DEFAULT ''"));

    // 迁移：为 messages 表补 @mention 列（兼容旧数据库）
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN mentioned_ids TEXT NOT NULL DEFAULT ''"));

    // 迁移：为 conversations 表补 @mention 标记列（兼容旧数据库）
    query.exec(QStringLiteral("ALTER TABLE conversations ADD COLUMN has_mention_me INTEGER NOT NULL DEFAULT 0"));

    // 迁移：为 messages 表补表情回应列（兼容旧数据库）
    query.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN reactions_json TEXT NOT NULL DEFAULT ''"));

    // 迁移：为 group_members 表补 role 列（兼容旧数据库）
    query.exec(QStringLiteral("ALTER TABLE group_members ADD COLUMN role TEXT NOT NULL DEFAULT 'member'"));

    // 消息置顶表（每个群最多3条）
    // 迁移：旧表可能用 conversation_id 单主键或缺少列，需要重建为复合主键 (conversation_id, message_id)
    // 使用数据保留迁移：RENAME → CREATE → INSERT SELECT → DROP
    {
        QSqlQuery checkQuery(QSqlDatabase::database(m_connectionName, false));
        bool needsRebuild = false;
        checkQuery.exec(QStringLiteral("PRAGMA table_info(pinned_messages)"));
        bool tableExists = false;
        bool messageIdInPK = false;
        bool hasPinnerName = false;
        bool hasAuthorName = false;
        bool hasPinnedBody = false;
        while (checkQuery.next()) {
            tableExists = true;
            const QString colName = checkQuery.value(1).toString();
            const int pkIndex = checkQuery.value(5).toInt(); // pk: 0=非主键, 1+=主键序号
            if (colName == QStringLiteral("message_id") && pkIndex > 0) {
                messageIdInPK = true;
            }
            if (colName == QStringLiteral("pinner_name")) {
                hasPinnerName = true;
            }
            if (colName == QStringLiteral("author_name")) {
                hasAuthorName = true;
            }
            if (colName == QStringLiteral("pinned_body")) {
                hasPinnedBody = true;
            }
        }
        if (tableExists && (!messageIdInPK || !hasPinnerName)) {
            needsRebuild = true;
        }
        if (needsRebuild) {
            // 数据保留迁移：先备份旧表，再创建新表并迁移数据
            query.exec(QStringLiteral("ALTER TABLE pinned_messages RENAME TO pinned_messages_old"));
            query.exec(QStringLiteral(R"(
                CREATE TABLE pinned_messages (
                    conversation_id TEXT NOT NULL,
                    message_id      TEXT NOT NULL,
                    pinner_id       TEXT NOT NULL,
                    pinner_name     TEXT NOT NULL DEFAULT '',
                    author_name     TEXT NOT NULL DEFAULT '',
                    pinned_body     TEXT NOT NULL DEFAULT '',
                    pinned_at_ms    INTEGER NOT NULL,
                    PRIMARY KEY (conversation_id, message_id)
                )
            )"));
            // 从旧表迁移数据，缺失列用默认值填充
            const QString insertSql = QStringLiteral(
                "INSERT OR IGNORE INTO pinned_messages "
                "(conversation_id, message_id, pinner_id, pinner_name, author_name, pinned_body, pinned_at_ms) "
                "SELECT conversation_id, message_id, pinner_id, %1, %2, %3, pinned_at_ms "
                "FROM pinned_messages_old")
                .arg(hasPinnerName ? QStringLiteral("pinner_name") : QStringLiteral("''"),
                     hasAuthorName ? QStringLiteral("author_name") : QStringLiteral("''"),
                     hasPinnedBody ? QStringLiteral("pinned_body") : QStringLiteral("''"));
            query.exec(insertSql);
            query.exec(QStringLiteral("DROP TABLE pinned_messages_old"));
        }
    }
    query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS pinned_messages (
            conversation_id TEXT NOT NULL,
            message_id      TEXT NOT NULL,
            pinner_id       TEXT NOT NULL,
            pinner_name     TEXT NOT NULL DEFAULT '',
            author_name     TEXT NOT NULL DEFAULT '',
            pinned_body     TEXT NOT NULL DEFAULT '',
            pinned_at_ms    INTEGER NOT NULL,
            PRIMARY KEY (conversation_id, message_id)
        )
    )"));

    query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS pending_group_envelopes (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            target_id       TEXT NOT NULL,
            group_id        TEXT NOT NULL,
            envelope_blob   BLOB NOT NULL,
            created_at_ms   INTEGER NOT NULL
        )
    )"));
    query.exec(QStringLiteral(R"(
        CREATE INDEX IF NOT EXISTS idx_pending_group_envelopes_target
            ON pending_group_envelopes (target_id)
    )"));

    query.finish();

    return true;
}

bool DatabaseManager::shouldMigrateLegacyDatabase() const {
    if (!QFile::exists(m_databasePath)) {
        return false;
    }

    if (!m_databasePath.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive)) {
        return false;
    }

    const DatabaseContentSummary canonicalSummary = summarizeDatabaseContents(m_connectionName);
    if (canonicalSummary.hasNonProfileData) {
        return false;
    }

    QString firstRichCandidate;
    QString firstAnyCandidate;
    const auto candidates = legacyDatabaseCandidates(m_databasePath);
    for (const QString& candidatePath : candidates) {
        if (candidatePath.isEmpty() || candidatePath == m_databasePath || !QFile::exists(candidatePath)) {
            continue;
        }

        const auto candidateSummary = summarizeDatabaseFile(candidatePath);
        if (!candidateSummary || !candidateSummary->hasAnyData()) {
            continue;
        }

        if (candidateSummary->hasNonProfileData && firstRichCandidate.isEmpty()) {
            firstRichCandidate = candidatePath;
        }
        if (firstAnyCandidate.isEmpty()) {
            firstAnyCandidate = candidatePath;
        }
    }

    QString selectedCandidate;
    if (canonicalSummary.hasProfile) {
        selectedCandidate = firstRichCandidate;
    } else if (!firstRichCandidate.isEmpty()) {
        selectedCandidate = firstRichCandidate;
    } else {
        selectedCandidate = firstAnyCandidate;
    }

    if (selectedCandidate.isEmpty()) {
        return false;
    }

    m_cachedLegacyPath = selectedCandidate.toStdWString();
    return true;
}

void DatabaseManager::discardConnection() {
    if (!QSqlDatabase::contains(m_connectionName)) {
        return;
    }

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (database.isValid() && database.isOpen()) {
        database.close();
    }
}
