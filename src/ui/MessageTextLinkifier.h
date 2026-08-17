#pragma once

#include <QString>

class QTextDocument;

namespace MessageTextLinkifier {

QString plainTextWithAutoLinksToHtml(const QString& text);
void applyAutoLinksToDocument(QTextDocument* document, const QString& linkColorName = QString());

}  // namespace MessageTextLinkifier
