#pragma once

#include "app/AppSettings.h"

#include <ElaTheme.h>

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>
#include <QString>
#include <QtGlobal>

namespace AppStyle {

enum class ThemeMode {
    FollowSystem,
    Light,
    Dark
};

inline ThemeMode themeModeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("dark")) {
        return ThemeMode::Dark;
    }
    if (normalized == QStringLiteral("light")) {
        return ThemeMode::Light;
    }
    if (normalized == QStringLiteral("system")) {
        return ThemeMode::FollowSystem;
    }
    return ThemeMode::Light;
}

inline ThemeMode themeModeFromEla(ElaThemeType::ThemeMode mode)
{
    return mode == ElaThemeType::Dark ? ThemeMode::Dark : ThemeMode::Light;
}

inline QString themeModeToString(ThemeMode mode)
{
    switch (mode) {
    case ThemeMode::Light:
        return QStringLiteral("light");
    case ThemeMode::Dark:
        return QStringLiteral("dark");
    case ThemeMode::FollowSystem:
    default:
        return QStringLiteral("system");
    }
}

inline ThemeMode storedThemeMode()
{
    QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
    return themeModeFromString(cfg.value(QStringLiteral("appearance/themeMode"),
                                         QStringLiteral("light")).toString());
}

inline ThemeMode currentThemeMode()
{
    if (qApp) {
        const QVariant propertyValue = qApp->property("leyochat.themeMode");
        if (propertyValue.isValid()) {
            return themeModeFromString(propertyValue.toString());
        }
    }
    return storedThemeMode();
}

inline bool followsSystemTheme(ThemeMode mode = currentThemeMode())
{
    return mode == ThemeMode::FollowSystem;
}

