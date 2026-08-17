#include <QtTest>

#include "integrations/LocalAzureDevOpsAdapter.h"

namespace {

class FakeAzureDevOpsApiTransport final : public IAzureDevOpsApiTransport {
public:
    QHash<QString, QJsonDocument> documentsByUrl;

    std::optional<QJsonDocument> getJson(const QUrl& url,
                                         const AzureDevOpsConnectionSettings&,
                                         QString* errorMessage) const override
    {
        const QString key = url.toString();
        const auto it = documentsByUrl.constFind(key);
        if (it == documentsByUrl.cend()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("missing fake document");
            }
            return std::nullopt;
        }
        return it.value();
    }

    std::optional<QJsonDocument> postJson(const QUrl& url,
                                          const AzureDevOpsConnectionSettings&,
                                          const QJsonDocument&,
                                          QString* errorMessage) const override
    {
        const QString key = QStringLiteral("POST %1").arg(url.toString());
        const auto it = documentsByUrl.constFind(key);
        if (it == documentsByUrl.cend()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("missing fake post document");
            }
            return std::nullopt;
        }
        return it.value();
    }
};

AzureDevOpsConnectionSettings configuredSettings()
{
    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://dev.azure.com");
    settings.organization = QStringLiteral("leyochat");
    settings.project = QStringLiteral("LeyoChat");
    settings.personalAccessToken = QStringLiteral("pat-token-123");
    return settings;
}

AzureDevOpsConnectionSettings onPremSettings()
{
    AzureDevOpsConnectionSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("https://devops.example.com");
    settings.personalAccessToken = QStringLiteral("pat-token-123");
    return settings;
}

}  // namespace

class TestLocalAzureDevOpsAdapter : public QObject {
    Q_OBJECT

private slots:
    void workItemLink_resolvesStructuredPayload();
    void pullRequestLink_resolvesStructuredPayload();
    void buildLink_resolvesStructuredPayload();
    void buildLink_resolvesWithoutSelectedProjectContext();
    void manualLocator_resolvesStructuredPayload();
    void testConnection_returnsSuccessForReachableProject();
    void testConnection_acceptsCredentialOnlyConfiguration();
    void testConnection_surfacesTransportFailure();
    void testConnection_onPrem_usesConnectionDataEndpoint();
    void discoverOrganizations_returnsAvailableAccounts();
    void discoverOrganizations_onPrem_returnsProjectCollections();
    void discoverProjects_returnsProjectsForOrganization();
    void discoverCurrentUser_onPrem_populatesFromConnectionData();
};

