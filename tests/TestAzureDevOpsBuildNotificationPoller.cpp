#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "services/AzureDevOpsBuildNotificationPoller.h"

class FakeAzureDevOpsApiTransport : public IAzureDevOpsApiTransport {
public:
    explicit FakeAzureDevOpsApiTransport(QJsonDocument document)
        : m_document(std::move(document))
    {
    }

    std::optional<QJsonDocument> getJson(const QUrl&,
                                         const AzureDevOpsConnectionSettings&,
                                         QString*) const override
    {
        return m_document;
    }

    std::optional<QJsonDocument> postJson(const QUrl&,
                                          const AzureDevOpsConnectionSettings&,
                                          const QJsonDocument&,
                                          QString*) const override
    {
        return m_document;
    }

private:
    QJsonDocument m_document;
};

class TestAzureDevOpsBuildNotificationPoller : public QObject {
    Q_OBJECT

private slots:
    void emitsNotificationForNewBuild();
    void suppressesNotificationWhenBuildAlreadySeen();
    void pollsMultipleTrackedTargetsWithoutOverwritingProgress();
    void skipsDisabledTargets();
    void emitsNotificationForNewPullRequest();
    void emitsReviewRequestedNotificationForPendingReviewer();
    void emitsNotificationForAssignedWorkItem();
    void emitsNotificationForWorkItemStateChange();
    void emitsNotificationForWorkItemMention();
    void emitsNotificationForWorkItemComment();
    void emitsBuildRecoveredWhenSuccessfulBuildFollowsFailure();
    void pollsTrackedNotificationsAcrossBuildPullRequestAndWorkItem();
    void pollTrackedNotifications_recordsFailureStatePerTarget();
    void pollTrackedNotifications_promotesAuthFailureIntoSettingsSummary();
    void pollTrackedNotifications_clearsSettingsFailureAfterRecovery();
};

void TestAzureDevOpsBuildNotificationPoller::emitsNotificationForNewBuild()
{
    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("leyochat");
    settings.project = QStringLiteral("LeyoChat");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;

    const QJsonDocument document(QJsonObject{
        {QStringLiteral("value"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("id"), 88},
                 {QStringLiteral("buildNumber"), QStringLiteral("2026.04.10.1")},
                 {QStringLiteral("result"), QStringLiteral("succeeded")},
                 {QStringLiteral("definition"),
                  QJsonObject{{QStringLiteral("name"), QStringLiteral("Nightly")}}},
                 {QStringLiteral("requestedFor"),
                  QJsonObject{{QStringLiteral("displayName"), QStringLiteral("CI Bot")}}},
                 {QStringLiteral("_links"),
                  QJsonObject{{QStringLiteral("web"),
                               QJsonObject{{QStringLiteral("href"),
                                            QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_build/results?buildId=88")}}}}},
             },
         }},
    });

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<FakeAzureDevOpsApiTransport>(document));

    int latestObservedBuildId = 0;
    const auto event = poller.pollLatestBuild(20, &latestObservedBuildId, nullptr);
    QVERIFY(event.has_value());
    QCOMPARE(latestObservedBuildId, 88);
    QCOMPARE(event->kind, AzureDevOpsNotificationKind::BuildCompleted);
    QCOMPARE(event->resourceId, QStringLiteral("build:88"));
    QCOMPARE(event->title, QStringLiteral("Nightly"));
    QCOMPARE(event->status, QStringLiteral("succeeded"));
}

void TestAzureDevOpsBuildNotificationPoller::suppressesNotificationWhenBuildAlreadySeen()
{
    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("leyochat");
    settings.project = QStringLiteral("LeyoChat");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;

    const QJsonDocument document(QJsonObject{
        {QStringLiteral("value"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("id"), 88},
                 {QStringLiteral("buildNumber"), QStringLiteral("2026.04.10.1")},
             },
         }},
    });

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<FakeAzureDevOpsApiTransport>(document));

    int latestObservedBuildId = 0;
    const auto event = poller.pollLatestBuild(88, &latestObservedBuildId, nullptr);
    QVERIFY(!event.has_value());
    QCOMPARE(latestObservedBuildId, 88);
}

