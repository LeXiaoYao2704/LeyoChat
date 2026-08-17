#pragma once

#include <QString>

struct ServiceBinding {
    QString boundServiceId;
    bool sharedFilesEnabled = false;
    bool sharedEditingEnabled = false;
    bool connectorsEnabled = false;
};
