#pragma once

#include <QString>

class QWidget;

void showDocumentDialog(QWidget* parent,
                        const QString& title,
                        const QString& subtitle,
                        const QString& content,
                        bool markdown);