void TestLocalAzureDevOpsAdapter::workItemLink_resolvesStructuredPayload()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    transport->documentsByUrl.insert(
        QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_apis/wit/workitems/1234?api-version=7.0"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("id"), 1234},
            {QStringLiteral("fields"), QJsonObject{
                {QStringLiteral("System.Title"), QStringLiteral("修复群文件状态卡�?)},
                {QStringLiteral("System.State"), QStringLiteral("进行�?)},
                {QStringLiteral("System.WorkItemType"), QStringLiteral("Bug")},
                {QStringLiteral("System.TeamProject"), QStringLiteral("LeyoChat")},
                {QStringLiteral("System.AssignedTo"), QJsonObject{
                    {QStringLiteral("displayName"), QStringLiteral("张小�?)},
                }},
            }},
            {QStringLiteral("_links"), QJsonObject{
                {QStringLiteral("html"), QJsonObject{
                    {QStringLiteral("href"), QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_workitems/edit/1234")},
                }},
            }},
        }));

    LocalAzureDevOpsAdapter adapter(configuredSettings(), transport);
    QString errorMessage;
    const auto payload = adapter.payloadForLink(
        QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_workitems/edit/1234"),
        &errorMessage);

    QVERIFY2(payload.has_value(), qPrintable(errorMessage));
    QCOMPARE(payload->kind, QStringLiteral("devops_work_item"));
    QCOMPARE(payload->resourceId, QStringLiteral("work-item:1234"));
    QCOMPARE(payload->title, QStringLiteral("修复群文件状态卡�?));
    QCOMPARE(payload->status, QStringLiteral("进行�?));
    QCOMPARE(payload->actions.size(), 1);
    QCOMPARE(payload->actions.front().target,
             QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_workitems/edit/1234"));
    QCOMPARE(adapter.visibleResourcesForWorkspace(QStringLiteral("azure-devops:leyochat:LeyoChat")).size(), 1);
}

void TestLocalAzureDevOpsAdapter::pullRequestLink_resolvesStructuredPayload()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    transport->documentsByUrl.insert(
        QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_apis/git/repositories/desktop/pullRequests/88?api-version=7.0"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("pullRequestId"), 88},
            {QStringLiteral("title"), QStringLiteral("接入 stage2 resource_ref 卡片")},
            {QStringLiteral("status"), QStringLiteral("active")},
            {QStringLiteral("createdBy"), QJsonObject{
                {QStringLiteral("displayName"), QStringLiteral("侯晓�?)},
            }},
            {QStringLiteral("repository"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("desktop")},
                {QStringLiteral("project"), QJsonObject{
                    {QStringLiteral("name"), QStringLiteral("LeyoChat")},
                }},
            }},
            {QStringLiteral("_links"), QJsonObject{
                {QStringLiteral("web"), QJsonObject{
                    {QStringLiteral("href"), QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_git/desktop/pullrequest/88")},
                }},
            }},
        }));

    LocalAzureDevOpsAdapter adapter(configuredSettings(), transport);
    QString errorMessage;
    const auto payload = adapter.payloadForLink(
        QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_git/desktop/pullrequest/88"),
        &errorMessage);

    QVERIFY2(payload.has_value(), qPrintable(errorMessage));
    QCOMPARE(payload->kind, QStringLiteral("devops_pull_request"));
    QCOMPARE(payload->resourceId, QStringLiteral("pull-request:desktop:88"));
    QCOMPARE(payload->title, QStringLiteral("接入 stage2 resource_ref 卡片"));
    QVERIFY(payload->subtitle.contains(QStringLiteral("desktop")));
}

void TestLocalAzureDevOpsAdapter::buildLink_resolvesStructuredPayload()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    transport->documentsByUrl.insert(
        QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_apis/build/builds/31?api-version=7.0"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("id"), 31},
            {QStringLiteral("result"), QStringLiteral("succeeded")},
            {QStringLiteral("sourceBranch"), QStringLiteral("refs/heads/main")},
            {QStringLiteral("definition"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Beta Release")},
            }},
            {QStringLiteral("project"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("LeyoChat")},
            }},
            {QStringLiteral("requestedFor"), QJsonObject{
                {QStringLiteral("displayName"), QStringLiteral("乔志�?)},
            }},
            {QStringLiteral("_links"), QJsonObject{
                {QStringLiteral("web"), QJsonObject{
                    {QStringLiteral("href"), QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_build/results?buildId=31&view=results")},
                }},
            }},
        }));

    LocalAzureDevOpsAdapter adapter(configuredSettings(), transport);
    QString errorMessage;
    const auto payload = adapter.payloadForLink(
        QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_build/results?buildId=31&view=results"),
        &errorMessage);

    QVERIFY2(payload.has_value(), qPrintable(errorMessage));
    QCOMPARE(payload->kind, QStringLiteral("devops_build"));
    QCOMPARE(payload->resourceId, QStringLiteral("build:31"));
    QCOMPARE(payload->title, QStringLiteral("Beta Release"));
    QCOMPARE(payload->status, QStringLiteral("succeeded"));
    QVERIFY(adapter.payloadForResource(QStringLiteral("build:31")).has_value());
}

void TestLocalAzureDevOpsAdapter::buildLink_resolvesWithoutSelectedProjectContext()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    transport->documentsByUrl.insert(
        QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_apis/build/builds/41?api-version=7.0"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("id"), 41},
            {QStringLiteral("result"), QStringLiteral("failed")},
            {QStringLiteral("sourceBranch"), QStringLiteral("refs/heads/release")},
            {QStringLiteral("definition"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Nightly")},
            }},
            {QStringLiteral("project"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("LeyoChat")},
            }},
            {QStringLiteral("requestedFor"), QJsonObject{
                {QStringLiteral("displayName"), QStringLiteral("Ops Bot")},
            }},
            {QStringLiteral("_links"), QJsonObject{
                {QStringLiteral("web"), QJsonObject{
                    {QStringLiteral("href"), QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_build/results?buildId=41")},
                }},
            }},
        }));

    AzureDevOpsConnectionSettings settings = configuredSettings();
    settings.organization.clear();
    settings.project.clear();

    LocalAzureDevOpsAdapter adapter(settings, transport);
    QString errorMessage;
    const auto payload = adapter.payloadForLink(
        QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_build/results?buildId=41"),
        &errorMessage);

    QVERIFY2(payload.has_value(), qPrintable(errorMessage));
    QCOMPARE(payload->resourceId, QStringLiteral("build:41"));
    QCOMPARE(payload->status, QStringLiteral("failed"));
}

void TestLocalAzureDevOpsAdapter::manualLocator_resolvesStructuredPayload()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    transport->documentsByUrl.insert(
        QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_apis/git/repositories/desktop/pullRequests/109?api-version=7.0"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("pullRequestId"), 109},
            {QStringLiteral("title"), QStringLiteral("�?Azure DevOps 插卡增加手动模式")},
            {QStringLiteral("status"), QStringLiteral("active")},
            {QStringLiteral("createdBy"), QJsonObject{
                {QStringLiteral("displayName"), QStringLiteral("侯晓�?)},
            }},
            {QStringLiteral("repository"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("desktop")},
                {QStringLiteral("project"), QJsonObject{
                    {QStringLiteral("name"), QStringLiteral("LeyoChat")},
                }},
            }},
            {QStringLiteral("_links"), QJsonObject{
                {QStringLiteral("web"), QJsonObject{
                    {QStringLiteral("href"), QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_git/desktop/pullrequest/109")},
                }},
            }},
        }));

    AzureDevOpsResourceLocator locator;
    locator.kind = AzureDevOpsResourceKind::PullRequest;
    locator.organization = QStringLiteral("leyochat");
    locator.project = QStringLiteral("LeyoChat");
    locator.repository = QStringLiteral("desktop");
    locator.resourceId = QStringLiteral("109");
    locator.webUrl = QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_git/desktop/pullrequest/109");

    LocalAzureDevOpsAdapter adapter(configuredSettings(), transport);
    QString errorMessage;
    const auto payload = adapter.payloadForLocator(locator, &errorMessage);

    QVERIFY2(payload.has_value(), qPrintable(errorMessage));
    QCOMPARE(payload->kind, QStringLiteral("devops_pull_request"));
    QCOMPARE(payload->resourceId, QStringLiteral("pull-request:desktop:109"));
    QCOMPARE(payload->title, QStringLiteral("�?Azure DevOps 插卡增加手动模式"));
    QVERIFY(adapter.payloadForResource(QStringLiteral("pull-request:desktop:109")).has_value());
}

void TestLocalAzureDevOpsAdapter::testConnection_returnsSuccessForReachableProject()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    transport->documentsByUrl.insert(
        QStringLiteral("https://dev.azure.com/leyochat/_apis/projects/LeyoChat?api-version=7.0"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("project-1")},
            {QStringLiteral("name"), QStringLiteral("LeyoChat")},
            {QStringLiteral("state"), QStringLiteral("wellFormed")},
        }));

    LocalAzureDevOpsAdapter adapter(configuredSettings(), transport);
    QString errorMessage;

    QVERIFY(adapter.testConnection(&errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
}

void TestLocalAzureDevOpsAdapter::testConnection_acceptsCredentialOnlyConfiguration()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    transport->documentsByUrl.insert(
        QStringLiteral("https://app.vssps.visualstudio.com/_apis/profile/profiles/me?api-version=7.0"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("profile-123")},
            {QStringLiteral("displayName"), QStringLiteral("Leyo User")},
        }));

    AzureDevOpsConnectionSettings settings = configuredSettings();
    settings.organization.clear();
    settings.project.clear();

    LocalAzureDevOpsAdapter adapter(settings, transport);
    QString errorMessage;

    QVERIFY(adapter.testConnection(&errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
}

void TestLocalAzureDevOpsAdapter::testConnection_surfacesTransportFailure()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    LocalAzureDevOpsAdapter adapter(configuredSettings(), transport);
    QString errorMessage;

    QVERIFY(!adapter.testConnection(&errorMessage));
    QVERIFY(!errorMessage.trimmed().isEmpty());
}

