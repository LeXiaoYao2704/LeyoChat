#include "ClientAppearance.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QApplication>

#include "AppStyle.h"
#include "ElaApplication.h"
#include "ElaTheme.h"
#include "ElaWindow.h"

namespace {

QString resolveWindowMoviePath(const QString& fileName)
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString deployedPath = appDir.filePath(QStringLiteral("resources/branding/%1").arg(fileName));
    if (QFileInfo::exists(deployedPath))
    {
        return deployedPath;
    }

    const QString sourceTreePath = appDir.filePath(QStringLiteral("../resources/branding/%1").arg(fileName));
    if (QFileInfo::exists(sourceTreePath))
    {
        return QDir::cleanPath(sourceTreePath);
    }

    return QString();
}

const QString& lightWindowMoviePath()
{
    static const QString path = resolveWindowMoviePath(QStringLiteral("window-movie-light.gif"));
    return path;
}

const QString& darkWindowMoviePath()
{
    static const QString path = resolveWindowMoviePath(QStringLiteral("window-movie-dark.gif"));
    return path;
}

void setWindowMoviePathIfNeeded(ElaWindow* window,
                                ElaThemeType::ThemeMode themeMode,
                                const QString& moviePath)
{
    if (!window)
    {
        return;
    }

    if (!moviePath.isEmpty() && window->getWindowMoviePath(themeMode) != moviePath)
    {
        window->setWindowMoviePath(themeMode, moviePath);
    }
}

struct BackgroundPalette
{
    QColor top;
    QColor middle;
    QColor bottom;
    QColor halo;
    QColor edgeGlow;
    QColor watermark;
    QColor frame;
};

BackgroundPalette backgroundPalette(ClientWindowBackgroundStyle style, ElaThemeType::ThemeMode themeMode)
{
    const bool dark = themeMode == ElaThemeType::Dark;
    switch (style)
    {
    case ClientWindowBackgroundStyle::GreenMist:
        return dark
            ? BackgroundPalette{QColor(18, 29, 29), QColor(16, 22, 26), QColor(13, 18, 21),
                                QColor(72, 190, 150, 76), QColor(76, 210, 188, 58),
                                QColor(65, 205, 160, 34), QColor(78, 190, 150, 50)}
            : BackgroundPalette{QColor(235, 248, 244), QColor(252, 254, 252), QColor(242, 249, 246),
                                QColor(85, 185, 145, 42), QColor(100, 205, 185, 34),
                                QColor(80, 200, 160, 28), QColor(70, 160, 135, 30)};
    case ClientWindowBackgroundStyle::WarmPaper:
        return dark
            ? BackgroundPalette{QColor(31, 27, 23), QColor(22, 22, 23), QColor(18, 18, 19),
                                QColor(230, 170, 96, 58), QColor(240, 190, 122, 42),
                                QColor(235, 178, 105, 24), QColor(190, 142, 82, 42)}
            : BackgroundPalette{QColor(250, 246, 238), QColor(255, 254, 250), QColor(247, 244, 238),
                                QColor(236, 184, 112, 36), QColor(244, 210, 155, 28),
                                QColor(235, 188, 120, 22), QColor(186, 148, 92, 28)};
    case ClientWindowBackgroundStyle::CoolGray:
        return dark
            ? BackgroundPalette{QColor(24, 27, 31), QColor(19, 21, 24), QColor(16, 18, 21),
                                QColor(118, 150, 188, 46), QColor(120, 180, 210, 34),
                                QColor(120, 170, 190, 18), QColor(120, 150, 180, 34)}
            : BackgroundPalette{QColor(239, 244, 248), QColor(252, 253, 254), QColor(241, 244, 247),
                                QColor(130, 165, 200, 28), QColor(150, 190, 210, 22),
                                QColor(120, 170, 190, 18), QColor(100, 130, 160, 24)};
    case ClientWindowBackgroundStyle::BlueMist:
    case ClientWindowBackgroundStyle::Standard:
    case ClientWindowBackgroundStyle::Dynamic:
    default:
        return dark
            ? BackgroundPalette{QColor(18, 25, 34), QColor(14, 20, 27), QColor(12, 17, 24),
                                QColor(70, 145, 230, 88), QColor(61, 140, 255, 64),
                                QColor(60, 170, 220, 30), QColor(70, 145, 230, 42)}
            : BackgroundPalette{QColor(232, 244, 255), QColor(250, 253, 255), QColor(241, 248, 252),
                                QColor(80, 160, 230, 52), QColor(18, 186, 214, 38),
                                QColor(68, 205, 215, 30), QColor(39, 135, 210, 34)};
    }
}

