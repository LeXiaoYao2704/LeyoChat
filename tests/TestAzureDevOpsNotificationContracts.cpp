#include <QtTest>

#include "integrations/AzureDevOpsNotificationContracts.h"

class TestAzureDevOpsNotificationContracts : public QObject {
    Q_OBJECT

private slots:
    void buildNotificationPayload_containsOpenAction()
    {
        const AzureDevOpsNotificationEvent event{
            AzureDevOpsNotificationKind::BuildCompleted,
            QStringLiteral("svc-devops"),
            QStringLiteral("workspace-alpha"),
            QStringLiteral("build:31"),
            QStringLiteral("Beta Release"),
            QStringLiteral("main 分支构建成功"),
            QStringLiteral("succeeded"),
            QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_build/results?buildId=31"),
            QStringLiteral("CI Bot"),
        };

        const ResourceRefPayload payload =
            AzureDevOpsNotificationContracts::makeNotificationPayload(event);
        QCOMPARE(payload.kind, QStringLiteral("devops_build"));
        QCOMPARE(payload.resourceId, QStringLiteral("build:31"));
        QCOMPARE(payload.status, QStringLiteral("succeeded"));
        QCOMPARE(payload.actions.size(), 1);
        QCOMPARE(payload.actions.front().target,
                 QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_build/results?buildId=31"));
        QCOMPARE(payload.actions.front().label, QStringLiteral("打开详情"));
    }

    void pullRequestNotificationPayload_usesPullRequestKind()
    {
        const AzureDevOpsNotificationEvent event{
            AzureDevOpsNotificationKind::PullRequestUpdated,
            QStringLiteral("svc-devops"),
            QStringLiteral("workspace-alpha"),
            QStringLiteral("pull-request:desktop:91"),
            QStringLiteral("完善 PR 通知"),
            QStringLiteral("desktop 仓库有新的 PR 更新"),
            QStringLiteral("active"),
            QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_git/desktop/pullrequest/91"),
            QStringLiteral("张大乐"),
        };

        const ResourceRefPayload payload =
            AzureDevOpsNotificationContracts::makeNotificationPayload(event);
        QCOMPARE(payload.kind, QStringLiteral("devops_pull_request"));
        QCOMPARE(payload.resourceId, QStringLiteral("pull-request:desktop:91"));
        QCOMPARE(payload.status, QStringLiteral("active"));
    }

    void workItemNotificationPayload_usesWorkItemKind()
    {
        const AzureDevOpsNotificationEvent event{
            AzureDevOpsNotificationKind::WorkItemUpdated,
            QStringLiteral("svc-devops"),
            QStringLiteral("workspace-alpha"),
            QStringLiteral("work-item:123"),
            QStringLiteral("处理分配给我的工作项"),
            QStringLiteral("分配给我 / 需要我处理"),
            QStringLiteral("active"),
            QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_workitems/edit/123"),
            QStringLiteral("侯晓刚"),
        };

        const ResourceRefPayload payload =
            AzureDevOpsNotificationContracts::makeNotificationPayload(event);
        QCOMPARE(payload.kind, QStringLiteral("devops_work_item"));
        QCOMPARE(payload.resourceId, QStringLiteral("work-item:123"));
        QCOMPARE(payload.status, QStringLiteral("active"));
    }

    void mentionedWorkItemPayload_usesMentionLabel()
    {
        const AzureDevOpsNotificationEvent event{
            AzureDevOpsNotificationKind::WorkItemMentioned,
            QStringLiteral("svc-devops"),
            QStringLiteral("workspace-alpha"),
            QStringLiteral("work-item:456"),
            QString(),
            QStringLiteral("评论中提及了你"),
            QStringLiteral("active"),
            QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_workitems/edit/456"),
            QStringLiteral("张三"),
        };

        const ResourceRefPayload payload =
            AzureDevOpsNotificationContracts::makeNotificationPayload(event);
        QCOMPARE(payload.kind, QStringLiteral("devops_work_item"));
        QCOMPARE(payload.title, QStringLiteral("工作项提及"));
        QCOMPARE(payload.resourceId, QStringLiteral("work-item:456"));
    }

    void reviewRequestedPayload_usesReviewLabel()
    {
        const AzureDevOpsNotificationEvent event{
            AzureDevOpsNotificationKind::PullRequestReviewRequested,
            QStringLiteral("svc-devops"),
            QStringLiteral("workspace-alpha"),
            QStringLiteral("pull-request:desktop:92"),
            QString(),
            QStringLiteral("等待 Alice、Bob 审阅"),
            QStringLiteral("active"),
            QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_git/desktop/pullrequest/92"),
            QStringLiteral("Alice"),
        };

        const ResourceRefPayload payload =
            AzureDevOpsNotificationContracts::makeNotificationPayload(event);
        QCOMPARE(payload.title, QStringLiteral("PR 审核请求"));
        QCOMPARE(payload.subtitle, QStringLiteral("等待 Alice、Bob 审阅"));
    }
};

QTEST_MAIN(TestAzureDevOpsNotificationContracts)
#include "TestAzureDevOpsNotificationContracts.moc"