void TestLocalAzureDevOpsAdapter::discoverOrganizations_returnsAvailableAccounts()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    transport->documentsByUrl.insert(
        QStringLiteral("https://app.vssps.visualstudio.com/_apis/profile/profiles/me?api-version=7.0"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("profile-123")},
            {QStringLiteral("displayName"), QStringLiteral("Leyo User")},
        }));
    transport->documentsByUrl.insert(
        QStringLiteral("https://app.vssps.visualstudio.com/_apis/accounts?memberId=profile-123&api-version=7.0-preview.1"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("count"), 2},
            {QStringLiteral("value"), QJsonArray{
                QJsonObject{
                    {QStringLiteral("accountId"), QStringLiteral("acc-1")},
                    {QStringLiteral("accountName"), QStringLiteral("leyochat")},
                    {QStringLiteral("accountUri"), QStringLiteral("https://dev.azure.com/leyochat")},
                },
                QJsonObject{
                    {QStringLiteral("accountId"), QStringLiteral("acc-2")},
                    {QStringLiteral("accountName"), QStringLiteral("platform")},
                    {QStringLiteral("accountUri"), QStringLiteral("https://dev.azure.com/platform")},
                },
            }},
        }));

    AzureDevOpsConnectionSettings settings = configuredSettings();
    settings.organization.clear();
    settings.project.clear();

    LocalAzureDevOpsAdapter adapter(settings, transport);
    QString errorMessage;
    const auto organizations = adapter.discoverOrganizations(&errorMessage);

    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(organizations.size(), 2);
    QCOMPARE(organizations.at(0).organizationName, QStringLiteral("leyochat"));
    QCOMPARE(organizations.at(1).organizationName, QStringLiteral("platform"));
}