void TestAzureDevOpsBuildNotificationPoller::pollsMultipleTrackedTargetsWithoutOverwritingProgress()
{
    class RoutedTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl& url,
                                             const AzureDevOpsConnectionSettings&,
                                             QString*) const override
        {
            const QString path = url.toString();
            if (path.contains(QStringLiteral("/org-a/Project-1/"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("id"), 150},
                             {QStringLiteral("buildNumber"), QStringLiteral("2026.04.11.1")},
                             {QStringLiteral("result"), QStringLiteral("succeeded")},
                             {QStringLiteral("definition"),
                              QJsonObject{{QStringLiteral("name"), QStringLiteral("A-Pipeline")}}},
                         },
                     }},
                });
            }
            if (path.contains(QStringLiteral("/org-b/Project-2/"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("id"), 305},
                             {QStringLiteral("buildNumber"), QStringLiteral("2026.04.11.9")},
                             {QStringLiteral("result"), QStringLiteral("failed")},
                             {QStringLiteral("definition"),
                              QJsonObject{{QStringLiteral("name"), QStringLiteral("B-Pipeline")}}},
                         },
                     }},
                });
            }
            return std::nullopt;
        }

        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString*) const override
        {
            return std::nullopt;
        }
    };

    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("org-a");
    settings.project = QStringLiteral("Project-1");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;
    settings.notificationTargets.push_back(
        {QStringLiteral("org-a"), QStringLiteral("Project-1"), true, 100});
    settings.notificationTargets.push_back(
        {QStringLiteral("org-b"), QStringLiteral("Project-2"), true, 300});

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<RoutedTransport>());

    QString errorMessage;
    const auto events = poller.pollTrackedBuilds(&settings, &errorMessage);
    QCOMPARE(errorMessage, QString());
    QCOMPARE(events.size(), 2);
    QCOMPARE(settings.notificationTargets.at(0).lastNotifiedBuildId, 150);
    QCOMPARE(settings.notificationTargets.at(1).lastNotifiedBuildId, 305);
}

void TestAzureDevOpsBuildNotificationPoller::skipsDisabledTargets()
{
    class RoutedTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl& url,
                                             const AzureDevOpsConnectionSettings&,
                                             QString*) const override
        {
            const QString path = url.toString();
            if (path.contains(QStringLiteral("/org-a/Project-1/"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("id"), 151},
                             {QStringLiteral("buildNumber"), QStringLiteral("2026.04.11.2")},
                         },
                     }},
                });
            }
            if (path.contains(QStringLiteral("/org-b/Project-2/"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("id"), 999},
                             {QStringLiteral("buildNumber"), QStringLiteral("2026.04.11.99")},
                         },
                     }},
                });
            }
            return std::nullopt;
        }

        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString*) const override
        {
            return std::nullopt;
        }
    };

    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("org-a");
    settings.project = QStringLiteral("Project-1");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;
    settings.notificationTargets.push_back(
        {QStringLiteral("org-a"), QStringLiteral("Project-1"), true, 100});
    settings.notificationTargets.push_back(
        {QStringLiteral("org-b"), QStringLiteral("Project-2"), false, 300});

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<RoutedTransport>());

    QString errorMessage;
    const auto events = poller.pollTrackedBuilds(&settings, &errorMessage);
    QCOMPARE(errorMessage, QString());
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.at(0).resourceId, QStringLiteral("build:151"));
    QCOMPARE(settings.notificationTargets.at(0).lastNotifiedBuildId, 151);
    QCOMPARE(settings.notificationTargets.at(1).lastNotifiedBuildId, 300);
}

