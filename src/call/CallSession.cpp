#include "call/CallSession.h"

#include <QDateTime>

namespace {
constexpr int kRingTimeoutMs = 30000;
}

CallSession::CallSession(QObject* parent)
    : QObject(parent)
{
    m_ringTimer.setSingleShot(true);
    connect(&m_ringTimer, &QTimer::timeout, this, [this]() {
        if (m_state == State::OutgoingRing) {
            cancelCall();
        } else if (m_state == State::IncomingRing) {
            rejectCall(QStringLiteral("timeout"));
        }
    });
}

CallSession::State CallSession::state() const { return m_state; }
QString CallSession::callId() const { return m_callId; }
QString CallSession::peerId() const { return m_peerId; }
int CallSession::mediaFlags() const { return m_mediaFlags; }
bool CallSession::isAudioMuted() const { return m_audioMuted; }
bool CallSession::isScreenSharing() const { return m_screenSharing; }
bool CallSession::isRemoteControlActive() const { return m_remoteControlActive; }

void CallSession::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    if (state == State::Active) {
        m_activeStartMs = QDateTime::currentMSecsSinceEpoch();
    }
    emit stateChanged(m_state);
}

void CallSession::cleanup()
{
    m_ringTimer.stop();
    m_callId.clear();
    m_peerId.clear();
    m_mediaFlags = 0;
    m_audioMuted = false;
    m_screenSharing = false;
    m_remoteControlActive = false;
    m_activeStartMs = 0;
}

bool CallSession::startCall(const QString& callId, const QString& peerId, int mediaFlags)
{
    if (m_state != State::Idle) {
        return false;
    }

    m_callId = callId;
    m_peerId = peerId;
    m_mediaFlags = mediaFlags;
    setState(State::OutgoingRing);
    m_ringTimer.start(kRingTimeoutMs);

    CallControlPayload payload;
    payload.type = CallControlType::Offer;
    payload.callId = callId.toStdString();
    payload.targetId = peerId.toStdString();
    payload.mediaFlags = mediaFlags;
    emit outgoingSignal(payload);
    return true;
}

bool CallSession::handleIncomingOffer(const QString& callId, const QString& peerId, int mediaFlags)
{
    if (m_state != State::Idle) {
        CallControlPayload busyPayload;
        busyPayload.type = CallControlType::Busy;
        busyPayload.callId = callId.toStdString();
        busyPayload.targetId = peerId.toStdString();
        emit outgoingSignal(busyPayload);
        return false;
    }

    m_callId = callId;
    m_peerId = peerId;
    m_mediaFlags = mediaFlags;
    setState(State::IncomingRing);
    m_ringTimer.start(kRingTimeoutMs);
    return true;
}

void CallSession::answerCall()
{
    if (m_state != State::IncomingRing) {
        return;
    }

    m_ringTimer.stop();
    CallControlPayload payload;
    payload.type = CallControlType::Answer;
    payload.callId = m_callId.toStdString();
    payload.targetId = m_peerId.toStdString();
    emit outgoingSignal(payload);
    setState(State::Connecting);
}

void CallSession::rejectCall(const QString& reason)
{
    if (m_state != State::IncomingRing) {
        return;
    }

    m_ringTimer.stop();
    CallControlPayload payload;
    payload.type = CallControlType::Reject;
    payload.callId = m_callId.toStdString();
    payload.targetId = m_peerId.toStdString();
    payload.reason = reason.toStdString();
    emit outgoingSignal(payload);

    setState(State::Ended);
    cleanup();
    setState(State::Idle);
}

void CallSession::cancelCall()
{
    if (m_state != State::OutgoingRing) {
        return;
    }

    m_ringTimer.stop();

    const bool wasTimeout = !m_ringTimer.isActive() && m_state == State::OutgoingRing;
    const QString reason = wasTimeout ? QStringLiteral("no_answer") : QStringLiteral("cancelled");
    const QString savedCallId = m_callId;
    const QString savedPeerId = m_peerId;

    CallControlPayload payload;
    payload.type = CallControlType::Cancel;
    payload.callId = m_callId.toStdString();
    payload.targetId = m_peerId.toStdString();
    emit outgoingSignal(payload);

    emit callError(reason);
    emit callEnded(savedCallId, savedPeerId, reason, 0);
    setState(State::Ended);
    cleanup();
    setState(State::Idle);
}

void CallSession::hangup()
{
    if (m_state != State::Active && m_state != State::Connecting) {
        return;
    }

    const QString savedCallId = m_callId;
    const QString savedPeerId = m_peerId;
    const qint64 duration = m_activeStartMs > 0
        ? QDateTime::currentMSecsSinceEpoch() - m_activeStartMs : 0;

    CallControlPayload payload;
    payload.type = CallControlType::Hangup;
    payload.callId = m_callId.toStdString();
    payload.targetId = m_peerId.toStdString();
    emit outgoingSignal(payload);

    emit callEnded(savedCallId, savedPeerId, QStringLiteral("completed"), duration);
    setState(State::Ended);
    cleanup();
    setState(State::Idle);
}

void CallSession::setAudioMuted(bool muted)
{
    if (m_audioMuted == muted) {
        return;
    }
    m_audioMuted = muted;
    emit audioMutedChanged(muted);
}

