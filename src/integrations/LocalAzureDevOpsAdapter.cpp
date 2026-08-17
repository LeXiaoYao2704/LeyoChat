#include "integrations/LocalAzureDevOpsAdapter.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

namespace {

QString basicAuthHeader(const QString& personalAccessToken)
{
    return QStringLiteral("Basic %1")
        .arg(QString::fromLatin1(
            QByteArray(QStringLiteral(":%1").arg(personalAccessToken).toUtf8()).toBase64()));
}

QString normalizedBaseUrl(const AzureDevOpsConnectionSettings& settings)
{
    QString baseUrl = settings.baseUrl.trimmed();
    if (baseUrl.isEmpty()) {
        baseUrl = QStringLiteral("https://dev.azure.com");
    }
    while (baseUrl.endsWith(QLatin1Char('/'))) {
        baseUrl.chop(1);
    }
    return baseUrl;
}

QUrl apiUrlForLocator(const AzureDevOpsConnectionSettings& settings,
                      const AzureDevOpsResourceLocator& locator)
{
    const QString baseUrl = normalizedBaseUrl(settings);

    if (locator.kind == AzureDevOpsResourceKind::WorkItem) {
        return QUrl(QStringLiteral("%1/%2/%3/_apis/wit/workitems/%4?api-version=7.0")
                        .arg(baseUrl, locator.organization, locator.project, locator.resourceId));
    }
    if (locator.kind == AzureDevOpsResourceKind::PullRequest) {
        return QUrl(QStringLiteral("%1/%2/%3/_apis/git/repositories/%4/pullRequests/%5?api-version=7.0")
                        .arg(baseUrl,
                             locator.organization,
                             locator.project,
                             locator.repository,
                             locator.resourceId));
    }
    if (locator.kind == AzureDevOpsResourceKind::Build) {
        return QUrl(QStringLiteral("%1/%2/%3/_apis/build/builds/%4?api-version=7.0")
                        .arg(baseUrl, locator.organization, locator.project, locator.resourceId));
    }
    return {};
}

QUrl apiProjectUrl(const AzureDevOpsConnectionSettings& settings)
{
    return QUrl(QStringLiteral("%1/%2/_apis/projects/%3?api-version=7.0")
                    .arg(normalizedBaseUrl(settings),
                         settings.organization.trimmed(),
                         settings.project.trimmed()));
}

QUrl profileMeUrl()
{
    return QUrl(
        QStringLiteral("https://app.vssps.visualstudio.com/_apis/profile/profiles/me?api-version=7.0"));
}

QUrl accountsUrl(const QString& memberId)
{
    return QUrl(QStringLiteral(
                    "https://app.vssps.visualstudio.com/_apis/accounts?memberId=%1&api-version=7.0-preview.1")
                    .arg(memberId.trimmed()));
}

// Returns true when the configured baseUrl points to an on-premises Azure DevOps Server
// rather than the Azure DevOps cloud service (dev.azure.com / visualstudio.com).
bool isOnPremServer(const AzureDevOpsConnectionSettings& settings)
{
    const QString base = settings.baseUrl.trimmed().toLower();
    return !base.isEmpty()
        && !base.contains(QStringLiteral("dev.azure.com"))
        && !base.contains(QStringLiteral("visualstudio.com"));
}

QUrl connectionDataUrl(const AzureDevOpsConnectionSettings& settings)
{
    // on-prem Azure DevOps Server 的 connectionData 端点不支持 7.0，
    // 必须使用 5.0-preview 才能拿到 authenticatedUser 信息
    const QString apiVersion = isOnPremServer(settings)
        ? QStringLiteral("5.0-preview")
        : QStringLiteral("7.0");
    return QUrl(QStringLiteral("%1/_apis/connectionData?api-version=%2")
                    .arg(normalizedBaseUrl(settings), apiVersion));
}

QUrl projectCollectionsUrl(const AzureDevOpsConnectionSettings& settings)
{
    return QUrl(QStringLiteral("%1/_apis/projectcollections?api-version=7.0")
                    .arg(normalizedBaseUrl(settings)));
}

QUrl projectsUrl(const AzureDevOpsConnectionSettings& settings, const QString& organization)
{
    return QUrl(QStringLiteral("%1/%2/_apis/projects?api-version=7.0")
                    .arg(normalizedBaseUrl(settings), organization.trimmed()));
}

QString stringAtPath(const QJsonObject& object, std::initializer_list<QStringView> path)
{
    QJsonValue current(object);
    for (const QStringView part : path) {
        if (!current.isObject()) {
            return {};
        }
        current = current.toObject().value(part.toString());
    }

    if (current.isString()) {
        return current.toString().trimmed();
    }
    if (current.isDouble()) {
        return QString::number(static_cast<qint64>(current.toDouble()));
    }
    if (current.isObject()) {
        const QJsonObject nested = current.toObject();
        if (nested.contains(QStringLiteral("displayName"))) {
            return nested.value(QStringLiteral("displayName")).toString().trimmed();
        }
        if (nested.contains(QStringLiteral("name"))) {
            return nested.value(QStringLiteral("name")).toString().trimmed();
        }
        if (nested.contains(QStringLiteral("id"))) {
            return nested.value(QStringLiteral("id")).toString().trimmed();
        }
    }
    return {};
}

QString chooseFirstNonEmpty(std::initializer_list<QString> values)
{
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            return trimmed;
        }
    }
    return {};
}

