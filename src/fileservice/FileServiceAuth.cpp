#include "FileServiceAuth.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

FileServiceAuth::FileServiceAuth(FileServiceDatabase* db)
    : m_db(db)
{
}

std::optional<AuthenticatedClient> FileServiceAuth::validate(
    const QString& bearerHeader) const
{
    if (bearerHeader.isEmpty())
        return std::nullopt;

    QString token = bearerHeader;
    if (token.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive))
        token = token.mid(7);
    token = token.trimmed();

    if (token.isEmpty())
        return std::nullopt;

    const auto record = m_db->findToken(token);
    if (!record)
        return std::nullopt;

    return AuthenticatedClient{
        record->clientId,
        record->allowedWorkspaces,
        record->role,
        record->scopes.trimmed().isEmpty() ? QStringLiteral("*") : record->scopes
    };
}

bool FileServiceAuth::canAccessWorkspace(const AuthenticatedClient& client,
                                         const QString& workspaceId) const
{
    if (client.allowedWorkspaces == QStringLiteral("*"))
        return true;

    const auto doc = QJsonDocument::fromJson(client.allowedWorkspaces.toUtf8());
    if (!doc.isArray())
        return false;

    const auto arr = doc.array();
    for (const auto& val : arr) {
        if (val.toString() == workspaceId)
            return true;
    }
    return false;
}

bool FileServiceAuth::canUseScope(const AuthenticatedClient& client,
                                  const QString& scope) const
{
    const QString normalizedScope = scope.trimmed();
    if (normalizedScope.isEmpty())
        return false;

    const QString scopes = client.scopes.trimmed();
    if (scopes.isEmpty() || scopes == QStringLiteral("*"))
        return true;

    const auto doc = QJsonDocument::fromJson(scopes.toUtf8());
    if (!doc.isArray())
        return false;

    const auto arr = doc.array();
    for (const auto& val : arr) {
        if (val.toString().trimmed() == normalizedScope)
            return true;
    }
    return false;
}

bool FileServiceAuth::hasRole(const AuthenticatedClient& client,
                              const QString& role) const
{
    return !role.trimmed().isEmpty()
        && client.role.trimmed().compare(role.trimmed(), Qt::CaseInsensitive) == 0;
}

std::optional<AuthenticatedClient> FileServiceAuth::resolveRequestClient(
    const AuthenticatedClient& authenticatedClient,
    const QString& requestedClientId) const
{
    const QString authenticatedId = authenticatedClient.clientId.trimmed();
    const QString requestedId = requestedClientId.trimmed();
    if (authenticatedId.isEmpty()) {
        return std::nullopt;
    }
    if (requestedId.isEmpty() || requestedId == authenticatedId) {
        AuthenticatedClient resolved = authenticatedClient;
        resolved.clientId = authenticatedId;
        return resolved;
    }
    if (!hasRole(authenticatedClient, QStringLiteral("admin"))) {
        return std::nullopt;
    }

    AuthenticatedClient resolved = authenticatedClient;
    resolved.clientId = requestedId;
    return resolved;
}

bool FileServiceAuth::seedTokenIfEmpty(const QString& token,
                                       const QString& clientId,
                                       const QString& displayName) const
{
    const QString connName = QStringLiteral("leyo-file-service");
    QSqlDatabase db = QSqlDatabase::database(connName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM service_tokens"))) {
        qWarning() << "seedTokenIfEmpty count error:" << q.lastError().text();
        return false;
    }
    q.next();
    const int count = q.value(0).toInt();
    if (count > 0)
        return true;

    ServiceToken st;
    st.token = token;
    st.clientId = clientId;
    st.displayName = displayName;
    st.createdAtMs = QDateTime::currentMSecsSinceEpoch();
    st.allowedWorkspaces = QStringLiteral("*");
    st.role = QStringLiteral("admin");
    st.scopes = QStringLiteral("*");
    const bool ok = m_db->insertToken(st);
    if (ok)
        qInfo() << "Seeded admin token for client:" << clientId;
    return ok;
}

bool FileServiceAuth::seedOrUpdateTokenScope(const QString& token,
                                             const QString& clientId,
                                             const QString& displayName,
                                             const QString& allowedWorkspaces) const
{
    return seedOrUpdateTokenSecurity(token,
                                     clientId,
                                     displayName,
                                     allowedWorkspaces,
                                     QStringLiteral("member"),
                                     QStringLiteral("*"));
}

bool FileServiceAuth::seedOrUpdateTokenSecurity(
    const QString& token,
    const QString& clientId,
    const QString& displayName,
    const QString& allowedWorkspaces,
    const QString& role,
    const QString& scopes) const
{
    if (allowedWorkspaces.trimmed().isEmpty()) {
        qWarning() << "seedOrUpdateTokenSecurity: allowedWorkspaces must not be empty";
        return false;
    }
    if (scopes.trimmed().isEmpty()) {
        qWarning() << "seedOrUpdateTokenSecurity: scopes must not be empty";
        return false;
    }

    const QString normalizedRole =
        role.trimmed().isEmpty() ? QStringLiteral("member") : role.trimmed();
    const auto existing = m_db->findToken(token);
    if (!existing.has_value()) {
        ServiceToken st;
        st.token = token;
        st.clientId = clientId;
        st.displayName = displayName;
        st.createdAtMs = QDateTime::currentMSecsSinceEpoch();
        st.allowedWorkspaces = allowedWorkspaces;
        st.role = normalizedRole;
        st.scopes = scopes;
        const bool ok = m_db->insertToken(st);
        if (ok)
            qInfo() << "Seeded token for client:" << clientId
                    << "scope:" << allowedWorkspaces;
        return ok;
    }

    bool ok = true;
    if (existing->allowedWorkspaces != allowedWorkspaces)
        ok = m_db->updateTokenScope(token, allowedWorkspaces) && ok;
    if (existing->role != normalizedRole)
        ok = m_db->updateTokenRole(token, normalizedRole) && ok;

    const QString existingScopes =
        existing->scopes.trimmed().isEmpty() ? QStringLiteral("*") : existing->scopes;
    if (existingScopes != scopes)
        ok = m_db->updateTokenScopes(token, scopes) && ok;

    if (ok)
        qInfo() << "Updated token security for client:" << clientId;
    return ok;
}
