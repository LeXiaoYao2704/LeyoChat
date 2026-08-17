#include "app/SystemActivityHelpers.h"

#include <QString>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

qint64 systemIdleMilliseconds() {
#ifdef Q_OS_WIN
    LASTINPUTINFO inputInfo{};
    inputInfo.cbSize = sizeof(LASTINPUTINFO);
    if (GetLastInputInfo(&inputInfo)) {
        return static_cast<qint64>(GetTickCount64() - inputInfo.dwTime);
    }
#endif
    return 0;
}

bool workstationLocked()
{
#ifdef Q_OS_WIN
    HDESK desktop = OpenInputDesktop(0, FALSE, GENERIC_READ);
    if (!desktop) {
        return false;
    }

    WCHAR desktopName[256] = {};
    DWORD requiredBytes = 0;
    bool locked = false;
    if (GetUserObjectInformationW(desktop,
                                  UOI_NAME,
                                  desktopName,
                                  sizeof(desktopName),
                                  &requiredBytes)) {
        const QString name = QString::fromWCharArray(desktopName).trimmed().toLower();
        locked = (name == QStringLiteral("winlogon")
                  || name == QStringLiteral("screen-saver"));
    }
    CloseDesktop(desktop);
    return locked;
#else
    return false;
#endif
}
