#pragma once

#include <memory>
#include <optional>

#include <QHash>
#include <QUrl>

#include "integrations/OutlookAdapterContracts.h"
#include "integrations/OutlookEwsTransport.h"
#include "integrations/OutlookSettings.h"

struct OutlookUserProfile {
    QString email;
    QString displayName;
};

class LocalOutlookAdapter : public IOutlookAdapter {
public:
    explicit LocalOutlookAdapter(
        OutlookConnectionSettings settings = {},
        std::shared_ptr<IOutlookEwsTransport> ewsTransport =
            std::make_shared<NetworkOutlookEwsTransport>());

    QString adapterId() const override;
    QString displayName() const override;
    QVector<ResourceReference> visibleResourcesForWorkspace(const QString& workspaceId) const override;
    std::optional<ResourceRefPayload> payloadForResource(const QString& resourceId) const override;

    const OutlookConnectionSettings& settings() const;
    bool testConnection(QString* errorMessage = nullptr);
    std::optional<OutlookUserProfile> fetchProfile(QString* errorMessage = nullptr);
    QVector<OutlookMailResource> fetchUnreadMail(int maxItems = 10, QString* errorMessage = nullptr);
    QVector<OutlookCalendarEventResource> fetchUpcomingEvents(const QDateTime& now,
                                                              int horizonMinutes,
                                                              QString* errorMessage = nullptr);

private:
    bool checkCredentials(QString* errorMessage) const;
    void cacheMail(const OutlookMailResource& resource);
    void cacheEvent(const OutlookCalendarEventResource& resource);

    OutlookConnectionSettings m_settings;
    std::shared_ptr<IOutlookEwsTransport> m_ewsTransport;
    QHash<QString, ResourceRefPayload> m_payloadCache;
    QHash<QString, ResourceReference> m_referenceCache;
};
