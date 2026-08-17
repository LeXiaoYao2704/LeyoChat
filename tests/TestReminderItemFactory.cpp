#include <QtTest/QtTest>

#include <QJsonDocument>
#include <QJsonObject>

#include "app/ReminderItemFactory.h"

class TestReminderItemFactory : public QObject {
    Q_OBJECT

private slots:
    void contactReminderTrimsSnapshotsAndDefaultsToTomorrowMorning()
    {
        const QDateTime now(QDate(2026, 6, 28), QTime(22, 15), QTimeZone::systemTimeZone());

        const auto item = makeContactReminderItem(QStringLiteral(" client-1 "),
                                                  QStringLiteral(" Alice "),
                                                  QStringLiteral(" online "),
                                                  now);

        QVERIFY(item.has_value());
        QCOMPARE(item->targetType, QStringLiteral("contact"));
        QCOMPARE(item->targetId, QStringLiteral("client-1"));
        QCOMPARE(item->contactId, QStringLiteral("client-1"));
        QCOMPARE(item->titleSnapshot, QStringLiteral("Alice"));
        QCOMPARE(item->previewSnapshot, QStringLiteral("online"));
        QCOMPARE(QDateTime::fromMSecsSinceEpoch(item->dueAtMs, now.timeZone()).date(),
                 QDate(2026, 6, 29));
        QCOMPARE(QDateTime::fromMSecsSinceEpoch(item->dueAtMs, now.timeZone()).time(),
                 QTime(9, 0));
        QCOMPARE(item->createdAtMs, now.toMSecsSinceEpoch());
        QCOMPARE(item->updatedAtMs, now.toMSecsSinceEpoch());
        QCOMPARE(item->state, QStringLiteral("scheduled"));
    }

    void contactReminderRejectsEmptyContactId()
    {
        const QDateTime now(QDate(2026, 6, 28), QTime(12, 0), QTimeZone::systemTimeZone());

        QVERIFY(!makeContactReminderItem(QStringLiteral("  "),
                                         QStringLiteral("Alice"),
                                         QStringLiteral("online"),
                                         now).has_value());
    }

    void groupAnnouncementReminderStoresAnnouncementPayload()
    {
        const QDateTime now(QDate(2026, 6, 28), QTime(12, 0), QTimeZone::systemTimeZone());
        const qint64 dueAtMs = now.addSecs(3600).toMSecsSinceEpoch();

        const auto item = makeGroupAnnouncementReminderItem(QStringLiteral(" group-1 "),
                                                            QStringLiteral(" Project Team "),
                                                            QStringLiteral(" Ship the review package "),
                                                            QStringLiteral(" note "),
                                                            dueAtMs,
                                                            now);

        QVERIFY(item.has_value());
        QCOMPARE(item->targetType, QStringLiteral("group_announcement"));
        QCOMPARE(item->targetId, QStringLiteral("group-1"));
        QCOMPARE(item->groupId, QStringLiteral("group-1"));
        QCOMPARE(item->titleSnapshot, QStringLiteral("Project Team"));
        QCOMPARE(item->previewSnapshot, QStringLiteral("Ship the review package"));
        QCOMPARE(item->note, QStringLiteral("note"));
        QCOMPARE(item->dueAtMs, dueAtMs);
        const QJsonObject payload =
            QJsonDocument::fromJson(item->payloadJson.toUtf8()).object();
        QCOMPARE(payload.value(QStringLiteral("kind")).toString(),
                 QStringLiteral("announcement"));
    }

    void groupFileReminderStoresResourcePayload()
    {
        const QDateTime now(QDate(2026, 6, 28), QTime(12, 0), QTimeZone::systemTimeZone());
        const qint64 dueAtMs = now.addSecs(7200).toMSecsSinceEpoch();

        const auto item = makeGroupFileReminderItem(QStringLiteral(" group-1 "),
                                                    QStringLiteral(" file-1 "),
                                                    QStringLiteral(" plan.docx "),
                                                    QStringLiteral(" file preview "),
                                                    QStringLiteral(" later "),
                                                    dueAtMs,
                                                    now);

        QVERIFY(item.has_value());
        QCOMPARE(item->targetType, QStringLiteral("group_file"));
        QCOMPARE(item->targetId, QStringLiteral("file-1"));
        QCOMPARE(item->groupId, QStringLiteral("group-1"));
        QCOMPARE(item->resourceId, QStringLiteral("file-1"));
        QCOMPARE(item->titleSnapshot, QStringLiteral("plan.docx"));
        QCOMPARE(item->previewSnapshot, QStringLiteral("file preview"));
        QCOMPARE(item->note, QStringLiteral("later"));
        const QJsonObject payload =
            QJsonDocument::fromJson(item->payloadJson.toUtf8()).object();
        QCOMPARE(payload.value(QStringLiteral("kind")).toString(),
                 QStringLiteral("group_file"));
        QCOMPARE(payload.value(QStringLiteral("groupId")).toString(),
                 QStringLiteral("group-1"));
        QCOMPARE(payload.value(QStringLiteral("resourceId")).toString(),
                 QStringLiteral("file-1"));
        QCOMPARE(payload.value(QStringLiteral("fileName")).toString(),
                 QStringLiteral("plan.docx"));
    }

    void messageReminderStoresSourceMessageId()
    {
        const QDateTime now(QDate(2026, 6, 28), QTime(12, 0), QTimeZone::systemTimeZone());
        const qint64 dueAtMs = now.addSecs(1800).toMSecsSinceEpoch();

        const auto item = makeMessageReminderItem(QStringLiteral(" msg-1 "),
                                                  QStringLiteral(" direct:alice "),
                                                  QStringLiteral(" Alice "),
                                                  QStringLiteral(" reply soon "),
                                                  QStringLiteral(" note "),
                                                  dueAtMs,
                                                  now);

        QVERIFY(item.has_value());
        QCOMPARE(item->targetType, QStringLiteral("message"));
        QCOMPARE(item->targetId, QStringLiteral("msg-1"));
        QCOMPARE(item->conversationId, QStringLiteral("direct:alice"));
        QCOMPARE(item->sourceMessageId, QStringLiteral("msg-1"));
        QCOMPARE(item->titleSnapshot, QStringLiteral("Alice"));
        QCOMPARE(item->previewSnapshot, QStringLiteral("reply soon"));
        QCOMPARE(item->note, QStringLiteral("note"));
        QCOMPARE(item->dueAtMs, dueAtMs);
    }

    void messageReminderRejectsEmptyMessageId()
    {
        const QDateTime now(QDate(2026, 6, 28), QTime(12, 0), QTimeZone::systemTimeZone());
        const qint64 dueAtMs = now.addSecs(1800).toMSecsSinceEpoch();

        QVERIFY(!makeMessageReminderItem(QStringLiteral("  "),
                                         QStringLiteral("direct:alice"),
                                         QStringLiteral("Alice"),
                                         QStringLiteral("reply soon"),
                                         QStringLiteral("note"),
                                         dueAtMs,
                                         now).has_value());
    }
};

QTEST_MAIN(TestReminderItemFactory)
#include "TestReminderItemFactory.moc"