void populateCurrentUserFromProfile(const QJsonObject& profileObject,
                                    AzureDevOpsConnectionSettings* settings)
{
    if (!settings) {
        return;
    }

    settings->currentUserId = chooseFirstNonEmpty({
        stringAtPath(profileObject, {QStringLiteral("id")}),
        stringAtPath(profileObject, {QStringLiteral("coreAttributes"), QStringLiteral("id"), QStringLiteral("value")}),
    });
    settings->currentUserDisplayName = chooseFirstNonEmpty({
        stringAtPath(profileObject, {QStringLiteral("displayName")}),
        stringAtPath(profileObject, {QStringLiteral("coreAttributes"), QStringLiteral("displayName"), QStringLiteral("value")}),
        stringAtPath(profileObject, {QStringLiteral("publicAlias")}),
    });
    settings->currentUserUniqueName = chooseFirstNonEmpty({
        stringAtPath(profileObject, {QStringLiteral("emailAddress")}),
        stringAtPath(profileObject, {QStringLiteral("coreAttributes"), QStringLiteral("emailAddress"), QStringLiteral("value")}),
        stringAtPath(profileObject, {QStringLiteral("uniqueName")}),
    });
    settings->currentUserEmail = chooseFirstNonEmpty({
        stringAtPath(profileObject, {QStringLiteral("emailAddress")}),
        stringAtPath(profileObject, {QStringLiteral("coreAttributes"), QStringLiteral("emailAddress"), QStringLiteral("value")}),
    });
}

}  // namespace

std::optional<QJsonDocument> NetworkAzureDevOpsApiTransport::getJson(
    const QUrl& url,
    const AzureDevOpsConnectionSettings& settings,
    QString* errorMessage) const
{
    QElapsedTimer elapsed;
    elapsed.start();
    qInfo().noquote() << QStringLiteral("[integrations][azure-devops] GET %1").arg(url.toString());

    const QString curlPath = QStandardPaths::findExecutable(QStringLiteral("curl"));
    if (curlPath.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("找不到 curl 可执行文件");
        return std::nullopt;
    }

    QStringList args;
    args << QStringLiteral("-s")
         << QStringLiteral("-k")
         << QStringLiteral("--max-time") << QStringLiteral("30")
         << QStringLiteral("-H") << QStringLiteral("Content-Type: application/json")
         << QStringLiteral("-H") << QStringLiteral("Authorization: %1").arg(basicAuthHeader(settings.personalAccessToken))
         << url.toString();

    QProcess process;
    process.start(curlPath, args);
    if (!process.waitForFinished(35000)) {
        process.kill();
        process.waitForFinished(5000);
        qWarning().noquote() << QStringLiteral("[integrations][azure-devops] GET timeout after %1 ms: %2")
                                    .arg(QString::number(elapsed.elapsed()), url.toString());
        if (errorMessage) *errorMessage = QStringLiteral("Azure DevOps 请求超时");
        return std::nullopt;
    }

    const QByteArray body = process.readAllStandardOutput();
    const QByteArray stderrOutput = process.readAllStandardError();
    if (process.exitCode() != 0) {
        qWarning().noquote() << QStringLiteral("[integrations][azure-devops] GET failed (exit=%1) after %2 ms: %3")
                                    .arg(process.exitCode()).arg(elapsed.elapsed()).arg(QString::fromUtf8(stderrOutput).trimmed());
        if (errorMessage) {
            *errorMessage = chooseFirstNonEmpty({
                QString::fromUtf8(stderrOutput).trimmed(),
                QStringLiteral("Azure DevOps 请求失败 (curl exit=%1)").arg(process.exitCode()),
            });
        }
        return std::nullopt;
    }

    qInfo().noquote() << QStringLiteral("[integrations][azure-devops] GET completed in %1 ms, bodyLen=%2")
                             .arg(QString::number(elapsed.elapsed()), QString::number(body.size()));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        const QString bodyPreview = QString::fromUtf8(body.left(200)).trimmed();
        qWarning().noquote() << QStringLiteral("[integrations][azure-devops] JSON parse failed: %1, bodyPreview: %2")
                                    .arg(parseError.errorString(), bodyPreview);
        if (errorMessage) {
            if (bodyPreview.startsWith(QLatin1Char('<'))) {
                *errorMessage = QStringLiteral("Azure DevOps 返回了 HTML 而非 JSON（可能是认证页面或地址错误）");
            } else if (body.isEmpty()) {
                *errorMessage = QStringLiteral("Azure DevOps 返回了空响应");
            } else {
                *errorMessage = QStringLiteral("Azure DevOps 返回了无法解析的 JSON: %1").arg(parseError.errorString());
            }
        }
        return std::nullopt;
    }

    return document;
}

