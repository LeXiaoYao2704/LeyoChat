#pragma once
#include <QSettings>
#include <QString>

struct RemoteFileServiceConnectionSettings {
    bool    enabled                   = false;
    QString baseUrl;                  // e.g. "http://localhost:8765"
    QString bearerToken;
    QString defaultWorkspaceId;
    // Health tracking (same pattern as AzureDevOpsConnectionSettings)
    qint64  lastPollAttemptAtMs       = 0;
    qint64  lastPollSuccessAtMs       = 0;
    QString lastPollErrorMessage;
    int     consecutivePollFailures   = 0;

    bool hasCredentialConfiguration() const {
        return !baseUrl.trimmed().isEmpty() && !bearerToken.trimmed().isEmpty();
    }
};

namespace RemoteFileServiceSettingsStore {
    RemoteFileServiceConnectionSettings load(QSettings* settings = nullptr);
    void save(const RemoteFileServiceConnectionSettings& config,
              QSettings* settings = nullptr);
}

// 按群存储的文件服务配置（替换全局 defaultWorkspaceId 的职责）
struct GroupFileServiceConfig {
    QString groupId;
    bool    enabled     = false;
    QString baseUrl;
    QString bearerToken;
    QString workspaceId;
    int     chatFileTtlDays = 7;
    int     chatFileQuotaMb = 2048;
};

inline bool operator==(const GroupFileServiceConfig& lhs, const GroupFileServiceConfig& rhs)
{
    return lhs.groupId == rhs.groupId
        && lhs.enabled == rhs.enabled
        && lhs.baseUrl == rhs.baseUrl
        && lhs.bearerToken == rhs.bearerToken
        && lhs.workspaceId == rhs.workspaceId
        && lhs.chatFileTtlDays == rhs.chatFileTtlDays
        && lhs.chatFileQuotaMb == rhs.chatFileQuotaMb;
}

inline bool operator!=(const GroupFileServiceConfig& lhs, const GroupFileServiceConfig& rhs)
{
    return !(lhs == rhs);
}

struct OnlyOfficeSettings {
    bool    enabled = false;
    QString documentServerUrl;  // e.g. "http://192.0.2.100:8080"
};

inline bool operator==(const OnlyOfficeSettings& a, const OnlyOfficeSettings& b)
{
    return a.enabled == b.enabled && a.documentServerUrl == b.documentServerUrl;
}

namespace GroupFileServiceSettingsStore {
    GroupFileServiceConfig load(const QString& groupId, QSettings* settings = nullptr);
    void save(const GroupFileServiceConfig& config, QSettings* settings = nullptr);
}

// 本地文件服务全局配置（机器级别，不按群区分）
struct LocalFileServiceConfig {
    quint16 port              = 8765;
    QString onlyOfficeUrl;           // e.g. "http://localhost:8088"
    QString onlyOfficeJwtSecret;
    QString externalUrl;             // e.g. "https://chat.example.com"（空则自动探测）
    int     chatFileTtlDays   = 7;
    int     chatFileQuotaMb   = 2048;
};

namespace LocalFileServiceSettingsStore {
    LocalFileServiceConfig load(QSettings* settings = nullptr);
    void save(const LocalFileServiceConfig& config, QSettings* settings = nullptr);
}
