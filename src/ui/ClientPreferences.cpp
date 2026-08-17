#include "ClientPreferences.h"

#include "app/AppSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace {

QString preferencesFileName()
{
    return QStringLiteral("client-preferences.json");
}

ElaThemeType::ThemeMode themeModeFromValue(int value)
{
    switch (value)
    {
    case ElaThemeType::Dark:
        return ElaThemeType::Dark;
    case ElaThemeType::Light:
    default:
        return ElaThemeType::Light;
    }
}

ElaThemeType::ThemeMode themeModeFromSettingsValue(const QVariant& value,
                                                   ElaThemeType::ThemeMode fallback)
{
    if (!value.isValid())
    {
        return fallback;
    }

    const QString text = value.toString().trimmed().toLower();
    if (text == QStringLiteral("dark"))
    {
        return ElaThemeType::Dark;
    }
    if (text == QStringLiteral("light"))
    {
        return ElaThemeType::Light;
    }
    bool ok = false;
    const int numericValue = value.toInt(&ok);
    return ok ? themeModeFromValue(numericValue) : fallback;
}

ElaNavigationType::NavigationDisplayMode navigationModeFromValue(int value)
{
    switch (value)
    {
    case ElaNavigationType::Compact:
        return ElaNavigationType::Compact;
    case ElaNavigationType::Maximal:
        return ElaNavigationType::Maximal;
    case ElaNavigationType::Auto:
        return ElaNavigationType::Auto;
    default:
        return ElaNavigationType::Maximal;
    }
}

ElaWindowType::StackSwitchMode stackSwitchModeFromValue(int value)
{
    switch (value)
    {
    case ElaWindowType::None:
        return ElaWindowType::None;
    case ElaWindowType::Scale:
        return ElaWindowType::Scale;
    case ElaWindowType::Flip:
        return ElaWindowType::Flip;
    case ElaWindowType::Blur:
        return ElaWindowType::Blur;
    case ElaWindowType::Popup:
    default:
        return ElaWindowType::Popup;
    }
}

ElaWindowType::PaintMode windowPaintModeFromValue(int value)
{
    switch (value)
    {
    case ElaWindowType::Pixmap:
        return ElaWindowType::Pixmap;
    case ElaWindowType::Movie:
        return ElaWindowType::Movie;
    case ElaWindowType::Normal:
    default:
        return ElaWindowType::Normal;
    }
}

ClientWindowBackgroundStyle windowBackgroundStyleFromValue(int value)
{
    switch (value)
    {
    case static_cast<int>(ClientWindowBackgroundStyle::Standard):
        return ClientWindowBackgroundStyle::Standard;
    case static_cast<int>(ClientWindowBackgroundStyle::GreenMist):
        return ClientWindowBackgroundStyle::GreenMist;
    case static_cast<int>(ClientWindowBackgroundStyle::WarmPaper):
        return ClientWindowBackgroundStyle::WarmPaper;
    case static_cast<int>(ClientWindowBackgroundStyle::CoolGray):
        return ClientWindowBackgroundStyle::CoolGray;
    case static_cast<int>(ClientWindowBackgroundStyle::Dynamic):
        return ClientWindowBackgroundStyle::Dynamic;
    case static_cast<int>(ClientWindowBackgroundStyle::BlueMist):
    default:
        return ClientWindowBackgroundStyle::BlueMist;
    }
}

ClientWindowBackgroundStyle backgroundStyleFromPaintMode(ElaWindowType::PaintMode mode)
{
    switch (mode)
    {
    case ElaWindowType::Normal:
        return ClientWindowBackgroundStyle::Standard;
    case ElaWindowType::Movie:
        return ClientWindowBackgroundStyle::Dynamic;
    case ElaWindowType::Pixmap:
    default:
        return ClientWindowBackgroundStyle::BlueMist;
    }
}

ElaWindowType::PaintMode paintModeFromBackgroundStyle(ClientWindowBackgroundStyle style)
{
    switch (style)
    {
    case ClientWindowBackgroundStyle::Standard:
        return ElaWindowType::Normal;
    case ClientWindowBackgroundStyle::Dynamic:
        return ElaWindowType::Movie;
    case ClientWindowBackgroundStyle::BlueMist:
    case ClientWindowBackgroundStyle::GreenMist:
    case ClientWindowBackgroundStyle::WarmPaper:
    case ClientWindowBackgroundStyle::CoolGray:
    default:
        return ElaWindowType::Pixmap;
    }
}