void TestAzureDevOpsBuildNotificationPoller::emitsNotificationForNewPullRequest()
{
    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("leyochat");
    settings.project = QStringLiteral("LeyoChat");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;

    const QJsonDocument document(QJsonObject{
        {QStringLiteral("value"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("pullRequestId"), 91},
                 {QStringLiteral("title"), QStringLiteral("PR update")},
                 {QStringLiteral("status"), QStringLiteral("active")},
                 {QStringLiteral("creationDate"), QStringLiteral("2026-04-11T12:34:56Z")},
                 {QStringLiteral("createdBy"), QJsonObject{{QStringLiteral("displayName"), QStringLiteral("Alice")}}},
                 {QStringLiteral("repository"), QJsonObject{{QStringLiteral("name"), QStringLiteral("desktop")}}},
                 {QStringLiteral("_links"),
                  QJsonObject{{QStringLiteral("web"),
                               QJsonObject{{QStringLiteral("href"),
                                            QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_git/desktop/pullrequest/91")}}}}},
             },
         }},
    });

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<FakeAzureDevOpsApiTransport>(document));

    qint64 latestObservedUpdatedAtMs = 0;
    const auto event = poller.pollLatestPullRequest(0, &latestObservedUpdatedAtMs, nullptr);
    QVERIFY(event.has_value());
    QCOMPARE(event->kind, AzureDevOpsNotificationKind::PullRequestUpdated);
    QCOMPARE(event->resourceId, QStringLiteral("pull-request:desktop:91"));
    QCOMPARE(event->title, QStringLiteral("PR update"));
    QVERIFY(latestObservedUpdatedAtMs > 0);
}

void TestAzureDevOpsBuildNotificationPoller::emitsReviewRequestedNotificationForPendingReviewer()
{
    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("leyochat");
    settings.project = QStringLiteral("LeyoChat");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;

    const QJsonDocument document(QJsonObject{
        {QStringLiteral("value"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("pullRequestId"), 92},
                 {QStringLiteral("title"), QStringLiteral("PR review needed")},
                 {QStringLiteral("status"), QStringLiteral("active")},
                 {QStringLiteral("creationDate"), QStringLiteral("2026-04-11T13:34:56Z")},
                 {QStringLiteral("createdBy"), QJsonObject{{QStringLiteral("displayName"), QStringLiteral("Bob")}}},
                 {QStringLiteral("repository"), QJsonObject{{QStringLiteral("name"), QStringLiteral("desktop")}}},
                 {QStringLiteral("reviewers"),
                  QJsonArray{
                      QJsonObject{
                          {QStringLiteral("displayName"), QStringLiteral("Reviewer One")},
                          {QStringLiteral("isRequired"), true},
                          {QStringLiteral("vote"), 0},
                      },
                  }},
             },
         }},
    });

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<FakeAzureDevOpsApiTransport>(document));

    qint64 latestObservedUpdatedAtMs = 0;
    const auto event = poller.pollLatestPullRequest(0, &latestObservedUpdatedAtMs, nullptr);
    QVERIFY(event.has_value());
    QCOMPARE(event->kind, AzureDevOpsNotificationKind::PullRequestReviewRequested);
    QCOMPARE(event->resourceId, QStringLiteral("pull-request:desktop:92"));
    QVERIFY(event->summary.contains(QStringLiteral("review"), Qt::CaseInsensitive)
            || event->summary.contains(QStringLiteral("审阅")));
    QVERIFY(latestObservedUpdatedAtMs > 0);
}

void TestAzureDevOpsBuildNotificationPoller::emitsNotificationForAssignedWorkItem()
{
    class AssignedWorkItemTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl& url,
                                             const AzureDevOpsConnectionSettings&,
                                             QString*) const override
        {
            const QString path = url.toString();
            if (path.contains(QStringLiteral("/_apis/wit/workitems/123/updates"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("revisedDate"), QStringLiteral("2026-04-11T13:00:00Z")},
                             {QStringLiteral("revisedBy"), QJsonObject{{QStringLiteral("displayName"), QStringLiteral("Dana")}}},
                         },
                     }},
                });
            }
            if (path.contains(QStringLiteral("/_apis/wit/workitems/123"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("id"), 123},
                    {QStringLiteral("fields"),
                     QJsonObject{
                         {QStringLiteral("System.Title"), QStringLiteral("Assigned work item")},
                         {QStringLiteral("System.WorkItemType"), QStringLiteral("Task")},
                         {QStringLiteral("System.State"), QStringLiteral("Active")},
                         {QStringLiteral("System.ChangedDate"), QStringLiteral("2026-04-11T13:00:00Z")},
                         {QStringLiteral("System.AssignedTo"),
                          QJsonObject{{QStringLiteral("displayName"), QStringLiteral("Owner")}}},
                     }},
                    {QStringLiteral("_links"),
                     QJsonObject{{QStringLiteral("html"),
                                  QJsonObject{{QStringLiteral("href"),
                                               QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_workitems/edit/123")}}}}},
                });
            }
            return std::nullopt;
        }

        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString*) const override
        {
            return QJsonDocument(QJsonObject{
                {QStringLiteral("workItems"),
                 QJsonArray{
                     QJsonObject{{QStringLiteral("id"), 123}},
                 }},
            });
        }
    };

    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("leyochat");
    settings.project = QStringLiteral("LeyoChat");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<AssignedWorkItemTransport>());

    qint64 latestObservedUpdatedAtMs = 0;
    const auto event = poller.pollLatestAssignedWorkItem(0, &latestObservedUpdatedAtMs, nullptr);
    QVERIFY(event.has_value());
    QCOMPARE(event->kind, AzureDevOpsNotificationKind::WorkItemCommented);
    QCOMPARE(event->resourceId, QStringLiteral("work-item:123"));
    QCOMPARE(event->title, QStringLiteral("Assigned work item"));
    QCOMPARE(event->summary, QStringLiteral("工作项有新的评论"));
    QVERIFY(latestObservedUpdatedAtMs > 0);
}

