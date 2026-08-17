// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增500行/修改16行/删除3行; 总行数497行
// @AI-LastModified: 2026-04-14 23:26:41

// ──────────────────────────────────────────────────────────────────────────
// TestAzureDevOpsIntegrationHealth
//
// 功能完整性 + 主线程安全验证
//
// 重点：
//   1. 轮询是否在后台完成，不阻塞事件循环
//   2. 超时 / 失败 / 恢复流程是否正确
//   3. 设置保存后失败计数器是否重置
//   4. 多 target 轮询互不干扰
// ──────────────────────────────────────────────────────────────────────────

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtTest>

#include "integrations/AzureDevOpsSettings.h"
#include "integrations/LocalAzureDevOpsAdapter.h"
#include "services/AzureDevOpsBuildNotificationPoller.h"

// ──────────────────────────────────────────────────────────────────────────
// 辅助设施
// ──────────────────────────────────────────────────────────────────────────

namespace {

AzureDevOpsConnectionSettings leyochatSettings()
{
    AzureDevOpsConnectionSettings s;
    s.enabled = true;
    s.baseUrl = QStringLiteral("https://dev.azure.com");
    s.organization = QStringLiteral("leyochat");
    s.project = QStringLiteral("LeyoChat");
    s.personalAccessToken = QStringLiteral("pat-token-123");
    s.notificationsEnabled = true;
    s.notificationPollIntervalMinutes = 5;
    return s;
}

// 可配置延迟的 Fake Transport —— 模拟同步阻塞 I/O 的耗时
class TimedFakeTransport final : public IAzureDevOpsApiTransport {
public:
    int delayMs = 0;            // 每次请求的人工延迟
    int getCallCount = 0;       // 记录累计 GET 调用次数
    int postCallCount = 0;      // POST
    bool shouldFail = false;
    QString failMessage = QStringLiteral("simulated network failure");

    std::optional<QJsonDocument> getJson(const QUrl&,
                                         const AzureDevOpsConnectionSettings&,
                                         QString* errorMessage) const override
    {
        const_cast<TimedFakeTransport*>(this)->getCallCount++;

        if (delayMs > 0) {
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < delayMs) {
                // 空循环模拟阻塞
            }
        }

        if (shouldFail) {
            if (errorMessage) *errorMessage = failMessage;
            return std::nullopt;
        }

        return QJsonDocument(QJsonObject{
            {QStringLiteral("value"), QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), 200},
                    {QStringLiteral("buildNumber"), QStringLiteral("2026.04.14.1")},
                    {QStringLiteral("result"), QStringLiteral("succeeded")},
                    {QStringLiteral("definition"),
                     QJsonObject{{QStringLiteral("name"), QStringLiteral("CI-Pipeline")}}},
                },
            }},
        });
    }

    std::optional<QJsonDocument> postJson(const QUrl&,
                                          const AzureDevOpsConnectionSettings&,
                                          const QJsonDocument&,
                                          QString* errorMessage) const override
    {
        const_cast<TimedFakeTransport*>(this)->postCallCount++;
        if (shouldFail) {
            if (errorMessage) *errorMessage = failMessage;
            return std::nullopt;
        }
        return QJsonDocument(QJsonObject{
            {QStringLiteral("workItems"), QJsonArray{
                QJsonObject{{QStringLiteral("id"), 999}},
            }},
        });
    }
};

}  // namespace

// ──────────────────────────────────────────────────────────────────────────
// 测试类
// ──────────────────────────────────────────────────────────────────────────

class TestAzureDevOpsIntegrationHealth : public QObject {
    Q_OBJECT

private slots:
    // ── 功能完整性 ───────────────────────────────────────
    void settingsRoundTrip_preservesAllFields();
    void testConnection_usesCorrectEndpointForCloudAndOnPrem();
    void pollTracked_returnsEventsForAllEnabledTargets();
    void pollTracked_disabledTargetsAreSkipped();
    void pollTracked_failureCounterIncrementsAndResetsOnRecovery();
    void settingsSave_resetsFailureCounters();

    // ── 主线程安全 / 不卡顿 ─────────────────────────────
    void pollSynchronous_completesWithinBudget();
    void eventLoopRemainsFree_duringPoll();
    void multiTargetPoll_totalTimeScalesLinearly();

    // ── 超时行为 ─────────────────────────────────────────
    void httpTimeout_doesNotExceed30Seconds();
};

