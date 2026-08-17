#include <QApplication>
#include <QtTest/QTest>

#include "ui/ContextPanel.h"

class TestContextPanel : public QObject {
    Q_OBJECT

private slots:
    void private_chat_starts_collapsed();
    void group_chat_supports_sections();
};

void TestContextPanel::private_chat_starts_collapsed()
{
    ContextPanel panel;

    panel.showPrivateProfile(QStringLiteral("direct:zhangsan"));

    QVERIFY(!panel.isPinnedOpen());
}

void TestContextPanel::group_chat_supports_sections()
{
    ContextPanel panel;

    panel.showGroupContext(QStringLiteral("group:project"));

    QVERIFY(panel.hasSection(QStringLiteral("announcements")));
    QVERIFY(panel.hasSection(QStringLiteral("members")));
    QVERIFY(panel.hasSection(QStringLiteral("files")));
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestContextPanel tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestContextPanel.moc"