void TestAzureDevOpsBuildNotificationPoller::emitsNotificationForWorkItemStateChange()
{
    class WorkItemUpdateTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl& url,
                                             const AzureDevOpsConnectionSettings&,
                                             QString*) const override
        {
            const QString path = url.toString();
            if (path.contains(QStringLiteral("/_apis/wit/workitems/123/updates"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("revisedDate"), QStringLiteral("2026-04-11T13:10:00Z")},
                             {QStringLiteral("revisedBy"), QJsonObject{{QStringLiteral("displayName"), QStringLiteral("Carol")}}},
                             {QStringLiteral("fields"),
                              QJsonObject{
                                  {QStringLiteral("System.State"),
                                   QJsonObject{
                                       {QStringLiteral("oldValue"), QStringLiteral("New")},
                                       {QStringLiteral("newValue"), QStringLiteral("Active")},
                                   }},
                              }},
                         },
                     }},
                });
            }
            if (path.contains(QStringLiteral("/_apis/wit/workitems/123"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("id"), 123},
                    {QStringLiteral("fields"),
                     QJsonObject{
                         {QStringLiteral("System.Title"), QStringLiteral("Investigate outage")},
                         {QStringLiteral("System.WorkItemType"), QStringLiteral("Task")},
                         {QStringLiteral("System.State"), QStringLiteral("Active")},
                         {QStringLiteral("System.ChangedDate"), QStringLiteral("2026-04-11T13:10:00Z")},
                         {QStringLiteral("System.AssignedTo"),
                          QJsonObject{{QStringLiteral("displayName"), QStringLiteral("Owner")}}},
                     }},
                });
            }
            return std::nullopt;
        }

        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString*) const override
        {
            return QJsonDocument(QJsonObject{
                {QStringLiteral("workItems"),
                 QJsonArray{
                     QJsonObject{{QStringLiteral("id"), 123}},
                 }},
            });
        }
    };

    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("leyochat");
    settings.project = QStringLiteral("LeyoChat");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<WorkItemUpdateTransport>());

    qint64 latestObservedUpdatedAtMs = 0;
    const auto event = poller.pollLatestAssignedWorkItem(0, &latestObservedUpdatedAtMs, nullptr);
    QVERIFY(event.has_value());
    QCOMPARE(event->kind, AzureDevOpsNotificationKind::WorkItemUpdated);
    QCOMPARE(event->resourceId, QStringLiteral("work-item:123"));
    QVERIFY(event->summary.contains(QStringLiteral("state"), Qt::CaseInsensitive)
            || event->summary.contains(QStringLiteral("状态")));
    QVERIFY(latestObservedUpdatedAtMs > 0);
}

