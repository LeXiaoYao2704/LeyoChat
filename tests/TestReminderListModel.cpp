#include <QtTest/QtTest>

#include "domain/ReminderItem.h"
#include "ui/ReminderListModel.h"

class TestReminderListModel : public QObject {
    Q_OBJECT

private slots:
    void exposesReminderRowsSortedByDueTime()
    {
        ReminderItem later;
        later.reminderId = QStringLiteral("r-later");
        later.targetType = QStringLiteral("group_file");
        later.titleSnapshot = QStringLiteral("Design.pdf");
        later.previewSnapshot = QStringLiteral("group file");
        later.dueAtMs = 3000;
        later.state = QStringLiteral("scheduled");

        ReminderItem earlier;
        earlier.reminderId = QStringLiteral("r-earlier");
        earlier.targetType = QStringLiteral("message");
        earlier.titleSnapshot = QStringLiteral("Alice");
        earlier.previewSnapshot = QStringLiteral("reply later");
        earlier.dueAtMs = 1000;
        earlier.state = QStringLiteral("due");

        ReminderListModel model;
        model.setItems(QVector<ReminderItem>{later, earlier});

        QCOMPARE(model.rowCount(), 2);
        const QModelIndex first = model.index(0, 0);
        QVERIFY(first.isValid());
        QCOMPARE(first.data(ReminderListModel::ReminderIdRole).toString(), QStringLiteral("r-earlier"));
        QCOMPARE(first.data(ReminderListModel::TitleRole).toString(), QStringLiteral("Alice"));
        QCOMPARE(first.data(ReminderListModel::PreviewRole).toString(), QStringLiteral("reply later"));
        QCOMPARE(first.data(ReminderListModel::DueTimeRole).toLongLong(), qint64(1000));
        QCOMPARE(first.data(ReminderListModel::StateRole).toString(), QStringLiteral("due"));
        QCOMPARE(first.data(ReminderListModel::TargetTypeRole).toString(), QStringLiteral("message"));
    }

    void itemAtReturnsStoredReminder()
    {
        ReminderItem item;
        item.reminderId = QStringLiteral("r-1");
        item.targetType = QStringLiteral("contact");
        item.titleSnapshot = QStringLiteral("Bob");
        item.previewSnapshot = QStringLiteral("follow up");
        item.dueAtMs = 2000;
        item.state = QStringLiteral("scheduled");

        ReminderListModel model;
        model.setItems(QVector<ReminderItem>{item});

        QCOMPARE(model.itemAt(0).reminderId, QStringLiteral("r-1"));
        QVERIFY(model.itemAt(-1).reminderId.isEmpty());
        QVERIFY(model.itemAt(1).reminderId.isEmpty());
    }
};

QTEST_MAIN(TestReminderListModel)
#include "TestReminderListModel.moc"
