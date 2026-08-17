#pragma once

#include <QString>
#include <QStringList>

struct ClientStartupOptions
{
    bool supervised = false;
    bool recoveredFromCrash = false;
    QString recoverySessionId;

    static ClientStartupOptions fromArguments(const QStringList& arguments);

    bool validRecoveryRequest() const;
    bool suppressSplash() const;
    bool shouldRegisterWindowsArr() const;
    QString windowsArrCommandLine() const;
    QString stateSessionId() const;
};
