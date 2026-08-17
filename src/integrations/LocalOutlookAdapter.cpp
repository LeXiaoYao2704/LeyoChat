#include "integrations/LocalOutlookAdapter.h"

#include <QDateTime>
#include <QDebug>
#include <QTextDocument>
#include <QUrl>

namespace {

QString chooseFirstNonEmpty(std::initializer_list<QString> values)
{
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty())
            return trimmed;
    }
    return {};
}

}  // namespace

LocalOutlookAdapter::LocalOutlookAdapter(
    OutlookConnectionSettings settings,
    std::shared_ptr<IOutlookEwsTransport> ewsTransport)
    : m_settings(std::move(settings))
    , m_ewsTransport(std::move(ewsTransport))
{
}

QString LocalOutlookAdapter::adapterId() const
{
    return QStringLiteral("outlook-local");
}

QString LocalOutlookAdapter::displayName() const
{
    return QStringLiteral("Outlook");
}

QVector<ResourceReference> LocalOutlookAdapter::visibleResourcesForWorkspace(
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

std::optional<ResourceRefPayload> LocalOutlookAdapter::payloadForResource(
    const QString& resourceId) const
{
    const auto it = m_payloadCache.constFind(resourceId.trimmed());
    if (it == m_payloadCache.cend())
        return std::nullopt;
    return it.value();
}

const OutlookConnectionSettings& LocalOutlookAdapter::settings() const
{
    return m_settings;
}

bool LocalOutlookAdapter::checkCredentials(QString* errorMessage) const
{
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("请先填写 Exchange 服务器地址和用户名");
        return false;
    }
    if (!m_ewsTransport) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Outlook EWS 传输层未初始化");
        return false;
    }
    return true;
}

std::optional<OutlookUserProfile> LocalOutlookAdapter::fetchProfile(QString* errorMessage)
{
    if (!checkCredentials(errorMessage))
        return std::nullopt;

    // Try ResolveNames to get the actual email address for this username.
    const QUrl endpoint = OutlookEws::ewsEndpoint(m_settings);
    const auto response = m_ewsTransport->soapPost(
        endpoint,
        OutlookEws::resolveNamesEnvelope(m_settings.username.trimmed()),
        m_settings,
        errorMessage);

    if (response.has_value()) {
        const auto resolved = OutlookEws::parseResolveNamesResponse(*response);
        if (resolved.has_value()) {
            OutlookUserProfile profile;
            profile.email = resolved->email;
            profile.displayName = chooseFirstNonEmpty({resolved->displayName,
                                                       m_settings.username.trimmed()});
            m_settings.accountEmail = profile.email;
            m_settings.displayName = profile.displayName;
            qInfo().noquote()
                << QStringLiteral("[integrations][outlook] resolved user: %1 <%2>")
                       .arg(profile.displayName, profile.email);
            return profile;
        }
    }

    // Fall back to verifying credentials with GetFolder and using username as identity.
    const auto folderResponse = m_ewsTransport->soapPost(
        endpoint,
        OutlookEws::getFolderInboxEnvelope(),
        m_settings,
        errorMessage);

    if (!folderResponse.has_value()) {
        qWarning().noquote()
            << QStringLiteral("[integrations][outlook] EWS connection test failed: %1")
                   .arg(errorMessage ? errorMessage->trimmed() : QStringLiteral("unknown"));
        return std::nullopt;
    }

    OutlookUserProfile profile;
    profile.email = chooseFirstNonEmpty({m_settings.accountEmail,
                                         m_settings.username.trimmed()});
    profile.displayName = chooseFirstNonEmpty({m_settings.displayName,
                                               m_settings.username.trimmed()});
    m_settings.accountEmail = profile.email;
    m_settings.displayName = profile.displayName;
    qInfo().noquote()
        << QStringLiteral("[integrations][outlook] EWS connection ok for %1").arg(profile.email);
    return profile;
}

bool LocalOutlookAdapter::testConnection(QString* errorMessage)
{
    return fetchProfile(errorMessage).has_value();
}

