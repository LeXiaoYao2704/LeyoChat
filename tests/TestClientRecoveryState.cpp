#include <QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "recovery/ClientRecoveryState.h"

class TestClientRecoveryState : public QObject
{
    Q_OBJECT

private:
    static ClientRecoveryState sampleState()
    {
        ClientRecoveryState state;
        state.sessionId = QStringLiteral("session-1");
        state.savedAtMs = 1000;
        state.windowMode = RecoveryWindowMode::Visible;
        state.windowGeometry = QByteArrayLiteral("geometry");
        state.windowMaximized = true;
        state.navigationPageId = QStringLiteral("messages");
        state.conversationId = QStringLiteral("direct:a:b");
        state.composerHtml = QStringLiteral("<p>draft</p>");
        state.replyMessageId = QStringLiteral("reply-1");
        state.replySenderId = QStringLiteral("peer-a");
        state.replySenderName = QStringLiteral("Peer A");
        state.replyBody = QStringLiteral("reply body");
        state.editingMessageId = QStringLiteral("edit-1");
        state.editingBody = QStringLiteral("<p>editing</p>");
        return state;
    }

private slots:
    void roundTripsSessionScopedState()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        ClientRecoveryStateStore store(temp.filePath(QStringLiteral("nested/recovery-state.json")));
        QString error;

        const ClientRecoveryState original = sampleState();
        QVERIFY2(store.save(original, &error), qPrintable(error));
        const auto restored = store.loadForSession(QStringLiteral("session-1"), 1100, &error);

