#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QtTest/QTest>

#include "ui/ChatHeaderWidget.h"

class TestChatHeaderWidget : public QObject {
    Q_OBJECT

private slots:
    void switchesBetweenDirectAndGroupPresentation()
    {
        ChatHeaderWidget widget;

        widget.setDirectChatState(QStringLiteral("\u5F20\u4E09"),
                                  QStringLiteral("\u5728\u7EBF"),
                                  QStringLiteral("\u4ECA\u5929\u4F1A\u5728\u529E\u516C\u5BA4"));
        QCOMPARE(widget.titleText(), QStringLiteral("\u5F20\u4E09"));
        QCOMPARE(widget.subtitleText(), QStringLiteral("\u5728\u7EBF"));
        QVERIFY(widget.secondaryText().isEmpty());
        QVERIFY(!widget.isGroupMode());

        widget.setGroupChatState(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                                 QStringLiteral("12 \u4F4D\u6210\u5458"));
        QCOMPARE(widget.titleText(), QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));
        QCOMPARE(widget.subtitleText(), QStringLiteral("12 \u4F4D\u6210\u5458"));
        QVERIFY(widget.isGroupMode());
    }

    void exposesControlBandAndModeChipForConsoleStyleHeader()
    {
        ChatHeaderWidget widget;
        widget.setGroupChatState(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                                 QStringLiteral("12 \u4F4D\u6210\u5458"));

        auto* controlBand = widget.findChild<QWidget*>(QStringLiteral("headerControlBand"));
        QVERIFY(controlBand != nullptr);
        QVERIFY(controlBand->isHidden());

        auto* modeChip = widget.findChild<QLabel*>(QStringLiteral("headerModeChip"));
        QVERIFY(modeChip != nullptr);
        QVERIFY(!modeChip->text().trimmed().isEmpty());

        auto* actionTray = widget.findChild<QWidget*>(QStringLiteral("headerActionTray"));
        QVERIFY(actionTray != nullptr);
    }

    void removesRetryActionFromDirectHeader()
    {
        ChatHeaderWidget widget;
        widget.setDirectChatState(QStringLiteral("\u5F20\u4E09"),
                                  QStringLiteral("\u5728\u7EBF"),
                                  QStringLiteral("\u7B7E\u540D"));
        widget.show();
        QTest::qWait(10);

        const auto buttons = widget.findChildren<QPushButton*>();
        for (QPushButton* button : buttons) {
            QVERIFY(button->text() != QStringLiteral("\u91CD\u8BD5\u5F85\u53D1"));
        }
    }

    void keepsHeaderActionsCompactAcrossModes()
    {
        ChatHeaderWidget widget;
        widget.show();

        widget.setDirectChatState(QStringLiteral("\u5F20\u4E09"),
                                  QStringLiteral("\u5728\u7EBF"),
                                  QStringLiteral("\u7B7E\u540D"));
        QTest::qWait(10);

        int directVisibleButtons = 0;
        for (QPushButton* button : widget.findChildren<QPushButton*>()) {
            if (button->isVisible()) {
                ++directVisibleButtons;
            }
        }
        QVERIFY(directVisibleButtons <= 2);

        widget.setGroupChatState(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                                 QStringLiteral("12 \u4F4D\u6210\u5458"));
        QTest::qWait(10);

        int groupVisibleButtons = 0;
        for (QPushButton* button : widget.findChildren<QPushButton*>()) {
            if (button->isVisible()) {
                ++groupVisibleButtons;
            }
        }
        QVERIFY(groupVisibleButtons <= 3);
    }

    void exposesLiveStatusStripAcrossModes()
    {
        ChatHeaderWidget widget;
        widget.setDirectChatState(QStringLiteral("\u5F20\u4E09"),
                                  QStringLiteral("\u5728\u7EBF"),
                                  QStringLiteral("\u4ECA\u5929\u4F1A\u5728\u529E\u516C\u5BA4"));

        auto* statusStrip = widget.findChild<QWidget*>(QStringLiteral("headerStatusStrip"));
        auto* statusValue = widget.findChild<QLabel*>(QStringLiteral("headerStatusValue"));
        QVERIFY(statusStrip != nullptr);
        QVERIFY(statusValue != nullptr);
        QCOMPARE(statusValue->text(), QStringLiteral("\u76F4\u8FDE\u5728\u7EBF"));

        widget.setGroupChatState(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                                 QStringLiteral("12 \u4F4D\u6210\u5458"));
        QCOMPARE(statusValue->text(), QStringLiteral("\u7FA4\u534F\u4F5C\u8FDB\u884C\u4E2D"));
    }

    void group_mode_exposes_info_button()
    {
        ChatHeaderWidget widget;
        widget.setGroupChatState(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                                 QStringLiteral("12 \u4F4D\u6210\u5458"));

        QVERIFY(widget.findChild<QWidget*>(QStringLiteral("groupInfoButton")) != nullptr);
    }

    void direct_mode_does_not_show_persistent_details()
    {
        ChatHeaderWidget widget;
        widget.setDirectChatState(QStringLiteral("\u5F20\u4E09"),
                                  QStringLiteral("\u5728\u7EBF"),
                                  QStringLiteral("\u4ECA\u5929\u4F1A\u5728\u529E\u516C\u5BA4"));

        QVERIFY(widget.secondaryText().isEmpty());
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestChatHeaderWidget tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestChatHeaderWidget.moc"
