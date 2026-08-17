#pragma once
#include <QString>
#include <optional>
#include "FileServiceDatabase.h"

struct AuthenticatedClient {
    QString clientId;
    QString allowedWorkspaces; // "*" or JSON array string
    QString role;
    QString scopes; // "*" or JSON array string
};

class FileServiceAuth {
public:
    explicit FileServiceAuth(FileServiceDatabase* db);

    // Returns AuthenticatedClient if token valid, nullopt if invalid
    std::optional<AuthenticatedClient> validate(const QString& bearerHeader) const;

    // Returns true if this client can access the given workspaceId
    bool canAccessWorkspace(const AuthenticatedClient& client,
                            const QString& workspaceId) const;
    bool canUseScope(const AuthenticatedClient& client,
                     const QString& scope) const;
    bool hasRole(const AuthenticatedClient& client,
                 const QString& role) const;
    // The installed service currently uses a shared admin bootstrap token.
    // Allow that token to carry the workstation identity in X-Client-Id while
    // preventing member tokens from claiming a different client identity.
    std::optional<AuthenticatedClient> resolveRequestClient(
        const AuthenticatedClient& authenticatedClient,
        const QString& requestedClientId) const;

    // Inserts token with given scope if not present; updates scope if token already exists
    // with a different allowedWorkspaces. Returns false if allowedWorkspaces is empty.
    bool seedOrUpdateTokenScope(const QString& token,
                                const QString& clientId,
                                const QString& displayName,
                                const QString& allowedWorkspaces) const;
    bool seedOrUpdateTokenSecurity(const QString& token,
                                   const QString& clientId,
                                   const QString& displayName,
                                   const QString& allowedWorkspaces,
                                   const QString& role,
                                   const QString& scopes) const;

    [[deprecated("use seedOrUpdateTokenScope instead")]]
    bool seedTokenIfEmpty(const QString& token,
                          const QString& clientId,
                          const QString& displayName) const;

private:
    FileServiceDatabase* m_db;
};
