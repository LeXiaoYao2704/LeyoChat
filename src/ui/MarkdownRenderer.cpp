#include "ui/MarkdownRenderer.h"

#include "md4c-html.h"

#include <QByteArray>
#include <QRegularExpression>

namespace MarkdownRenderer {
namespace {

QString unwrapMarkdownFence(QString text)
{
    static const QRegularExpression fencedMarkdownRx(
        QStringLiteral("^```(?:markdown|md)?\\s*\\n([\\s\\S]*?)\\n```\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = fencedMarkdownRx.match(text.trimmed());
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return text;
}

QString stripFrontmatter(QString text)
{
    static const QRegularExpression frontmatterRx(
        QStringLiteral("^---\\n[\\s\\S]*?\\n---\\s*\\n?"),
        QRegularExpression::MultilineOption);
    text.replace(frontmatterRx, QString());
    return text;
}

QString generateToc(const QString& markdown)
{
    static const QRegularExpression headingRx(
        QStringLiteral("^(#{1,6})\\s+(.+)$"),
        QRegularExpression::MultilineOption);
    QString toc;
    auto it = headingRx.globalMatch(markdown);
    while (it.hasNext()) {
        const auto m = it.next();
        const int level = m.captured(1).length();
        const QString title = m.captured(2).trimmed();
        if (title.isEmpty()) continue;
        // 生成缩进列表
        toc += QString(QStringLiteral("%1- %2\n"))
                   .arg(QString((level - 1) * 2, QLatin1Char(' ')), title);
    }
    return toc;
}

QString normalizeMarkdown(const QString& rawMarkdown)
{
    QString text = unwrapMarkdownFence(rawMarkdown.trimmed());
    text = stripFrontmatter(text);

    // 处理 [[TOC]] / [[toc]] 目录宏
    static const QRegularExpression tocRx(
        QStringLiteral("^\\[\\[\\s*[Tt][Oo][Cc]\\s*\\]\\]\\s*$"),
        QRegularExpression::MultilineOption);
    if (text.contains(tocRx)) {
        const QString toc = generateToc(text);
        if (!toc.isEmpty()) {
            text.replace(tocRx, QStringLiteral("## 目录\n\n") + toc);
        } else {
            text.replace(tocRx, QString());
        }
    }

    return text.trimmed();
}

void appendHtmlChunk(const MD_CHAR* text, MD_SIZE size, void* userdata)
{
    auto* output = static_cast<QByteArray*>(userdata);
    output->append(text, static_cast<qsizetype>(size));
}

QString replaceCitationBadges(QString html)
{
    static const QRegularExpression sourceCitationRx(QStringLiteral("\\[来源(\\d+)\\]"));
    html.replace(sourceCitationRx,
                 QStringLiteral("<sup style=\"color:#2F6FED; font-size:11px; font-weight:700; cursor:pointer; margin:0 1px;\">\\1</sup>"));

    static const QRegularExpression numericCitationRx(QStringLiteral("\\[(\\d+)\\]"));
    html.replace(numericCitationRx,
                 QStringLiteral("<sup style=\"color:#2F6FED; font-size:11px; font-weight:700; cursor:pointer; margin:0 1px;\">\\1</sup>"));
    return html;
}

QString wrapHtmlDocument(const QString& bodyHtml)
{
    return QStringLiteral(
               "<html><head><meta charset=\"utf-8\"/>"
               "<style>"
               "body { margin:0; padding:8px 16px; color:#111827; font-family: -apple-system, 'Segoe UI', 'Microsoft YaHei UI', sans-serif; font-size:14px; line-height:1.8; }"
               "p { margin:0 0 12px 0; }"
               "h1 { font-size:22px; font-weight:700; margin:24px 0 12px 0; padding-bottom:8px; border-bottom:2px solid #E5EAF1; color:#111827; }"
               "h2 { font-size:18px; font-weight:700; margin:20px 0 10px 0; padding-bottom:6px; border-bottom:1px solid #E5EAF1; color:#1F2937; }"
               "h3 { font-size:16px; font-weight:600; margin:16px 0 8px 0; color:#374151; }"
               "h4,h5,h6 { font-size:14px; font-weight:600; margin:14px 0 6px 0; color:#4B5563; }"
               "ul,ol { margin:8px 0 12px 0; padding-left:24px; }"
               "li { margin:4px 0; line-height:1.7; }"
               "li > ul, li > ol { margin:2px 0 2px 0; }"
               "blockquote { margin:12px 0; padding:10px 16px; border-left:3px solid #2F6FED; background:#F0F4FF; color:#374151; border-radius:0 6px 6px 0; }"
               "blockquote p:last-child { margin-bottom:0; }"
               "pre { margin:8px 0; padding:10px 14px; border-radius:8px; background:#F5F7FA; color:#24292F; overflow-x:auto; border:1px solid #E5EAF1; }"
               "pre code { font-size:13px; color:#24292F; background:transparent; padding:0; }"
               "code { font-family: Consolas, 'Cascadia Code', 'Courier New', monospace; font-size:13px; }"
               "p code, li code, td code { background:#EEF2F7; color:#C7254E; padding:1px 4px; border-radius:3px; font-size:12px; }"
               "table { width:100%; border-collapse:collapse; margin:8px 0; border-radius:6px; overflow:hidden; }"
               "th { background:#F0F4FA; font-weight:600; color:#1F2937; padding:6px 10px; text-align:left; border:1px solid #E5EAF1; }"
               "td { padding:5px 10px; text-align:left; vertical-align:top; border:1px solid #E5EAF1; }"
               "tr:nth-child(even) td { background:#FAFBFC; }"
               "tr:hover td { background:#F0F4FF; }"
               "hr { border:none; border-top:1px solid #E5EAF1; margin:12px 0; }"
               "a { color:#2F6FED; text-decoration:none; font-weight:500; }"
               "a:hover { text-decoration:underline; color:#1D5BD6; }"
               "img { max-width:100%; border-radius:6px; margin:4px 0; }"
               "del { color:#9CA3AF; }"
               "input[type=checkbox] { margin-right:4px; }"
               "</style></head><body>%1</body></html>")
        .arg(bodyHtml);
}

QString plainTextFallback(const QString& text)
{
    return wrapHtmlDocument(QStringLiteral("<p>%1</p>")
                                .arg(text.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br/>"))));
}

}  // namespace

QString renderMarkdownToHtml(const QString& rawMarkdown, const Options& options)
{
    QString markdown = normalizeMarkdown(rawMarkdown);
    if (markdown.isEmpty()) {
        markdown = options.emptyPlaceholder.trimmed();
    }

    if (markdown.isEmpty()) {
        return wrapHtmlDocument(QString());
    }

    const QByteArray utf8 = markdown.toUtf8();
    QByteArray html;
    const int result = md_html(utf8.constData(),
                               static_cast<MD_SIZE>(utf8.size()),
                               appendHtmlChunk,
                               &html,
                               MD_DIALECT_GITHUB,
                               0);
    if (result != 0) {
        return plainTextFallback(markdown);
    }

    QString bodyHtml = QString::fromUtf8(html);
    if (options.highlightCitations) {
        bodyHtml = replaceCitationBadges(bodyHtml);
    }

    return wrapHtmlDocument(bodyHtml);
}

}  // namespace MarkdownRenderer