void TestAzureDevOpsBuildNotificationPoller::emitsNotificationForWorkItemMention()
{
    class WorkItemMentionTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl& url,
                                             const AzureDevOpsConnectionSettings&,
                                             QString*) const override
        {
            const QString path = url.toString();
            if (path.contains(QStringLiteral("/_apis/wit/workitems/123/updates"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("revisedDate"), QStringLiteral("2026-04-11T13:20:00Z")},
                             {QStringLiteral("revisedBy"), QJsonObject{{QStringLiteral("displayName"), QStringLiteral("Carol")}}},
                             {QStringLiteral("fields"),
                              QJsonObject{
                                  {QStringLiteral("System.History"),
                                   QJsonObject{{QStringLiteral("newValue"), QStringLiteral("@me please take a look")}}},
                              }},
                         },
                     }},
                });
            }
            if (path.contains(QStringLiteral("/_apis/wit/workitems/123"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("id"), 123},
                    {QStringLiteral("fields"),
                     QJsonObject{
                         {QStringLiteral("System.Title"), QStringLiteral("Investigate outage")},
                         {QStringLiteral("System.WorkItemType"), QStringLiteral("Task")},
                         {QStringLiteral("System.State"), QStringLiteral("Active")},
                         {QStringLiteral("System.ChangedDate"), QStringLiteral("2026-04-11T13:20:00Z")},
                     }},
                });
            }
            return std::nullopt;
        }

        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString*) const override
        {
            return QJsonDocument(QJsonObject{
                {QStringLiteral("workItems"),
                 QJsonArray{
                     QJsonObject{{QStringLiteral("id"), 123}},
                 }},
            });
        }
    };

    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("leyochat");
    settings.project = QStringLiteral("LeyoChat");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<WorkItemMentionTransport>());

    qint64 latestObservedUpdatedAtMs = 0;
    const auto event = poller.pollLatestAssignedWorkItem(0, &latestObservedUpdatedAtMs, nullptr);
    QVERIFY(event.has_value());
    QCOMPARE(event->kind, AzureDevOpsNotificationKind::WorkItemMentioned);
    QVERIFY(event->summary.contains(QStringLiteral("提及")));
    QVERIFY(latestObservedUpdatedAtMs > 0);
}

void TestAzureDevOpsBuildNotificationPoller::emitsNotificationForWorkItemComment()
{
    class WorkItemCommentTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl& url,
                                             const AzureDevOpsConnectionSettings&,
                                             QString*) const override
        {
            const QString path = url.toString();
            if (path.contains(QStringLiteral("/_apis/wit/workitems/123/updates"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("revisedDate"), QStringLiteral("2026-04-11T13:30:00Z")},
                             {QStringLiteral("revisedBy"), QJsonObject{{QStringLiteral("displayName"), QStringLiteral("Dana")}}},
                             {QStringLiteral("fields"),
                              QJsonObject{
                                  {QStringLiteral("System.History"),
                                   QJsonObject{{QStringLiteral("newValue"), QStringLiteral("Added more details")}}},
                              }},
                         },
                     }},
                });
            }
            if (path.contains(QStringLiteral("/_apis/wit/workitems/123"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("id"), 123},
                    {QStringLiteral("fields"),
                     QJsonObject{
                         {QStringLiteral("System.Title"), QStringLiteral("Investigate outage")},
                         {QStringLiteral("System.WorkItemType"), QStringLiteral("Task")},
                         {QStringLiteral("System.State"), QStringLiteral("Active")},
                         {QStringLiteral("System.ChangedDate"), QStringLiteral("2026-04-11T13:30:00Z")},
                     }},
                });
            }
            return std::nullopt;
        }

        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString*) const override
        {
            return QJsonDocument(QJsonObject{
                {QStringLiteral("workItems"),
                 QJsonArray{
                     QJsonObject{{QStringLiteral("id"), 123}},
                 }},
            });
        }
    };

    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("leyochat");
    settings.project = QStringLiteral("LeyoChat");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<WorkItemCommentTransport>());

    qint64 latestObservedUpdatedAtMs = 0;
    const auto event = poller.pollLatestAssignedWorkItem(0, &latestObservedUpdatedAtMs, nullptr);
    QVERIFY(event.has_value());
    QCOMPARE(event->kind, AzureDevOpsNotificationKind::WorkItemCommented);
    QVERIFY(event->summary.contains(QStringLiteral("评论")));
    QVERIFY(latestObservedUpdatedAtMs > 0);
}