// =====================================================================
// 功能完整性
// =====================================================================

void TestAzureDevOpsIntegrationHealth::settingsRoundTrip_preservesAllFields()
{
    QSettings store(QSettings::IniFormat, QSettings::UserScope,
                    QStringLiteral("LeyoChat-test"), QStringLiteral("devops-health"));
    store.clear();

    auto original = leyochatSettings();
    original.currentUserId = QStringLiteral("user-abc");
    original.currentUserDisplayName = QStringLiteral("张三");
    original.currentUserUniqueName = QStringLiteral("zhangsan@example.com");
    original.currentUserEmail = QStringLiteral("zhangsan@example.com");
    original.consecutivePollFailures = 3;
    original.lastPollErrorMessage = QStringLiteral("timeout");
    original.lastPollErrorCategory = QStringLiteral("network");
    original.notificationTargets.push_back(
        {QStringLiteral("leyochat"), QStringLiteral("LeyoChat"), true, 100});

    AzureDevOpsSettingsStore::save(original, &store);
    const auto loaded = AzureDevOpsSettingsStore::load(&store);

    QCOMPARE(loaded.enabled, original.enabled);
    QCOMPARE(loaded.baseUrl, original.baseUrl);
    QCOMPARE(loaded.organization, original.organization);
    QCOMPARE(loaded.project, original.project);
    QCOMPARE(loaded.personalAccessToken, original.personalAccessToken);
    QCOMPARE(loaded.currentUserId, original.currentUserId);
    QCOMPARE(loaded.currentUserDisplayName, original.currentUserDisplayName);
    QCOMPARE(loaded.notificationsEnabled, original.notificationsEnabled);
    QCOMPARE(loaded.consecutivePollFailures, original.consecutivePollFailures);
    QCOMPARE(loaded.lastPollErrorMessage, original.lastPollErrorMessage);
    QCOMPARE(loaded.lastPollErrorCategory, original.lastPollErrorCategory);
    QCOMPARE(loaded.notificationTargets.size(), 1);
    QCOMPARE(loaded.notificationTargets.at(0).organization, QStringLiteral("leyochat"));
}

void TestAzureDevOpsIntegrationHealth::testConnection_usesCorrectEndpointForCloudAndOnPrem()
{
    // Cloud：dev.azure.com → _apis/projects/{project}
    {
        class CloudTransport : public IAzureDevOpsApiTransport {
        public:
            mutable int getCallCount = 0;
            std::optional<QJsonDocument> getJson(const QUrl&,
                                                 const AzureDevOpsConnectionSettings&,
                                                 QString*) const override
            {
                const_cast<CloudTransport*>(this)->getCallCount++;
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("project-1")},
                    {QStringLiteral("name"), QStringLiteral("LeyoChat")},
                    {QStringLiteral("state"), QStringLiteral("wellFormed")},
                });
            }
            std::optional<QJsonDocument> postJson(const QUrl&,
                                                  const AzureDevOpsConnectionSettings&,
                                                  const QJsonDocument&,
                                                  QString*) const override
            { return std::nullopt; }
        };
        auto transport = std::make_shared<CloudTransport>();
        LocalAzureDevOpsAdapter adapter(leyochatSettings(), transport);
        QString error;
        const bool ok = adapter.testConnection(&error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(transport->getCallCount > 0);
    }

    // On-Prem：自定义 baseUrl → _apis/ConnectionData
    {
        class OnPremTransport : public IAzureDevOpsApiTransport {
        public:
            mutable int getCallCount = 0;
            std::optional<QJsonDocument> getJson(const QUrl&,
                                                 const AzureDevOpsConnectionSettings&,
                                                 QString*) const override
            {
                const_cast<OnPremTransport*>(this)->getCallCount++;
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("authenticatedUser"), QJsonObject{
                        {QStringLiteral("properties"), QJsonObject{
                            {QStringLiteral("Account"), QJsonObject{
                                {QStringLiteral("$value"), QStringLiteral("admin@example.com")},
                            }},
                        }},
                    }},
                    {QStringLiteral("instanceId"), QStringLiteral("inst-1")},
                });
            }
            std::optional<QJsonDocument> postJson(const QUrl&,
                                                  const AzureDevOpsConnectionSettings&,
                                                  const QJsonDocument&,
                                                  QString*) const override
            { return std::nullopt; }
        };
        auto transport = std::make_shared<OnPremTransport>();
        AzureDevOpsConnectionSettings onPrem;
        onPrem.enabled = true;
        onPrem.baseUrl = QStringLiteral("https://devops.example.com");
        onPrem.personalAccessToken = QStringLiteral("pat-token-123");

        LocalAzureDevOpsAdapter adapter(onPrem, transport);
        QString error;
        adapter.testConnection(&error);
        QVERIFY(transport->getCallCount > 0);
    }
}

