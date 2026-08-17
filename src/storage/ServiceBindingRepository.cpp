#include "storage/ServiceBindingRepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>

namespace {
const QString kCurrentSelectionKey = QStringLiteral("current");

QString normalizedText(const QString& value)
{
    return value.isNull() ? QStringLiteral("") : value;
}
}

ServiceBindingRepository::ServiceBindingRepository(QString connectionName)
    : m_connectionName(connectionName)
{
}

bool ServiceBindingRepository::replaceWorkspaceBindings(
    const QVector<WorkspaceServiceBindingSnapshot>& bindings) const
{
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isValid() || !database.transaction()) {
        return false;
    }

    QSqlQuery deleteQuery(database);
    if (!deleteQuery.exec(QStringLiteral("DELETE FROM workspace_service_bindings"))) {
        database.rollback();
        return false;
    }

    for (const WorkspaceServiceBindingSnapshot& binding : bindings) {
        QString boundServiceId;
        bool sharedFilesEnabled = false;
        bool sharedEditingEnabled = false;
        bool connectorsEnabled = false;

        if (!binding.groupBindings.isEmpty()) {
            const GroupServiceBindingSnapshot& primary = binding.groupBindings.front();
            boundServiceId = primary.binding.boundServiceId;
            sharedFilesEnabled = primary.binding.sharedFilesEnabled;
            sharedEditingEnabled = primary.binding.sharedEditingEnabled;
            connectorsEnabled = primary.binding.connectorsEnabled;
        }

        QSqlQuery insertQuery(database);
        insertQuery.prepare(QStringLiteral(R"(
            INSERT INTO workspace_service_bindings
            (workspace_id, workspace_name, bound_service_id, shared_files_enabled,
             shared_editing_enabled, connectors_enabled, updated_at_ms)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        )"));
        insertQuery.addBindValue(normalizedText(binding.workspaceId));
        insertQuery.addBindValue(normalizedText(binding.workspaceName));
        insertQuery.addBindValue(normalizedText(boundServiceId));
        insertQuery.addBindValue(sharedFilesEnabled ? 1 : 0);
        insertQuery.addBindValue(sharedEditingEnabled ? 1 : 0);
        insertQuery.addBindValue(connectorsEnabled ? 1 : 0);
        insertQuery.addBindValue(0);
        if (!insertQuery.exec()) {
            database.rollback();
            return false;
        }
    }

    return database.commit();
}

bool ServiceBindingRepository::replaceGroupBindings(
    const QVector<GroupServiceBindingSnapshot>& bindings) const
{
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isValid() || !database.transaction()) {
        return false;
    }

    QSqlQuery deleteQuery(database);
    if (!deleteQuery.exec(QStringLiteral("DELETE FROM group_service_bindings"))) {
        database.rollback();
        return false;
    }

    for (const GroupServiceBindingSnapshot& binding : bindings) {
        QSqlQuery insertQuery(database);
        insertQuery.prepare(QStringLiteral(R"(
            INSERT INTO group_service_bindings
            (group_id, workspace_id, group_name_snapshot, bound_service_id, shared_files_enabled,
             shared_editing_enabled, connectors_enabled, primary_resource_id, primary_resource_kind,
             enabled, updated_at_ms)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )"));
        insertQuery.addBindValue(normalizedText(binding.groupId));
        insertQuery.addBindValue(normalizedText(binding.primaryResource.workspaceId));
        insertQuery.addBindValue(normalizedText(binding.groupName));
        insertQuery.addBindValue(normalizedText(binding.binding.boundServiceId));
        insertQuery.addBindValue(binding.binding.sharedFilesEnabled ? 1 : 0);
        insertQuery.addBindValue(binding.binding.sharedEditingEnabled ? 1 : 0);
        insertQuery.addBindValue(binding.binding.connectorsEnabled ? 1 : 0);
        insertQuery.addBindValue(normalizedText(binding.primaryResource.resourceId));
        insertQuery.addBindValue(normalizedText(binding.primaryResource.resourceKind));
        insertQuery.addBindValue(binding.enabled ? 1 : 0);
        insertQuery.addBindValue(binding.discoverySnapshot.observedAtMs);
        if (!insertQuery.exec()) {
            database.rollback();
            return false;
        }
    }

    return database.commit();
}

