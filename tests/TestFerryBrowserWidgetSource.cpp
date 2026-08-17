#include <QFile>
#include <QtTest>

class TestFerryBrowserWidgetSource : public QObject
{
    Q_OBJECT

private slots:
    void serviceEndpointComesFromEnvironment()
    {
        const QString source = readSource();
        QVERIFY(source.contains(
            QStringLiteral("qEnvironmentVariable(\"LEYOCHAT_FILE_EXCHANGE_URL\")")));
        QVERIFY(source.contains(QStringLiteral("serviceUrl.scheme() == QStringLiteral(\"https\")")));
        QVERIFY(source.contains(QStringLiteral("serviceUrl.scheme() == QStringLiteral(\"http\")")));
        QVERIFY(!source.contains(QStringLiteral("QUrl serviceUrl(QStringLiteral(")));
    }

    void unconfiguredPlaceholderCannotReportConnected()
    {
        const QString source = readSource();
        QVERIFY(source.contains(QStringLiteral("m_serviceConfigured = isSupportedUrl;")));

        const qsizetype handler = source.indexOf(
            QStringLiteral("void FerryBrowserWidget::onLoadFinished(bool ok)"));
        QVERIFY(handler >= 0);
        const qsizetype nextFunction = source.indexOf(
            QStringLiteral("void FerryBrowserWidget::onRenderProcessTerminated"), handler);
        QVERIFY(nextFunction > handler);

        const QString block = source.mid(handler, nextFunction - handler);
        const qsizetype guard = block.indexOf(
            QStringLiteral("if (!m_serviceConfigured)"));
        const qsizetype connected = block.indexOf(
            QStringLiteral("m_statusLabel->setText(QStringLiteral(\"已连接\"))"));
        QVERIFY(guard >= 0);
        QVERIFY(connected > guard);
    }

private:
    static QString readSource()
    {
        QFile source(QStringLiteral(LEYOCHAT_FERRY_BROWSER_SOURCE_PATH));
        if (!source.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(source.readAll());
    }
};

QTEST_MAIN(TestFerryBrowserWidgetSource)
#include "TestFerryBrowserWidgetSource.moc"