void TestAzureDevOpsBuildNotificationPoller::emitsBuildRecoveredWhenSuccessfulBuildFollowsFailure()
{
    class BuildTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl&,
                                             const AzureDevOpsConnectionSettings&,
                                             QString*) const override
        {
            return QJsonDocument(QJsonObject{
                {QStringLiteral("value"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("id"), 201},
                         {QStringLiteral("buildNumber"), QStringLiteral("2026.04.11.21")},
                         {QStringLiteral("result"), QStringLiteral("succeeded")},
                         {QStringLiteral("definition"),
                          QJsonObject{{QStringLiteral("name"), QStringLiteral("Nightly")}}},
                     },
                 }},
            });
        }

        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString*) const override
        {
            return std::nullopt;
        }
    };

    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("org-a");
    settings.project = QStringLiteral("Project-1");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;
    settings.notificationTargets.push_back(
        {QStringLiteral("org-a"), QStringLiteral("Project-1"), true, 180, 0, 0, QStringLiteral("failed")});

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<BuildTransport>());

    QString errorMessage;
    const auto events = poller.pollTrackedNotifications(&settings, &errorMessage);
    QCOMPARE(errorMessage, QString());
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.at(0).kind, AzureDevOpsNotificationKind::BuildRecovered);
    QCOMPARE(settings.notificationTargets.at(0).lastNotifiedBuildId, 201);
    QCOMPARE(settings.notificationTargets.at(0).lastNotifiedBuildResult, QStringLiteral("succeeded"));
}

void TestAzureDevOpsBuildNotificationPoller::pollsTrackedNotificationsAcrossBuildPullRequestAndWorkItem()
{
    class RoutedTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl& url,
                                             const AzureDevOpsConnectionSettings&,
                                             QString*) const override
        {
            const QString path = url.toString();
            if (path.contains(QStringLiteral("/_apis/build/builds?"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("id"), 150},
                             {QStringLiteral("buildNumber"), QStringLiteral("2026.04.11.1")},
                             {QStringLiteral("result"), QStringLiteral("succeeded")},
                             {QStringLiteral("definition"),
                              QJsonObject{{QStringLiteral("name"), QStringLiteral("Nightly")}}},
                         },
                     }},
                });
            }
            if (path.contains(QStringLiteral("/_apis/git/pullrequests?"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("pullRequestId"), 91},
                             {QStringLiteral("title"), QStringLiteral("PR update")},
                             {QStringLiteral("status"), QStringLiteral("active")},
                             {QStringLiteral("creationDate"), QStringLiteral("2026-04-11T12:34:56Z")},
                             {QStringLiteral("createdBy"), QJsonObject{{QStringLiteral("displayName"), QStringLiteral("Alice")}}},
                             {QStringLiteral("repository"), QJsonObject{{QStringLiteral("name"), QStringLiteral("desktop")}}},
                             {QStringLiteral("reviewers"),
                              QJsonArray{
                                  QJsonObject{
                                      {QStringLiteral("displayName"), QStringLiteral("Bob")},
                                      {QStringLiteral("isRequired"), true},
                                      {QStringLiteral("vote"), 0},
                                  },
                              }},
                         },
                     }},
                });
            }
            if (path.contains(QStringLiteral("/_apis/wit/workitems/123/updates"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("revisedDate"), QStringLiteral("2026-04-11T13:00:00Z")},
                             {QStringLiteral("revisedBy"), QJsonObject{{QStringLiteral("displayName"), QStringLiteral("Reviewer")}}},
                         },
                     }},
                });
            }
            if (path.contains(QStringLiteral("/_apis/wit/workitems/123"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("id"), 123},
                    {QStringLiteral("fields"),
                     QJsonObject{
                         {QStringLiteral("System.Title"), QStringLiteral("Assigned work item")},
                         {QStringLiteral("System.WorkItemType"), QStringLiteral("Task")},
                         {QStringLiteral("System.State"), QStringLiteral("Active")},
                         {QStringLiteral("System.ChangedDate"), QStringLiteral("2026-04-11T13:00:00Z")},
                         {QStringLiteral("System.AssignedTo"),
                          QJsonObject{{QStringLiteral("displayName"), QStringLiteral("Owner")}}},
                     }},
                });
            }
            return std::nullopt;
        }

        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString*) const override
        {
            return QJsonDocument(QJsonObject{
                {QStringLiteral("workItems"),
                 QJsonArray{
                     QJsonObject{{QStringLiteral("id"), 123}},
                 }},
            });
        }
    };

    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("org-a");
    settings.project = QStringLiteral("Project-1");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;
    settings.notificationTargets.push_back(
        {QStringLiteral("org-a"), QStringLiteral("Project-1"), true, 100, 0, 0});

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<RoutedTransport>());

    QString errorMessage;
    const auto events = poller.pollTrackedNotifications(&settings, &errorMessage);
    QCOMPARE(errorMessage, QString());
    QCOMPARE(events.size(), 3);
    QCOMPARE(events.at(1).kind, AzureDevOpsNotificationKind::PullRequestReviewRequested);
    QCOMPARE(events.at(1).summary, QStringLiteral("等待 Bob 审阅"));
    QCOMPARE(events.at(2).kind, AzureDevOpsNotificationKind::WorkItemCommented);
    QCOMPARE(events.at(2).summary, QStringLiteral("工作项有新的评论"));
    QCOMPARE(settings.notificationTargets.at(0).lastNotifiedBuildId, 150);
    QVERIFY(settings.notificationTargets.at(0).lastNotifiedPullRequestUpdatedAtMs > 0);
    QVERIFY(settings.notificationTargets.at(0).lastNotifiedAssignedWorkItemUpdatedAtMs > 0);
}

