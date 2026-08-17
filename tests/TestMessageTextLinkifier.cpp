#include <QtTest/QTest>

#include <QTextCursor>
#include <QTextDocument>

#include "ui/MessageTextLinkifier.h"

class TestMessageTextLinkifier : public QObject {
    Q_OBJECT

private slots:
    void plainTextUrlBecomesClickableAnchor()
    {
        const QString html = MessageTextLinkifier::plainTextWithAutoLinksToHtml(
            QStringLiteral("open https://example.com/path?a=1&b=2 now"));

        QVERIFY(html.contains(QStringLiteral(
            "<a href=\"https://example.com/path?a=1&amp;b=2\">https://example.com/path?a=1&amp;b=2</a>")));
        QVERIFY(html.contains(QStringLiteral("open ")));
        QVERIFY(html.contains(QStringLiteral(" now")));
    }

    void nonUrlHtmlIsEscaped()
    {
        const QString html = MessageTextLinkifier::plainTextWithAutoLinksToHtml(
            QStringLiteral("<b>safe</b> https://example.com"));

        QVERIFY(html.contains(QStringLiteral("&lt;b&gt;safe&lt;/b&gt;")));
        QVERIFY(!html.contains(QStringLiteral("<b>safe</b>")));
        QVERIFY(html.contains(QStringLiteral("<a href=\"https://example.com\">")));
    }

    void generatedUrlHasDocumentAnchor()
    {
        QTextDocument document;
        document.setHtml(MessageTextLinkifier::plainTextWithAutoLinksToHtml(
            QStringLiteral("please open https://example.com/path.")));

        const QString plainText = document.toPlainText();
        const int urlOffset = plainText.indexOf(QStringLiteral("https://example.com/path"));
        QVERIFY(urlOffset >= 0);

        QTextCursor cursor(&document);
        cursor.setPosition(urlOffset + 3);

        QCOMPARE(cursor.charFormat().anchorHref(),
                 QStringLiteral("https://example.com/path"));
        QVERIFY(plainText.endsWith(QLatin1Char('.')));
    }

    void htmlBodyPlainUrlBecomesDocumentAnchor()
    {
        QTextDocument document;
        document.setHtml(QStringLiteral(
            "<p><span style=\"font-weight:600\">open</span> https://example.com/path?a=1&amp;b=2.</p>"));

        MessageTextLinkifier::applyAutoLinksToDocument(&document);

        const QString plainText = document.toPlainText();
        const int urlOffset = plainText.indexOf(QStringLiteral("https://example.com/path"));
        QVERIFY(urlOffset >= 0);

        QTextCursor cursor(&document);
        cursor.setPosition(urlOffset + 5);

        QCOMPARE(cursor.charFormat().anchorHref(),
                 QStringLiteral("https://example.com/path?a=1&b=2"));
        QVERIFY(plainText.endsWith(QLatin1Char('.')));
    }
};

QTEST_MAIN(TestMessageTextLinkifier)
#include "TestMessageTextLinkifier.moc"
