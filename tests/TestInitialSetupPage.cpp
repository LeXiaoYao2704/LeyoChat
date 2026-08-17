#include <QtTest>

#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "ui/ClientPreferences.h"
#include "ui/InitialSetupPage.h"

class TestInitialSetupPage : public QObject {
    Q_OBJECT

private slots:
    void single_task_layout_for_entry_flow();
    void completion_action_emits_shell_entry_signal();
};

void TestInitialSetupPage::single_task_layout_for_entry_flow()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Profile profile;
    profile.displayName = L"测试用户";
    ClientPreferences preferences;

    InitialSetupPage page(profile, preferences, tempDir.path());

    QCOMPARE(page.stepCount(), 1);
    QCOMPARE(page.currentStep(), 1);
    QVERIFY(!page.hasPreviousStep());
    QVERIFY(!page.hasNextStep());
}

void TestInitialSetupPage::completion_action_emits_shell_entry_signal()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Profile profile;
    profile.displayName = L"测试用户";
    ClientPreferences preferences;

    InitialSetupPage page(profile, preferences, tempDir.path());

    QSignalSpy completedSpy(&page, &InitialSetupPage::setupCompleted);
    QVERIFY(completedSpy.isValid());

    QPushButton* startButton = nullptr;
    for (QPushButton* button : page.findChildren<QPushButton*>()) {
        if (button && button->text().contains(QStringLiteral("保存并进入"))) {
            startButton = button;
            break;
        }
    }

    QVERIFY(startButton != nullptr);
    startButton->click();
    QCOMPARE(completedSpy.count(), 1);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestInitialSetupPage tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestInitialSetupPage.moc"