        QVERIFY2(restored.has_value(), qPrintable(error));
        QCOMPARE(*restored, original);
    }

    void laterSaveAtomicallyReplacesEarlierState()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        ClientRecoveryStateStore store(temp.filePath(QStringLiteral("recovery-state.json")));
        QString error;
        ClientRecoveryState state = sampleState();
        QVERIFY(store.save(state, &error));
        state.composerHtml = QStringLiteral("<p>newer</p>");
        state.savedAtMs = 1200;
        QVERIFY(store.save(state, &error));

        const auto restored = store.loadForSession(QStringLiteral("session-1"), 1300, &error);
        QVERIFY(restored.has_value());
        QCOMPARE(restored->composerHtml, QStringLiteral("<p>newer</p>"));
    }

    void rejectsWrongSession()
    {
        QTemporaryDir temp;
        ClientRecoveryStateStore store(temp.filePath(QStringLiteral("recovery-state.json")));
        QString error;
        QVERIFY(store.save(sampleState(), &error));

        const auto restored = store.loadForSession(QStringLiteral("session-2"), 1100, &error);
        QVERIFY(!restored.has_value());
        QVERIFY(error.contains(QStringLiteral("session"), Qt::CaseInsensitive));
    }

    void rejectsStateOlderThanFifteenMinutes()
    {
        QTemporaryDir temp;
        ClientRecoveryStateStore store(temp.filePath(QStringLiteral("recovery-state.json")));
        QString error;
        QVERIFY(store.save(sampleState(), &error));

        const auto restored = store.loadForSession(
            QStringLiteral("session-1"),
            1000 + ClientRecoveryStateStore::maximumRecoveryAgeMs() + 1,
            &error);
        QVERIFY(!restored.has_value());
        QVERIFY(error.contains(QStringLiteral("stale"), Qt::CaseInsensitive));
    }

    void rejectsUnsupportedSchemaAndMalformedJson()
    {
        QTemporaryDir temp;
        const QString path = temp.filePath(QStringLiteral("recovery-state.json"));
        ClientRecoveryStateStore store(path);
        QString error;

        QJsonObject object;
        object.insert(QStringLiteral("schemaVersion"), 99);
        object.insert(QStringLiteral("sessionId"), QStringLiteral("session-1"));
        object.insert(QStringLiteral("savedAtMs"), 1000.0);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
        file.close();
        QVERIFY(!store.loadForSession(QStringLiteral("session-1"), 1100, &error).has_value());
        QVERIFY(error.contains(QStringLiteral("schema"), Qt::CaseInsensitive));

        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("{broken");
        file.close();
        QVERIFY(!store.loadForSession(QStringLiteral("session-1"), 1100, &error).has_value());
        QVERIFY(error.contains(QStringLiteral("JSON"), Qt::CaseInsensitive));
    }

    void clearsOversizedOptionalFieldsWithoutRejectingState()
    {
        QTemporaryDir temp;
        ClientRecoveryStateStore store(temp.filePath(QStringLiteral("recovery-state.json")));
        QString error;
        ClientRecoveryState state = sampleState();
        state.composerHtml = QString(ClientRecoveryStateStore::maximumComposerHtmlLength() + 1,
                                     QLatin1Char('x'));
        state.replyBody = QString(ClientRecoveryStateStore::maximumContextTextLength() + 1,
                                  QLatin1Char('r'));
        QVERIFY(store.save(state, &error));

        const auto restored = store.loadForSession(QStringLiteral("session-1"), 1100, &error);
        QVERIFY(restored.has_value());
        QVERIFY(restored->composerHtml.isEmpty());
        QVERIFY(restored->replyBody.isEmpty());
        QCOMPARE(restored->conversationId, QStringLiteral("direct:a:b"));
    }

    void rejectsOversizedRequiredIdentifiers()
    {
        QTemporaryDir temp;
        ClientRecoveryStateStore store(temp.filePath(QStringLiteral("recovery-state.json")));
        QString error;
        ClientRecoveryState state = sampleState();
        state.sessionId = QString(129, QLatin1Char('s'));

        QVERIFY(!store.save(state, &error));
        QVERIFY(error.contains(QStringLiteral("session"), Qt::CaseInsensitive));
    }

    void refreshesUnchangedSnapshotOnlyAtHeartbeatInterval()
    {
        QVERIFY(ClientRecoveryStateStore::shouldPersistSnapshot(true, 1000, 1001));
        QVERIFY(!ClientRecoveryStateStore::shouldPersistSnapshot(
            false, 1000, 1000 + ClientRecoveryStateStore::snapshotRefreshIntervalMs() - 1));
        QVERIFY(ClientRecoveryStateStore::shouldPersistSnapshot(
            false, 1000, 1000 + ClientRecoveryStateStore::snapshotRefreshIntervalMs()));
    }

    void rejectsOversizedRecoveryFileBeforeParsing()
    {
        QTemporaryDir temp;
        const QString path = temp.filePath(QStringLiteral("recovery-state.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(ClientRecoveryStateStore::maximumFileSizeBytes() + 1));
        file.close();

        ClientRecoveryStateStore store(path);
        QString error;
        QVERIFY(!store.loadForSession(QStringLiteral("session-1"), 1100, &error).has_value());
        QVERIFY(error.contains(QStringLiteral("large"), Qt::CaseInsensitive));
    }

    void clearsOversizedWindowGeometry()
    {
        QTemporaryDir temp;
        ClientRecoveryStateStore store(temp.filePath(QStringLiteral("recovery-state.json")));
        ClientRecoveryState state = sampleState();
        state.windowGeometry = QByteArray(
            ClientRecoveryStateStore::maximumWindowGeometryBytes() + 1, 'g');
        QString error;

        QVERIFY2(store.save(state, &error), qPrintable(error));
        const auto restored = store.loadForSession(QStringLiteral("session-1"), 1100, &error);
        QVERIFY(restored.has_value());
        QVERIFY(restored->windowGeometry.isEmpty());
        QCOMPARE(restored->composerHtml, QStringLiteral("<p>draft</p>"));
    }
};

QTEST_GUILESS_MAIN(TestClientRecoveryState)
#include "TestClientRecoveryState.moc"
