#include <QDir>
#include <QUuid>
#include <QtTest>

#include "services/AzureDevOpsNotificationDispatcher.h"
#include "services/GroupService.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"
#include "storage/GroupRepository.h"

class TestAzureDevOpsNotificationDispatcher : public QObject {
    Q_OBJECT

private slots:
    void buildDirectDraft_createsResourceReferenceEnvelope()
    {
        const AzureDevOpsNotificationEvent event{
            AzureDevOpsNotificationKind::BuildCompleted,
            QStringLiteral("svc-devops"),
            QStringLiteral("workspace-alpha"),
            QStringLiteral("build:42"),
            QStringLiteral("Nightly Build"),
            QStringLiteral("nightly 构建成功"),
            QStringLiteral("succeeded"),
            QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_build/results?buildId=42"),
            QStringLiteral("CI Bot"),
        };

        const auto draft = AzureDevOpsNotificationDispatcher::buildDirectDraft(
            QStringLiteral("local-user"),
            QStringLiteral("peer-user"),
            QStringLiteral("张小乐"),
            event);
        QVERIFY(draft.has_value());
        QCOMPARE(draft->targetClientId, QStringLiteral("peer-user"));
        QCOMPARE(draft->conversationTitle, QStringLiteral("张小乐"));
        QCOMPARE(draft->message.messageType, std::wstring(L"resource_ref"));
        QCOMPARE(draft->envelope.type, MessageType::ResourceReference);
        QCOMPARE(QString::fromStdString(draft->envelope.resourceKind),
                 QStringLiteral("devops_build"));
        QCOMPARE(QString::fromStdString(draft->envelope.targetId), QStringLiteral("peer-user"));
    }

    void buildGroupFanOut_emitsEnvelopePerActiveRecipient()
    {
        const QString tempRoot = QDir::temp().filePath(
            QStringLiteral("leyochat-test-%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        QDir().mkpath(tempRoot);
        const QString databasePath = QDir(tempRoot).filePath(QStringLiteral("group-notify.db"));
        const QString connectionName = QStringLiteral("group-notify-%1")
                                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

        DatabaseManager db(databasePath, connectionName);
        QVERIFY(db.open());
        ConversationRepository conversationRepository(connectionName);
        GroupRepository groupRepository(connectionName);
        GroupService groupService(&groupRepository, &conversationRepository);

        Group group;
        QVERIFY(groupService.createGroup(QStringLiteral("owner"),
                                         QStringLiteral("ADO 群"),
                                         {QStringLiteral("alice"), QStringLiteral("bob")},
                                         &group));

        const AzureDevOpsNotificationEvent event{
            AzureDevOpsNotificationKind::PullRequestUpdated,
            QStringLiteral("svc-devops"),
            QStringLiteral("workspace-alpha"),
            QStringLiteral("pr:7"),
            QStringLiteral("PR #7"),
            QStringLiteral("等待评审"),
            QStringLiteral("active"),
            QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_git/LeyoChat/pullrequest/7"),
            QStringLiteral("Reviewer Bot"),
        };

        const auto envelopes = AzureDevOpsNotificationDispatcher::buildGroupFanOut(
            QStringLiteral("owner"),
            QString::fromStdWString(group.groupId),
            event,
            &groupService);

        QCOMPARE(static_cast<int>(envelopes.size()), 2);
        for (const auto& envelope : envelopes) {
            QCOMPARE(envelope.type, MessageType::ResourceReference);
            QCOMPARE(QString::fromStdString(envelope.conversationId),
                     QString::fromStdWString(group.groupId));
            QCOMPARE(QString::fromStdString(envelope.resourceKind),
                     QStringLiteral("devops_pull_request"));
        }

        QDir(tempRoot).removeRecursively();
    }
};

QTEST_MAIN(TestAzureDevOpsNotificationDispatcher)
#include "TestAzureDevOpsNotificationDispatcher.moc"
