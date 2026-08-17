#pragma once

#include <QString>

#include "architecture/ResourceReference.h"
#include "domain/ResourceRefPayload.h"

struct SharedFileResource {
    QString serviceId;
    QString workspaceId;
    QString resourceId;
    QString title;
    QString ownerName;
    QString version;
    QString summary;
    QString downloadTarget;
    QString openTarget;
    qint64 sizeBytes = 0;
};

namespace SharedFileResourceContracts {

ResourceReference makeReference(const SharedFileResource& resource);
ResourceRefPayload makePayload(const SharedFileResource& resource);

}
