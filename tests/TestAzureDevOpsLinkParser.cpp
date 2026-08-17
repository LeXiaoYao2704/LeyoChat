#include <QtTest>

#include "integrations/AzureDevOpsLinkParser.h"

class TestAzureDevOpsLinkParser : public QObject {
    Q_OBJECT

private slots:
    void parsesWorkItemUrl();
    void parsesPullRequestUrl();
    void parsesBuildUrl();
    void parsesLegacyVisualStudioUrl();
    void rejectsUnsupportedUrl();
};

void TestAzureDevOpsLinkParser::parsesWorkItemUrl()
{
    const auto locator = AzureDevOpsLinkParser::parse(
        QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_workitems/edit/1234"));

    QVERIFY(locator.has_value());
    QCOMPARE(locator->kind, AzureDevOpsResourceKind::WorkItem);
    QCOMPARE(locator->organization, QStringLiteral("leyochat"));
    QCOMPARE(locator->project, QStringLiteral("LeyoChat"));
    QCOMPARE(locator->resourceId, QStringLiteral("1234"));
    QCOMPARE(locator->kindName(), QStringLiteral("devops_work_item"));
}

void TestAzureDevOpsLinkParser::parsesPullRequestUrl()
{
    const auto locator = AzureDevOpsLinkParser::parse(
        QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_git/desktop/pullrequest/88"));

    QVERIFY(locator.has_value());
    QCOMPARE(locator->kind, AzureDevOpsResourceKind::PullRequest);
    QCOMPARE(locator->repository, QStringLiteral("desktop"));
    QCOMPARE(locator->resourceId, QStringLiteral("88"));
    QCOMPARE(locator->kindName(), QStringLiteral("devops_pull_request"));
}

void TestAzureDevOpsLinkParser::parsesBuildUrl()
{
    const auto locator = AzureDevOpsLinkParser::parse(
        QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_build/results?buildId=31&view=results"));

    QVERIFY(locator.has_value());
    QCOMPARE(locator->kind, AzureDevOpsResourceKind::Build);
    QCOMPARE(locator->resourceId, QStringLiteral("31"));
    QCOMPARE(locator->kindName(), QStringLiteral("devops_build"));
}

void TestAzureDevOpsLinkParser::parsesLegacyVisualStudioUrl()
{
    const auto locator = AzureDevOpsLinkParser::parse(
        QStringLiteral("https://example.visualstudio.com/LeyoChat/_workitems/edit/2048"));

    QVERIFY(locator.has_value());
    QCOMPARE(locator->organization, QStringLiteral("example"));
    QCOMPARE(locator->project, QStringLiteral("LeyoChat"));
    QCOMPARE(locator->resourceId, QStringLiteral("2048"));
}

void TestAzureDevOpsLinkParser::rejectsUnsupportedUrl()
{
    const auto locator = AzureDevOpsLinkParser::parse(
        QStringLiteral("https://example.com/issues/1234"));

    QVERIFY(!locator.has_value());
}

QTEST_MAIN(TestAzureDevOpsLinkParser)
#include "TestAzureDevOpsLinkParser.moc"
