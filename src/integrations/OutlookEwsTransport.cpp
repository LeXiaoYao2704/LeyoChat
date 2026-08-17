#include "integrations/OutlookEwsTransport.h"

#include <QAuthenticator>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSslError>
#include <QTimer>
#include <QXmlStreamReader>
#include <QDebug>

#include "integrations/SyncNetworkReply.h"

// ──────────────────────────────────────────────────────────────────────────
// NetworkOutlookEwsTransport
// ──────────────────────────────────────────────────────────────────────────

std::optional<QByteArray> NetworkOutlookEwsTransport::soapPost(
    const QUrl& endpoint,
    const QByteArray& envelope,
    const OutlookConnectionSettings& settings,
    QString* errorMessage) const
{
    QNetworkAccessManager manager;
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("text/xml; charset=utf-8"));
    request.setRawHeader("SOAPAction", "\"\"");

    // Try HTTP Basic Auth first; Exchange also accepts it over HTTPS.
    const QString credentials = settings.username.trimmed() + QLatin1Char(':') + settings.password;
    request.setRawHeader("Authorization",
                         QByteArray("Basic ") + credentials.toUtf8().toBase64());

    QPointer<QNetworkReply> reply(manager.post(request, envelope));

    // Support NTLM / Negotiate if the server rejects Basic (authenticationRequired fires).
    QObject::connect(&manager, &QNetworkAccessManager::authenticationRequired, &manager,
        [&settings](QNetworkReply*, QAuthenticator* auth) {
            auth->setUser(settings.username.trimmed());
            auth->setPassword(settings.password);
        });

    // Ignore self-signed / private-CA certificates (on-prem Exchange common case).
    QObject::connect(reply, &QNetworkReply::sslErrors, reply.data(),
        [reply](const QList<QSslError>&) { if (reply) reply->ignoreSslErrors(); });

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(30000);
    loop.exec();

    if (!reply) {
        if (errorMessage) *errorMessage = QStringLiteral("Outlook EWS 请求对象提前释放");
        return std::nullopt;
    }
    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else {
        reply->abort();
        if (errorMessage) *errorMessage = QStringLiteral("Outlook EWS 请求超时");
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = body.trimmed().isEmpty()
                ? reply->errorString()
                : QString::fromUtf8(body).trimmed();
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    deleteSynchronousNetworkReply(reply);
    return body;
}

// ──────────────────────────────────────────────────────────────────────────
// OutlookEws namespace — builders and parsers
// ──────────────────────────────────────────────────────────────────────────

