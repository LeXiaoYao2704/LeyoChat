#include <QtTest/QTest>

#include <QFile>
#include <QString>

class TestHttpFileClientSource : public QObject {
    Q_OBJECT

private slots:
    void uploadFileStreamsDeviceInsteadOfReadAll()
    {
        QFile source(QStringLiteral(LEYOCHAT_HTTP_FILE_CLIENT_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin = text.indexOf(QStringLiteral("void HttpFileClient::uploadFile"));
        QVERIFY2(begin >= 0, "HttpFileClient::uploadFile must exist");
        const qsizetype end = text.indexOf(QStringLiteral("void HttpFileClient::startDownload"), begin);
        QVERIFY2(end > begin, "HttpFileClient::uploadFile block should be bounded");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(!block.contains(QStringLiteral("file->readAll()")),
                 "HTTP file upload must not buffer a local QFile into QByteArray before posting");
        QVERIFY2(!block.contains(QStringLiteral("m_uploadFile->readAll()")),
                 "HTTP file upload must stream the upload QFile/QIODevice instead of reading it all");
        QVERIFY2(block.contains(QStringLiteral("m_nam.post(request, m_uploadFile)")),
                 "HTTP file upload should post the open QFile device directly");
    }

    void uploadFileHasStallTimeoutCleanup()
    {
        QFile source(QStringLiteral(LEYOCHAT_HTTP_FILE_CLIENT_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin = text.indexOf(QStringLiteral("void HttpFileClient::uploadFile"));
        QVERIFY2(begin >= 0, "HttpFileClient::uploadFile must exist");
        const qsizetype end = text.indexOf(QStringLiteral("void HttpFileClient::startDownload"), begin);
        QVERIFY2(end > begin, "HttpFileClient::uploadFile block should be bounded");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(text.contains(QStringLiteral("m_uploadStallTimer")),
                 "HTTP upload should have a stall timer so tail-end hangs are surfaced");
        QVERIFY2(text.contains(QStringLiteral("m_currentUpload->abort()")),
                 "HTTP upload stall handling should abort the stuck network reply");
        QVERIFY2(block.contains(QStringLiteral("resetUploadStallTimer()")),
                 "HTTP upload should start or refresh the stall timer while uploading");
        QVERIFY2(block.contains(QStringLiteral("uploadFailed")),
                 "HTTP upload stall handling should emit uploadFailed and let callers fall back");
    }

    void uploadProgressDoesNotReportHundredBeforeReplyFinished()
    {
        QFile source(QStringLiteral(LEYOCHAT_HTTP_FILE_CLIENT_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin = text.indexOf(QStringLiteral("void HttpFileClient::uploadFile"));
        QVERIFY2(begin >= 0, "HttpFileClient::uploadFile must exist");
        const qsizetype end = text.indexOf(QStringLiteral("void HttpFileClient::startDownload"), begin);
        QVERIFY2(end > begin, "HttpFileClient::uploadFile block should be bounded");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("emit uploadProgress(displaySent, total)")),
                 "HTTP upload progress should emit display-safe bytes, not raw sent bytes");
        QVERIFY2(block.contains(QStringLiteral("total - 1")),
                 "HTTP upload progress must keep the final byte pending until the server response arrives");
        QVERIFY2(block.contains(QStringLiteral("emit uploadProgress(uploadTotal, uploadTotal)")),
                 "HTTP upload success should report 100% only when the reply has finished successfully");
    }
};

QTEST_MAIN(TestHttpFileClientSource)
#include "TestHttpFileClientSource.moc"