ElaApplicationType::WindowDisplayMode windowDisplayModeFromValue(int value)
{
    switch (value)
    {
    case ElaApplicationType::ElaMica:
        return ElaApplicationType::ElaMica;
#if defined(Q_OS_WIN)
    case ElaApplicationType::Mica:
        return ElaApplicationType::Mica;
    case ElaApplicationType::MicaAlt:
        return ElaApplicationType::MicaAlt;
    case ElaApplicationType::Acrylic:
        return ElaApplicationType::Acrylic;
    case ElaApplicationType::DWMBlur:
        return ElaApplicationType::DWMBlur;
#endif
    case ElaApplicationType::Normal:
    default:
        return ElaApplicationType::Normal;
    }
}

ClientCloseAction closeWindowActionFromValue(int value)
{
    switch (value)
    {
    case static_cast<int>(ClientCloseAction::MinimizeToTray):
        return ClientCloseAction::MinimizeToTray;
    case static_cast<int>(ClientCloseAction::ExitApplication):
        return ClientCloseAction::ExitApplication;
    case static_cast<int>(ClientCloseAction::AskEveryTime):
    default:
        return ClientCloseAction::AskEveryTime;
    }
}

void overlayQSettingsPreferences(ClientPreferences& preferences)
{
    QSettings settings(AppSettings::organizationName(), AppSettings::applicationName());
    if (settings.contains(QStringLiteral("appearance/themeMode")))
    {
        preferences.themeMode = themeModeFromSettingsValue(
            settings.value(QStringLiteral("appearance/themeMode")),
            preferences.themeMode);
    }
    if (settings.contains(QStringLiteral("appearance/navigationDisplayMode")))
    {
        preferences.navigationDisplayMode = navigationModeFromValue(
            settings.value(QStringLiteral("appearance/navigationDisplayMode")).toInt());
    }
    if (settings.contains(QStringLiteral("appearance/stackSwitchMode")))
    {
        preferences.stackSwitchMode = stackSwitchModeFromValue(
            settings.value(QStringLiteral("appearance/stackSwitchMode")).toInt());
    }
    if (settings.contains(QStringLiteral("appearance/windowPaintMode")))
    {
        preferences.windowPaintMode = windowPaintModeFromValue(
            settings.value(QStringLiteral("appearance/windowPaintMode")).toInt());
        preferences.windowBackgroundStyle = backgroundStyleFromPaintMode(preferences.windowPaintMode);
    }
    if (settings.contains(QStringLiteral("appearance/windowBackgroundStyle")))
    {
        preferences.windowBackgroundStyle = windowBackgroundStyleFromValue(
            settings.value(QStringLiteral("appearance/windowBackgroundStyle")).toInt());
        preferences.windowPaintMode = paintModeFromBackgroundStyle(preferences.windowBackgroundStyle);
    }
    if (settings.contains(QStringLiteral("appearance/windowDisplayMode")))
    {
        preferences.windowDisplayMode = windowDisplayModeFromValue(
            settings.value(QStringLiteral("appearance/windowDisplayMode")).toInt());
    }
    if (settings.contains(QStringLiteral("appearance/userInfoCardVisible")))
    {
        preferences.userInfoCardVisible =
            settings.value(QStringLiteral("appearance/userInfoCardVisible")).toBool();
    }
}

void saveQSettingsDefaults(const ClientPreferences& preferences)
{
    QSettings settings(AppSettings::organizationName(), AppSettings::applicationName());
    settings.setValue(QStringLiteral("appearance/themeMode"),
                      preferences.themeMode == ElaThemeType::Dark
                          ? QStringLiteral("dark")
                          : QStringLiteral("light"));
    settings.setValue(QStringLiteral("appearance/navigationDisplayMode"),
                      static_cast<int>(preferences.navigationDisplayMode));
    settings.setValue(QStringLiteral("appearance/stackSwitchMode"),
                      static_cast<int>(preferences.stackSwitchMode));
    settings.setValue(QStringLiteral("appearance/windowPaintMode"),
                      static_cast<int>(preferences.windowPaintMode));
    settings.setValue(QStringLiteral("appearance/windowBackgroundStyle"),
                      static_cast<int>(preferences.windowBackgroundStyle));
    settings.setValue(QStringLiteral("appearance/windowDisplayMode"),
                      static_cast<int>(preferences.windowDisplayMode));
    settings.setValue(QStringLiteral("appearance/userInfoCardVisible"),
                      preferences.userInfoCardVisible);
    settings.sync();
}