namespace OutlookEws {

QUrl ewsEndpoint(const OutlookConnectionSettings& settings)
{
    QString base = settings.serverUrl.trimmed();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    if (!base.endsWith(QStringLiteral("/EWS/Exchange.asmx"), Qt::CaseInsensitive)) {
        base += QStringLiteral("/EWS/Exchange.asmx");
    }
    return QUrl(base);
}

QByteArray buildSoapEnvelope(const QByteArray& body)
{
    return QByteArrayLiteral(
               "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
               "<soap:Envelope"
               " xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\""
               " xmlns:t=\"http://schemas.microsoft.com/exchange/services/2006/types\""
               " xmlns:m=\"http://schemas.microsoft.com/exchange/services/2006/messages\">"
               "<soap:Header>"
                 "<t:RequestServerVersion Version=\"Exchange2010\"/>"
               "</soap:Header>"
               "<soap:Body>")
        + body
        + QByteArrayLiteral("</soap:Body></soap:Envelope>");
}

QByteArray findUnreadMailEnvelope(int maxItems)
{
    // EWS FindItem 对邮件文件夹仅支持 Traversal="Shallow"（不支持 Deep），
    // 使用 inbox 作为父文件夹搜索收件箱中的未读邮件。
    // 注意：Outlook 规则移入子文件夹的邮件无法通过单次 FindItem 获取，
    // 需要通过 streaming subscription 或 FindFolder+多次 FindItem 实现。
    const QByteArray body =
        QByteArrayLiteral(
            "<m:FindItem Traversal=\"Shallow\">"
            "<m:ItemShape>"
              "<t:BaseShape>Default</t:BaseShape>"
              "<t:AdditionalProperties>"
                "<t:FieldURI FieldURI=\"message:IsRead\"/>"
                "<t:FieldURI FieldURI=\"message:From\"/>"
              "</t:AdditionalProperties>"
            "</m:ItemShape>"
            "<m:IndexedPageItemView MaxEntriesReturned=\"")
        + QByteArray::number(qMax(1, maxItems))
        + QByteArrayLiteral("\" Offset=\"0\" BasePoint=\"Beginning\"/>"
            "<m:Restriction>"
              "<t:And>"
                "<t:IsEqualTo>"
                  "<t:FieldURI FieldURI=\"message:IsRead\"/>"
                  "<t:FieldURIOrConstant><t:Constant Value=\"false\"/></t:FieldURIOrConstant>"
                "</t:IsEqualTo>"
                "<t:IsNotEqualTo>"
                  "<t:FieldURI FieldURI=\"item:ItemClass\"/>"
                  "<t:FieldURIOrConstant><t:Constant Value=\"IPM.Schedule.Meeting.Request\"/></t:FieldURIOrConstant>"
                "</t:IsNotEqualTo>"
              "</t:And>"
            "</m:Restriction>"
            "<m:SortOrder>"
              "<t:FieldOrder Order=\"Descending\">"
                "<t:FieldURI FieldURI=\"item:DateTimeReceived\"/>"
              "</t:FieldOrder>"
            "</m:SortOrder>"
            "<m:ParentFolderIds>"
              "<t:DistinguishedFolderId Id=\"inbox\"/>"
            "</m:ParentFolderIds>"
            "</m:FindItem>");
    return buildSoapEnvelope(body);
}

QByteArray findInboxSubfoldersEnvelope()
{
    // FindFolder Traversal="Deep" 递归获取 inbox 下所有子文件夹 ID
    const QByteArray body = QByteArrayLiteral(
        "<m:FindFolder Traversal=\"Deep\">"
          "<m:FolderShape>"
            "<t:BaseShape>IdOnly</t:BaseShape>"
          "</m:FolderShape>"
          "<m:ParentFolderIds>"
            "<t:DistinguishedFolderId Id=\"inbox\"/>"
          "</m:ParentFolderIds>"
        "</m:FindFolder>");
    return buildSoapEnvelope(body);
}

QByteArray findUnreadMailInFoldersEnvelope(const QStringList& folderIds, int maxItems)
{
    // 在指定的多个文件夹中搜索未读邮件（Shallow traversal）
    QByteArray folderIdsPart;
    for (const QString& folderId : folderIds) {
        folderIdsPart += QByteArrayLiteral("<t:FolderId Id=\"")
                       + folderId.toUtf8()
                       + QByteArrayLiteral("\"/>");
    }

    const QByteArray body =
        QByteArrayLiteral(
            "<m:FindItem Traversal=\"Shallow\">"
            "<m:ItemShape>"
              "<t:BaseShape>Default</t:BaseShape>"
              "<t:AdditionalProperties>"
                "<t:FieldURI FieldURI=\"message:IsRead\"/>"
                "<t:FieldURI FieldURI=\"message:From\"/>"
              "</t:AdditionalProperties>"
            "</m:ItemShape>"
            "<m:IndexedPageItemView MaxEntriesReturned=\"")
        + QByteArray::number(qMax(1, maxItems))
        + QByteArrayLiteral("\" Offset=\"0\" BasePoint=\"Beginning\"/>"
            "<m:Restriction>"
              "<t:And>"
                "<t:IsEqualTo>"
                  "<t:FieldURI FieldURI=\"message:IsRead\"/>"
                  "<t:FieldURIOrConstant><t:Constant Value=\"false\"/></t:FieldURIOrConstant>"
                "</t:IsEqualTo>"
                "<t:IsNotEqualTo>"
                  "<t:FieldURI FieldURI=\"item:ItemClass\"/>"
                  "<t:FieldURIOrConstant><t:Constant Value=\"IPM.Schedule.Meeting.Request\"/></t:FieldURIOrConstant>"
                "</t:IsNotEqualTo>"
              "</t:And>"
            "</m:Restriction>"
            "<m:SortOrder>"
              "<t:FieldOrder Order=\"Descending\">"
                "<t:FieldURI FieldURI=\"item:DateTimeReceived\"/>"
              "</t:FieldOrder>"
            "</m:SortOrder>"
            "<m:ParentFolderIds>")
        + folderIdsPart
        + QByteArrayLiteral(
            "</m:ParentFolderIds>"
            "</m:FindItem>");
    return buildSoapEnvelope(body);
}

QByteArray findCalendarEnvelope(const QDateTime& startUtc, const QDateTime& endUtc)
{
    const QByteArray body =
        QByteArrayLiteral(
            "<m:FindItem Traversal=\"Shallow\">"
            "<m:ItemShape>"
              "<t:BaseShape>Default</t:BaseShape>"
              "<t:AdditionalProperties>"
                "<t:FieldURI FieldURI=\"calendar:Start\"/>"
                "<t:FieldURI FieldURI=\"calendar:End\"/>"
                "<t:FieldURI FieldURI=\"calendar:Location\"/>"
                "<t:FieldURI FieldURI=\"calendar:IsCancelled\"/>"
                "<t:FieldURI FieldURI=\"calendar:Organizer\"/>"
              "</t:AdditionalProperties>"
            "</m:ItemShape>"
            "<m:CalendarView MaxEntriesReturned=\"50\" StartDate=\"")
        + startUtc.toString(Qt::ISODate).toUtf8()
        + QByteArrayLiteral("\" EndDate=\"")
        + endUtc.toString(Qt::ISODate).toUtf8()
        + QByteArrayLiteral("\" />"
            "<m:ParentFolderIds>"
              "<t:DistinguishedFolderId Id=\"calendar\"/>"
            "</m:ParentFolderIds>"
            "</m:FindItem>");
    return buildSoapEnvelope(body);
}

namespace {
QByteArray xmlEscape(const QString& value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('&'),  QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'),  QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'),  QStringLiteral("&gt;"));
    escaped.replace(QLatin1Char('"'),  QStringLiteral("&quot;"));
    escaped.replace(QLatin1Char('\''), QStringLiteral("&apos;"));
    return escaped.toUtf8();
}
}  // namespace

