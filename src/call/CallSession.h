#pragma once

#include "domain/CallProtocol.h"

#include <QObject>
#include <QString>
#include <QTimer>

class CallSession : public QObject {
    Q_OBJECT

public:
    enum class State {
        Idle,
        OutgoingRing,
        IncomingRing,
        Connecting,
        Active,
        Ended
    };
    Q_ENUM(State)

    explicit CallSession(QObject* parent = nullptr);

    State state() const;
    QString callId() const;
    QString peerId() const;
    int mediaFlags() const;
    bool isAudioMuted() const;
    bool isScreenSharing() const;
    bool isRemoteControlActive() const;

    bool startCall(const QString& callId, const QString& peerId, int mediaFlags);
    bool handleIncomingOffer(const QString& callId, const QString& peerId, int mediaFlags);
    void answerCall();
    void rejectCall(const QString& reason = QString());
    void cancelCall();
    void hangup();

    void setAudioMuted(bool muted);
    void startScreenShare();
    void stopScreenShare();
    void requestRemoteControl();
    void grantRemoteControl();
    void revokeRemoteControl();

    void handleCallControl(const CallControlPayload& payload);

signals:
    void stateChanged(CallSession::State state);
    void outgoingSignal(const CallControlPayload& payload);
    void audioMutedChanged(bool muted);
    void screenShareChanged(bool sharing);
    void remoteControlChanged(bool enabled);
    void callError(const QString& message);
    void callEnded(const QString& callId, const QString& peerId,
                   const QString& result, qint64 durationMs);
    void mediaChannelReady(quint16 audioUdpPort, quint16 screenTcpPort);
    void remoteControlRequested(const QString& peerId);

private:
    void setState(State state);
    void cleanup();

    State m_state = State::Idle;
    QString m_callId;
    QString m_peerId;
    int m_mediaFlags = 0;
    bool m_audioMuted = false;
    bool m_screenSharing = false;
    bool m_remoteControlActive = false;
    qint64 m_activeStartMs = 0;
    QTimer m_ringTimer;
};
