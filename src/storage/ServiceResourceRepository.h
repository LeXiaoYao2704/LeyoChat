#pragma once

#include <QString>
#include <QVector>

#include "architecture/ResourceReference.h"

class ServiceResourceRepository {
public:
    explicit ServiceResourceRepository(QString connectionName);

    bool replaceResources(const QVector<ResourceReference>& resources) const;
    bool upsertResource(const ResourceReference& resource) const;
    QVector<ResourceReference> loadResources() const;

private:
    QString m_connectionName;
};
