#include <QtTest>

#include "app/UiRestoreHelpers.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

class TestUiRestoreHelpers : public QObject {
    Q_OBJECT

private slots:
    void windowStateSummary_handlesNullWindow();
    void windowStateSummary_reportsNormalWindowState();
    void windowStateSummary_reportsMaximizedWindowState();
    void shouldMaximizeMainWindowOnStartup_returnsFalseForNullWindow();
    void shouldMaximizeMainWindowOnStartup_keepsSmallWindowNormal();
    void shouldMaximizeMainWindowOnStartup_maximizesOversizedWindow();
};

void TestUiRestoreHelpers::windowStateSummary_handlesNullWindow()
{
    QCOMPARE(windowStateSummary(nullptr), QStringLiteral("window=null"));
}

void TestUiRestoreHelpers::windowStateSummary_reportsNormalWindowState()
{
    QWidget window;

    const QString summary = windowStateSummary(&window);

    QVERIFY(summary.contains(QStringLiteral("visible=false")));
    QVERIFY(summary.contains(QStringLiteral("state=normal")));
}

void TestUiRestoreHelpers::windowStateSummary_reportsMaximizedWindowState()
{
    QWidget window;
    window.setWindowState(Qt::WindowMaximized);

    const QString summary = windowStateSummary(&window);

    QVERIFY(summary.contains(QStringLiteral("state=maximized")));
}

void TestUiRestoreHelpers::shouldMaximizeMainWindowOnStartup_returnsFalseForNullWindow()
{
    QVERIFY(!shouldMaximizeMainWindowOnStartup(nullptr));
}

void TestUiRestoreHelpers::shouldMaximizeMainWindowOnStartup_keepsSmallWindowNormal()
{
    QWidget window;
    window.resize(320, 240);
    window.setMinimumSize(120, 90);

    QVERIFY(!shouldMaximizeMainWindowOnStartup(&window));
}

void TestUiRestoreHelpers::shouldMaximizeMainWindowOnStartup_maximizesOversizedWindow()
{
    QWidget window;
    const QScreen* screen = QGuiApplication::primaryScreen();
    QVERIFY(screen);

    const QSize availableSize = screen->availableGeometry().size();
    window.resize(availableSize.width() + 200, availableSize.height() + 200);

    QVERIFY(shouldMaximizeMainWindowOnStartup(&window));
}

QTEST_MAIN(TestUiRestoreHelpers)
#include "TestUiRestoreHelpers.moc"
