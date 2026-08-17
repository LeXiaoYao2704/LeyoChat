#pragma once

#include <QString>

enum class ResourceOrigin {
    Local,
    Service
};

struct ResourceReference {
    QString serviceId;
    QString workspaceId;
    QString resourceId;
    QString resourceKind;
    QString title;
    QString version;
    QString summary;
    ResourceOrigin origin = ResourceOrigin::Local;
};
