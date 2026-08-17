#include "integrations/SharedFileResourceContracts.h"

namespace SharedFileResourceContracts {

ResourceReference makeReference(const SharedFileResource& resource)
{
    ResourceReference reference;
    reference.serviceId = resource.serviceId.trimmed();
    reference.workspaceId = resource.workspaceId.trimmed();
    reference.resourceId = resource.resourceId.trimmed();
    reference.resourceKind = QStringLiteral("shared_file");
    reference.title = resource.title.trimmed();
    reference.version = resource.version.trimmed();
    reference.summary = resource.summary.trimmed().isEmpty()
        ? resource.ownerName.trimmed()
        : resource.summary.trimmed();
    reference.origin = ResourceOrigin::Service;
    return reference;
}

ResourceRefPayload makePayload(const SharedFileResource& resource)
{
    const ResourceReference reference = makeReference(resource);

    ResourceRefPayload payload;
    payload.serviceId = reference.serviceId;
    payload.workspaceId = reference.workspaceId;
    payload.origin = QStringLiteral("service");
    payload.kind = reference.resourceKind;
    payload.resourceId = reference.resourceId;
    payload.title = reference.title;
    payload.subtitle = reference.summary;
    payload.status = reference.version;
    payload.snapshotVersion = QStringLiteral("v1");

    if (!resource.downloadTarget.trimmed().isEmpty()) {
        payload.actions.push_back(ResourceRefAction{
            QStringLiteral("download"),
            QStringLiteral("下载"),
            resource.downloadTarget.trimmed(),
            true,
        });
    }
    if (!resource.openTarget.trimmed().isEmpty()) {
        payload.actions.push_back(ResourceRefAction{
            QStringLiteral("open"),
            QStringLiteral("打开"),
            resource.openTarget.trimmed(),
            false,
        });
    }

    return payload;
}

}
