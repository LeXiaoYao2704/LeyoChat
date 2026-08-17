#pragma once

#include <QPixmap>
#include <QSize>

#include "ClientPreferences.h"

class ElaWindow;

int themeModeToIndex(ElaThemeType::ThemeMode themeMode);
ElaThemeType::ThemeMode indexToThemeMode(int index);

int navigationModeToIndex(ElaNavigationType::NavigationDisplayMode mode);
ElaNavigationType::NavigationDisplayMode indexToNavigationMode(int index);

int stackSwitchModeToIndex(ElaWindowType::StackSwitchMode mode);
ElaWindowType::StackSwitchMode indexToStackSwitchMode(int index);

int windowPaintModeToIndex(ElaWindowType::PaintMode mode);
ElaWindowType::PaintMode indexToWindowPaintMode(int index);

int windowBackgroundStyleToIndex(ClientWindowBackgroundStyle style);
ClientWindowBackgroundStyle indexToWindowBackgroundStyle(int index);

int windowDisplayModeToIndex(ElaApplicationType::WindowDisplayMode mode);
ElaApplicationType::WindowDisplayMode indexToWindowDisplayMode(int index);

QPixmap buildWindowEffectPixmap(ElaThemeType::ThemeMode themeMode,
                                ClientWindowBackgroundStyle style = ClientWindowBackgroundStyle::BlueMist,
                                const QSize& size = {});
void applyClientAppearance(ElaWindow* window,
                           const ClientPreferences& preferences,
                           bool applyNavigationChrome);
