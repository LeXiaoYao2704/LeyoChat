#pragma once

#include <QString>

namespace UiIcons {

inline QString navMessages()
{
    return QStringLiteral("\u2709");
}

inline QString navChats()
{
    return navMessages();
}

inline QString navContacts()
{
    return QStringLiteral("\u25A6");
}

inline QString navTransfers()
{
    return QStringLiteral("\u21C5");
}

inline QString navGroups()
{
    return QStringLiteral("\u25A3");
}

inline QString navNotifications()
{
    return QStringLiteral("\u26A0");
}

inline QString navKnowledge()
{
    return QStringLiteral("\u2605");
}

inline QString navFerry()
{
    return QStringLiteral("\u21C4");
}

inline QString actionAdd()
{
    return QStringLiteral("+");
}

inline QString actionConnect()
{
    return QStringLiteral("\u2197");
}

inline QString actionFiles()
{
    return QStringLiteral("\u25A4");
}

inline QString actionFile()
{
    return actionFiles();
}

inline QString actionScreenshot()
{
    return QStringLiteral("\u29C9");
}

inline QString actionSearch()
{
    return QStringLiteral("\u2315");
}

inline QString actionMore()
{
    return QStringLiteral("\u22EF");
}

inline QString actionRefresh()
{
    return QStringLiteral("\u21BB");
}

inline QString actionRetry()
{
    return actionRefresh();
}

inline QString actionBack()
{
    return QStringLiteral("\u2190");
}

inline QString actionClose()
{
    return QStringLiteral("\u00D7");
}

} // namespace UiIcons
