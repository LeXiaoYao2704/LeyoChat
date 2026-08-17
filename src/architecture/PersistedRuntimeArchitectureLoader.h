#pragma once

#include <QString>

#include "architecture/RuntimeArchitectureSnapshot.h"

class PersistedRuntimeArchitectureLoader {
public:
    explicit PersistedRuntimeArchitectureLoader(QString connectionName);

    RuntimeArchitectureSnapshot loadSnapshot() const;

private:
    QString m_connectionName;
};
