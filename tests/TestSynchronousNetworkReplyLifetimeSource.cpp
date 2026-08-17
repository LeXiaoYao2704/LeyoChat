#include <QtTest/QTest>

#include <QFile>
#include <QString>

namespace {

QString readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

}  // namespace

class TestSynchronousNetworkReplyLifetimeSource : public QObject {
    Q_OBJECT

private slots:
    void helperForcesDeferredDeleteProcessing()
    {
        const QString text =
            readTextFile(QStringLiteral(LEYOCHAT_SYNC_REPLY_HEADER_PATH));
        QVERIFY2(!text.isEmpty(), "failed to read SyncNetworkReply.h");
        QVERIFY2(text.contains(QStringLiteral("deleteLater()")),
                 "helper should retain Qt's deferred-delete reply cleanup");
        QVERIFY2(text.contains(QStringLiteral("sendPostedEvents")),
                 "helper must process DeferredDelete for synchronous worker threads");
        QVERIFY2(text.contains(QStringLiteral("QEvent::DeferredDelete")),
                 "helper should explicitly drain DeferredDelete events");
    }

    void synchronousHttpSourcesUseHelper_data()
    {
        QTest::addColumn<QString>("sourcePath");
        QTest::newRow("server-message-client")
            << QStringLiteral(LEYOCHAT_SERVER_MESSAGE_CLIENT_SOURCE_PATH);
        QTest::newRow("remote-file-service-adapter")
            << QStringLiteral(LEYOCHAT_REMOTE_FILE_SERVICE_ADAPTER_SOURCE_PATH);
        QTest::newRow("outlook-ews-transport")
            << QStringLiteral(LEYOCHAT_OUTLOOK_EWS_TRANSPORT_SOURCE_PATH);
        QTest::newRow("onlyoffice-callback")
            << QStringLiteral(LEYOCHAT_ONLYOFFICE_CALLBACK_SOURCE_PATH);
    }

    void synchronousHttpSourcesUseHelper()
    {
        QFETCH(QString, sourcePath);
        const QString text = readTextFile(sourcePath);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(sourcePath)));
        QVERIFY2(text.contains(QStringLiteral("SyncNetworkReply.h")),
                 qPrintable(QStringLiteral("%1 should include SyncNetworkReply.h").arg(sourcePath)));
        QVERIFY2(text.contains(QStringLiteral("deleteSynchronousNetworkReply(reply)")),
                 qPrintable(QStringLiteral("%1 should use the synchronous reply cleanup helper").arg(sourcePath)));
        QVERIFY2(!text.contains(QStringLiteral("reply->deleteLater();")),
                 qPrintable(QStringLiteral("%1 must not leave synchronous replies to an absent outer event loop").arg(sourcePath)));
    }
};

QTEST_MAIN(TestSynchronousNetworkReplyLifetimeSource)
#include "TestSynchronousNetworkReplyLifetimeSource.moc"