QByteArray resolveNamesEnvelope(const QString& nameOrEmail)
{
    const QByteArray body =
        QByteArrayLiteral("<m:ResolveNames ReturnFullContactData=\"false\">"
                          "<m:UnresolvedEntry>")
        + xmlEscape(nameOrEmail)
        + QByteArrayLiteral("</m:UnresolvedEntry></m:ResolveNames>");
    return buildSoapEnvelope(body);
}

QByteArray getFolderInboxEnvelope()
{
    return buildSoapEnvelope(QByteArrayLiteral(
        "<m:GetFolder>"
        "<m:FolderShape><t:BaseShape>Default</t:BaseShape></m:FolderShape>"
        "<m:FolderIds><t:DistinguishedFolderId Id=\"inbox\"/></m:FolderIds>"
        "</m:GetFolder>"));
}

QByteArray getItemBodyEnvelope(const QStringList& itemIds)
{
    QByteArray ids;
    for (const auto& id : itemIds) {
        ids += QByteArrayLiteral("<t:ItemId Id=\"")
               + id.toUtf8()
               + QByteArrayLiteral("\"/>");
    }
    const QByteArray body =
        QByteArrayLiteral(
            "<m:GetItem>"
            "<m:ItemShape>"
              "<t:BaseShape>IdOnly</t:BaseShape>"
              "<t:BodyType>HTML</t:BodyType>"
              "<t:AdditionalProperties>"
                "<t:FieldURI FieldURI=\"item:Body\"/>"
              "</t:AdditionalProperties>"
            "</m:ItemShape>"
            "<m:ItemIds>")
        + ids
        + QByteArrayLiteral("</m:ItemIds></m:GetItem>");
    return buildSoapEnvelope(body);
}