QStringList stringListFromJsonArray(const QJsonValue& value)
{
    QStringList values;
    const QJsonArray array = value.toArray();
    values.reserve(array.size());
    for (const QJsonValue& item : array)
    {
        const QString text = item.toString().trimmed();
        if (!text.isEmpty())
        {
            values.append(text);
        }
    }
    return values;
}

QJsonArray stringListToJsonArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values)
    {
        if (!value.trimmed().isEmpty())
        {
            array.append(value.trimmed());
        }
    }
    return array;
}

} // namespace

ClientPreferences ClientPreferencesStore::load(const QString& dataRoot)
{
    ClientPreferences preferences;

    QFile preferencesFile(filePath(dataRoot));
    if (!preferencesFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        overlayQSettingsPreferences(preferences);
        return preferences;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(preferencesFile.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        overlayQSettingsPreferences(preferences);
        return preferences;
    }

    const QJsonObject object = document.object();
    preferences.themeMode = themeModeFromValue(object.value(QStringLiteral("themeMode")).toInt(preferences.themeMode));
    preferences.navigationDisplayMode = navigationModeFromValue(
        object.value(QStringLiteral("navigationDisplayMode")).toInt(preferences.navigationDisplayMode));
    preferences.stackSwitchMode = stackSwitchModeFromValue(
        object.value(QStringLiteral("stackSwitchMode")).toInt(preferences.stackSwitchMode));
    const bool hasBackgroundStyle = object.contains(QStringLiteral("windowBackgroundStyle"));
    preferences.windowPaintMode = windowPaintModeFromValue(
        object.value(QStringLiteral("windowPaintMode")).toInt(preferences.windowPaintMode));
    preferences.windowBackgroundStyle = hasBackgroundStyle
        ? windowBackgroundStyleFromValue(object.value(QStringLiteral("windowBackgroundStyle")).toInt(
              static_cast<int>(preferences.windowBackgroundStyle)))
        : backgroundStyleFromPaintMode(preferences.windowPaintMode);
    preferences.windowPaintMode = paintModeFromBackgroundStyle(preferences.windowBackgroundStyle);
    preferences.windowDisplayMode = windowDisplayModeFromValue(
        object.value(QStringLiteral("windowDisplayMode")).toInt(preferences.windowDisplayMode));
    preferences.userInfoCardVisible = object.value(QStringLiteral("userInfoCardVisible")).toBool(preferences.userInfoCardVisible);
    preferences.initialSetupCompleted = object.value(QStringLiteral("initialSetupCompleted")).toBool(false);
    preferences.closeWindowAction = closeWindowActionFromValue(
        object.value(QStringLiteral("closeWindowAction")).toInt(static_cast<int>(preferences.closeWindowAction)));
    preferences.customNetworkSegments = stringListFromJsonArray(object.value(QStringLiteral("customNetworkSegments")));
    overlayQSettingsPreferences(preferences);
    return preferences;
}

bool ClientPreferencesStore::save(const QString& dataRoot,
                                  const ClientPreferences& preferences,
                                  QString* errorMessage)
{
    QDir().mkpath(dataRoot);

    QJsonObject object;
    object.insert(QStringLiteral("themeMode"), preferences.themeMode);
    object.insert(QStringLiteral("navigationDisplayMode"), preferences.navigationDisplayMode);
    object.insert(QStringLiteral("stackSwitchMode"), preferences.stackSwitchMode);
    object.insert(QStringLiteral("windowPaintMode"), preferences.windowPaintMode);
    object.insert(QStringLiteral("windowBackgroundStyle"), static_cast<int>(preferences.windowBackgroundStyle));
    object.insert(QStringLiteral("windowDisplayMode"), preferences.windowDisplayMode);
    object.insert(QStringLiteral("userInfoCardVisible"), preferences.userInfoCardVisible);
    object.insert(QStringLiteral("initialSetupCompleted"), preferences.initialSetupCompleted);
    object.insert(QStringLiteral("closeWindowAction"), static_cast<int>(preferences.closeWindowAction));
    object.insert(QStringLiteral("customNetworkSegments"), stringListToJsonArray(preferences.customNetworkSegments));

    saveQSettingsDefaults(preferences);

    QFile file(filePath(dataRoot));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("无法打开文件：%1").arg(file.fileName());
        }
        return false;
    }

    if (file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0 || !file.flush())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("无法写入文件：%1").arg(file.fileName());
        }
        return false;
    }
    file.close();
    return file.error() == QFile::NoError;
}

QString ClientPreferencesStore::filePath(const QString& dataRoot)
{
    return QDir(dataRoot).filePath(QStringLiteral("client-preferences.json"));
}
