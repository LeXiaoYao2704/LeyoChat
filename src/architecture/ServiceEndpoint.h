#pragma once

#include <QtGlobal>
#include <QString>

struct ServiceEndpoint {
    QString serviceId;
    QString host;
    quint16 port = 0;
    bool tlsEnabled = false;
    QString routePrefix;
};