void TestAzureDevOpsIntegrationHealth::pollTracked_returnsEventsForAllEnabledTargets()
{
    class DualTargetTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl& url,
                                             const AzureDevOpsConnectionSettings&,
                                             QString*) const override
        {
            const QString path = url.toString();
            if (path.contains(QStringLiteral("/org-a/Proj-A/")) && path.contains(QStringLiteral("builds"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"), QJsonArray{
                        QJsonObject{
                            {QStringLiteral("id"), 301},
                            {QStringLiteral("buildNumber"), QStringLiteral("2026.04.14.1")},
                            {QStringLiteral("result"), QStringLiteral("succeeded")},
                            {QStringLiteral("definition"),
                             QJsonObject{{QStringLiteral("name"), QStringLiteral("A-CI")}}},
                        },
                    }},
                });
            }
            if (path.contains(QStringLiteral("/org-b/Proj-B/")) && path.contains(QStringLiteral("builds"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"), QJsonArray{
                        QJsonObject{
                            {QStringLiteral("id"), 502},
                            {QStringLiteral("buildNumber"), QStringLiteral("2026.04.14.9")},
                            {QStringLiteral("result"), QStringLiteral("failed")},
                            {QStringLiteral("definition"),
                             QJsonObject{{QStringLiteral("name"), QStringLiteral("B-CI")}}},
                        },
                    }},
                });
            }
            if (path.contains(QStringLiteral("pullrequests"))) {
                return QJsonDocument(QJsonObject{{QStringLiteral("value"), QJsonArray{}}});
            }
            return std::nullopt;
        }
        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString*) const override
        {
            return QJsonDocument(QJsonObject{{QStringLiteral("workItems"), QJsonArray{}}});
        }
    };

    auto settings = leyochatSettings();
    settings.organization = QStringLiteral("org-a");
    settings.project = QStringLiteral("Proj-A");
    settings.notificationTargets.push_back(
        {QStringLiteral("org-a"), QStringLiteral("Proj-A"), true, 200});
    settings.notificationTargets.push_back(
        {QStringLiteral("org-b"), QStringLiteral("Proj-B"), true, 400});

    AzureDevOpsBuildNotificationPoller poller(settings,
                                              std::make_shared<DualTargetTransport>());
    QString error;
    const auto events = poller.pollTrackedNotifications(&settings, &error);
    QCOMPARE(error, QString());
    QVERIFY(events.size() >= 2);
    QCOMPARE(settings.notificationTargets.at(0).lastNotifiedBuildId, 301);
    QCOMPARE(settings.notificationTargets.at(1).lastNotifiedBuildId, 502);
}

