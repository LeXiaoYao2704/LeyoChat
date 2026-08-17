#include <QtTest>
#include "ui/WorkbenchPage.h"

class TestWorkbenchPage : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void exposes_knowledge_base_card();
    void exposes_devops_card();
    void exposes_outlook_card();
    void exposes_ferry_card();
    void cleanupTestCase();
};

void TestWorkbenchPage::initTestCase()
{
}

void TestWorkbenchPage::exposes_knowledge_base_card()
{
    WorkbenchPage page;
    QVERIFY(page.hasCardForTesting(QStringLiteral("knowledge")));
}

void TestWorkbenchPage::exposes_devops_card()
{
    WorkbenchPage page;
    QVERIFY(page.hasCardForTesting(QStringLiteral("devops")));
}

void TestWorkbenchPage::exposes_outlook_card()
{
    WorkbenchPage page;
    QVERIFY(page.hasCardForTesting(QStringLiteral("outlook")));
}

void TestWorkbenchPage::exposes_ferry_card()
{
    WorkbenchPage page;
    QVERIFY(page.hasCardForTesting(QStringLiteral("ferry")));
}

void TestWorkbenchPage::cleanupTestCase()
{
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestWorkbenchPage tp;
    return QTest::qExec(&tp, argc, argv);
}

#include "TestWorkbenchPage.moc"