std::optional<QJsonDocument> NetworkAzureDevOpsApiTransport::postJson(
    const QUrl& url,
    const AzureDevOpsConnectionSettings& settings,
    const QJsonDocument& body,
    QString* errorMessage) const
{
    QElapsedTimer elapsed;
    elapsed.start();
    qInfo().noquote() << QStringLiteral("[integrations][azure-devops] POST %1").arg(url.toString());

    const QString curlPath = QStandardPaths::findExecutable(QStringLiteral("curl"));
    if (curlPath.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("找不到 curl 可执行文件");
        return std::nullopt;
    }

    QStringList args;
    args << QStringLiteral("-s")
         << QStringLiteral("-k")
         << QStringLiteral("--max-time") << QStringLiteral("30")
         << QStringLiteral("-X") << QStringLiteral("POST")
         << QStringLiteral("-H") << QStringLiteral("Content-Type: application/json")
         << QStringLiteral("-H") << QStringLiteral("Authorization: %1").arg(basicAuthHeader(settings.personalAccessToken))
         << QStringLiteral("-d") << QString::fromUtf8(body.toJson(QJsonDocument::Compact))
         << url.toString();

    QProcess process;
    process.start(curlPath, args);
    if (!process.waitForFinished(35000)) {
        process.kill();
        process.waitForFinished(5000);
        qWarning().noquote() << QStringLiteral("[integrations][azure-devops] POST timeout after %1 ms: %2")
                                    .arg(QString::number(elapsed.elapsed()), url.toString());
        if (errorMessage) *errorMessage = QStringLiteral("Azure DevOps 请求超时");
        return std::nullopt;
    }

    const QByteArray responseBody = process.readAllStandardOutput();
    const QByteArray stderrOutput = process.readAllStandardError();
    if (process.exitCode() != 0) {
        qWarning().noquote() << QStringLiteral("[integrations][azure-devops] POST failed (exit=%1) after %2 ms: %3")
                                    .arg(process.exitCode()).arg(elapsed.elapsed()).arg(QString::fromUtf8(stderrOutput).trimmed());
        if (errorMessage) {
            *errorMessage = chooseFirstNonEmpty({
                QString::fromUtf8(stderrOutput).trimmed(),
                QStringLiteral("Azure DevOps 请求失败 (curl exit=%1)").arg(process.exitCode()),
            });
        }
        return std::nullopt;
    }

    qInfo().noquote() << QStringLiteral("[integrations][azure-devops] POST completed in %1 ms, bodyLen=%2")
                             .arg(QString::number(elapsed.elapsed()), QString::number(responseBody.size()));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(responseBody, &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        const QString bodyPreview = QString::fromUtf8(responseBody.left(200)).trimmed();
        qWarning().noquote() << QStringLiteral("[integrations][azure-devops] POST JSON parse failed: %1, bodyPreview: %2")
                                    .arg(parseError.errorString(), bodyPreview);
        if (errorMessage) {
            if (bodyPreview.startsWith(QLatin1Char('<'))) {
                *errorMessage = QStringLiteral("Azure DevOps 返回了 HTML 而非 JSON（可能是认证页面或地址错误）");
            } else if (responseBody.isEmpty()) {
                *errorMessage = QStringLiteral("Azure DevOps 返回了空响应");
            } else {
                *errorMessage = QStringLiteral("Azure DevOps 返回了无法解析的 JSON: %1").arg(parseError.errorString());
            }
        }
        return std::nullopt;
    }

    return document;
}