void CallSession::startScreenShare()
{
    if (m_state != State::Active || m_screenSharing) {
        return;
    }

    m_screenSharing = true;
    // 注意：不在此处 emit outgoingSignal，由 LeyoApplication
    // 在 screenShareToggled 处理中统一发送（携带 TCP 端口信息）
    emit screenShareChanged(true);
}

void CallSession::stopScreenShare()
{
    if (!m_screenSharing) {
        return;
    }

    m_screenSharing = false;
    m_remoteControlActive = false;
    CallControlPayload payload;
    payload.type = CallControlType::ScreenShareStop;
    payload.callId = m_callId.toStdString();
    payload.targetId = m_peerId.toStdString();
    emit outgoingSignal(payload);
    emit screenShareChanged(false);
    emit remoteControlChanged(false);
}

void CallSession::requestRemoteControl()
{
    if (m_state != State::Active) {
        return;
    }
    CallControlPayload payload;
    payload.type = CallControlType::RemoteControlRequest;
    payload.callId = m_callId.toStdString();
    payload.targetId = m_peerId.toStdString();
    emit outgoingSignal(payload);
}

void CallSession::grantRemoteControl()
{
    if (m_state != State::Active || !m_screenSharing) {
        return;
    }
    m_remoteControlActive = true;
    CallControlPayload payload;
    payload.type = CallControlType::RemoteControlGrant;
    payload.callId = m_callId.toStdString();
    payload.targetId = m_peerId.toStdString();
    emit outgoingSignal(payload);
    emit remoteControlChanged(true);
}

void CallSession::revokeRemoteControl()
{
    if (!m_remoteControlActive) {
        return;
    }
    m_remoteControlActive = false;
    CallControlPayload payload;
    payload.type = CallControlType::RemoteControlRevoke;
    payload.callId = m_callId.toStdString();
    payload.targetId = m_peerId.toStdString();
    emit outgoingSignal(payload);
    emit remoteControlChanged(false);
}

void CallSession::handleCallControl(const CallControlPayload& payload)
{
    const QString payloadCallId = QString::fromStdString(payload.callId);
    if (!payloadCallId.isEmpty() && !m_callId.isEmpty() && payloadCallId != m_callId) {
        return;
    }

    switch (payload.type) {
    case CallControlType::Offer: {
        const QString incomingCallId = QString::fromStdString(payload.callId);
        const QString incomingPeerId = QString::fromStdString(payload.senderId);
        handleIncomingOffer(incomingCallId, incomingPeerId, payload.mediaFlags);
        break;
    }
    case CallControlType::Answer:
        if (m_state == State::OutgoingRing) {
            m_ringTimer.stop();
            setState(State::Connecting);
        }
        break;
    case CallControlType::Reject:
        if (m_state == State::OutgoingRing) {
            m_ringTimer.stop();
            const QString savedCallId = m_callId;
            const QString savedPeerId = m_peerId;
            emit callError(QStringLiteral("peer_rejected"));
            emit callEnded(savedCallId, savedPeerId, QStringLiteral("rejected"), 0);
            setState(State::Ended);
            cleanup();
            setState(State::Idle);
        }
        break;
    case CallControlType::Busy:
        if (m_state == State::OutgoingRing) {
            m_ringTimer.stop();
            const QString savedCallId = m_callId;
            const QString savedPeerId = m_peerId;
            emit callError(QStringLiteral("peer_busy"));
            emit callEnded(savedCallId, savedPeerId, QStringLiteral("busy"), 0);
            setState(State::Ended);
            cleanup();
            setState(State::Idle);
        }
        break;
    case CallControlType::Cancel:
        if (m_state == State::IncomingRing) {
            m_ringTimer.stop();
            const QString savedCallId = m_callId;
            const QString savedPeerId = m_peerId;
            emit callEnded(savedCallId, savedPeerId, QStringLiteral("cancelled"), 0);
            setState(State::Ended);
            cleanup();
            setState(State::Idle);
        }
        break;
    case CallControlType::Hangup:
        if (m_state == State::Active || m_state == State::Connecting) {
            const QString savedCallId = m_callId;
            const QString savedPeerId = m_peerId;
            const qint64 duration = m_activeStartMs > 0
                ? QDateTime::currentMSecsSinceEpoch() - m_activeStartMs : 0;
            emit callEnded(savedCallId, savedPeerId, QStringLiteral("completed"), duration);
            setState(State::Ended);
            cleanup();
            setState(State::Idle);
        }
        break;
    case CallControlType::MediaReady:
        if (m_state == State::Connecting) {
            setState(State::Active);
            emit mediaChannelReady(payload.audioUdpPort, payload.screenTcpPort);
        }
        break;
    case CallControlType::ScreenShareStart:
        if (m_state == State::Active) {
            emit screenShareChanged(true);
        }
        break;
    case CallControlType::ScreenShareStop:
        if (m_state == State::Active) {
            m_remoteControlActive = false;
            emit screenShareChanged(false);
            emit remoteControlChanged(false);
        }
        break;
    case CallControlType::RemoteControlRequest:
        if (m_state == State::Active) {
            emit remoteControlRequested(QString::fromStdString(payload.senderId));
        }
        break;
    case CallControlType::RemoteControlGrant:
        if (m_state == State::Active) {
            m_remoteControlActive = true;
            emit remoteControlChanged(true);
        }
        break;
    case CallControlType::RemoteControlRevoke:
        if (m_state == State::Active) {
            m_remoteControlActive = false;
            emit remoteControlChanged(false);
        }
        break;
    default:
        break;
    }
}
