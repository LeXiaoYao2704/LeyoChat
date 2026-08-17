#pragma once

#include <memory>

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include "integrations/OutlookNotificationContracts.h"
#include "integrations/OutlookSettings.h"

// ──────────────────────────────────────────────────────────────────────────
// OutlookStreamingConnection
//
// Maintains a long-lived EWS Streaming Notification subscription.
// Flow:
//   1. Subscribe to Inbox (NewMail) + Calendar (Created/Modified/Deleted)
//   2. GetStreamingEvents with a 30-minute connection timeout
//   3. When events arrive → emit outlookStreamingEventsReceived()
//   4. On disconnect/timeout → automatically reconnect
//
// Lives on the main thread; HTTP I/O is non-blocking (QNetworkReply signals).
// ──────────────────────────────────────────────────────────────────────────

class OutlookStreamingConnection : public QObject {
    Q_OBJECT

public:
    explicit OutlookStreamingConnection(QObject* parent = nullptr);
    ~OutlookStreamingConnection() override;

    // Start streaming with the given settings.
    // If already running, tears down the old connection and restarts.
    void start(const OutlookConnectionSettings& settings);

    // Gracefully stop the streaming connection.
    void stop();

    bool isRunning() const;

signals:
    // Emitted when the streaming connection receives new-mail or calendar events.
    // The caller should fetch full item details (via existing poll path) and dispatch.
    void streamingEventReceived();

    // Emitted when the streaming connection encounters a fatal error.
    void streamingError(const QString& errorMessage);

private slots:
    void onSubscribeFinished();
    void onGetStreamingEventsFinished();

private:
    void teardown();
    void sendSubscribeRequest();
    void sendGetStreamingEventsRequest();
    void scheduleReconnect();

    QByteArray buildSubscribeEnvelope() const;
    QByteArray buildGetStreamingEventsEnvelope() const;
    QString parseSubscriptionId(const QByteArray& xml) const;
    bool parseStreamingEvents(const QByteArray& xml) const;
    void configureRequest(QNetworkRequest& request) const;

    OutlookConnectionSettings m_settings;
    QNetworkAccessManager m_networkManager;
    QPointer<QNetworkReply> m_activeReply;
    QTimer m_reconnectTimer;

    QString m_subscriptionId;
    bool m_running = false;
    int m_consecutiveFailures = 0;

    static constexpr int kStreamingTimeoutMinutes = 30;
    static constexpr int kBaseReconnectDelayMs = 3000;
    static constexpr int kMaxReconnectDelayMs = 5 * 60 * 1000;
    static constexpr int kMaxConsecutiveFailures = 10;  // 连续失败超过此数则自动停止
};
