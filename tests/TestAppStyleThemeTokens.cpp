#include <QApplication>
#include <QtTest/QTest>

#include "ui/AppStyle.h"

class TestAppStyleThemeTokens : public QObject {
    Q_OBJECT

private slots:
    void exposes_structural_tokens_for_light_and_dark();
};

void TestAppStyleThemeTokens::exposes_structural_tokens_for_light_and_dark()
{
    const auto lightMode = AppStyle::ThemeMode::Light;
    const auto darkMode = AppStyle::ThemeMode::Dark;

    QCOMPARE(AppStyle::workspaceBase(lightMode), AppStyle::windowBg(lightMode));
    QCOMPARE(AppStyle::panelBase(lightMode), AppStyle::surface(lightMode));
    QCOMPARE(AppStyle::panelRaised(lightMode), AppStyle::surfaceAlt(lightMode));
    QCOMPARE(AppStyle::panelOverlay(lightMode), AppStyle::surfaceMuted(lightMode));
    QCOMPARE(AppStyle::dividerSubtle(lightMode), AppStyle::border(lightMode));
    QCOMPARE(AppStyle::dividerStrong(lightMode), AppStyle::borderStrong(lightMode));

    QCOMPARE(AppStyle::workspaceBase(darkMode), AppStyle::windowBg(darkMode));
    QCOMPARE(AppStyle::panelBase(darkMode), AppStyle::surface(darkMode));
    QCOMPARE(AppStyle::panelRaised(darkMode), AppStyle::surfaceAlt(darkMode));
    QCOMPARE(AppStyle::panelOverlay(darkMode), AppStyle::surfaceMuted(darkMode));
    QCOMPARE(AppStyle::dividerSubtle(darkMode), AppStyle::border(darkMode));
    QCOMPARE(AppStyle::dividerStrong(darkMode), AppStyle::borderStrong(darkMode));
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestAppStyleThemeTokens tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestAppStyleThemeTokens.moc"