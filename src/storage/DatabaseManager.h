#pragma once

#include <string>

#include <QString>

class DatabaseManager {
public:
    DatabaseManager(QString databasePath, QString connectionName);
    ~DatabaseManager();

    bool open();

private:
    bool ensureConnection();
    bool runMigrations();
    bool shouldMigrateLegacyDatabase() const;
    void discardConnection();

    QString m_databasePath;
    QString m_connectionName;
    mutable std::wstring m_cachedLegacyPath;
};
