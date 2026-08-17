#include <QApplication>
#include <QDateTime>
#include <QLabel>
#include <QLineEdit>
#include <QtTest>

#include "ui/ReminderDialog.h"

class TestReminderDialog : public QObject {
    Q_OBJECT

private slots:
    void defaultsToFutureOneHourReminder()
    {
        const QDateTime before = QDateTime::currentDateTime();
        ReminderDialog dialog;

        const QDateTime due = dialog.selectedDueTime();

        QVERIFY(due.isValid());
        QVERIFY(due > before);
        QVERIFY(due <= QDateTime::currentDateTime().addSecs(65 * 60));
        QVERIFY(due >= before.addSecs(55 * 60));
        QVERIFY(dialog.okEnabledForTesting());
    }

    void tomorrowQuickOptionUsesNineAmTomorrow()
    {
        const QDate currentDate = QDate::currentDate();
        ReminderDialog dialog;

        dialog.selectQuickOptionForTesting(2);
        const QDateTime due = dialog.selectedDueTime();

        QVERIFY(due.isValid());
        QVERIFY(due.date() > currentDate);
        QCOMPARE(due.time().hour(), 9);
        QCOMPARE(due.time().minute(), 0);
        QVERIFY(dialog.okEnabledForTesting());
    }

    void pastCustomTimeDisablesOk()
    {
        ReminderDialog dialog;

        dialog.setCustomDueTimeForTesting(QDateTime::currentDateTime().addSecs(-60));

        QVERIFY(!dialog.okEnabledForTesting());
    }

    void contextPreviewUsesSnapshotsAndFallbacks()
    {
        ReminderDialog dialog;

        auto* titleLabel = dialog.findChild<QLabel*>(QStringLiteral("reminderContextTitle"));
        auto* previewLabel = dialog.findChild<QLabel*>(QStringLiteral("reminderContextPreview"));
        QVERIFY(titleLabel != nullptr);
        QVERIFY(previewLabel != nullptr);

        dialog.setContextPreview(QStringLiteral("  群文件：方案.docx  "),
                                 QStringLiteral("  需要确认最终版本  "));
        QCOMPARE(titleLabel->text(), QStringLiteral("群文件：方案.docx"));
        QCOMPARE(previewLabel->text(), QStringLiteral("需要确认最终版本"));

        dialog.setContextPreview(QStringLiteral("   "), QStringLiteral("   "));
        QCOMPARE(titleLabel->text(), QStringLiteral("当前内容"));
        QCOMPARE(previewLabel->text(), QStringLiteral("将在指定时间提醒你回来处理。"));
    }

    void noteReturnsTrimmedOptionalText()
    {
        ReminderDialog dialog;

        auto* noteEdit = dialog.findChild<QLineEdit*>(QStringLiteral("reminderNoteEdit"));
        QVERIFY(noteEdit != nullptr);

        noteEdit->setText(QStringLiteral("  回看会议纪要  "));

        QCOMPARE(dialog.note(), QStringLiteral("回看会议纪要"));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestReminderDialog tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestReminderDialog.moc"
