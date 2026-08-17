#include "ui/MessageTextLinkifier.h"

#include <QBrush>
#include <QColor>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>

namespace {

void appendEscapedPlainText(QString* output, const QString& text)
{
    if (!output || text.isEmpty()) {
        return;
    }

    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QString escaped = normalized.toHtmlEscaped();
    escaped.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    output->append(escaped);
}

bool isTrailingUrlPunctuation(QChar ch)
{
    static const QString chars = QStringLiteral(".,;:!?)]}");
    return chars.contains(ch);
}

QString normalizedHref(const QString& urlText)
{
    QString href = urlText.trimmed();
    if (href.startsWith(QStringLiteral("www."), Qt::CaseInsensitive)) {
        href.prepend(QStringLiteral("https://"));
    }

    const QUrl url = QUrl::fromUserInput(href);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid()
        || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
        return {};
    }
    return url.toString();
}

}  // namespace

namespace MessageTextLinkifier {

QString plainTextWithAutoLinksToHtml(const QString& text)
{
    static const QRegularExpression urlPattern(
        QStringLiteral(R"((https?://[^\s<>"']+|www\.[^\s<>"']+))"),
        QRegularExpression::CaseInsensitiveOption);

    QString html;
    qsizetype cursor = 0;
    QRegularExpressionMatchIterator it = urlPattern.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const qsizetype start = match.capturedStart(0);
        const qsizetype end = match.capturedEnd(0);
        if (start < cursor) {
            continue;
        }

        appendEscapedPlainText(&html, text.mid(cursor, start - cursor));

        QString linkText = match.captured(0);
        QString trailing;
        while (!linkText.isEmpty() && isTrailingUrlPunctuation(linkText.back())) {
            trailing.prepend(linkText.back());
            linkText.chop(1);
        }

        const QString href = normalizedHref(linkText);
        if (href.isEmpty()) {
            appendEscapedPlainText(&html, match.captured(0));
        } else {
            html.append(QStringLiteral("<a href=\"%1\">%2</a>")
                            .arg(href.toHtmlEscaped(),
                                 linkText.toHtmlEscaped()));
            appendEscapedPlainText(&html, trailing);
        }
        cursor = end;
    }

    appendEscapedPlainText(&html, text.mid(cursor));
    return html;
}

void applyAutoLinksToDocument(QTextDocument* document, const QString& linkColorName)
{
    if (!document) {
        return;
    }

    static const QRegularExpression urlPattern(
        QStringLiteral(R"((https?://[^\s<>"']+|www\.[^\s<>"']+))"),
        QRegularExpression::CaseInsensitiveOption);

    const QString plainText = document->toPlainText();
    QRegularExpressionMatchIterator it = urlPattern.globalMatch(plainText);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const qsizetype start = match.capturedStart(0);
        const qsizetype end = match.capturedEnd(0);
        if (start < 0 || end <= start) {
            continue;
        }

        QString linkText = match.captured(0);
        qsizetype linkEnd = end;
        while (!linkText.isEmpty() && isTrailingUrlPunctuation(linkText.back())) {
            linkText.chop(1);
            --linkEnd;
        }
        if (linkText.isEmpty()) {
            continue;
        }

        const QString href = normalizedHref(linkText);
        if (href.isEmpty()) {
            continue;
        }

        QTextCursor probe(document);
        probe.setPosition(static_cast<int>(start));
        if (!probe.charFormat().anchorHref().isEmpty()) {
            continue;
        }

        QTextCursor cursor(document);
        cursor.setPosition(static_cast<int>(start));
        cursor.setPosition(static_cast<int>(linkEnd), QTextCursor::KeepAnchor);

        QTextCharFormat format;
        format.setAnchor(true);
        format.setAnchorHref(href);
        format.setFontUnderline(true);
        if (!linkColorName.trimmed().isEmpty()) {
            format.setForeground(QBrush(QColor(linkColorName)));
        }
        cursor.mergeCharFormat(format);
    }
}

}  // namespace MessageTextLinkifier