QHash<QString, QString> parseGetItemBodyResponse(const QByteArray& xml)
{
    QHash<QString, QString> result;
    QXmlStreamReader reader(xml);
    QString currentItemId;
    bool inItem = false;

    while (!reader.atEnd() && !reader.hasError()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::StartElement) {
            const QString name = reader.name().toString();
            if (name == QStringLiteral("Message") || name == QStringLiteral("Item")) {
                inItem = true;
                currentItemId.clear();
            } else if (inItem && name == QStringLiteral("ItemId")) {
                currentItemId = reader.attributes().value(QStringLiteral("Id")).toString().trimmed();
            } else if (inItem && name == QStringLiteral("Body")) {
                const QString bodyContent = reader.readElementText().trimmed();
                if (!currentItemId.isEmpty() && !bodyContent.isEmpty()) {
                    result.insert(currentItemId, bodyContent);
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            const QString name = reader.name().toString();
            if (name == QStringLiteral("Message") || name == QStringLiteral("Item")) {
                inItem = false;
            }
        }
    }
    return result;
}

bool isSoapResponseOk(const QByteArray& xml, QString* errorMessage)
{
    QXmlStreamReader reader(xml);
    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNext();
        if (!reader.isStartElement()) continue;
        const QString name = reader.name().toString();
        if (name == QStringLiteral("Fault")) {
            // SOAP Fault — look for faultstring
            while (!reader.atEnd() && !reader.hasError()) {
                reader.readNext();
                if (reader.isStartElement()
                    && reader.name().toString() == QStringLiteral("faultstring")) {
                    if (errorMessage)
                        *errorMessage = reader.readElementText().trimmed();
                    return false;
                }
                if (reader.isEndElement()
                    && reader.name().toString() == QStringLiteral("Fault"))
                    break;
            }
            if (errorMessage && errorMessage->isEmpty())
                *errorMessage = QStringLiteral("Outlook EWS 返回 SOAP Fault");
            return false;
        }
        if (name == QStringLiteral("ResponseCode")) {
            const QString code = reader.readElementText().trimmed();
            if (code != QStringLiteral("NoError")) {
                if (errorMessage)
                    *errorMessage = code;
                return false;
            }
            return true;
        }
    }
    // No ResponseCode found — treat as ok (envelope is valid)
    return true;
}

QStringList parseFindFolderResponse(const QByteArray& xml)
{
    QStringList folderIds;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd() && !reader.hasError()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (reader.name() == QStringLiteral("FolderId")) {
                const QString id = reader.attributes().value(QStringLiteral("Id")).toString().trimmed();
                if (!id.isEmpty())
                    folderIds.push_back(id);
            }
        }
    }
    return folderIds;
}

