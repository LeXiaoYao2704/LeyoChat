#pragma once

#include <optional>

#include <QString>
#include <QVector>

#include "architecture/ResourceReference.h"
#include "domain/ResourceRefPayload.h"

struct OutlookMailResource {
    QString serviceId;
    QString workspaceId;
    QString resourceId;
    QString mailbox;
    QString subject;
    QString sender;
    QString receivedAtLabel;
    QString webUrl;
    QString bodyPreview;
    QString htmlBody;
};

struct OutlookCalendarEventResource {
    QString serviceId;
    QString workspaceId;
    QString resourceId;
    QString subject;
    QString organizer;
    QString whenLabel;
    QString location;
    QString webUrl;
    QString changeKey;
    QString lastModifiedLabel;
    bool cancelled = false;
};

class IOutlookAdapter {
public:
    virtual ~IOutlookAdapter() = default;

    virtual QString adapterId() const = 0;
    virtual QString displayName() const = 0;

    virtual QVector<ResourceReference> visibleResourcesForWorkspace(const QString& workspaceId) const = 0;
    virtual std::optional<ResourceRefPayload> payloadForResource(const QString& resourceId) const = 0;
};

namespace OutlookAdapterContracts {

ResourceReference makeMailReference(const OutlookMailResource& resource);
ResourceReference makeCalendarEventReference(const OutlookCalendarEventResource& resource);

ResourceRefPayload makeMailPayload(const OutlookMailResource& resource);
ResourceRefPayload makeCalendarEventPayload(const OutlookCalendarEventResource& resource);

}
