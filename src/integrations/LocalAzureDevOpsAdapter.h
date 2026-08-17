#pragma once

#include <memory>
#include <optional>

#include <QHash>
#include <QJsonDocument>
#include <QString>

#include "integrations/AzureDevOpsLinkParser.h"
#include "integrations/AzureDevOpsSettings.h"
#include "integrations/DevOpsAdapterContracts.h"

class QUrl;

class IAzureDevOpsApiTransport {
public:
    virtual ~IAzureDevOpsApiTransport() = default;
    virtual std::optional<QJsonDocument> getJson(const QUrl& url,
                                                 const AzureDevOpsConnectionSettings& settings,
                                                 QString* errorMessage) const = 0;
    virtual std::optional<QJsonDocument> postJson(const QUrl& url,
                                                  const AzureDevOpsConnectionSettings& settings,
                                                  const QJsonDocument& body,
                                                  QString* errorMessage) const = 0;
};

class NetworkAzureDevOpsApiTransport : public IAzureDevOpsApiTransport {
public:
    std::optional<QJsonDocument> getJson(const QUrl& url,
                                         const AzureDevOpsConnectionSettings& settings,
                                         QString* errorMessage) const override;
    std::optional<QJsonDocument> postJson(const QUrl& url,
                                          const AzureDevOpsConnectionSettings& settings,
                                          const QJsonDocument& body,
                                          QString* errorMessage) const override;
};

class LocalAzureDevOpsAdapter : public IDevOpsAdapter {
public:
    explicit LocalAzureDevOpsAdapter(
        AzureDevOpsConnectionSettings settings = {},
        std::shared_ptr<IAzureDevOpsApiTransport> transport = std::make_shared<NetworkAzureDevOpsApiTransport>());

    QString adapterId() const override;
    QString displayName() const override;
    QVector<ResourceReference> visibleResourcesForWorkspace(const QString& workspaceId) const override;
    std::optional<ResourceRefPayload> payloadForResource(const QString& resourceId) const override;

    QVector<AzureDevOpsOrganizationInfo> discoverOrganizations(QString* errorMessage = nullptr) const;
    QVector<AzureDevOpsProjectInfo> discoverProjects(const QString& organization,
                                                     QString* errorMessage = nullptr) const;
    bool testConnection(QString* errorMessage = nullptr) const;
    bool discoverCurrentUser(AzureDevOpsConnectionSettings* resolvedSettings,
                             QString* errorMessage = nullptr) const;
    std::optional<ResourceRefPayload> payloadForLink(const QString& link, QString* errorMessage = nullptr);
    std::optional<ResourceRefPayload> payloadForLocator(const AzureDevOpsResourceLocator& locator,
                                                        QString* errorMessage = nullptr);

private:
    std::optional<ResourceRefPayload> resolveWorkItem(const AzureDevOpsResourceLocator& locator,
                                                      QString* errorMessage);
    std::optional<ResourceRefPayload> resolvePullRequest(const AzureDevOpsResourceLocator& locator,
                                                         QString* errorMessage);
    std::optional<ResourceRefPayload> resolveBuild(const AzureDevOpsResourceLocator& locator,
                                                   QString* errorMessage);
    void cachePayload(const ResourceRefPayload& payload);

    AzureDevOpsConnectionSettings m_settings;
    std::shared_ptr<IAzureDevOpsApiTransport> m_transport;
    QHash<QString, ResourceRefPayload> m_payloadCache;
    QHash<QString, ResourceReference> m_referenceCache;
};