LocalAzureDevOpsAdapter::LocalAzureDevOpsAdapter(
    AzureDevOpsConnectionSettings settings,
    std::shared_ptr<IAzureDevOpsApiTransport> transport)
    : m_settings(std::move(settings))
    , m_transport(std::move(transport))
{
}

QString LocalAzureDevOpsAdapter::adapterId() const
{
    return QStringLiteral("azure-devops-local");
}

QString LocalAzureDevOpsAdapter::displayName() const
{
    return QStringLiteral("Azure DevOps");
}

QVector<ResourceReference> LocalAzureDevOpsAdapter::visibleResourcesForWorkspace(
    const QString& workspaceId) const
{
    QVector<ResourceReference> resources;
    const QString expectedWorkspaceId = workspaceId.trimmed();
    for (auto it = m_referenceCache.cbegin(); it != m_referenceCache.cend(); ++it) {
        if (!expectedWorkspaceId.isEmpty()
            && it.value().workspaceId.trimmed() != expectedWorkspaceId) {
            continue;
        }
        resources.push_back(it.value());
    }
    return resources;
}

std::optional<ResourceRefPayload> LocalAzureDevOpsAdapter::payloadForResource(
    const QString& resourceId) const
{
    const auto it = m_payloadCache.constFind(resourceId.trimmed());
    if (it == m_payloadCache.cend()) {
        return std::nullopt;
    }
    return it.value();
}