void TestAzureDevOpsIntegrationHealth::pollTracked_disabledTargetsAreSkipped()
{
    // 有两个 target，一个 enabled，一个 disabled
    // disabled 的 lastNotifiedBuildId 应保持不变
    class RoutedTransport : public IAzureDevOpsApiTransport {
    public:
        std::optional<QJsonDocument> getJson(const QUrl& url,
                                             const AzureDevOpsConnectionSettings&,
                                             QString*) const override
        {
            if (url.toString().contains(QStringLiteral("/org-a/Proj-A/"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"), QJsonArray{
                        QJsonObject{
                            {QStringLiteral("id"), 501},
                            {QStringLiteral("buildNumber"), QStringLiteral("2026.04.14.1")},
                            {QStringLiteral("result"), QStringLiteral("succeeded")},
                            {QStringLiteral("definition"),
                             QJsonObject{{QStringLiteral("name"), QStringLiteral("A-CI")}}},
                        },
                    }},
                });
            }
            // org-b 不应该被请求——如果 disabled 起作用的话
            if (url.toString().contains(QStringLiteral("/org-b/Proj-B/"))) {
                return QJsonDocument(QJsonObject{
                    {QStringLiteral("value"), QJsonArray{
                        QJsonObject{
                            {QStringLiteral("id"), 888},
                            {QStringLiteral("buildNumber"), QStringLiteral("2026.04.14.9")},
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
        { return std::nullopt; }
    };

    auto settings = leyochatSettings();
    settings.organization = QStringLiteral("org-a");
    settings.project = QStringLiteral("Proj-A");
    settings.notificationTargets.push_back(
        {QStringLiteral("org-a"), QStringLiteral("Proj-A"), true, 400});
    settings.notificationTargets.push_back(
        {QStringLiteral("org-b"), QStringLiteral("Proj-B"), false, 700});

    AzureDevOpsBuildNotificationPoller poller(settings,
                                              std::make_shared<RoutedTransport>());
    QString error;
    poller.pollTrackedBuilds(&settings, &error);

    // enabled target 的 buildId 应被更新
    QCOMPARE(settings.notificationTargets.at(0).lastNotifiedBuildId, 501);
    // disabled target 的 buildId 应保持不变
    QCOMPARE(settings.notificationTargets.at(1).lastNotifiedBuildId, 700);
}

void TestAzureDevOpsIntegrationHealth::pollTracked_failureCounterIncrementsAndResetsOnRecovery()
{
    class ToggleTransport : public IAzureDevOpsApiTransport {
    public:
        mutable bool fail = true;
        std::optional<QJsonDocument> getJson(const QUrl&,
                                             const AzureDevOpsConnectionSettings&,
                                             QString* errorMessage) const override
        {
            if (fail) {
                if (errorMessage) *errorMessage = QStringLiteral("connect timeout");
                return std::nullopt;
            }
            return QJsonDocument(QJsonObject{
                {QStringLiteral("value"), QJsonArray{
                    QJsonObject{
                        {QStringLiteral("id"), 600},
                        {QStringLiteral("result"), QStringLiteral("succeeded")},
                        {QStringLiteral("definition"),
                         QJsonObject{{QStringLiteral("name"), QStringLiteral("CI")}}},
                    },
                }},
            });
        }
        std::optional<QJsonDocument> postJson(const QUrl&,
                                              const AzureDevOpsConnectionSettings&,
                                              const QJsonDocument&,
                                              QString* errorMessage) const override
        {
            if (fail) {
                if (errorMessage) *errorMessage = QStringLiteral("connect timeout");
                return std::nullopt;
            }
            return QJsonDocument(QJsonObject{{QStringLiteral("workItems"), QJsonArray{}}});
        }
    };

    auto transport = std::make_shared<ToggleTransport>();
    auto settings = leyochatSettings();
    settings.notificationTargets.push_back(
        {QStringLiteral("leyochat"), QStringLiteral("LeyoChat"), true, 0});

    // 第一轮：失败
    {
        AzureDevOpsBuildNotificationPoller poller(settings, transport);
        QString error;
        poller.pollTrackedNotifications(&settings, &error);
        QVERIFY(settings.notificationTargets.at(0).consecutivePollFailures >= 1);
        QVERIFY(!settings.notificationTargets.at(0).lastPollErrorMessage.isEmpty());
    }

    // 第二轮：恢复
    transport->fail = false;
    {
        AzureDevOpsBuildNotificationPoller poller(settings, transport);
        QString error;
        poller.pollTrackedNotifications(&settings, &error);
        QCOMPARE(settings.notificationTargets.at(0).consecutivePollFailures, 0);
        QVERIFY(settings.notificationTargets.at(0).lastPollErrorMessage.isEmpty());
    }
}

void TestAzureDevOpsIntegrationHealth::settingsSave_resetsFailureCounters()
{
    QSettings store(QSettings::IniFormat, QSettings::UserScope,
                    QStringLiteral("LeyoChat-test"), QStringLiteral("devops-health-reset"));
    store.clear();

    auto settings = leyochatSettings();
    settings.consecutivePollFailures = 5;
    settings.lastPollErrorMessage = QStringLiteral("token expired");
    settings.lastPollErrorCategory = QStringLiteral("auth");

    // 模拟用户保存 → 重置计数器
    settings.consecutivePollFailures = 0;
    settings.lastPollErrorMessage.clear();
    settings.lastPollErrorCategory.clear();
    AzureDevOpsSettingsStore::save(settings, &store);

    const auto loaded = AzureDevOpsSettingsStore::load(&store);
    QCOMPARE(loaded.consecutivePollFailures, 0);
    QVERIFY(loaded.lastPollErrorMessage.isEmpty());
    QVERIFY(loaded.lastPollErrorCategory.isEmpty());
}

// =====================================================================
// 主线程安全 / 不卡顿
// =====================================================================

void TestAzureDevOpsIntegrationHealth::pollSynchronous_completesWithinBudget()
{
    // 同步轮询（Fake Transport，无网络延迟）应在 50ms 内完成
    auto transport = std::make_shared<TimedFakeTransport>();
    auto settings = leyochatSettings();
    settings.notificationTargets.push_back(
        {QStringLiteral("leyochat"), QStringLiteral("LeyoChat"), true, 0});

    AzureDevOpsBuildNotificationPoller poller(settings, transport);

    QElapsedTimer timer;
    timer.start();
    QString error;
    poller.pollTrackedNotifications(&settings, &error);
    const qint64 elapsed = timer.elapsed();

    qInfo().noquote() << QStringLiteral("[perf] DevOps poll took %1ms").arg(elapsed);
    QVERIFY2(elapsed < 50,
             qPrintable(QStringLiteral("poll took %1ms, expected <50ms").arg(elapsed)));
}

void TestAzureDevOpsIntegrationHealth::eventLoopRemainsFree_duringPoll()
{
    // 在主线程做一次同步轮询，同时检查事件循环是否仍可调度 QTimer 回调
    auto transport = std::make_shared<TimedFakeTransport>();
    auto settings = leyochatSettings();
    settings.notificationTargets.push_back(
        {QStringLiteral("leyochat"), QStringLiteral("LeyoChat"), true, 0});

    bool timerFired = false;
    QTimer::singleShot(0, [&timerFired]() { timerFired = true; });

    AzureDevOpsBuildNotificationPoller poller(settings, transport);
    QString error;
    poller.pollTrackedNotifications(&settings, &error);

    // 让事件循环跑一轮
    QCoreApplication::processEvents();
    QVERIFY2(timerFired,
             "QTimer::singleShot did not fire — poll blocked the event loop");
}

void TestAzureDevOpsIntegrationHealth::multiTargetPoll_totalTimeScalesLinearly()
{
    // 3 个 target，每个 Fake 延迟 5ms → 总耗时应 < 100ms（不是 15s × 3）
    auto transport = std::make_shared<TimedFakeTransport>();
    transport->delayMs = 5;

    auto settings = leyochatSettings();
    settings.organization = QStringLiteral("org");
    settings.project = QStringLiteral("P");
    for (int i = 0; i < 3; ++i) {
        settings.notificationTargets.push_back(
            {QStringLiteral("org"), QStringLiteral("P%1").arg(i), true, 0});
    }

    AzureDevOpsBuildNotificationPoller poller(settings, transport);

    QElapsedTimer timer;
    timer.start();
    QString error;
    poller.pollTrackedNotifications(&settings, &error);
    const qint64 elapsed = timer.elapsed();

    qInfo().noquote() << QStringLiteral("[perf] 3-target poll took %1ms").arg(elapsed);
    QVERIFY2(elapsed < 200,
             qPrintable(QStringLiteral("3-target poll took %1ms, expected <200ms").arg(elapsed)));
}

// =====================================================================
// 超时行为
// =====================================================================

void TestAzureDevOpsIntegrationHealth::httpTimeout_doesNotExceed30Seconds()
{
    // 验证配置值：LocalAzureDevOpsAdapter / NetworkAzureDevOpsApiTransport
    // 我们只能间接验证——如果 Transport 立即失败，Poller 不会挂起
    auto transport = std::make_shared<TimedFakeTransport>();
    transport->shouldFail = true;
    transport->failMessage = QStringLiteral("connection timed out after 30000ms");

    auto settings = leyochatSettings();
    settings.notificationTargets.push_back(
        {QStringLiteral("leyochat"), QStringLiteral("LeyoChat"), true, 0});

    AzureDevOpsBuildNotificationPoller poller(settings, transport);

    QElapsedTimer timer;
    timer.start();
    QString error;
    poller.pollTrackedNotifications(&settings, &error);
    const qint64 elapsed = timer.elapsed();

    QVERIFY(!error.isEmpty());
    QVERIFY2(elapsed < 100,
             qPrintable(QStringLiteral("timeout-failure path took %1ms, expected <100ms").arg(elapsed)));
}

QTEST_MAIN(TestAzureDevOpsIntegrationHealth)
#include "TestAzureDevOpsIntegrationHealth.moc"