inline bool isDarkTheme(ThemeMode mode = currentThemeMode())
{
    if (mode == ThemeMode::Dark) {
        return true;
    }
    if (mode == ThemeMode::Light) {
        return false;
    }
    return qApp && qApp->styleHints()
           && qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

// ElaTheme bridge — 从 ElaTheme 取颜色并返回 hex 字符串
inline QString elaColor(ElaThemeType::ThemeColor color, ThemeMode mode = currentThemeMode())
{
    auto elaMode = isDarkTheme(mode) ? ElaThemeType::Dark : ElaThemeType::Light;
    return eTheme->getThemeColor(elaMode, color).name();
}

inline ElaThemeType::ThemeMode toElaThemeMode(ThemeMode mode = currentThemeMode())
{
    return isDarkTheme(mode) ? ElaThemeType::Dark : ElaThemeType::Light;
}

inline constexpr int kSpace2  = 2;
inline constexpr int kSpace4  = 4;
inline constexpr int kSpace6  = 6;
inline constexpr int kSpace8  = 8;
inline constexpr int kSpace10 = 10;
inline constexpr int kSpace12 = 12;
inline constexpr int kSpace14 = 14;
inline constexpr int kSpace16 = 16;
inline constexpr int kSpace20 = 20;
inline constexpr int kSpace24 = 24;

inline constexpr int kRadiusSm = 6;
inline constexpr int kRadiusMd = 10;
inline constexpr int kRadiusLg = 12;
inline constexpr int kRadiusXl = 16;
inline constexpr int kRadiusPill = 999;

inline constexpr int kConversationRowHeight = 68;
inline constexpr int kContactRowHeight = 52;
inline constexpr int kTransferRowHeight = 56;
inline constexpr int kAvatarSize = 40;
inline constexpr int kBubbleRadius = kRadiusLg;
inline constexpr int kBubbleMaxWidthPct = 72;

inline QString accent(ThemeMode mode = currentThemeMode())        { return elaColor(ElaThemeType::PrimaryNormal, mode); }
inline QString accentHover(ThemeMode mode = currentThemeMode())   { return elaColor(ElaThemeType::PrimaryHover, mode); }
inline QString accentPressed(ThemeMode mode = currentThemeMode()) { return elaColor(ElaThemeType::PrimaryPress, mode); }
inline QString accentSoft(ThemeMode mode = currentThemeMode())    { return elaColor(ElaThemeType::PrimaryHover, mode); }
inline QString windowBg(ThemeMode mode = currentThemeMode())      { return elaColor(ElaThemeType::WindowCentralStackBase, mode); }
inline QString surface(ThemeMode mode = currentThemeMode())       { return elaColor(ElaThemeType::WindowBase, mode); }
inline QString surfaceAlt(ThemeMode mode = currentThemeMode())    { return elaColor(ElaThemeType::BasicBase, mode); }
inline QString surfaceMuted(ThemeMode mode = currentThemeMode())  { return elaColor(ElaThemeType::BasicBaseDeep, mode); }
inline QString border(ThemeMode mode = currentThemeMode())        { return elaColor(ElaThemeType::BasicBorder, mode); }
inline QString borderStrong(ThemeMode mode = currentThemeMode())  { return elaColor(ElaThemeType::BasicBorderDeep, mode); }
inline QString workspaceBase(ThemeMode mode = currentThemeMode()) { return windowBg(mode); }
inline QString panelBase(ThemeMode mode = currentThemeMode())     { return surface(mode); }
inline QString panelRaised(ThemeMode mode = currentThemeMode())   { return surfaceAlt(mode); }
inline QString panelOverlay(ThemeMode mode = currentThemeMode())  { return surfaceMuted(mode); }
inline QString dividerSubtle(ThemeMode mode = currentThemeMode()) { return border(mode); }
inline QString dividerStrong(ThemeMode mode = currentThemeMode()) { return borderStrong(mode); }

// ── 独立面板 token ──────────────────────────────────────────────────
// 不绑定 Ela 的 WindowBase / BasicBase，可自由调整而不影响框架控件。
// 深色值对齐 EchoChat 测试过的暗面板色。
inline QString chatStageBg(ThemeMode mode = currentThemeMode()) {
    Q_UNUSED(mode);
    // EchoChat 的会话页让 ElaWindow 的窗口效果图承担大面积背景。
    // 这里保持透明，只让搜索框、卡片、气泡等局部控件自己上色。
    return QStringLiteral("transparent");
}
inline QString chatCardBg(ThemeMode mode = currentThemeMode()) {
    // Header 卡片、Composer 卡片等浮起面板的底色。
    return isDarkTheme(mode) ? QStringLiteral("rgba(30,34,40,196)") : QStringLiteral("rgba(255,255,255,206)");
}
inline QString hoverBg(ThemeMode mode = currentThemeMode())       { return elaColor(ElaThemeType::BasicHover, mode); }
inline QString selectedBg(ThemeMode mode = currentThemeMode())    { return elaColor(ElaThemeType::BasicSelectedHover, mode); }
inline QString textPrimary(ThemeMode mode = currentThemeMode())   { return elaColor(ElaThemeType::BasicText, mode); }
inline QString textSecondary(ThemeMode mode = currentThemeMode()) { return elaColor(ElaThemeType::BasicDetailsText, mode); }
inline QString textMuted(ThemeMode mode = currentThemeMode())     { return elaColor(ElaThemeType::BasicTextNoFocus, mode); }
inline QString success(ThemeMode mode = currentThemeMode())       {
    return isDarkTheme(mode) ? QStringLiteral("#4FC978") : QStringLiteral("#00C853");
}
inline QString danger(ThemeMode mode = currentThemeMode())        { return elaColor(ElaThemeType::StatusDanger, mode); }
inline QString warning(ThemeMode mode = currentThemeMode())       {
    return isDarkTheme(mode) ? QStringLiteral("#F7B84B") : QStringLiteral("#F59E0B");
}
inline QString bubbleOut(ThemeMode mode = currentThemeMode())     {
    // Echo style: light = QColor(205,230,255), dark = primary.lighter(125)
    if (isDarkTheme(mode)) {
        QColor c = ElaThemeColor(ElaThemeType::Dark, PrimaryNormal);
        return c.lighter(125).name();
    }
    return QColor(205, 230, 255).name();
}
inline QString bubbleIn(ThemeMode mode = currentThemeMode())      {
    // Echo style: light = QColor(231,243,255), dark = BasicHover (panelSoftBackground)
    if (isDarkTheme(mode)) {
        return elaColor(ElaThemeType::BasicHover, mode);
    }
    return QColor(231, 243, 255).name();
}
inline QString bubbleOutText(ThemeMode mode = currentThemeMode()) {
    // Echo style: light = text (dark on light bubble), dark = invertedText (white on colored bubble)
    if (isDarkTheme(mode)) {
        return elaColor(ElaThemeType::BasicTextInvert, mode);
    }
    return elaColor(ElaThemeType::BasicText, mode);
}
inline QString bubbleInText(ThemeMode mode = currentThemeMode())  { return elaColor(ElaThemeType::BasicText, mode); }
inline QString bubbleOutBorder(ThemeMode mode = currentThemeMode()) {
    // Echo style: light = QColor(173,210,243), dark = PrimaryNormal
    if (isDarkTheme(mode)) {
        return elaColor(ElaThemeType::PrimaryNormal, mode);
    }
    return QColor(173, 210, 243).name();
}
inline QString bubbleInBorder(ThemeMode mode = currentThemeMode())  {
    // Echo style: light = QColor(173,210,243), dark = BasicBorderHover
    if (isDarkTheme(mode)) {
        return elaColor(ElaThemeType::BasicBorderHover, mode);
    }
    return QColor(173, 210, 243).name();
}
inline QString navBg(ThemeMode mode = currentThemeMode())         { return elaColor(ElaThemeType::WindowBase, mode); }

inline QPalette applicationPalette(ThemeMode mode = currentThemeMode())
{
    QPalette palette;
    const auto em = toElaThemeMode(mode);
    palette.setColor(QPalette::Window, QColor(Qt::transparent));
    palette.setColor(QPalette::WindowText, eTheme->getThemeColor(em, ElaThemeType::BasicText));
    palette.setColor(QPalette::Base, QColor(Qt::transparent));
    palette.setColor(QPalette::AlternateBase, eTheme->getThemeColor(em, ElaThemeType::BasicBase));
    palette.setColor(QPalette::ToolTipBase, eTheme->getThemeColor(em, ElaThemeType::PopupBase));
    palette.setColor(QPalette::ToolTipText, eTheme->getThemeColor(em, ElaThemeType::BasicText));
    palette.setColor(QPalette::Text, eTheme->getThemeColor(em, ElaThemeType::BasicText));
    palette.setColor(QPalette::Button, eTheme->getThemeColor(em, ElaThemeType::BasicBase));
    palette.setColor(QPalette::ButtonText, eTheme->getThemeColor(em, ElaThemeType::BasicText));
    palette.setColor(QPalette::BrightText, QColor("#FFFFFF"));
    palette.setColor(QPalette::Highlight, eTheme->getThemeColor(em, ElaThemeType::PrimaryNormal));
    palette.setColor(QPalette::HighlightedText, eTheme->getThemeColor(em, ElaThemeType::BasicTextInvert));
    palette.setColor(QPalette::PlaceholderText, eTheme->getThemeColor(em, ElaThemeType::BasicTextNoFocus));
    return palette;
}

inline QFont bodyFont(const QFont& base)
{
    return base;
}

inline QFont strongFont(const QFont& base)
{
    QFont font(base);
    font.setBold(true);
    return font;
}

inline QFont titleFont(const QFont& base)
{
    QFont font(base);
    font.setBold(true);
    if (font.pointSizeF() > 0.0) {
        font.setPointSizeF(font.pointSizeF() + 1.0);
    } else if (font.pixelSize() > 0) {
        font.setPixelSize(font.pixelSize() + 1);
    }
    return font;
}

inline QFont captionFont(const QFont& base)
{
    QFont font(base);
    if (font.pointSizeF() > 0.0) {
        font.setPointSizeF(qMax(8.0, font.pointSizeF() - 1.0));
    } else if (font.pixelSize() > 0) {
        font.setPixelSize(qMax(8, font.pixelSize() - 1));
    }
    return font;
}

inline QFont navFont(const QFont& base)
{
    QFont font(base);
    font.setWeight(QFont::Bold);
    if (font.pointSizeF() > 0.0) {
        font.setPointSizeF(qMax(12.0, font.pointSizeF() + 0.5));
    } else if (font.pixelSize() > 0) {
        font.setPixelSize(qMax(12, font.pixelSize() + 1));
    }
    return font;
}

inline QFont primaryFont(const QFont& base, bool bold = false)
{
    QFont font = bodyFont(base);
    font.setBold(bold);
    return font;
}

inline QFont secondaryFont(const QFont& base)
{
    return captionFont(base);
}

inline int avatarSizeForFont(const QFont& base)
{
    const int fontHeight = QFontMetrics(strongFont(base)).height();
    return qMax(kAvatarSize, fontHeight + kSpace14);
}

inline int conversationRowHeightForFont(const QFont& base)
{
    const int titleHeight = QFontMetrics(titleFont(base)).height();
    const int previewHeight = QFontMetrics(captionFont(base)).height();
    const int contentHeight = titleHeight + kSpace4 + previewHeight;
    const int avatarHeight = avatarSizeForFont(base);
    const int scalePadding =
        qMax(0, titleHeight - 15) + qMax(0, previewHeight - 10) + qMax(0, QFontMetrics(base).height() - 14);
    return qMax(kConversationRowHeight,
                kSpace20 + qMax(contentHeight, avatarHeight) + scalePadding);
}

inline int contactRowHeightForFont(const QFont& base)
{
    const int titleHeight = QFontMetrics(titleFont(base)).height();
    const int statusHeight = QFontMetrics(captionFont(base)).height();
    const int contentHeight = titleHeight + kSpace4 + statusHeight;
    const int scalePadding = qMax(0, QFontMetrics(base).height() - 16);
    return qMax(kContactRowHeight, kSpace20 + contentHeight + scalePadding);
}

inline int transferRowHeightForFont(const QFont& base)
{
    const int titleHeight = QFontMetrics(titleFont(base)).height();
    const int statusHeight = QFontMetrics(captionFont(base)).height();
    const int iconHeight = qMax(28, titleHeight + kSpace10);
    const int contentHeight = titleHeight + kSpace4 + statusHeight;
    const int scalePadding = qMax(0, QFontMetrics(base).height() - 16);
    return qMax(kTransferRowHeight, kSpace20 + qMax(contentHeight, iconHeight) + scalePadding);
}

inline int iconButtonSizeForFont(const QFont& base)
{
    const int iconHeight = QFontMetrics(navFont(base)).height();
    return qMax(32, iconHeight + kSpace12);
}

inline int sidebarHeaderHeightForFont(const QFont& base)
{
    const int titleHeight = QFontMetrics(titleFont(base)).height();
    return qMax(48, titleHeight + kSpace16);
}

// 前向声明 (stylesheet 内部使用)
inline QString stylesheet_welcomePage(ThemeMode mode)
{
    const bool dark = isDarkTheme(mode);
    const QString txt  = textPrimary(mode);
    const QString txt2 = textSecondary(mode);
    const QString txtM = textMuted(mode);
    const QString brd  = border(mode);
    const QString acc  = accent(mode);
    const QString sMuted = surfaceMuted(mode);
    const QString pageBg = QStringLiteral("transparent");
    const QString shellBg = dark
        ? QStringLiteral("rgba(24,28,34,38)")
        : QStringLiteral("rgba(255,255,255,44)");
    const QString cardBg = dark
        ? QStringLiteral("rgba(28,30,34,218)")
        : QStringLiteral("rgba(255,255,255,218)");
    const QString subtleBg = dark
        ? QStringLiteral("rgba(35,38,43,206)")
        : QStringLiteral("rgba(247,250,254,206)");
    const QString innerCardBg = dark
        ? QStringLiteral("rgba(45,48,54,190)")
        : QStringLiteral("rgba(244,247,251,205)");
    const QString chipBg = dark
        ? QStringLiteral("rgba(58,62,70,178)")
        : QStringLiteral("rgba(236,242,250,178)");

    return QStringLiteral(
        "QWidget#welcomePage, QWidget#welcomePage QWidget { background:%1; }"
        "QFrame#welcomeHeroShell { background:%2; border:1px solid %3; border-radius:18px; }"
        "QFrame#welcomeHeroChrome { background:%4; border:1px solid %3; border-radius:10px; }"
        "QFrame#welcomeHeroStage { background:transparent; border:none; }"
        "QFrame#welcomeCard, QFrame#welcomePreviewCard, QFrame#welcomeAtmospherePanel,"
        "QFrame#welcomeAtmosphereSignalCard, QFrame#welcomeAtmosphereSpotlight,"
        "QFrame#welcomeAtmosphereDockCard, QFrame#welcomeMetricCard {"
        "  background:%5; border:1px solid %3; border-radius:14px;"
        "}"
        "QFrame#welcomePreviewCard, QFrame#welcomeAtmosphereSpotlight { background:%4; }"
        "QFrame#welcomeAtmosphereSignalCard, QFrame#welcomeMetricCard, QFrame#welcomeAtmosphereDockCard {"
        "  background:%6; border-color:%3; border-radius:10px;"
        "}"
        "QFrame#welcomeHeroChromeDot { background:%7; border-radius:4px; }"
        "QFrame#welcomeHeroChromeDot[chromeTone=\"accent\"] { background:%8; }"
        "QFrame#welcomeHeroChromeDot[chromeTone=\"soft\"] { background:%9; }"
        "QFrame#welcomeHeroChromeDot[chromeTone=\"ghost\"] { background:%7; }"
        "QLabel#welcomeHeroChromeMode, QLabel#welcomeHeroChromeStatus, QLabel#welcomeKicker,"
        "QLabel#welcomeAtmosphereKicker, QLabel#welcomeMeta, QLabel[welcomeSignalRole=\"chip\"] {"
        "  color:%10; background:%11; border:1px solid %3; border-radius:999px;"
        "  font-size:11px; font-weight:600; padding:4px 10px;"
        "}"
        "QLabel#welcomeHeroChromeStatus { color:%12; }"
        "QLabel#welcomeTitle { color:%13; font-size:23px; font-weight:700; }"
        "QLabel#welcomeRuntimeSummary { color:%13; font-size:16px; font-weight:700; }"
        "QLabel#welcomeRuntimeDetail { color:%12; font-size:13px; font-weight:500; }"
        "QLabel#welcomeSubtitle, QLabel#welcomePreviewBody, QLabel#welcomeAtmosphereBody,"
        "QLabel#welcomeAtmosphereSignalDetail { color:%12; font-size:13px; }"
        "QLabel#welcomeAtmosphereTitle { color:%13; font-size:16px; font-weight:700; }"
        "QLabel#welcomeAtmosphereSignalTitle { color:%13; font-size:13px; font-weight:700; }"
        "QLabel#welcomeMark { color:%8; background:%11; border:1px solid %3; border-radius:12px;"
        "  font-size:18px; font-weight:700; min-width:38px; min-height:38px; padding:6px; }"
        "QLabel[welcomeMetricRole=\"value\"] { color:%13; font-size:20px; font-weight:700; }"
        "QLabel#welcomePreviewLabel { color:%14; font-size:12px; font-weight:600; }"
        "QFrame#welcomeAtmospherePulse, QFrame#welcomeAtmosphereBar, QFrame#welcomeAtmosphereDockBar {"
        "  background:%7; border-radius:4px;"
        "}"
        "QFrame#welcomeAtmospherePulse[pulseTone=\"lead\"], QFrame#welcomeAtmosphereBar[barTone=\"accent\"],"
        "QFrame#welcomeAtmosphereDockBar[dockTone=\"accent\"] { background:%8; }"
        "QFrame#welcomeAtmospherePulse[pulseTone=\"soft\"], QFrame#welcomeAtmosphereBar[barTone=\"soft\"] { background:%9; }"
        "QFrame#welcomeAtmospherePulse[pulseTone=\"thin\"], QFrame#welcomeAtmosphereBar[barTone=\"ghost\"] { background:%7; }"
        "QFrame#welcomeAtmosphereLane { background:%6; border:1px solid %3; border-radius:10px; }"
        "QFrame#welcomeAtmosphereNode { background:%9; border-radius:5px; }"
        "QFrame#welcomeAtmosphereNode[nodeTone=\"accent\"] { background:%8; }"
        "QFrame#welcomeAtmosphereNode[nodeTone=\"muted\"] { background:%7; }"
        "QPushButton#welcomePrimaryAction { background:%8; color:#FFFFFF; border:1px solid %8;"
        "  border-radius:10px; font-size:13px; font-weight:600; padding:9px 16px; }"
        "QPushButton#welcomePrimaryAction:hover { background:%15; border-color:%15; }"
        "QPushButton#welcomeSecondaryAction { background:%6; color:%13; border:1px solid %3;"
        "  border-radius:10px; font-size:13px; font-weight:600; padding:9px 14px; }"
        "QPushButton#welcomeSecondaryAction:hover { background:%11; }")
        .arg(pageBg,
             shellBg,
             brd,
             subtleBg,
             cardBg,
             innerCardBg,
             sMuted,
             acc,
             dark ? QStringLiteral("#4F7DD8") : QStringLiteral("#9CB6F4"),
             acc,
             chipBg,
             txt2,
             txt,
             txtM,
             dark ? QStringLiteral("#5E8CFF") : QStringLiteral("#2B61D1"));
}
inline QString stylesheet_messageStage(ThemeMode mode);

inline QString stylesheet(ThemeMode mode = currentThemeMode())
{
    const QString bg   = windowBg(mode);
    const QString txt  = textPrimary(mode);
    const QString brd  = border(mode);
    const QString brdS = borderStrong(mode);
    const QString txt2 = textSecondary(mode);
    const QString acc  = accent(mode);
    const QString srf  = surface(mode);
    const QString txtM = textMuted(mode);
    const QString sAlt = surfaceAlt(mode);
    const QString sMuted = surfaceMuted(mode);
    const QString hov  = hoverBg(mode);
    const QString accH = accentHover(mode);
    const QString accP = accentPressed(mode);
    const QString accS = accentSoft(mode);
    const QString selB = selectedBg(mode);
    const bool dark = isDarkTheme(mode);
    const QString txtInv = elaColor(ElaThemeType::BasicTextInvert, mode);
    // ── 所有派生颜色统一从 ElaTheme 取值，不再硬编码 dark/light hex ──
    const QString sideListBg = QStringLiteral("transparent");
    const QString cardBg = QStringLiteral("transparent");
    const QString searchCardBg = QStringLiteral("transparent");
    const QString hoverOverlay = dark ? QStringLiteral("rgba(255,255,255,0.06)") : QStringLiteral("rgba(0,0,0,0.05)");
    const QString hoverOverlayStrong = dark ? QStringLiteral("rgba(255,255,255,0.06)") : QStringLiteral("rgba(0,0,0,0.07)");
    // 筛选项选中背景 — 从 accentSoft/selectedBg 派生渐变
    const QString filterCheckedBg = QStringLiteral(
        "qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 %1, stop:1 %2)").arg(accS, selB);
    const QString filterCheckedBorder = brdS;
    // chip 背景 — 同上
    const QString modeChipBg = QStringLiteral(
        "qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 %1, stop:1 %2)").arg(accS, selB);
    const QString modeChipBorder = brdS;
    const QString modeChipColor = acc;
    // 头像按钮 — 从 accent 系列派生
    const QString avatarGrad = QStringLiteral(
        "qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 %1, stop:1 %2)").arg(accP, acc);
    const QString avatarGradHover = QStringLiteral(
        "qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 %1, stop:1 %2)").arg(acc, accH);
    // 侧栏底部按钮 — 统一从 ElaTheme 派生
    const QString footerPrimaryBg = accS;
    const QString footerPrimaryBgHover = selB;
    const QString footerPrimaryBorder = brdS;
    const QString footerSecondaryBgHover = hov;

    return QStringLiteral(
        "QWidget#chatPageRoot {"
        "  background: transparent;"
        "}"
        "QMainWindow {"
        "  background-color: transparent;"
        "  color: %1;"
        "}")
        .arg(txt)
    + QStringLiteral(
        "QWidget#navBar {"
        "  background-color: transparent;"
        "}")
        .arg(srf)
    + QStringLiteral(
        "QPushButton[navRole] {"
        "  border: none; background: transparent; border-radius: 8px;"
        "  color: %1; font-size: 12px; font-weight: 500;"
        "  min-height: 44px; padding: 8px 6px;"
        "}"
        "QPushButton[navRole]:hover:!checked {"
        "  background: %2; color: %3;"
        "}"
        "QPushButton[navRole]:checked {"
        "  background: transparent; color: %4; font-weight: 600;"
        "}"
        "QPushButton[navRole]:checked:hover {"
        "  background: rgba(%5,0.08);"
        "}")
        .arg(txtM, hoverOverlay, txt2, acc, acc)
    + QStringLiteral(
        "QFrame#navActivePill { background: %1; border-radius: 1px; }").arg(acc)
    + QStringLiteral(
        "QPushButton#avatarBtn {"
        "  background: %1; color: #FFFFFF; border: none;"
        "  border-radius: 20px; font-size: 15px; font-weight: 700;"
        "}"
        "QPushButton#avatarBtn:hover { background: %2; }")
        .arg(avatarGrad, avatarGradHover)
    + QStringLiteral(
        "QStackedWidget#sideStack {"
        "  background-color: transparent;"
        "  border: none;"
        "}"
        "QFrame#sideHeaderBar {"
        "  background-color: transparent;"
        "}")
    + QStringLiteral(
        "QFrame#conversationsHeaderCard, QFrame#contactsHeaderCard, QFrame#transferHeaderCard {"
        "  background: %1; border: none; border-radius: 0px;"
        "}")
        .arg(cardBg)
    + QStringLiteral(
        "QFrame#conversationSearchCard, QFrame#contactsQuickActionsCard, QFrame#transferFilterBand {"
        "  background: %1; border: none; border-radius: 0px;"
        "}")
        .arg(searchCardBg)
    + QStringLiteral(
        "QFrame#sideSurfaceBand { background: transparent; border: none; }")
    + QStringLiteral(
        "QLabel[surfaceChipRole=\"mode\"] {"
        "  background: %1; color: %2; border: none;"
        "  border-radius: 999px; font-size: 11px; font-weight: 700; padding: 4px 10px;"
        "}"
        "QLabel[surfaceChipRole=\"status\"] {"
        "  background: %3; color: %4; border: none;"
        "  border-radius: 999px; font-size: 11px; font-weight: 600; padding: 4px 10px;"
        "}")
        .arg(modeChipBg, modeChipColor, srf, txt2)
    + QStringLiteral(
        "QLabel#sideHeaderTitle, QLabel#conversationWorkspaceTitle {"
        "  background: transparent; color: %1; font-size: 17px; font-weight: 700;"
        "}"
        "QLabel#sideHeader {"
        "  background-color: %2; color: %1; font-size: 17px; font-weight: 700;"
        "  padding: 18px 16px 12px 16px;"
        "}")
        .arg(txt, sideListBg)
    + QStringLiteral(
        "QPushButton#sideIconBtn {"
        "  border: none; background: transparent; border-radius: 8px;"
        "  color: %1; font-size: 14px; min-width: 32px; min-height: 32px;"
        "}"
        "QPushButton#sideIconBtn:hover { background: %2; color: %3; }")
        .arg(txtM, hoverOverlayStrong, txt)
    + QStringLiteral(
        "QPushButton#sidebarFooterPrimary {"
        "  background: %1; color: %2; border: none;"
        "  border-radius: 8px; font-size: 13px; font-weight: 700; padding: 8px 12px; text-align: left;"
        "}"
        "QPushButton#sidebarFooterPrimary:hover { background: %3; }"
        "QPushButton#sidebarFooterSecondary {"
        "  background: %4; color: %5; border: none;"
        "  border-radius: 8px; font-size: 13px; font-weight: 600; padding: 8px 12px; text-align: left;"
        "}"
        "QPushButton#sidebarFooterSecondary:hover { background: %6; }")
        .arg(footerPrimaryBg, acc, footerPrimaryBgHover,
             srf, txt, footerSecondaryBgHover)
    + QStringLiteral(
        "QFrame#filterPanel {"
        "  background-color: %1; border: none; border-radius: 10px;"
        "}"
        "QLabel#filterGroupLabel {"
        "  background: transparent; color: %2; font-size: 11px; font-weight: 600;"
        "}"
        "QPushButton#filterItem {"
        "  border: none; background: transparent; border-radius: 6px;"
        "  color: %3; font-size: 13px; text-align: left; padding: 0 6px; margin: 0 6px;"
        "}"
        "QPushButton#filterItem:hover:!checked { background: %4; }"
        "QPushButton#filterItem:checked {"
        "  background: %5; color: %6; border: none; font-weight: 600;"
        "}")
        .arg(srf, txtM, txt2,
             hoverOverlay, filterCheckedBg, modeChipColor)
    + QStringLiteral(
        "QFrame#workspaceShellFrame {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 14px;"
        "}"
        "QWidget#conversationSidebarWidget { background: transparent; border-left: none; border-right: none; }"
        "QStackedWidget#contentWorkspaceStack { background-color: transparent; }"
        "QWidget#chatWorkspaceWidget { background: transparent; }"
        "QWidget#chatWorkspaceBody, QWidget#chatWorkspacePrimaryColumn, QWidget#messageStageViewport, QFrame#messageStageFrame {"
        "  background: transparent; border: none;"
        "}"
        "QSplitter#chatWorkspaceSplitter, QSplitter#workspaceSplitter { background: transparent; border: none; }"
        "QSplitter#workspaceSplitter::handle, QSplitter#chatWorkspaceSplitter::handle { background: transparent; border: none; }")
        .arg(srf, brd)
    + stylesheet_welcomePage(mode)
    + stylesheet_messageStage(mode)
    + QStringLiteral(
        "QStatusBar {"
        "  background-color: %1; color: %2; font-size: 12px;"
        "}")
        .arg(sAlt, txtM);
}

inline QString stylesheet_messageStage(ThemeMode mode)
{
    const bool dark = isDarkTheme(mode);
    const QString bg   = windowBg(mode);
    const QString txt  = textPrimary(mode);
    const QString txt2 = textSecondary(mode);
    const QString txtM = textMuted(mode);
    const QString brd  = border(mode);
    const QString acc  = accent(mode);
    const QString srf  = surface(mode);
    const QString sAlt = surfaceAlt(mode);
    const QString sMuted = surfaceMuted(mode);

    if (dark) {
        return QStringLiteral(
            "QFrame#messageStageFrame {"
            "  background:transparent;"
            "  border:none;"
            "}"
            "QFrame#messageStageEmptyCard {"
            "  background:transparent;"
            "  border:none;"
            "}"
            "QLabel#messageStageEmptyTitle { color:%2; }"
            "QLabel#messageStageEmptyBody { color:%1; }")
            .arg(txt2, txt);
    }

    return QStringLiteral(
        "QFrame#messageStageFrame {"
        "  background:transparent;"
        "  border:none; border-radius:0px;"
        "}"
        "QFrame#messageStageEmptyCard {"
        "  background:transparent;"
        "  border:none; border-radius:0px; min-width:0px;"
        "}"
        "QLabel#messageStageEmptyTitle { color:%2; font-size:20px; font-weight:700; }"
        "QLabel#messageStageEmptyBody { color:%1; font-size:13px; line-height:1.5; }"
    ).arg(txt2, txt);
}

inline QString activeStylesheet(ThemeMode mode = currentThemeMode())
{
    return stylesheet(mode);
}
} // namespace AppStyle