QVector<AzureDevOpsOrganizationInfo> LocalAzureDevOpsAdapter::discoverOrganizations(
    QString* errorMessage) const
{
    QVector<AzureDevOpsOrganizationInfo> organizations;
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先填写 Azure DevOps 地址和 PAT");
        }
        return organizations;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 传输层未初始化");
        }
        return organizations;
    }

    if (isOnPremServer(m_settings)) {
        const auto collectionsDoc = m_transport->getJson(
            projectCollectionsUrl(m_settings), m_settings, errorMessage);
        if (collectionsDoc.has_value() && collectionsDoc->isObject()) {
            const QJsonArray values = collectionsDoc->object().value(QStringLiteral("value")).toArray();
            for (const QJsonValue& value : values) {
                if (!value.isObject())
                    continue;
                const QJsonObject object = value.toObject();
                AzureDevOpsOrganizationInfo info;
                info.organizationId   = stringAtPath(object, {QStringLiteral("id")});
                info.organizationName = stringAtPath(object, {QStringLiteral("name")});
                info.organizationUrl  = stringAtPath(object, {QStringLiteral("url")});
                if (!info.organizationName.trimmed().isEmpty())
                    organizations.push_back(info);
            }
        }
        // Fallback: 当 projectcollections 不可用时，通过 connectionData 提取 collection 名称
        if (organizations.isEmpty()) {
            qInfo().noquote() << QStringLiteral(
                "[integrations][azure-devops] projectcollections 不可用，尝试通过 connectionData 发现 collections");
            const QString baseUrl = normalizedBaseUrl(m_settings);
            const auto connDoc = m_transport->getJson(
                connectionDataUrl(m_settings), m_settings, errorMessage);
            if (!connDoc.has_value() || !connDoc->isObject()) {
                qWarning().noquote() << QStringLiteral(
                    "[integrations][azure-devops] connectionData fallback also failed: %1")
                    .arg(errorMessage ? errorMessage->trimmed() : QStringLiteral("unknown"));
            } else {
                // connectionData.locationServiceData.serviceDefinitions[].serviceOwner / locationMappings
                // 中的 locationUrl 包含 collection 路径，例如 http://server/TeamResource/...
                const QJsonObject root = connDoc->object();
                qInfo().noquote() << QStringLiteral(
                    "[integrations][azure-devops] connectionData top-level keys: %1")
                    .arg(QStringList(root.keys()).join(QStringLiteral(", ")));
                const QJsonObject locData = root.value(QStringLiteral("locationServiceData")).toObject();
                const QJsonArray defs = locData.value(QStringLiteral("serviceDefinitions")).toArray();
                qInfo().noquote() << QStringLiteral(
                    "[integrations][azure-devops] connectionData serviceDefinitions count: %1")
                    .arg(defs.size());
                QSet<QString> seenCollections;
                const QString lowerBase = baseUrl.toLower();
                for (const QJsonValue& def : defs) {
                    if (!def.isObject()) continue;
                    const QJsonArray mappings = def.toObject()
                        .value(QStringLiteral("locationMappings")).toArray();
                    for (const QJsonValue& mapping : mappings) {
                        if (!mapping.isObject()) continue;
                        const QString loc = mapping.toObject()
                            .value(QStringLiteral("location")).toString().trimmed();
                        if (loc.isEmpty()) continue;
                        // 从 URL 中提取 base 之后的第一个路径段作为 collection 名称
                        // 例如 https://devops.example.com/TeamResource/_apis/... → TeamResource
                        QString remainder;
                        const QString lowerLoc = loc.toLower();
                        if (lowerLoc.startsWith(lowerBase + QStringLiteral("/"))) {
                            remainder = loc.mid(baseUrl.length() + 1);
                        } else if (lowerLoc.startsWith(lowerBase)) {
                            remainder = loc.mid(baseUrl.length());
                        }
                        if (remainder.isEmpty()) continue;
                        const int slashIdx = remainder.indexOf(QLatin1Char('/'));
                        const QString segment = (slashIdx > 0) ? remainder.left(slashIdx) : remainder;
                        if (segment.isEmpty() || segment.startsWith(QLatin1Char('_'))) continue;
                        const QString lowerSeg = segment.toLower();
                        if (seenCollections.contains(lowerSeg)) continue;
                        seenCollections.insert(lowerSeg);
                        AzureDevOpsOrganizationInfo info;
                        info.organizationName = segment;
                        info.organizationUrl  = baseUrl + QStringLiteral("/") + segment;
                        organizations.push_back(info);
                    }
                }
            }
        }
        if (organizations.isEmpty()) {
            qWarning().noquote() << QStringLiteral(
                "[integrations][azure-devops] on-prem collection discovery failed, no collections found");
            if (errorMessage)
                *errorMessage = QStringLiteral(
                    "无法自动发现组织/集合，请在组织栏手动输入集合名称（例如 TeamResource）");
        } else {
            if (errorMessage) errorMessage->clear();
            qInfo().noquote() << QStringLiteral(
                "[integrations][azure-devops] discovered %1 project collections (on-prem)")
                .arg(QString::number(organizations.size()));
        }
        return organizations;
    }

    const auto profileDocument = m_transport->getJson(profileMeUrl(), m_settings, errorMessage);
    if (!profileDocument.has_value() || !profileDocument->isObject()) {
        qWarning().noquote() << QStringLiteral("[integrations][azure-devops] organization discovery failed: %1")
                                    .arg(errorMessage ? errorMessage->trimmed()
                                                      : QStringLiteral("unknown"));
        return organizations;
    }

    const QString memberId = stringAtPath(profileDocument->object(), {QStringLiteral("id")});
    if (memberId.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法识别当前 Azure DevOps 身份");
        }
        return organizations;
    }

    const auto accountsDocument = m_transport->getJson(accountsUrl(memberId), m_settings, errorMessage);
    if (!accountsDocument.has_value() || !accountsDocument->isObject()) {
        qWarning().noquote() << QStringLiteral("[integrations][azure-devops] account listing failed: %1")
                                    .arg(errorMessage ? errorMessage->trimmed()
                                                      : QStringLiteral("unknown"));
        return organizations;
    }

    const QJsonArray values = accountsDocument->object().value(QStringLiteral("value")).toArray();
    for (const QJsonValue& value : values) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        AzureDevOpsOrganizationInfo info;
        info.organizationId = stringAtPath(object, {QStringLiteral("accountId")});
        info.organizationName = stringAtPath(object, {QStringLiteral("accountName")});
        info.organizationUrl = stringAtPath(object, {QStringLiteral("accountUri")});
        if (!info.organizationName.trimmed().isEmpty()) {
            organizations.push_back(info);
        }
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    qInfo().noquote() << QStringLiteral("[integrations][azure-devops] discovered %1 organizations")
                             .arg(QString::number(organizations.size()));
    return organizations;
}

QVector<AzureDevOpsProjectInfo> LocalAzureDevOpsAdapter::discoverProjects(
    const QString& organization,
    QString* errorMessage) const
{
    QVector<AzureDevOpsProjectInfo> projects;
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先填写 Azure DevOps 地址和 PAT");
        }
        return projects;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 传输层未初始化");
        }
        return projects;
    }
    if (organization.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先选择组织");
        }
        return projects;
    }

    const auto document = m_transport->getJson(projectsUrl(m_settings, organization), m_settings, errorMessage);
    if (!document.has_value() || !document->isObject()) {
        qWarning().noquote() << QStringLiteral("[integrations][azure-devops] project discovery failed for %1: %2")
                                    .arg(organization.trimmed(),
                                         errorMessage ? errorMessage->trimmed()
                                                      : QStringLiteral("unknown"));
        return projects;
    }

    const QJsonArray values = document->object().value(QStringLiteral("value")).toArray();
    for (const QJsonValue& value : values) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        AzureDevOpsProjectInfo info;
        info.projectId = stringAtPath(object, {QStringLiteral("id")});
        info.projectName = stringAtPath(object, {QStringLiteral("name")});
        info.state = stringAtPath(object, {QStringLiteral("state")});
        if (!info.projectName.trimmed().isEmpty()) {
            projects.push_back(info);
        }
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    qInfo().noquote() << QStringLiteral("[integrations][azure-devops] discovered %1 projects for %2")
                             .arg(QString::number(projects.size()), organization.trimmed());
    return projects;
}

