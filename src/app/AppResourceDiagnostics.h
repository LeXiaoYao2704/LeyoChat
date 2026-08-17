#pragma once

#include <QtGlobal>

struct AppResourceSnapshot {
    bool valid = false;
    quint32 handleCount = 0;
    quint32 userObjects = 0;
    quint32 gdiObjects = 0;
    quint32 threadCount = 0;
    quint64 workingSetBytes = 0;
    quint64 privateBytes = 0;
};

AppResourceSnapshot currentAppResourceSnapshot();
void logUserObjects(const char* label);
void logProcessResources(const char* label);
