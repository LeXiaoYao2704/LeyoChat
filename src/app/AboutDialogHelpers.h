#pragma once

#include <QString>

class QWidget;

void showCurrentReleaseNotesDialog(QWidget* parent,
                                   const QString& title,
                                   const QString& subtitle);
void showAboutDialogWindow(QWidget* parent, const QString& appDisplayName);