QVector<OutlookMailResource> LocalOutlookAdapter::fetchUnreadMail(int maxItems,
                                                                   QString* errorMessage)
{
    QVector<OutlookMailResource> mails;
    if (!checkCredentials(errorMessage))
        return mails;

    // Step 1: 搜索 inbox 顶层未读邮件
    const auto response = m_ewsTransport->soapPost(
        OutlookEws::ewsEndpoint(m_settings),
        OutlookEws::findUnreadMailEnvelope(maxItems),
        m_settings,
        errorMessage);

    if (!response.has_value()) {
        qWarning().noquote()
            << QStringLiteral("[integrations][outlook] EWS FindItem (mail) failed: %1")
                   .arg(errorMessage ? errorMessage->trimmed() : QStringLiteral("unknown"));
        return mails;
    }

    mails = OutlookEws::parseMailFindItemResponse(*response, m_settings.accountEmail);

    // Step 2: 递归搜索 inbox 所有子文件夹中的未读邮件
    {
        const auto folderResponse = m_ewsTransport->soapPost(
            OutlookEws::ewsEndpoint(m_settings),
            OutlookEws::findInboxSubfoldersEnvelope(),
            m_settings,
            nullptr);

        if (folderResponse.has_value()) {
            const QStringList subfolderIds = OutlookEws::parseFindFolderResponse(*folderResponse);
            if (!subfolderIds.isEmpty()) {
                qInfo().noquote()
                    << QStringLiteral("[integrations][outlook] Found %1 inbox subfolders, searching for unread mail")
                           .arg(subfolderIds.size());

                const int remainingItems = qMax(1, maxItems - mails.size());
                const auto subResponse = m_ewsTransport->soapPost(
                    OutlookEws::ewsEndpoint(m_settings),
                    OutlookEws::findUnreadMailInFoldersEnvelope(subfolderIds, remainingItems),
                    m_settings,
                    nullptr);

                if (subResponse.has_value()) {
                    const auto subMails = OutlookEws::parseMailFindItemResponse(
                        *subResponse, m_settings.accountEmail);
                    mails.append(subMails);
                }
            }
        }
    }

    if (mails.isEmpty()) {
        const QString snippet = QString::fromUtf8(response->left(2000));
        qInfo().noquote()
            << QStringLiteral("[integrations][outlook] FindItem (mail) returned 0 items, response snippet:")
            << snippet;
    }

    // 通过 GetItem 补充邮件正文预览（FindItem 不支持 item:Body/Preview 的旧版 Exchange）
    if (!mails.isEmpty()) {
        QStringList itemIds;
        for (const auto& mail : mails) {
            if (!mail.resourceId.trimmed().isEmpty())
                itemIds.push_back(mail.resourceId.trimmed());
        }
        if (!itemIds.isEmpty()) {
            const auto bodyResponse = m_ewsTransport->soapPost(
                OutlookEws::ewsEndpoint(m_settings),
                OutlookEws::getItemBodyEnvelope(itemIds),
                m_settings,
                nullptr);
            if (bodyResponse.has_value()) {
                const auto bodyMap = OutlookEws::parseGetItemBodyResponse(*bodyResponse);
                for (auto& mail : mails) {
                    const QString html = bodyMap.value(mail.resourceId.trimmed());
                    if (!html.isEmpty()) {
                        mail.htmlBody = html;
                        if (mail.bodyPreview.isEmpty()) {
                            // 从 HTML 中提取纯文本摘要
                            QTextDocument doc;
                            doc.setHtml(html);
                            mail.bodyPreview = doc.toPlainText().trimmed().left(200);
                        }
                    }
                }
            }
        }
    }

    // 根据 serverUrl 构造 OWA 具体邮件深链
    if (!m_settings.serverUrl.trimmed().isEmpty()) {
        const QUrl serverUrl(m_settings.serverUrl.trimmed());
        const QString owaScheme = serverUrl.scheme().isEmpty()
            ? QStringLiteral("https") : serverUrl.scheme();
        const QString owaHost = serverUrl.host();
        for (auto& mail : mails) {
            if (mail.webUrl.trimmed().isEmpty() && !mail.resourceId.trimmed().isEmpty()) {
                mail.webUrl = QStringLiteral("%1://%2/owa/?ItemID=%3&exvsurl=1&viewmodel=ReadMessageItem")
                    .arg(owaScheme, owaHost,
                         QString::fromUtf8(QUrl::toPercentEncoding(mail.resourceId.trimmed())));
            }
        }
    }

    for (const auto& mail : mails)
        cacheMail(mail);

    qInfo().noquote() << QStringLiteral("[integrations][outlook] fetched %1 unread mails via EWS")
                             .arg(QString::number(mails.size()));
    return mails;
}

QVector<OutlookCalendarEventResource> LocalOutlookAdapter::fetchUpcomingEvents(
    const QDateTime& now,
    int horizonMinutes,
    QString* errorMessage)
{
    QVector<OutlookCalendarEventResource> events;
    if (!checkCredentials(errorMessage))
        return events;

    // CalendarView already limits by date range on the server side.
    const QDateTime startUtc = now.addSecs(-5 * 60).toUTC();
    const QDateTime endUtc = now.addSecs(qMax(30, horizonMinutes) * 60).toUTC();

    const auto response = m_ewsTransport->soapPost(
        OutlookEws::ewsEndpoint(m_settings),
        OutlookEws::findCalendarEnvelope(startUtc, endUtc),
        m_settings,
        errorMessage);

    if (!response.has_value()) {
        qWarning().noquote()
            << QStringLiteral("[integrations][outlook] EWS FindItem (calendar) failed: %1")
                   .arg(errorMessage ? errorMessage->trimmed() : QStringLiteral("unknown"));
        return events;
    }

    events = OutlookEws::parseCalendarFindItemResponse(*response);

    // EWS FindItem 不返回 webUrl，根据 serverUrl 构造 OWA 日程链接
    if (!m_settings.serverUrl.trimmed().isEmpty()) {
        const QUrl serverUrl(m_settings.serverUrl.trimmed());
        const QString owaCalUrl = QStringLiteral("%1://%2/owa/?ae=Calendar")
            .arg(serverUrl.scheme().isEmpty() ? QStringLiteral("https") : serverUrl.scheme(),
                 serverUrl.host());
        for (auto& event : events) {
            if (event.webUrl.trimmed().isEmpty()) {
                event.webUrl = owaCalUrl;
            }
        }
    }

    for (const auto& event : events)
        cacheEvent(event);

    qInfo().noquote()
        << QStringLiteral("[integrations][outlook] fetched %1 upcoming events via EWS")
               .arg(QString::number(events.size()));
    return events;
}

void LocalOutlookAdapter::cacheMail(const OutlookMailResource& resource)
{
    const ResourceRefPayload payload = OutlookAdapterContracts::makeMailPayload(resource);
    m_payloadCache.insert(resource.resourceId.trimmed(), payload);
    m_referenceCache.insert(resource.resourceId.trimmed(),
                            OutlookAdapterContracts::makeMailReference(resource));
}

void LocalOutlookAdapter::cacheEvent(const OutlookCalendarEventResource& resource)
{
    const ResourceRefPayload payload = OutlookAdapterContracts::makeCalendarEventPayload(resource);
    m_payloadCache.insert(resource.resourceId.trimmed(), payload);
    m_referenceCache.insert(resource.resourceId.trimmed(),
                            OutlookAdapterContracts::makeCalendarEventReference(resource));
}