void TestLocalAzureDevOpsAdapter::discoverProjects_returnsProjectsForOrganization()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    transport->documentsByUrl.insert(
        QStringLiteral("https://dev.azure.com/leyochat/_apis/projects?api-version=7.0"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("count"), 2},
            {QStringLiteral("value"), QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("project-1")},
                    {QStringLiteral("name"), QStringLiteral("LeyoChat")},
                    {QStringLiteral("state"), QStringLiteral("wellFormed")},
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("project-2")},
                    {QStringLiteral("name"), QStringLiteral("InfraOps")},
                    {QStringLiteral("state"), QStringLiteral("wellFormed")},
                },
            }},
        }));

    LocalAzureDevOpsAdapter adapter(configuredSettings(), transport);
    QString errorMessage;
    const auto projects = adapter.discoverProjects(QStringLiteral("leyochat"), &errorMessage);

    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(projects.size(), 2);
    QCOMPARE(projects.at(0).projectName, QStringLiteral("LeyoChat"));
    QCOMPARE(projects.at(1).projectName, QStringLiteral("InfraOps"));
}

void TestLocalAzureDevOpsAdapter::testConnection_onPrem_usesConnectionDataEndpoint()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    transport->documentsByUrl.insert(
        QStringLiteral("https://devops.example.com/_apis/connectionData?api-version=7.0"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("locationServiceData"), QJsonObject{}},
            {QStringLiteral("authenticatedUser"), QJsonObject{
                {QStringLiteral("id"), QStringLiteral("user-onprem-1")},
                {QStringLiteral("providerDisplayName"), QStringLiteral("霍利用户")},
            }},
        }));

    AzureDevOpsConnectionSettings settings = onPremSettings();
    // No org/project �?should use connectionData endpoint
    LocalAzureDevOpsAdapter adapter(settings, transport);
    QString errorMessage;

    QVERIFY(adapter.testConnection(&errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
}

void TestLocalAzureDevOpsAdapter::discoverOrganizations_onPrem_returnsProjectCollections()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    transport->documentsByUrl.insert(
        QStringLiteral("https://devops.example.com/_apis/projectcollections?api-version=7.0"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("count"), 2},
            {QStringLiteral("value"), QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("col-1")},
                    {QStringLiteral("name"), QStringLiteral("DefaultCollection")},
                    {QStringLiteral("url"), QStringLiteral("https://devops.example.com/DefaultCollection")},
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("col-2")},
                    {QStringLiteral("name"), QStringLiteral("DevTeam")},
                    {QStringLiteral("url"), QStringLiteral("https://devops.example.com/DevTeam")},
                },
            }},
        }));

    LocalAzureDevOpsAdapter adapter(onPremSettings(), transport);
    QString errorMessage;
    const auto organizations = adapter.discoverOrganizations(&errorMessage);

    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(organizations.size(), 2);
    QCOMPARE(organizations.at(0).organizationName, QStringLiteral("DefaultCollection"));
    QCOMPARE(organizations.at(1).organizationName, QStringLiteral("DevTeam"));
}

void TestLocalAzureDevOpsAdapter::discoverCurrentUser_onPrem_populatesFromConnectionData()
{
    auto transport = std::make_shared<FakeAzureDevOpsApiTransport>();
    transport->documentsByUrl.insert(
        QStringLiteral("https://devops.example.com/_apis/connectionData?api-version=7.0"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("authenticatedUser"), QJsonObject{
                {QStringLiteral("id"), QStringLiteral("user-onprem-42")},
                {QStringLiteral("providerDisplayName"), QStringLiteral("张工")},
                {QStringLiteral("uniqueName"), QStringLiteral("test.user@example.com")},
            }},
        }));

    LocalAzureDevOpsAdapter adapter(onPremSettings(), transport);
    AzureDevOpsConnectionSettings resolved = onPremSettings();
    QString errorMessage;

    QVERIFY(adapter.discoverCurrentUser(&resolved, &errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(resolved.currentUserId, QStringLiteral("user-onprem-42"));
    QCOMPARE(resolved.currentUserDisplayName, QStringLiteral("张工"));
    QCOMPARE(resolved.currentUserUniqueName, QStringLiteral("test.user@example.com"));
}

QTEST_MAIN(TestLocalAzureDevOpsAdapter)
#include "TestLocalAzureDevOpsAdapter.moc"
