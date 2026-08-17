#include <QtTest/QTest>
#include <QCoreApplication>

#include "ui/MarkdownRenderer.h"

class TestMarkdownRenderer : public QObject {
    Q_OBJECT

private slots:
    void rendersMarkdownTableToHtml()
    {
        const QString markdown = QStringLiteral(
            "| 列1 | 列2 |\n"
            "| --- | --- |\n"
            "| A | B |\n");

        const QString html = MarkdownRenderer::renderMarkdownToHtml(markdown);

        QVERIFY2(html.contains(QStringLiteral("<table")), qPrintable(html));
        QVERIFY2(html.contains(QStringLiteral("<td>A</td>")), qPrintable(html));
    }

    void rendersFencedCodeBlockToHtml()
    {
        const QString markdown = QStringLiteral(
            "```cpp\n"
            "int value = 42;\n"
            "```\n");

        const QString html = MarkdownRenderer::renderMarkdownToHtml(markdown);

        QVERIFY2(html.contains(QStringLiteral("<pre><code")), qPrintable(html));
        QVERIFY2(html.contains(QStringLiteral("int value = 42;")), qPrintable(html));
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    TestMarkdownRenderer tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestMarkdownRenderer.moc"