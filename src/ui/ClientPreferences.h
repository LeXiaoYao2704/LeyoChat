#pragma once

#include <QString>
#include <QStringList>

#include "ElaDef.h"

enum class ClientCloseAction
{
    AskEveryTime = 0,
    MinimizeToTray = 1,
    ExitApplication = 2
};

enum class ClientWindowBackgroundStyle
{
    Standard = 0,
    BlueMist = 1,
    GreenMist = 2,
    WarmPaper = 3,
    CoolGray = 4,
    Dynamic = 5
};

struct ClientPreferences
{
    ElaThemeType::ThemeMode themeMode{ElaThemeType::Light};
    ElaNavigationType::NavigationDisplayMode navigationDisplayMode{ElaNavigationType::Maximal};
    ElaWindowType::StackSwitchMode stackSwitchMode{ElaWindowType::None};
    ElaWindowType::PaintMode windowPaintMode{ElaWindowType::Normal};
    ClientWindowBackgroundStyle windowBackgroundStyle{ClientWindowBackgroundStyle::Standard};
    ElaApplicationType::WindowDisplayMode windowDisplayMode{ElaApplicationType::Normal};
    bool userInfoCardVisible{true};
    bool initialSetupCompleted{false};
    ClientCloseAction closeWindowAction{ClientCloseAction::AskEveryTime};
    QStringList customNetworkSegments;
};

class ClientPreferencesStore
{
public:
    static ClientPreferences load(const QString& dataRoot);
    static bool save(const QString& dataRoot,
                     const ClientPreferences& preferences,
                     QString* errorMessage = nullptr);
    static QString filePath(const QString& dataRoot);
};
