#include <QtTest/QTest>

#include <QTemporaryDir>

#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"

class TestRemoteMessageEventCursor : public QObject {
    Q_OBJECT

private slots:
    void eventCursor_saveLoadAndClamp()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-remote-message-event-cursor");
        DatabaseManager mgr(dir.filePath(QStringLiteral("event-cursor.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QCOMPARE(repo.loadRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                   QStringLiteral("pc-a")),
                 qint64(0));
        QVERIFY(repo.saveRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                  QStringLiteral("pc-a"),
                                                  12));
        QCOMPARE(repo.loadRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                   QStringLiteral("pc-a")),
                 qint64(12));

        QVERIFY(repo.saveRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                  QStringLiteral("pc-a"),
                                                  -9));
        QCOMPARE(repo.loadRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                   QStringLiteral("pc-a")),
                 qint64(0));
    }

    void eventCursor_rejectsEmptyWorkspaceOrDevice()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-remote-message-event-empty");
        DatabaseManager mgr(dir.filePath(QStringLiteral("event-empty.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QCOMPARE(repo.loadRemoteMessageEventCursor(QString(), QStringLiteral("pc-a")),
                 qint64(0));
        QCOMPARE(repo.loadRemoteMessageEventCursor(QStringLiteral("ws-main"), QString()),
                 qint64(0));
        QVERIFY(!repo.saveRemoteMessageEventCursor(QString(), QStringLiteral("pc-a"), 1));
        QVERIFY(!repo.saveRemoteMessageEventCursor(QStringLiteral("ws-main"), QString(), 1));
    }

    void eventCursor_isIndependentPerWorkspaceAndDevice()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-remote-message-event-independent");
        DatabaseManager mgr(dir.filePath(QStringLiteral("event-independent.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.saveRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                  QStringLiteral("pc-a"),
                                                  7));
        QVERIFY(repo.saveRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                  QStringLiteral("pc-b"),
                                                  9));
        QVERIFY(repo.saveRemoteMessageEventCursor(QStringLiteral("ws-other"),
                                                  QStringLiteral("pc-a"),
                                                  11));

        QCOMPARE(repo.loadRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                   QStringLiteral("pc-a")),
                 qint64(7));
        QCOMPARE(repo.loadRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                   QStringLiteral("pc-b")),
                 qint64(9));
        QCOMPARE(repo.loadRemoteMessageEventCursor(QStringLiteral("ws-other"),
                                                   QStringLiteral("pc-a")),
                 qint64(11));
    }
};

QTEST_MAIN(TestRemoteMessageEventCursor)
#include "TestRemoteMessageEventCursor.moc"
