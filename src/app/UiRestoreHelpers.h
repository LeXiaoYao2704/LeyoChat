#pragma once

#include <QtGlobal>
#include <QString>

class QWidget;

constexpr qint64 kUiRestoreTraceWindowMs = 8000;

void logUiRestoreTrace(int sequence, const QString& phase, const QString& detail = QString());

QString windowStateSummary(const QWidget* widget);

bool shouldMaximizeMainWindowOnStartup(const QWidget* window);
