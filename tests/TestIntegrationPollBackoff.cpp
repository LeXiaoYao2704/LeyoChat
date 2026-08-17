#include <QtTest>

#include "app/IntegrationPollBackoff.h"

class TestIntegrationPollBackoff : public QObject {
    Q_OBJECT

private slots:
    void classifyIntegrationErrorCategory_recognizesAuthFailures();
    void classifyIntegrationErrorCategory_recognizesNetworkFailures();
    void classifyIntegrationErrorCategory_handlesEmptyAndUnknownMessages();
    void computeIntegrationPollIntervalMs_usesBaseIntervalWhenHealthy();
    void computeIntegrationPollIntervalMs_appliesCappedExponentialBackoff();
    void computeIntegrationPollIntervalMs_enforcesAuthAndMaximumCaps();
};

void TestIntegrationPollBackoff::classifyIntegrationErrorCategory_recognizesAuthFailures()
{
    QCOMPARE(classifyIntegrationErrorCategory(QStringLiteral("401 Unauthorized token expired")),
             QStringLiteral("auth"));
    QCOMPARE(classifyIntegrationErrorCategory(QStringLiteral("PAT refresh failed, login required")),
             QStringLiteral("auth"));
}

void TestIntegrationPollBackoff::classifyIntegrationErrorCategory_recognizesNetworkFailures()
{
    QCOMPARE(classifyIntegrationErrorCategory(QStringLiteral("Network timeout while connecting to host")),
             QStringLiteral("network"));
    QCOMPARE(classifyIntegrationErrorCategory(QStringLiteral("SSL connection refused temporarily")),
             QStringLiteral("network"));
}

void TestIntegrationPollBackoff::classifyIntegrationErrorCategory_handlesEmptyAndUnknownMessages()
{
    QCOMPARE(classifyIntegrationErrorCategory(QStringLiteral("   ")), QString());
    QCOMPARE(classifyIntegrationErrorCategory(QStringLiteral("unexpected service response")),
             QStringLiteral("unknown"));
}

void TestIntegrationPollBackoff::computeIntegrationPollIntervalMs_usesBaseIntervalWhenHealthy()
{
    QCOMPARE(computeIntegrationPollIntervalMs(5, 0, QString()), 5 * 60 * 1000);
    QCOMPARE(computeIntegrationPollIntervalMs(0, 0, QString()), 1 * 60 * 1000);
}

void TestIntegrationPollBackoff::computeIntegrationPollIntervalMs_appliesCappedExponentialBackoff()
{
    QCOMPARE(computeIntegrationPollIntervalMs(5, 1, QStringLiteral("network")), 5 * 60 * 1000);
    QCOMPARE(computeIntegrationPollIntervalMs(5, 2, QStringLiteral("network")), 10 * 60 * 1000);
    QCOMPARE(computeIntegrationPollIntervalMs(5, 4, QStringLiteral("network")), 40 * 60 * 1000);
    QCOMPARE(computeIntegrationPollIntervalMs(5, 7, QStringLiteral("network")), 40 * 60 * 1000);
}

void TestIntegrationPollBackoff::computeIntegrationPollIntervalMs_enforcesAuthAndMaximumCaps()
{
    QCOMPARE(computeIntegrationPollIntervalMs(5, 1, QStringLiteral("auth")), 15 * 60 * 1000);
    QCOMPARE(computeIntegrationPollIntervalMs(30, 4, QStringLiteral("network")), 60 * 60 * 1000);
}

QTEST_MAIN(TestIntegrationPollBackoff)
#include "TestIntegrationPollBackoff.moc"
