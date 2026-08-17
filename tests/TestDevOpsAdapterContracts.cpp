#include <QtTest>

#include "integrations/DevOpsAdapterContracts.h"

class TestDevOpsAdapterContracts : public QObject {
    Q_OBJECT

private slots:
    void workItemPayload_containsStructuredKindAndAction();
    void pullRequestReference_usesServiceOrigin();
    void buildPayload_usesDefinitionNameAndStatus();
};

void TestDevOpsAdapterContracts::workItemPayload_containsStructuredKindAndAction()
{
    const DevOpsWorkItemResource resource{
        QStringLiteral("svc-devops"),
        QStringLiteral("workspace-alpha"),
        QStringLiteral("wi-1024"),
        QStringLiteral("LeyoChat Contributors"),
        QStringLiteral("LeyoChat"),
        QStringLiteral("Bug"),
        QStringLiteral("修复群文件卡住"),
        QStringLiteral("进行中"),
        QStringLiteral("张小乐"),
        QStringLiteral("https://devops.example/workitems/1024"),
        1024,
    };

    const ResourceRefPayload payload = DevOpsAdapterContracts::makeWorkItemPayload(resource);
    QCOMPARE(payload.kind, QStringLiteral("devops_work_item"));
    QCOMPARE(payload.resourceId, QStringLiteral("wi-1024"));
    QCOMPARE(payload.status, QStringLiteral("进行中"));
    QVERIFY(payload.subtitle.contains(QStringLiteral("Bug")));
    QCOMPARE(payload.actions.size(), 1);
    QCOMPARE(payload.actions.front().label, QStringLiteral("打开工作项"));
    QCOMPARE(payload.actions.front().target, QStringLiteral("https://devops.example/workitems/1024"));
}

void TestDevOpsAdapterContracts::pullRequestReference_usesServiceOrigin()
{
    const DevOpsPullRequestResource resource{
        QStringLiteral("svc-devops"),
        QStringLiteral("workspace-alpha"),
        QStringLiteral("pr-88"),
        QStringLiteral("LeyoChat Contributors"),
        QStringLiteral("LeyoChat"),
        QStringLiteral("desktop"),
        QStringLiteral("接入 stage2 消息扩展"),
        QStringLiteral("等待合并"),
        QStringLiteral("侯晓刚"),
        QStringLiteral("https://devops.example/pullrequests/88"),
        88,
    };

    const ResourceReference reference = DevOpsAdapterContracts::makePullRequestReference(resource);
    QCOMPARE(reference.resourceKind, QStringLiteral("devops_pull_request"));
    QCOMPARE(reference.origin, ResourceOrigin::Service);
    QVERIFY(reference.summary.contains(QStringLiteral("desktop")));
}

void TestDevOpsAdapterContracts::buildPayload_usesDefinitionNameAndStatus()
{
    const DevOpsBuildResource resource{
        QStringLiteral("svc-devops"),
        QStringLiteral("workspace-alpha"),
        QStringLiteral("build-31"),
        QStringLiteral("LeyoChat Contributors"),
        QStringLiteral("LeyoChat"),
        QStringLiteral("Beta Release"),
        QStringLiteral("refs/heads/main"),
        QStringLiteral("已完成"),
        QStringLiteral("乔志晓"),
        QStringLiteral("https://devops.example/builds/31"),
        31,
    };

    const ResourceRefPayload payload = DevOpsAdapterContracts::makeBuildPayload(resource);
    QCOMPARE(payload.kind, QStringLiteral("devops_build"));
    QCOMPARE(payload.title, QStringLiteral("Beta Release"));
    QCOMPARE(payload.status, QStringLiteral("已完成"));
    QVERIFY(payload.subtitle.contains(QStringLiteral("refs/heads/main")));
}

QTEST_MAIN(TestDevOpsAdapterContracts)
#include "TestDevOpsAdapterContracts.moc"
