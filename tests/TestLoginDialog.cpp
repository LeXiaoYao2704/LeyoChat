#include <QtTest>

#include "ui/LoginDialog.h"

class TestLoginDialog : public QObject {
    Q_OBJECT

private slots:
    void has_idle_loading_error_states();
    void supports_minimal_state_transition();
    void first_run_entry_uses_echo_shell();
};

void TestLoginDialog::has_idle_loading_error_states()
{
    LoginDialog dialog;

    QVERIFY(dialog.hasStateForTesting(QStringLiteral("idle")));
    QVERIFY(dialog.hasStateForTesting(QStringLiteral("loading")));
    QVERIFY(dialog.hasStateForTesting(QStringLiteral("error")));
}

void TestLoginDialog::supports_minimal_state_transition()
{
    LoginDialog dialog;

    dialog.setStateForTesting(QStringLiteral("loading"));
    QVERIFY(dialog.currentStateForTesting() == QStringLiteral("loading"));

    dialog.setStateForTesting(QStringLiteral("error"));
    QVERIFY(dialog.currentStateForTesting() == QStringLiteral("error"));

    dialog.setStateForTesting(QStringLiteral("idle"));
    QVERIFY(dialog.currentStateForTesting() == QStringLiteral("idle"));
}

void TestLoginDialog::first_run_entry_uses_echo_shell()
{
    LoginDialog dialog;

    QVERIFY(dialog.findChild<QWidget*>(QStringLiteral("FirstRunEchoShell")) != nullptr);
    QVERIFY(dialog.findChild<QWidget*>(QStringLiteral("FirstRunEchoIdentityCard")) != nullptr);
    QVERIFY(dialog.findChild<QWidget*>(QStringLiteral("FirstRunEchoHeroCard")) != nullptr);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestLoginDialog tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestLoginDialog.moc"
