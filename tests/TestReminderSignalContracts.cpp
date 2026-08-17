#include <QtTest>

#include <QApplication>
#include "ui/ConversationsPage.h"
#include "ui/GroupFileManagerDialog.h"
#include "ui/GroupFileTableModel.h"

#include <QAction>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QSignalSpy>
#include <QTableView>
#include <QTimer>

namespace {

QString readSourceFile(const QString& relativePath)
{
    const QString path = QFINDTESTDATA(relativePath.toUtf8().constData());
    if (path.isEmpty()) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

class TestReminderSignalContracts : public QObject {
    Q_OBJECT

private slots:
    void applicationSchedulesContactRemindersLocally()
    {
        const QString source = readSourceFile(QStringLiteral("../src/app/LeyoApplication.cpp"));
        QVERIFY2(!source.isEmpty(), "LeyoApplication.cpp must be available");
        QVERIFY2(source.contains(QStringLiteral("&MainWindow::contactReminderRequested")),
                 "application must handle contact reminder requests");
        QVERIFY2(source.contains(QStringLiteral("makeContactReminderItem(")),
                 "contact reminder requests must use the tested reminder item factory");
        QVERIFY2(source.contains(QStringLiteral("reminder->targetType == QStringLiteral(\"contact\")")),
                 "opening reminder notifications must route contact reminders separately");
    }

    void applicationSchedulesGroupAnnouncementRemindersLocally()
    {
        const QString appSource =
            readSourceFile(QStringLiteral("../src/app/LeyoApplication.cpp"));

        QVERIFY2(!appSource.isEmpty(), "LeyoApplication.cpp must be available");
        QVERIFY2(appSource.contains(QStringLiteral(
                     "&MainWindow::groupAnnouncementReminderRequested")),
                 "application must handle group announcement reminder requests");
        QVERIFY2(appSource.contains(QStringLiteral(
                     "makeGroupAnnouncementReminderItem(")),
                 "group announcement reminder requests must use the tested reminder item factory");
        QVERIFY2(appSource.contains(QStringLiteral(
                     "reminder->targetType == QStringLiteral(\"group_announcement\")")),
                 "opening reminder notifications must route group announcement reminders separately");
    }

    void applicationSchedulesGroupFileRemindersLocally()
    {
        const QString appSource =
            readSourceFile(QStringLiteral("../src/app/LeyoApplication.cpp"));

        QVERIFY2(!appSource.isEmpty(), "LeyoApplication.cpp must be available");
        QVERIFY2(appSource.contains(QStringLiteral(
                     "&GroupFileManagerDialog::groupFileReminderRequested")),
                 "application must handle group file reminder requests from the manager dialog");
        QVERIFY2(appSource.contains(QStringLiteral(
                     "makeGroupFileReminderItem(")),
                 "group file reminder requests must use the tested reminder item factory");
        QVERIFY2(appSource.contains(QStringLiteral(
                     "reminder->targetType == QStringLiteral(\"group_file\")")),
                 "opening reminder notifications must route group file reminders separately");
    }

    void groupFileReminderContextMenuEmitsFileSnapshot()
    {
        GroupFileServiceConfig config;
        config.baseUrl = QStringLiteral("http://127.0.0.1:1");
        config.bearerToken = QStringLiteral("test-token");

        GroupFileManagerDialog dialog(QStringLiteral("group-1"),
                                      config,
                                      QStringLiteral("client-1"),
                                      QStringLiteral("Alice"),
                                      true);
        QSignalSpy reminderSpy(&dialog, &GroupFileManagerDialog::groupFileReminderRequested);

        auto* model = dialog.findChild<GroupFileTableModel*>();
        auto* tableView = dialog.findChild<QTableView*>();
        QVERIFY(model != nullptr);
        QVERIFY(tableView != nullptr);

        QJsonObject file;
        file.insert(QStringLiteral("file_id"), QStringLiteral("resource-1"));
        file.insert(QStringLiteral("file_name"), QStringLiteral("方案.docx"));
        file.insert(QStringLiteral("uploaded_by_id"), QStringLiteral("client-1"));
        model->setFiles(QJsonArray{file});
        QCOMPARE(model->rowCount(), 1);

        dialog.show();
        QTest::qWait(20);
        const QRect rowRect = tableView->visualRect(model->index(0, 0));
        QVERIFY(rowRect.isValid());

        QTimer::singleShot(50, []() {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                auto* menu = qobject_cast<QMenu*>(widget);
                if (!menu) {
                    continue;
                }
                const QList<QAction*> actions = menu->actions();
                if (actions.size() >= 4) {
                    actions.at(3)->trigger();
                    menu->close();
                    return;
                }
            }
        });

        QMetaObject::invokeMethod(tableView,
                                  "customContextMenuRequested",
                                  Qt::DirectConnection,
                                  Q_ARG(QPoint, rowRect.center()));

        QCOMPARE(reminderSpy.count(), 1);
        const QList<QVariant> args = reminderSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("group-1"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("resource-1"));
        QCOMPARE(args.at(2).toString(), QStringLiteral("方案.docx"));
        QVERIFY(args.at(3).toString().contains(QStringLiteral("方案.docx")));
    }

    void groupFileReminderContextMenuIgnoresFilesWithoutResourceIds()
    {
        GroupFileServiceConfig config;
        config.baseUrl = QStringLiteral("http://127.0.0.1:1");
        config.bearerToken = QStringLiteral("test-token");

        GroupFileManagerDialog dialog(QStringLiteral("group-1"),
                                      config,
                                      QStringLiteral("client-1"),
                                      QStringLiteral("Alice"),
                                      true);
        QSignalSpy reminderSpy(&dialog, &GroupFileManagerDialog::groupFileReminderRequested);

        auto* model = dialog.findChild<GroupFileTableModel*>();
        auto* tableView = dialog.findChild<QTableView*>();
        QVERIFY(model != nullptr);
        QVERIFY(tableView != nullptr);

        QJsonObject file;
        file.insert(QStringLiteral("file_name"), QStringLiteral("missing-id.docx"));
        file.insert(QStringLiteral("uploaded_by_id"), QStringLiteral("client-1"));
        model->setFiles(QJsonArray{file});
        QCOMPARE(model->rowCount(), 1);

        dialog.show();
        QTest::qWait(20);
        const QRect rowRect = tableView->visualRect(model->index(0, 0));
        QVERIFY(rowRect.isValid());

        QTimer::singleShot(50, []() {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                auto* menu = qobject_cast<QMenu*>(widget);
                if (!menu) {
                    continue;
                }
                const QList<QAction*> actions = menu->actions();
                if (actions.size() >= 4) {
                    actions.at(3)->trigger();
                    menu->close();
                    return;
                }
            }
        });

        QMetaObject::invokeMethod(tableView,
                                  "customContextMenuRequested",
                                  Qt::DirectConnection,
                                  Q_ARG(QPoint, rowRect.center()));

        QCOMPARE(reminderSpy.count(), 0);
    }
};

QTEST_MAIN(TestReminderSignalContracts)

#include "TestReminderSignalContracts.moc"