bool ServiceBindingRepository::saveCurrentSelection(const ServiceSelectionSnapshot& selection) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO service_selection_state
        (selection_key, workspace_id, group_id, service_id, service_name, selection_source,
         selected_resource_id, bound, updated_at_ms)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(selection_key) DO UPDATE SET
            workspace_id = excluded.workspace_id,
            group_id = excluded.group_id,
            service_id = excluded.service_id,
            service_name = excluded.service_name,
            selection_source = excluded.selection_source,
            selected_resource_id = excluded.selected_resource_id,
            bound = excluded.bound,
            updated_at_ms = excluded.updated_at_ms
    )"));
    query.addBindValue(kCurrentSelectionKey);
    query.addBindValue(normalizedText(selection.workspaceId));
    query.addBindValue(normalizedText(selection.groupId));
    query.addBindValue(normalizedText(selection.serviceId));
    query.addBindValue(normalizedText(selection.serviceName));
    query.addBindValue(normalizedText(selection.selectionSource));
    query.addBindValue(normalizedText(selection.selectedResource.resourceId));
    query.addBindValue(selection.bound ? 1 : 0);
    query.addBindValue(selection.discoverySnapshot.observedAtMs);
    return query.exec();
}

QVector<WorkspaceServiceBindingSnapshot> ServiceBindingRepository::loadWorkspaceBindings() const
{
    QVector<WorkspaceServiceBindingSnapshot> bindings;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(QStringLiteral(R"(
        SELECT workspace_id, workspace_name
        FROM workspace_service_bindings
        ORDER BY workspace_id ASC
    )"))) {
        return bindings;
    }

    while (query.next()) {
        WorkspaceServiceBindingSnapshot binding;
        binding.workspaceId = query.value(0).toString();
        binding.workspaceName = query.value(1).toString();
        bindings.push_back(binding);
    }

    return bindings;
}

QVector<GroupServiceBindingSnapshot> ServiceBindingRepository::loadGroupBindings() const
{
    QVector<GroupServiceBindingSnapshot> bindings;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(QStringLiteral(R"(
        SELECT group_id, workspace_id, group_name_snapshot, bound_service_id, shared_files_enabled,
               shared_editing_enabled, connectors_enabled, primary_resource_id, primary_resource_kind,
               enabled, updated_at_ms
        FROM group_service_bindings
        ORDER BY group_id ASC
    )"))) {
        return bindings;
    }

    while (query.next()) {
        GroupServiceBindingSnapshot binding;
        binding.groupId = query.value(0).toString();
        binding.groupName = query.value(2).toString();
        binding.binding.boundServiceId = query.value(3).toString();
        binding.binding.sharedFilesEnabled = query.value(4).toInt() != 0;
        binding.binding.sharedEditingEnabled = query.value(5).toInt() != 0;
        binding.binding.connectorsEnabled = query.value(6).toInt() != 0;
        binding.primaryResource.serviceId = query.value(3).toString();
        binding.primaryResource.workspaceId = query.value(1).toString();
        binding.primaryResource.resourceId = query.value(7).toString();
        binding.primaryResource.resourceKind = query.value(8).toString();
        binding.enabled = query.value(9).toInt() != 0;
        binding.discoverySnapshot.observedAtMs = query.value(10).toLongLong();
        bindings.push_back(binding);
    }

    return bindings;
}

ServiceSelectionSnapshot ServiceBindingRepository::loadCurrentSelection() const
{
    ServiceSelectionSnapshot selection;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT workspace_id, group_id, service_id, service_name, selection_source,
               selected_resource_id, bound, updated_at_ms
        FROM service_selection_state
        WHERE selection_key = ?
        LIMIT 1
    )"));
    query.addBindValue(kCurrentSelectionKey);
    if (!query.exec() || !query.next()) {
        return selection;
    }

    selection.workspaceId = query.value(0).toString();
    selection.groupId = query.value(1).toString();
    selection.serviceId = query.value(2).toString();
    selection.serviceName = query.value(3).toString();
    selection.selectionSource = query.value(4).toString();
    selection.selectedResource.resourceId = query.value(5).toString();
    selection.selectedResource.serviceId = query.value(2).toString();
    selection.selectedResource.workspaceId = query.value(0).toString();
    selection.bound = query.value(6).toInt() != 0;
    selection.discoverySnapshot.observedAtMs = query.value(7).toLongLong();
    return selection;
}
