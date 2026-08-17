#include <QtTest/QTest>
#include <QApplication>
#include <QFont>

#include "ui/AppStyle.h"

class TestDelegates : public QObject {
    Q_OBJECT

private slots:
    void styleFoundation_usesCalmVisualHierarchy() {
        QVERIFY(!AppStyle::stylesheet().trimmed().isEmpty());
        const QColor lightWindowBackground(AppStyle::windowBg(AppStyle::ThemeMode::Light));
        const QColor lightPrimaryText(AppStyle::textPrimary(AppStyle::ThemeMode::Light));
        QVERIFY(lightWindowBackground.isValid());
        QVERIFY(lightPrimaryText.isValid());
        QVERIFY(lightWindowBackground != lightPrimaryText);

        QFont baseFont(QStringLiteral("Microsoft YaHei UI"), 10);
        QFont largeFont(QStringLiteral("Microsoft YaHei UI"), 14);
        QVERIFY(AppStyle::conversationRowHeightForFont(largeFont)
                > AppStyle::conversationRowHeightForFont(baseFont));
    }

    void appStyle_stylesheet_and_rowHeights_areSet() {
        QVERIFY(!AppStyle::stylesheet().isEmpty());

        QFont baseFont;
        baseFont.setPointSize(9);

        QFont largeFont;
        largeFont.setPointSize(16);

        QVERIFY(AppStyle::conversationRowHeightForFont(largeFont) > AppStyle::conversationRowHeightForFont(baseFont));
        QVERIFY(AppStyle::contactRowHeightForFont(largeFont) > AppStyle::contactRowHeightForFont(baseFont));
        QVERIFY(AppStyle::transferRowHeightForFont(largeFont) > AppStyle::transferRowHeightForFont(baseFont));
    }

};

QTEST_MAIN(TestDelegates)
#include "TestDelegates.moc"