ElaWindowType::PaintMode paintModeForBackgroundStyle(ClientWindowBackgroundStyle style)
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

} // namespace

int themeModeToIndex(ElaThemeType::ThemeMode themeMode)
{
    return themeMode == ElaThemeType::Dark ? 1 : 0;
}

ElaThemeType::ThemeMode indexToThemeMode(int index)
{
    return index == 1 ? ElaThemeType::Dark : ElaThemeType::Light;
}

int navigationModeToIndex(ElaNavigationType::NavigationDisplayMode mode)
{
    switch (mode)
    {
    case ElaNavigationType::Maximal:
        return 0;
    case ElaNavigationType::Compact:
        return 1;
    case ElaNavigationType::Auto:
        return 2;
    default:
        return 0;
    }
}

ElaNavigationType::NavigationDisplayMode indexToNavigationMode(int index)
{
    switch (index)
    {
    case 0:
        return ElaNavigationType::Maximal;
    case 1:
        return ElaNavigationType::Compact;
    case 2:
        return ElaNavigationType::Auto;
    default:
        return ElaNavigationType::Maximal;
    }
}

int stackSwitchModeToIndex(ElaWindowType::StackSwitchMode mode)
{
    switch (mode)
    {
    case ElaWindowType::Scale:
        return 1;
    case ElaWindowType::Flip:
        return 2;
    case ElaWindowType::Blur:
        return 3;
    case ElaWindowType::None:
        return 4;
    case ElaWindowType::Popup:
    default:
        return 0;
    }
}

ElaWindowType::StackSwitchMode indexToStackSwitchMode(int index)
{
    switch (index)
    {
    case 1:
        return ElaWindowType::Scale;
    case 2:
        return ElaWindowType::Flip;
    case 3:
        return ElaWindowType::Blur;
    case 4:
        return ElaWindowType::None;
    case 0:
    default:
        return ElaWindowType::Popup;
    }
}

int windowPaintModeToIndex(ElaWindowType::PaintMode mode)
{
    switch (mode)
    {
    case ElaWindowType::Movie:
        return 2;
    case ElaWindowType::Pixmap:
        return 1;
    case ElaWindowType::Normal:
    default:
        return 0;
    }
}

ElaWindowType::PaintMode indexToWindowPaintMode(int index)
{
    switch (index)
    {
    case 2:
        return ElaWindowType::Movie;
    case 1:
        return ElaWindowType::Pixmap;
    case 0:
    default:
        return ElaWindowType::Normal;
    }
}

int windowBackgroundStyleToIndex(ClientWindowBackgroundStyle style)
{
    switch (style)
    {
    case ClientWindowBackgroundStyle::GreenMist:
        return 2;
    case ClientWindowBackgroundStyle::WarmPaper:
        return 3;
    case ClientWindowBackgroundStyle::CoolGray:
        return 4;
    case ClientWindowBackgroundStyle::Dynamic:
        return 5;
    case ClientWindowBackgroundStyle::Standard:
        return 0;
    case ClientWindowBackgroundStyle::BlueMist:
    default:
        return 1;
    }
}

ClientWindowBackgroundStyle indexToWindowBackgroundStyle(int index)
{
    switch (index)
    {
    case 0:
        return ClientWindowBackgroundStyle::Standard;
    case 2:
        return ClientWindowBackgroundStyle::GreenMist;
    case 3:
        return ClientWindowBackgroundStyle::WarmPaper;
    case 4:
        return ClientWindowBackgroundStyle::CoolGray;
    case 5:
        return ClientWindowBackgroundStyle::Dynamic;
    case 1:
    default:
        return ClientWindowBackgroundStyle::BlueMist;
    }
}

int windowDisplayModeToIndex(ElaApplicationType::WindowDisplayMode mode)
{
    switch (mode)
    {
    case ElaApplicationType::ElaMica:
        return 1;
#if defined(Q_OS_WIN)
    case ElaApplicationType::Mica:
        return 2;
    case ElaApplicationType::MicaAlt:
        return 3;
    case ElaApplicationType::Acrylic:
        return 4;
    case ElaApplicationType::DWMBlur:
        return 5;
#endif
    case ElaApplicationType::Normal:
    default:
        return 0;
    }
}

