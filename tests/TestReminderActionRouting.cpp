#include <QtTest>

#include "app/ReminderActionRouting.h"

class TestReminderActionRouting : public QObject {
    Q_OBJECT

private slots:
    void generatedReminderActionUrlsRoundTrip_data()
    {
        QTest::addColumn<QString>("verb");
        QTest::addColumn<QString>("inputId");
        QTest::addColumn<QString>("expectedId");

        QTest::newRow("open") << QStringLiteral("open")
                              << QStringLiteral("reminder-001")
                              << QStringLiteral("reminder-001");
        QTest::newRow("done") << QStringLiteral("done")
                              << QStringLiteral("reminder-002")
                              << QStringLiteral("reminder-002");
        QTest::newRow("dismiss") << QStringLiteral("dismiss")
                                 << QStringLiteral("reminder-003")
                                 << QStringLiteral("reminder-003");
        QTest::newRow("snooze") << QStringLiteral("snooze")
                                << QStringLiteral("reminder-004")
                                << QStringLiteral("reminder-004");
        QTest::newRow("normalizes verb and trims id") << QStringLiteral(" Snooze ")
                                                      << QStringLiteral(" reminder-005 ")
                                                      << QStringLiteral("reminder-005");
        QTest::newRow("percent encodes id") << QStringLiteral("open")
                                            << QStringLiteral("reminder/with space")
                                            << QStringLiteral("reminder/with space");
    }

    void generatedReminderActionUrlsRoundTrip()
    {
        QFETCH(QString, verb);
        QFETCH(QString, inputId);
        QFETCH(QString, expectedId);

        const QString url = reminderActionUrl(verb, inputId);

        QVERIFY(!url.trimmed().isEmpty());
        const auto action = parseReminderActionUrl(url);
        QVERIFY(action.has_value());
        QCOMPARE(action->verb, verb.trimmed().toLower());
        QCOMPARE(action->reminderId, expectedId);
    }

    void parsesCaseInsensitiveReminderUrls()
    {
        const auto action =
            parseReminderActionUrl(QStringLiteral(" LEYOCHAT://REMINDER/Done/reminder-006 "));

        QVERIFY(action.has_value());
        QCOMPARE(action->verb, QStringLiteral("done"));
        QCOMPARE(action->reminderId, QStringLiteral("reminder-006"));
    }

    void rejectsInvalidGeneratedActionUrls()
    {
        QVERIFY(reminderActionUrl(QStringLiteral("archive"), QStringLiteral("reminder-001")).isEmpty());
        QVERIFY(reminderActionUrl(QStringLiteral("open"), QStringLiteral("  ")).isEmpty());
    }

    void rejectsNonReminderUrls()
    {
        QVERIFY(!parseReminderActionUrl(QStringLiteral("https://example.com/reminder/open/r1")).has_value());
        QVERIFY(!parseReminderActionUrl(QStringLiteral("leyochat://conversation/c1")).has_value());
        QVERIFY(!parseReminderActionUrl(QStringLiteral("leyochat://reminder/open")).has_value());
        QVERIFY(!parseReminderActionUrl(QStringLiteral("leyochat://reminder/archive/r1")).has_value());
        QVERIFY(!parseReminderActionUrl(QStringLiteral("leyochat://reminder/open/%20")).has_value());
        QVERIFY(!parseReminderActionUrl(QStringLiteral("leyochat://reminder/open/r1/extra")).has_value());
    }
};

QTEST_MAIN(TestReminderActionRouting)

#include "TestReminderActionRouting.moc"