bool LocalAzureDevOpsAdapter::testConnection(QString* errorMessage) const
{
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先填写 Azure DevOps 地址和 PAT");
        }
        return false;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 传输层未初始化");
        }
        return false;
    }

    if (!m_settings.hasProjectSelection()) {
        const QUrl checkUrl = isOnPremServer(m_settings)
            ? connectionDataUrl(m_settings)
            : profileMeUrl();
        const auto profileDocument = m_transport->getJson(checkUrl, m_settings, errorMessage);
        const bool ok = profileDocument.has_value() && profileDocument->isObject();
        if (ok) {
            qInfo().noquote() << QStringLiteral("[integrations][azure-devops] connection test succeeded via profile endpoint");
        } else {
            qWarning().noquote() << QStringLiteral("[integrations][azure-devops] connection test failed: %1")
                                        .arg(errorMessage ? errorMessage->trimmed()
                                                          : QStringLiteral("unknown"));
        }
        return ok;
    }

    const auto document = m_transport->getJson(apiProjectUrl(m_settings), m_settings, errorMessage);
    if (!document.has_value() || !document->isObject()) {
        qWarning().noquote() << QStringLiteral("[integrations][azure-devops] connection test failed for %1 / %2: %3")
                                    .arg(m_settings.organization.trimmed(),
                                         m_settings.project.trimmed(),
                                         errorMessage ? errorMessage->trimmed()
                                                      : QStringLiteral("unknown"));
        return false;
    }

    const QJsonObject object = document->object();
    const QString projectName = chooseFirstNonEmpty({
        stringAtPath(object, {QStringLiteral("name")}),
        stringAtPath(object, {QStringLiteral("project"), QStringLiteral("name")}),
    });
    if (projectName.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 返回了无效项目数据");
        }
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    qInfo().noquote() << QStringLiteral("[integrations][azure-devops] connection test succeeded for %1 / %2")
                             .arg(m_settings.organization.trimmed(), m_settings.project.trimmed());
    return true;
}

bool LocalAzureDevOpsAdapter::discoverCurrentUser(AzureDevOpsConnectionSettings* resolvedSettings,
                                                   QString* errorMessage) const
{
    if (!resolvedSettings) {
        return false;
    }
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先填写 Azure DevOps 地址和 PAT");
        }
        return false;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 传输层未初始化");
        }
        return false;
    }

    if (isOnPremServer(m_settings)) {
        const auto connDataDoc = m_transport->getJson(
            connectionDataUrl(m_settings), m_settings, errorMessage);
        if (!connDataDoc.has_value() || !connDataDoc->isObject()) {
            qWarning().noquote() << QStringLiteral(
                "[integrations][azure-devops] on-prem current user discovery failed: %1")
                .arg(errorMessage ? errorMessage->trimmed() : QStringLiteral("unknown"));
            return false;
        }
        const QJsonObject authUser =
            connDataDoc->object().value(QStringLiteral("authenticatedUser")).toObject();
        resolvedSettings->currentUserId = stringAtPath(authUser, {QStringLiteral("id")});
        if (resolvedSettings->currentUserId.trimmed().isEmpty()) {
            qWarning().noquote() << QStringLiteral(
                "[integrations][azure-devops] on-prem connectionData 返回了空的 authenticatedUser.id");
            if (errorMessage)
                *errorMessage = QStringLiteral("on-prem connectionData 未返回有效的用户 ID");
            return false;
        }
        resolvedSettings->currentUserDisplayName = chooseFirstNonEmpty({
            stringAtPath(authUser, {QStringLiteral("providerDisplayName")}),
            stringAtPath(authUser, {QStringLiteral("customDisplayName")}),
        });
        resolvedSettings->currentUserUniqueName =
            stringAtPath(authUser, {QStringLiteral("uniqueName")});
        resolvedSettings->currentUserEmail = resolvedSettings->currentUserUniqueName;
        if (errorMessage)
            errorMessage->clear();
        qInfo().noquote() << QStringLiteral(
            "[integrations][azure-devops] discovered current user (on-prem): %1")
            .arg(resolvedSettings->currentUserDisplayName.trimmed());
        return true;
    }

    const auto profileDocument = m_transport->getJson(profileMeUrl(), m_settings, errorMessage);
    if (!profileDocument.has_value() || !profileDocument->isObject()) {
        qWarning().noquote() << QStringLiteral("[integrations][azure-devops] current user discovery failed: %1")
                                    .arg(errorMessage ? errorMessage->trimmed()
                                                      : QStringLiteral("unknown"));
        return false;
    }

    populateCurrentUserFromProfile(profileDocument->object(), resolvedSettings);
    if (resolvedSettings->currentUserId.trimmed().isEmpty()) {
        qWarning().noquote() << QStringLiteral(
            "[integrations][azure-devops] profile 返回了空的用户 ID");
        if (errorMessage)
            *errorMessage = QStringLiteral("profile 未返回有效的用户 ID");
        return false;
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    qInfo().noquote() << QStringLiteral("[integrations][azure-devops] discovered current user: %1")
                             .arg(resolvedSettings->currentUserDisplayName.trimmed());
    return true;
}

