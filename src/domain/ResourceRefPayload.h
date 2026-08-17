#pragma once

#include <QtGlobal>
#include <QString>
#include <QVector>

struct ResourceRefAction {
    QString actionId;
    QString label;
    QString target;
    bool primary = false;
};

struct ResourceRefPayload {
    QString serviceId;
    QString workspaceId;
    QString origin;
    QString kind;
    QString resourceId;
    QString title;
    QString subtitle;
    QString status;
    QVector<ResourceRefAction> actions;
    QString snapshotVersion;
    qint64 updatedAtMs = 0;
};
