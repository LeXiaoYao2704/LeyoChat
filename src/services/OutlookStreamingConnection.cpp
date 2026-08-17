#include "services/OutlookStreamingConnection.h"

#include <QAuthenticator>
#include <QDebug>
#include <QNetworkRequest>
#include <QSslError>
#include <QUrl>
#include <QXmlStreamReader>

// ──────────────────────────────────────────────────────────────────────────
// Lifecycle
// ──────────────────────────────────────────────────────────────────────────

OutlookStreamingConnection::OutlookStreamingConnection(QObject* parent)
    : QObject(parent)
{
    m_reconnectTimer.setSingleShot(true);
    QObject::connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_running) {
            sendSubscribeRequest();
        }
    });

    // Support NTLM / Negotiate if the server rejects Basic Auth.
    QObject::connect(&m_networkManager, &QNetworkAccessManager::authenticationRequired,
                     this, [this](QNetworkReply*, QAuthenticator* auth) {
        auth->setUser(m_settings.username.trimmed());
        auth->setPassword(m_settings.password);
    });
}

OutlookStreamingConnection::~OutlookStreamingConnection()
{
    stop();
}

void OutlookStreamingConnection::start(const OutlookConnectionSettings& settings)
{
    stop();
    m_settings = settings;
    m_running = true;
    m_consecutiveFailures = 0;
    m_subscriptionId.clear();

    qInfo().noquote() << QStringLiteral("[outlook-streaming] starting for %1").arg(m_settings.serverUrl);
    qInfo().noquote() << QStringLiteral("[outlook-streaming] about to sendSubscribeRequest");
    sendSubscribeRequest();
    qInfo().noquote() << QStringLiteral("[outlook-streaming] sendSubscribeRequest returned");
}

void OutlookStreamingConnection::stop()
{
    m_running = false;
    m_reconnectTimer.stop();
    teardown();
    m_subscriptionId.clear();
    m_consecutiveFailures = 0;
    qInfo().noquote() << QStringLiteral("[outlook-streaming] stopped");
}

bool OutlookStreamingConnection::isRunning() const
{
    return m_running;
}

void OutlookStreamingConnection::teardown()
{
    if (m_activeReply) {
        // 先断开所有信号，防止 abort() 过程中 SChannel/NTLM 回调触发
        // 导致 Qt6Network.dll 内部悬空指针访问（0xc0000005）
        m_activeReply->disconnect(this);
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────────
// SOAP envelope builders
// ──────────────────────────────────────────────────────────────────────────

void OutlookStreamingConnection::configureRequest(QNetworkRequest& request) const
{
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("text/xml; charset=utf-8"));
    request.setRawHeader("SOAPAction", "\"\"");

    // 不设置 Basic Auth 头——让 Qt 的 authenticationRequired 信号驱动
    // NTLM/Negotiate 多步握手。主动发 Basic 头会被 Exchange 直接拒绝，
    // 导致 Qt 不再触发 NTLM 协商流程。
}

QByteArray OutlookStreamingConnection::buildSubscribeEnvelope() const
{
    // Subscribe to Inbox (NewMail) and Calendar (Created, Modified, Deleted)
    // using EWS Streaming subscription.
    return QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<soap:Envelope"
        " xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\""
        " xmlns:t=\"http://schemas.microsoft.com/exchange/services/2006/types\""
        " xmlns:m=\"http://schemas.microsoft.com/exchange/services/2006/messages\">"
        "<soap:Header>"
          "<t:RequestServerVersion Version=\"Exchange2010_SP1\"/>"
        "</soap:Header>"
        "<soap:Body>"
        "<m:Subscribe>"
        "<m:StreamingSubscriptionRequest>"
        "<t:FolderIds>"
        "<t:DistinguishedFolderId Id=\"inbox\"/>"
        "<t:DistinguishedFolderId Id=\"calendar\"/>"
        "</t:FolderIds>"
        "<t:EventTypes>"
        "<t:EventType>NewMailEvent</t:EventType>"
        "<t:EventType>CreatedEvent</t:EventType>"
        "<t:EventType>ModifiedEvent</t:EventType>"
        "<t:EventType>DeletedEvent</t:EventType>"
        "</t:EventTypes>"
        "</m:StreamingSubscriptionRequest>"
        "</m:Subscribe>"
        "</soap:Body></soap:Envelope>");
}