std::optional<ResourceRefPayload> LocalAzureDevOpsAdapter::payloadForLink(const QString& link,
                                                                          QString* errorMessage)
{
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先完成 Azure DevOps 认证");
        }
        return std::nullopt;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 传输层未初始化");
        }
        return std::nullopt;
    }

    const auto locator = AzureDevOpsLinkParser::parse(link);
    if (!locator.has_value()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("当前只支持 Azure DevOps 工作项、PR 和构建链接");
        }
        return std::nullopt;
    }

    return payloadForLocator(*locator, errorMessage);
}

std::optional<ResourceRefPayload> LocalAzureDevOpsAdapter::payloadForLocator(
    const AzureDevOpsResourceLocator& locator,
    QString* errorMessage)
{
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先完成 Azure DevOps 认证");
        }
        return std::nullopt;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 传输层未初始化");
        }
        return std::nullopt;
    }
    if (!locator.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 资源信息不完整");
        }
        return std::nullopt;
    }

    if (locator.kind == AzureDevOpsResourceKind::WorkItem) {
        return resolveWorkItem(locator, errorMessage);
    }
    if (locator.kind == AzureDevOpsResourceKind::PullRequest) {
        return resolvePullRequest(locator, errorMessage);
    }
    if (locator.kind == AzureDevOpsResourceKind::Build) {
        return resolveBuild(locator, errorMessage);
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("暂不支持当前 Azure DevOps 资源类型");
    }
    return std::nullopt;
}

std::optional<ResourceRefPayload> LocalAzureDevOpsAdapter::resolveWorkItem(
    const AzureDevOpsResourceLocator& locator,
    QString* errorMessage)
{
    const auto document = m_transport->getJson(apiUrlForLocator(m_settings, locator), m_settings, errorMessage);
    if (!document.has_value() || !document->isObject()) {
        return std::nullopt;
    }

    const QJsonObject object = document->object();
    DevOpsWorkItemResource resource;
    resource.serviceId = QStringLiteral("azure-devops-local");
    resource.workspaceId = QStringLiteral("azure-devops:%1:%2").arg(locator.organization, locator.project);
    resource.resourceId = QStringLiteral("work-item:%1").arg(locator.resourceId);
    resource.organization = locator.organization;
    resource.project = chooseFirstNonEmpty({
        stringAtPath(object, {QStringLiteral("fields"), QStringLiteral("System.TeamProject")}),
        locator.project,
    });
    resource.workItemType =
        stringAtPath(object, {QStringLiteral("fields"), QStringLiteral("System.WorkItemType")});
    resource.title =
        stringAtPath(object, {QStringLiteral("fields"), QStringLiteral("System.Title")});
    resource.state =
        stringAtPath(object, {QStringLiteral("fields"), QStringLiteral("System.State")});
    resource.assignedTo =
        stringAtPath(object, {QStringLiteral("fields"), QStringLiteral("System.AssignedTo")});
    resource.webUrl = chooseFirstNonEmpty({
        stringAtPath(object, {QStringLiteral("_links"), QStringLiteral("html"), QStringLiteral("href")}),
        locator.webUrl,
    });
    resource.numericId = stringAtPath(object, {QStringLiteral("id")}).toInt();

    const ResourceRefPayload payload = DevOpsAdapterContracts::makeWorkItemPayload(resource);
    cachePayload(payload);
    return payload;
}