void TestAzureDevOpsBuildNotificationPoller::pollTrackedNotifications_recordsFailureStatePerTarget()
{
    class PartiallyFailingTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl& url,
                                             const AzureDevOpsConnectionSettings&,
                                             QString* errorMessage) const override
        {
            const QString path = url.toString();
            if (path.contains(QStringLiteral("/org-a/Project-1/"))
                && path.contains(QStringLiteral("/_apis/build/builds?"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("id"), 151},
                             {QStringLiteral("buildNumber"), QStringLiteral("2026.04.11.2")},
                             {QStringLiteral("result"), QStringLiteral("succeeded")},
                         },
                     }},
                });
            }
            if (errorMessage) {
                *errorMessage = QStringLiteral("network down");
            }
            return std::nullopt;
        }

        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString* errorMessage) const override
        {
            if (errorMessage) {
                *errorMessage = QStringLiteral("network down");
            }
            return std::nullopt;
        }
    };

    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("org-a");
    settings.project = QStringLiteral("Project-1");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;
    settings.notificationTargets.push_back(
        {QStringLiteral("org-a"), QStringLiteral("Project-1"), true, 100});
    settings.notificationTargets.push_back(
        {QStringLiteral("org-b"), QStringLiteral("Project-2"), true, 300});

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<PartiallyFailingTransport>());

    QString errorMessage;
    const auto events = poller.pollTrackedNotifications(&settings, &errorMessage);
    QCOMPARE(events.size(), 1);
    QVERIFY(errorMessage.contains(QStringLiteral("org-b/Project-2")));
    QCOMPARE(settings.notificationTargets.at(0).consecutivePollFailures, 1);
    QVERIFY(settings.notificationTargets.at(0).lastPollErrorMessage.contains(QStringLiteral("network down")));
    QCOMPARE(settings.notificationTargets.at(0).lastPollErrorCategory, QStringLiteral("network"));
    QCOMPARE(settings.notificationTargets.at(1).consecutivePollFailures, 1);
    QVERIFY(settings.notificationTargets.at(1).lastPollErrorMessage.contains(QStringLiteral("network down")));
    QCOMPARE(settings.notificationTargets.at(1).lastPollErrorCategory, QStringLiteral("network"));
}