QByteArray OutlookStreamingConnection::buildGetStreamingEventsEnvelope() const
{
    // GetStreamingEvents keeps the connection open for up to kStreamingTimeoutMinutes.
    // Exchange pushes events down this connection in real-time.
    const QByteArray timeoutStr = QByteArray::number(kStreamingTimeoutMinutes);
    return QByteArrayLiteral(
               "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
               "<soap:Envelope"
               " xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\""
               " xmlns:t=\"http://schemas.microsoft.com/exchange/services/2006/types\""
               " xmlns:m=\"http://schemas.microsoft.com/exchange/services/2006/messages\">"
               "<soap:Header>"
                 "<t:RequestServerVersion Version=\"Exchange2010_SP1\"/>"
               "</soap:Header>"
               "<soap:Body>"
               "<m:GetStreamingEvents>"
               "<m:SubscriptionIds>"
               "<t:SubscriptionId>")
        + m_subscriptionId.toUtf8()
        + QByteArrayLiteral(
              "</t:SubscriptionId>"
              "</m:SubscriptionIds>"
              "<m:ConnectionTimeout>")
        + timeoutStr
        + QByteArrayLiteral(
              "</m:ConnectionTimeout>"
              "</m:GetStreamingEvents>"
              "</soap:Body></soap:Envelope>");
}

// ──────────────────────────────────────────────────────────────────────────
// Network operations
// ──────────────────────────────────────────────────────────────────────────

void OutlookStreamingConnection::sendSubscribeRequest()
{
    teardown();
    if (!m_running) {
        return;
    }

    QString base = m_settings.serverUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    if (!base.endsWith(QStringLiteral("/EWS/Exchange.asmx"), Qt::CaseInsensitive)) {
        base += QStringLiteral("/EWS/Exchange.asmx");
    }
    const QUrl endpoint(base);

    QNetworkRequest request(endpoint);
    configureRequest(request);

    m_activeReply = m_networkManager.post(request, buildSubscribeEnvelope());

    // Ignore self-signed / private-CA certificates (on-prem Exchange).
    QObject::connect(m_activeReply, &QNetworkReply::sslErrors, m_activeReply,
                     [this](const QList<QSslError>&) {
        if (m_activeReply) {
            m_activeReply->ignoreSslErrors();
        }
    });

    QObject::connect(m_activeReply, &QNetworkReply::finished,
                     this, &OutlookStreamingConnection::onSubscribeFinished);

    qInfo().noquote() << QStringLiteral("[outlook-streaming] subscribe request sent");
}

void OutlookStreamingConnection::onSubscribeFinished()
{
    if (!m_activeReply || !m_running) {
        return;
    }

    const QByteArray body = m_activeReply->readAll();
    const bool networkError = m_activeReply->error() != QNetworkReply::NoError;
    const QString networkErrorString = m_activeReply->errorString();
    m_activeReply->deleteLater();
    m_activeReply = nullptr;

    if (networkError) {
        qWarning().noquote() << QStringLiteral("[outlook-streaming] subscribe failed: %1")
                                    .arg(networkErrorString);
        emit streamingError(networkErrorString);
        scheduleReconnect();
        return;
    }

    m_subscriptionId = parseSubscriptionId(body);
    if (m_subscriptionId.isEmpty()) {
        qWarning().noquote() << QStringLiteral("[outlook-streaming] subscribe response missing SubscriptionId");
        emit streamingError(QStringLiteral("EWS Subscribe 返回无效的 SubscriptionId"));
        scheduleReconnect();
        return;
    }

    qInfo().noquote() << QStringLiteral("[outlook-streaming] subscribed: %1").arg(m_subscriptionId);
    m_consecutiveFailures = 0;
    sendGetStreamingEventsRequest();
}

