#include "app/UiRestoreHelpers.h"

#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QStringList>
#include <QWidget>

void logUiRestoreTrace(int sequence, const QString& phase, const QString& detail)
{
    qInfo().noquote()
        << QStringLiteral("[ui-restore] seq=%1 phase=%2%3")
               .arg(QString::number(sequence),
                    phase,
                    detail.trimmed().isEmpty() ? QString() : QStringLiteral(" ") + detail.trimmed());
}

QString windowStateSummary(const QWidget* widget)
{
    if (!widget) {
        return QStringLiteral("window=null");
    }

    const Qt::WindowStates state = widget->windowState();
    QStringList flags;
    if (state.testFlag(Qt::WindowMinimized)) {
        flags.push_back(QStringLiteral("minimized"));
    }
    if (state.testFlag(Qt::WindowMaximized)) {
        flags.push_back(QStringLiteral("maximized"));
    }
    if (state.testFlag(Qt::WindowFullScreen)) {
        flags.push_back(QStringLiteral("fullscreen"));
    }
    if (flags.isEmpty()) {
        flags.push_back(QStringLiteral("normal"));
    }

    return QStringLiteral("visible=%1 active=%2 hidden=%3 state=%4")
        .arg(widget->isVisible() ? QStringLiteral("true") : QStringLiteral("false"),
             widget->isActiveWindow() ? QStringLiteral("true") : QStringLiteral("false"),
             widget->isHidden() ? QStringLiteral("true") : QStringLiteral("false"),
             flags.join(QLatin1Char('|')));
}

bool shouldMaximizeMainWindowOnStartup(const QWidget* window)
{
    const QScreen* screen = window && window->screen()
        ? window->screen()
        : QGuiApplication::primaryScreen();
    if (!window || !screen) {
        return false;
    }

    const QRect available = screen->availableGeometry();
    const QSize normalSize = window->size();
    const QSize minimumSize = window->minimumSize();
    constexpr int kWindowFrameReserve = 24;
    return normalSize.width() + kWindowFrameReserve > available.width()
        || normalSize.height() + kWindowFrameReserve > available.height()
        || minimumSize.width() + kWindowFrameReserve > available.width()
        || minimumSize.height() + kWindowFrameReserve > available.height();
}
