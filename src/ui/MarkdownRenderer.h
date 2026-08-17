#pragma once

#include <QString>

namespace MarkdownRenderer {

struct Options {
    QString emptyPlaceholder;
    bool highlightCitations = false;
};

QString renderMarkdownToHtml(const QString& rawMarkdown,
                             const Options& options = {});

}  // namespace MarkdownRenderer