ElaApplicationType::WindowDisplayMode indexToWindowDisplayMode(int index)
{
    switch (index)
    {
    case 1:
        return ElaApplicationType::ElaMica;
#if defined(Q_OS_WIN)
    case 2:
        return ElaApplicationType::Mica;
    case 3:
        return ElaApplicationType::MicaAlt;
    case 4:
        return ElaApplicationType::Acrylic;
    case 5:
        return ElaApplicationType::DWMBlur;
#endif
    case 0:
    default:
        return ElaApplicationType::Normal;
    }
}

QPixmap buildWindowEffectPixmap(ElaThemeType::ThemeMode themeMode, const QSize& size)
{
    const QSize targetSize = size.isValid() ? size : QSize(1600, 1200);
    QPixmap canvas(targetSize);
    canvas.fill(ElaThemeColor(themeMode, WindowBase));

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QColor overlayPrimary = ElaThemeColor(themeMode, PrimaryNormal);
    QColor overlaySecondary = themeMode == ElaThemeType::Light
        ? QColor(18, 186, 214)
        : QColor(61, 140, 255);
    QColor panelTone = ElaThemeColor(themeMode, WindowCentralStackBase);
    panelTone.setAlpha(themeMode == ElaThemeType::Light ? 210 : 188);

    // Layer 1: 对角线性渐变
    QLinearGradient backgroundGradient(0, 0, targetSize.width(), targetSize.height());
    backgroundGradient.setColorAt(0.0, panelTone.lighter(themeMode == ElaThemeType::Light ? 104 : 116));
    backgroundGradient.setColorAt(0.55, ElaThemeColor(themeMode, WindowBase));
    backgroundGradient.setColorAt(1.0, panelTone.darker(themeMode == ElaThemeType::Light ? 104 : 112));
    painter.fillRect(canvas.rect(), backgroundGradient);

    // Layer 2: 左上方径向光晕
    QColor radialColor = overlayPrimary;
    radialColor.setAlpha(themeMode == ElaThemeType::Light ? 72 : 88);
    QRadialGradient halo(QPointF(targetSize.width() * 0.18, targetSize.height() * 0.16), targetSize.width() * 0.42);
    halo.setColorAt(0.0, radialColor);
    halo.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(canvas.rect(), halo);

    // Layer 3: 右下方发光椭圆
    QColor edgeGlow = overlaySecondary;
    edgeGlow.setAlpha(themeMode == ElaThemeType::Light ? 42 : 64);
    painter.setPen(Qt::NoPen);
    painter.setBrush(edgeGlow);
    painter.drawEllipse(QRectF(targetSize.width() * 0.70,
                               targetSize.height() * 0.62,
                               targetSize.width() * 0.36,
                               targetSize.width() * 0.36));

    // Layer 4: 右下角半透明 Logo 水印
    const QPixmap logo(QStringLiteral(":/icons/app-logo.png"));
    if (!logo.isNull())
    {
        const QPixmap scaledLogo = logo.scaled(QSize(520, 520), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.save();
        painter.setOpacity(themeMode == ElaThemeType::Light ? 0.12 : 0.16);
        painter.drawPixmap(targetSize.width() - scaledLogo.width() - 84,
                           targetSize.height() - scaledLogo.height() - 92,
                           scaledLogo);
        painter.restore();
    }

    // Layer 5: 圆角矩形边框
    QColor frameColor = overlayPrimary;
    frameColor.setAlpha(themeMode == ElaThemeType::Light ? 26 : 42);
    painter.setPen(QPen(frameColor, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(34, 30, targetSize.width() - 68, targetSize.height() - 60), 34, 34);
    return canvas;
}

QPixmap buildWindowEffectPixmap(ElaThemeType::ThemeMode themeMode,
                                ClientWindowBackgroundStyle style,
                                const QSize& size)
{
    const QSize targetSize = size.isValid() ? size : QSize(1600, 1200);
    const BackgroundPalette palette = backgroundPalette(style, themeMode);
    QPixmap canvas(targetSize);
    canvas.fill(palette.middle);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QLinearGradient backgroundGradient(0, 0, targetSize.width(), targetSize.height());
    backgroundGradient.setColorAt(0.0, palette.top);
    backgroundGradient.setColorAt(0.56, palette.middle);
    backgroundGradient.setColorAt(1.0, palette.bottom);
    painter.fillRect(canvas.rect(), backgroundGradient);

    QRadialGradient halo(QPointF(targetSize.width() * 0.18, targetSize.height() * 0.16),
                         targetSize.width() * 0.42);
    halo.setColorAt(0.0, palette.halo);
    halo.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(canvas.rect(), halo);

    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.edgeGlow);
    painter.drawEllipse(QRectF(targetSize.width() * 0.70,
                               targetSize.height() * 0.62,
                               targetSize.width() * 0.36,
                               targetSize.width() * 0.36));

    const QPixmap logo(QStringLiteral(":/icons/app-logo.png"));
    if (!logo.isNull())
    {
        const QPixmap scaledLogo = logo.scaled(QSize(520, 520),
                                               Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation);
        painter.save();
        painter.setOpacity(themeMode == ElaThemeType::Light ? 0.10 : 0.14);
        painter.drawPixmap(targetSize.width() - scaledLogo.width() - 84,
                           targetSize.height() - scaledLogo.height() - 92,
                           scaledLogo);
        painter.restore();
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.watermark);
    painter.drawEllipse(QRectF(targetSize.width() * 0.73,
                               targetSize.height() * 0.68,
                               targetSize.width() * 0.28,
                               targetSize.width() * 0.28));

    painter.setPen(QPen(palette.frame, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(34, 30, targetSize.width() - 68, targetSize.height() - 60), 34, 34);
    return canvas;
}

void applyClientAppearance(ElaWindow* window,
                           const ClientPreferences& preferences,
                           bool applyNavigationChrome)
{
    if (!window)
    {
        return;
    }

    const ElaThemeType::ThemeMode currentThemeMode = eTheme->getThemeMode();
    const ElaApplicationType::WindowDisplayMode currentDisplayMode = eApp->getWindowDisplayMode();
    const ElaWindowType::PaintMode currentPaintMode = window->getWindowPaintMode();
    const ElaWindowType::PaintMode targetPaintMode = paintModeForBackgroundStyle(preferences.windowBackgroundStyle);
    const bool themeModeChanged = currentThemeMode != preferences.themeMode;
    const bool displayModeChanged = currentDisplayMode != preferences.windowDisplayMode;
    const bool paintModeChanged = currentPaintMode != targetPaintMode;

    window->setWindowPixmap(ElaThemeType::Light,
                            buildWindowEffectPixmap(ElaThemeType::Light, preferences.windowBackgroundStyle));
    window->setWindowPixmap(ElaThemeType::Dark,
                            buildWindowEffectPixmap(ElaThemeType::Dark, preferences.windowBackgroundStyle));
    setWindowMoviePathIfNeeded(window, ElaThemeType::Light, lightWindowMoviePath());
    setWindowMoviePathIfNeeded(window, ElaThemeType::Dark, darkWindowMoviePath());

    if (themeModeChanged)
    {
        eTheme->setThemeMode(preferences.themeMode);
    }
    if (qApp)
    {
        const AppStyle::ThemeMode appMode = AppStyle::themeModeFromEla(preferences.themeMode);
        qApp->setProperty("leyochat.themeMode", AppStyle::themeModeToString(appMode));
        qApp->setPalette(AppStyle::applicationPalette(appMode));
        // 全局 QToolTip 样式：确保深色模式下 tooltip 可读
        qApp->setStyleSheet(QStringLiteral(
            "QToolTip { color: %1; background-color: %2; border: 1px solid %3; padding: 4px; }")
            .arg(AppStyle::textPrimary(appMode),
                 AppStyle::surface(appMode),
                 AppStyle::border(appMode)));
    }

    if (displayModeChanged)
    {
        eApp->setWindowDisplayMode(preferences.windowDisplayMode);
    }

    if (paintModeChanged || (displayModeChanged && targetPaintMode == ElaWindowType::Movie))
    {
        window->setWindowPaintMode(targetPaintMode);
    }

    window->setStackSwitchMode(preferences.stackSwitchMode);
    if (applyNavigationChrome)
    {
        window->setNavigationBarDisplayMode(preferences.navigationDisplayMode);
        window->setUserInfoCardVisible(preferences.userInfoCardVisible);
    }
}
