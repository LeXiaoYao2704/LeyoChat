#include <QtTest/QTest>

#include "app/ChatRefreshPolicy.h"

class TestChatRefreshPolicy : public QObject {
    Q_OBJECT

private slots:
    void startupCooldown_isShortEnough()
    {
        QCOMPARE(chatRefreshCooldownMs(0), 3000);
        QCOMPARE(chatRefreshCooldownMs(14999), 3000);
    }

    void postStartupCooldown_isResponsive()
    {
        QCOMPARE(chatRefreshCooldownMs(15000), 1000);
        QCOMPARE(chatRefreshCooldownMs(60000), 1000);
    }

    void backoffDelay_isBoundedForUiSmoothness()
    {
        QCOMPARE(chatFlushBackoffDelayMs(0), 120);
        QCOMPARE(chatFlushBackoffDelayMs(9), 120);
        QCOMPARE(chatFlushBackoffDelayMs(20), 240);
        QCOMPARE(chatFlushBackoffDelayMs(40), 480);
        QCOMPARE(chatFlushBackoffDelayMs(80), 1920);
        QCOMPARE(chatFlushBackoffDelayMs(200), 5000);
    }

    void effectiveDelay_respectsCooldown()
    {
        // cooldown=1000ms, sinceLastFlush=200ms, remain=800ms → max(80, 800)=800
        QCOMPARE(chatRefreshEffectiveDelayMs(80, 200, 20000, true), 800);
        // requestedDelay=2000 > remain=800 → max(2000, 800)=2000
        QCOMPARE(chatRefreshEffectiveDelayMs(2000, 200, 20000, true), 2000);
        // sinceLastFlush=5000 >= cooldown=1000 → 直接返回 requested=80
        QCOMPARE(chatRefreshEffectiveDelayMs(80, 5000, 20000, true), 80);
        // hasLastFlush=false → 直接返回 requested=80
        QCOMPARE(chatRefreshEffectiveDelayMs(80, 200, 1000, false), 80);
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    TestChatRefreshPolicy tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestChatRefreshPolicy.moc"
