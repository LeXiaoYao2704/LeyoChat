#include <QtTest>

#include <ElaLineEdit.h>

#include "ui/GlobalSearchHistory.h"
#include "ui/GlobalSearchPanel.h"

class TestGlobalSearchPanel : public QObject {
    Q_OBJECT

private slots:
    void popup_hasClearFocusedSearchCue()
    {
        GlobalSearchHistory history(QStringLiteral("LeyoChatTest"),
                                    QStringLiteral("LeyoChatGlobalSearchPanelTest"));
        GlobalSearchPanel panel(&history);

        auto* edit = panel.findChild<ElaLineEdit*>();
        QVERIFY(edit != nullptr);

        QVERIFY2(edit->styleSheet().contains(QStringLiteral("QLineEdit:focus { border:2px solid")),
                 "The focused popup search field should have a visible focus cue beyond caret blink timing.");

        panel.popup(QPoint(80, 80));
        QTRY_VERIFY(panel.isVisible());
        QTRY_COMPARE(QApplication::focusWidget(), static_cast<QWidget*>(edit));

        panel.dismiss();
    }

    void popup_acceptsTypingWithoutClickingSearchEditAgain()
    {
        GlobalSearchHistory history(QStringLiteral("LeyoChatTest"),
                                    QStringLiteral("LeyoChatGlobalSearchPanelTest"));
        GlobalSearchPanel panel(&history);

        panel.popup(QPoint(80, 80));
        QTRY_VERIFY(panel.isVisible());

        auto* edit = panel.findChild<ElaLineEdit*>();
        QVERIFY(edit != nullptr);

        QTRY_COMPARE(QApplication::focusWidget(), static_cast<QWidget*>(edit));
        QTest::keyClicks(&panel, QStringLiteral("zhang"));

        QCOMPARE(edit->text(), QStringLiteral("zhang"));

        panel.dismiss();
    }
};

QTEST_MAIN(TestGlobalSearchPanel)
#include "TestGlobalSearchPanel.moc"