std::optional<ResourceRefPayload> LocalAzureDevOpsAdapter::resolvePullRequest(
    const AzureDevOpsResourceLocator& locator,
    QString* errorMessage)
{
    const auto document = m_transport->getJson(apiUrlForLocator(m_settings, locator), m_settings, errorMessage);
    if (!document.has_value() || !document->isObject()) {
        return std::nullopt;
    }

    const QJsonObject object = document->object();
    DevOpsPullRequestResource resource;
    resource.serviceId = QStringLiteral("azure-devops-local");
    resource.workspaceId = QStringLiteral("azure-devops:%1:%2").arg(locator.organization, locator.project);
    resource.resourceId = QStringLiteral("pull-request:%1:%2").arg(locator.repository, locator.resourceId);
    resource.organization = locator.organization;
    resource.project = chooseFirstNonEmpty({
        stringAtPath(object, {QStringLiteral("repository"), QStringLiteral("project"), QStringLiteral("name")}),
        locator.project,
    });
    resource.repository = chooseFirstNonEmpty({
        stringAtPath(object, {QStringLiteral("repository"), QStringLiteral("name")}),
        locator.repository,
    });
    resource.title = stringAtPath(object, {QStringLiteral("title")});
    resource.status = stringAtPath(object, {QStringLiteral("status")});
    resource.author =
        stringAtPath(object, {QStringLiteral("createdBy"), QStringLiteral("displayName")});
    resource.webUrl = chooseFirstNonEmpty({
        stringAtPath(object, {QStringLiteral("_links"), QStringLiteral("web"), QStringLiteral("href")}),
        locator.webUrl,
    });
    resource.pullRequestId = stringAtPath(object, {QStringLiteral("pullRequestId")}).toInt();

    const ResourceRefPayload payload = DevOpsAdapterContracts::makePullRequestPayload(resource);
    cachePayload(payload);
    return payload;
}

std::optional<ResourceRefPayload> LocalAzureDevOpsAdapter::resolveBuild(
    const AzureDevOpsResourceLocator& locator,
    QString* errorMessage)
{
    const auto document = m_transport->getJson(apiUrlForLocator(m_settings, locator), m_settings, errorMessage);
    if (!document.has_value() || !document->isObject()) {
        return std::nullopt;
    }

    const QJsonObject object = document->object();
    DevOpsBuildResource resource;
    resource.serviceId = QStringLiteral("azure-devops-local");
    resource.workspaceId = QStringLiteral("azure-devops:%1:%2").arg(locator.organization, locator.project);
    resource.resourceId = QStringLiteral("build:%1").arg(locator.resourceId);
    resource.organization = locator.organization;
    resource.project = chooseFirstNonEmpty({
        stringAtPath(object, {QStringLiteral("project"), QStringLiteral("name")}),
        locator.project,
    });
    resource.definitionName =
        stringAtPath(object, {QStringLiteral("definition"), QStringLiteral("name")});
    resource.branchName = stringAtPath(object, {QStringLiteral("sourceBranch")});
    resource.status = chooseFirstNonEmpty({
        stringAtPath(object, {QStringLiteral("result")}),
        stringAtPath(object, {QStringLiteral("status")}),
    });
    resource.requestedBy =
        stringAtPath(object, {QStringLiteral("requestedFor"), QStringLiteral("displayName")});
    resource.webUrl = chooseFirstNonEmpty({
        stringAtPath(object, {QStringLiteral("_links"), QStringLiteral("web"), QStringLiteral("href")}),
        locator.webUrl,
    });
    resource.buildId = stringAtPath(object, {QStringLiteral("id")}).toInt();

    const ResourceRefPayload payload = DevOpsAdapterContracts::makeBuildPayload(resource);
    cachePayload(payload);
    return payload;
}

void LocalAzureDevOpsAdapter::cachePayload(const ResourceRefPayload& payload)
{
    if (payload.resourceId.trimmed().isEmpty()) {
        return;
    }
    m_payloadCache.insert(payload.resourceId.trimmed(), payload);

    ResourceReference reference;
    reference.serviceId = payload.serviceId.trimmed();
    reference.workspaceId = payload.workspaceId.trimmed();
    reference.resourceId = payload.resourceId.trimmed();
    reference.resourceKind = payload.kind.trimmed();
    reference.title = payload.title.trimmed();
    reference.summary = payload.subtitle.trimmed();
    reference.version = payload.snapshotVersion.trimmed();
    reference.origin = ResourceOrigin::Local;
    m_referenceCache.insert(reference.resourceId, reference);
}