QVector<OutlookMailResource> parseMailFindItemResponse(const QByteArray& xml,
                                                       const QString& accountEmail)
{
    QVector<OutlookMailResource> result;
    QXmlStreamReader reader(xml);

    OutlookMailResource current;
    bool inMessage = false;
    bool inFrom = false;

    while (!reader.atEnd() && !reader.hasError()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::StartElement) {
            const QString name = reader.name().toString();
            if (name == QStringLiteral("Message")) {
                inMessage = true;
                inFrom = false;
                current = OutlookMailResource{};
                current.serviceId = QStringLiteral("local-outlook");
                current.workspaceId = QStringLiteral("local-outlook");
                current.mailbox = accountEmail;
            } else if (inMessage) {
                if (name == QStringLiteral("ItemId")) {
                    current.resourceId =
                        reader.attributes().value(QStringLiteral("Id")).toString().trimmed();
                } else if (name == QStringLiteral("Subject")) {
                    current.subject = reader.readElementText().trimmed();
                } else if (name == QStringLiteral("DateTimeReceived")) {
                    const QString raw = reader.readElementText().trimmed();
                    QDateTime dt = QDateTime::fromString(raw, Qt::ISODate);
                    if (!dt.isValid())
                        dt = QDateTime::fromString(raw, Qt::ISODateWithMs);
                    if (dt.isValid())
                        current.receivedAtLabel =
                            dt.toLocalTime().toString(QStringLiteral("MM-dd HH:mm"));
                } else if (name == QStringLiteral("From")) {
                    inFrom = true;
                } else if (inFrom && name == QStringLiteral("Name")) {
                    current.sender = reader.readElementText().trimmed();
                } else if (name == QStringLiteral("Preview") || name == QStringLiteral("Body")) {
                    if (current.bodyPreview.isEmpty())
                        current.bodyPreview = reader.readElementText().trimmed().left(200);
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            const QString name = reader.name().toString();
            if (name == QStringLiteral("Message") && inMessage) {
                inMessage = false;
                inFrom = false;
                if (!current.resourceId.trimmed().isEmpty()) {
                    if (current.subject.isEmpty())
                        current.subject = QStringLiteral("未命名邮件");
                    if (current.sender.isEmpty())
                        current.sender = QStringLiteral("Outlook");
                    result.push_back(current);
                }
            } else if (name == QStringLiteral("From")) {
                inFrom = false;
            }
        }
    }
    return result;
}

QVector<OutlookCalendarEventResource> parseCalendarFindItemResponse(const QByteArray& xml)
{
    QVector<OutlookCalendarEventResource> result;
    QXmlStreamReader reader(xml);

    OutlookCalendarEventResource current;
    bool inCalItem = false;
    bool inOrganizer = false;

    while (!reader.atEnd() && !reader.hasError()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::StartElement) {
            const QString name = reader.name().toString();
            if (name == QStringLiteral("CalendarItem")) {
                inCalItem = true;
                inOrganizer = false;
                current = OutlookCalendarEventResource{};
                current.serviceId = QStringLiteral("local-outlook");
                current.workspaceId = QStringLiteral("local-outlook");
            } else if (inCalItem) {
                if (name == QStringLiteral("ItemId")) {
                    current.resourceId =
                        reader.attributes().value(QStringLiteral("Id")).toString().trimmed();
                    current.changeKey =
                        reader.attributes().value(QStringLiteral("ChangeKey")).toString().trimmed();
                } else if (name == QStringLiteral("Subject")) {
                    current.subject = reader.readElementText().trimmed();
                } else if (name == QStringLiteral("Start")) {
                    const QString raw = reader.readElementText().trimmed();
                    QDateTime dt = QDateTime::fromString(raw, Qt::ISODate);
                    if (!dt.isValid())
                        dt = QDateTime::fromString(raw, Qt::ISODateWithMs);
                    if (dt.isValid())
                        current.whenLabel =
                            dt.toLocalTime().toString(QStringLiteral("MM-dd HH:mm"));
                } else if (name == QStringLiteral("Location")) {
                    current.location = reader.readElementText().trimmed();
                } else if (name == QStringLiteral("IsCancelled")) {
                    current.cancelled =
                        reader.readElementText().trimmed() == QStringLiteral("true");
                } else if (name == QStringLiteral("Organizer")) {
                    inOrganizer = true;
                } else if (inOrganizer && name == QStringLiteral("Name")) {
                    current.organizer = reader.readElementText().trimmed();
                    inOrganizer = false;
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            const QString name = reader.name().toString();
            if (name == QStringLiteral("CalendarItem") && inCalItem) {
                inCalItem = false;
                inOrganizer = false;
                if (!current.resourceId.trimmed().isEmpty()) {
                    if (current.subject.isEmpty())
                        current.subject = QStringLiteral("未命名会议");
                    if (current.organizer.isEmpty())
                        current.organizer = QStringLiteral("Outlook Calendar");
                    result.push_back(current);
                }
            } else if (name == QStringLiteral("Organizer")) {
                inOrganizer = false;
            }
        }
    }
    return result;
}

std::optional<ResolvedUser> parseResolveNamesResponse(const QByteArray& xml)
{
    QXmlStreamReader reader(xml);
    ResolvedUser user;
    bool inResolution = false;
    bool inMailbox = false;

    while (!reader.atEnd() && !reader.hasError()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::StartElement) {
            const QString name = reader.name().toString();
            if (name == QStringLiteral("Resolution"))
                inResolution = true;
            else if (inResolution && name == QStringLiteral("Mailbox"))
                inMailbox = true;
            else if (inMailbox) {
                if (name == QStringLiteral("Name"))
                    user.displayName = reader.readElementText().trimmed();
                else if (name == QStringLiteral("EmailAddress"))
                    user.email = reader.readElementText().trimmed();
            }
        } else if (token == QXmlStreamReader::EndElement) {
            const QString name = reader.name().toString();
            if (name == QStringLiteral("Resolution") && inResolution) {
                if (!user.email.isEmpty())
                    return user;
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

}  // namespace OutlookEws