void OutlookStreamingConnection::sendGetStreamingEventsRequest()
{
    teardown();
    if (!m_running || m_subscriptionId.isEmpty()) {
        return;
    }

    QString base = m_settings.serverUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    if (!base.endsWith(QStringLiteral("/EWS/Exchange.asmx"), Qt::CaseInsensitive)) {
        base += QStringLiteral("/EWS/Exchange.asmx");
    }
    const QUrl endpoint(base);

    QNetworkRequest request(endpoint);
    configureRequest(request);
    // Allow the reply to transfer data for up to 35 minutes (30 min + 5 min margin).
    request.setTransferTimeout((kStreamingTimeoutMinutes + 5) * 60 * 1000);

    m_activeReply = m_networkManager.post(request, buildGetStreamingEventsEnvelope());

    QObject::connect(m_activeReply, &QNetworkReply::sslErrors, m_activeReply,
                     [this](const QList<QSslError>&) {
        if (m_activeReply) {
            m_activeReply->ignoreSslErrors();
        }
    });

    QObject::connect(m_activeReply, &QNetworkReply::finished,
                     this, &OutlookStreamingConnection::onGetStreamingEventsFinished);

    qInfo().noquote() << QStringLiteral("[outlook-streaming] GetStreamingEvents sent (timeout=%1min)")
                             .arg(kStreamingTimeoutMinutes);
}

void OutlookStreamingConnection::onGetStreamingEventsFinished()
{
    if (!m_activeReply || !m_running) {
        return;
    }

    const QByteArray body = m_activeReply->readAll();
    const bool networkError = m_activeReply->error() != QNetworkReply::NoError;
    const QString networkErrorString = m_activeReply->errorString();
    m_activeReply->deleteLater();
    m_activeReply = nullptr;

    if (networkError && body.isEmpty()) {
        qWarning().noquote() << QStringLiteral("[outlook-streaming] GetStreamingEvents failed: %1")
                                    .arg(networkErrorString);
        emit streamingError(networkErrorString);
        scheduleReconnect();
        return;
    }

    // Parse the response — any events in the body trigger a notification.
    const bool hasEvents = parseStreamingEvents(body);
    if (hasEvents) {
        qInfo().noquote() << QStringLiteral("[outlook-streaming] events received, triggering poll");
        emit streamingEventReceived();
    }

    // GetStreamingEvents returns normally when the connection timeout expires
    // or when events are delivered. In either case, re-subscribe.
    // If the subscription is still valid, we can re-issue GetStreamingEvents directly.
    if (m_running) {
        m_consecutiveFailures = 0;
        // Re-open the streaming connection immediately.
        sendGetStreamingEventsRequest();
    }
}

void OutlookStreamingConnection::scheduleReconnect()
{
    if (!m_running) {
        return;
    }
    ++m_consecutiveFailures;

    // 连续失败过多说明认证配置有问题，停止重试避免无限循环浪费资源
    if (m_consecutiveFailures > kMaxConsecutiveFailures) {
        qWarning().noquote() << QStringLiteral("[outlook-streaming] %1 consecutive failures, giving up (check EWS credentials)")
                                    .arg(m_consecutiveFailures);
        emit streamingError(QStringLiteral("Outlook Streaming 连续失败 %1 次已停止，请检查 EWS 账号/密码配置")
                                .arg(m_consecutiveFailures));
        m_running = false;
        return;
    }

    const int delay = qMin(kMaxReconnectDelayMs,
                           kBaseReconnectDelayMs * (1 << qMin(m_consecutiveFailures - 1, 5)));
    qInfo().noquote() << QStringLiteral("[outlook-streaming] reconnect in %1ms (failures=%2/%3)")
                             .arg(delay).arg(m_consecutiveFailures).arg(kMaxConsecutiveFailures);

    // After multiple failures, re-subscribe from scratch.
    m_subscriptionId.clear();
    m_reconnectTimer.start(delay);
}

// ──────────────────────────────────────────────────────────────────────────
// XML parsers
// ──────────────────────────────────────────────────────────────────────────

QString OutlookStreamingConnection::parseSubscriptionId(const QByteArray& xml) const
{
    // Look for <SubscriptionId>...</SubscriptionId> in the Subscribe response.
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("SubscriptionId")) {
            return reader.readElementText().trimmed();
        }
    }
    return {};
}

bool OutlookStreamingConnection::parseStreamingEvents(const QByteArray& xml) const
{
    // Returns true if the response contains at least one notification event.
    // We look for any of: NewMailEvent, CreatedEvent, ModifiedEvent, DeletedEvent.
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            const auto name = reader.name();
            if (name == QStringLiteral("NewMailEvent")
                || name == QStringLiteral("CreatedEvent")
                || name == QStringLiteral("ModifiedEvent")
                || name == QStringLiteral("DeletedEvent")) {
                return true;
            }
        }
    }
    return false;
}