void TestAzureDevOpsBuildNotificationPoller::pollTrackedNotifications_promotesAuthFailureIntoSettingsSummary()
{
    class AuthFailingTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl&,
                                             const AzureDevOpsConnectionSettings&,
                                             QString* errorMessage) const override
        {
            if (errorMessage) {
                *errorMessage = QStringLiteral("401 unauthorized: PAT expired");
            }
            return std::nullopt;
        }

        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString* errorMessage) const override
        {
            if (errorMessage) {
                *errorMessage = QStringLiteral("401 unauthorized: PAT expired");
            }
            return std::nullopt;
        }
    };

    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("org-a");
    settings.project = QStringLiteral("Project-1");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;
    settings.notificationTargets.push_back(
        {QStringLiteral("org-a"), QStringLiteral("Project-1"), true, 100});
    settings.notificationTargets.push_back(
        {QStringLiteral("org-b"), QStringLiteral("Project-2"), true, 300});

    AzureDevOpsBuildNotificationPoller poller(
        settings,
        std::make_shared<AuthFailingTransport>());

    QString errorMessage;
    const auto events = poller.pollTrackedNotifications(&settings, &errorMessage);
    QVERIFY(events.isEmpty());
    QVERIFY(errorMessage.contains(QStringLiteral("PAT expired")));
    QCOMPARE(settings.lastPollErrorCategory, QStringLiteral("auth"));
    QVERIFY(settings.lastPollErrorMessage.contains(QStringLiteral("PAT expired")));
    QCOMPARE(settings.consecutivePollFailures, 1);
    QVERIFY(settings.lastPollAttemptAtMs > 0);
}

void TestAzureDevOpsBuildNotificationPoller::pollTrackedNotifications_clearsSettingsFailureAfterRecovery()
{
    class RecoveringTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl& url,
                                             const AzureDevOpsConnectionSettings&,
                                             QString* errorMessage) const override
        {
            if (m_failFirstRound) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("network timeout");
                }
                return std::nullopt;
            }

            const QString path = url.toString();
            if (path.contains(QStringLiteral("/_apis/build/builds?"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"),
                     QJsonArray{
                         QJsonObject{
                             {QStringLiteral("id"), 188},
                             {QStringLiteral("buildNumber"), QStringLiteral("2026.04.11.99")},
                             {QStringLiteral("result"), QStringLiteral("succeeded")},
                             {QStringLiteral("definition"),
                              QJsonObject{{QStringLiteral("name"), QStringLiteral("Nightly")}}},
                         },
                     }},
                });
            }
            if (path.contains(QStringLiteral("/_apis/git/pullrequests?"))) {
                return QJsonDocument(QJsonObject{{QStringLiteral("value"), QJsonArray{}}});
            }
            if (path.contains(QStringLiteral("/_apis/wit/workitems/"))) {
                return QJsonDocument(QJsonObject{});
            }
            return std::nullopt;
        }

        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString* errorMessage) const override
        {
            if (m_failFirstRound) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("network timeout");
                }
                return std::nullopt;
            }
            return QJsonDocument(QJsonObject{{QStringLiteral("workItems"), QJsonArray{}}});
        }

        void setFailFirstRound(bool enabled)
        {
            m_failFirstRound = enabled;
        }

    private:
        bool m_failFirstRound = true;
    };

    auto transport = std::make_shared<RecoveringTransport>();

    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("org-a");
    settings.project = QStringLiteral("Project-1");
    settings.personalAccessToken = QStringLiteral("token");
    settings.notificationsEnabled = true;
    settings.notificationTargets.push_back(
        {QStringLiteral("org-a"), QStringLiteral("Project-1"), true, 100});

    AzureDevOpsBuildNotificationPoller poller(settings, transport);

    QString errorMessage;
    const auto failedEvents = poller.pollTrackedNotifications(&settings, &errorMessage);
    QVERIFY(failedEvents.isEmpty());
    QCOMPARE(settings.lastPollErrorCategory, QStringLiteral("network"));
    QVERIFY(settings.lastPollErrorMessage.contains(QStringLiteral("timeout")));
    QCOMPARE(settings.consecutivePollFailures, 1);

    transport->setFailFirstRound(false);
    errorMessage.clear();
    const auto recoveredEvents = poller.pollTrackedNotifications(&settings, &errorMessage);
    QCOMPARE(errorMessage, QString());
    QCOMPARE(recoveredEvents.size(), 1);
    QVERIFY(settings.lastPollSuccessAtMs > 0);
    QCOMPARE(settings.lastPollErrorCategory, QString());
    QCOMPARE(settings.lastPollErrorMessage, QString());
    QCOMPARE(settings.consecutivePollFailures, 0);
}

QTEST_MAIN(TestAzureDevOpsBuildNotificationPoller)
#include "TestAzureDevOpsBuildNotificationPoller.moc"
