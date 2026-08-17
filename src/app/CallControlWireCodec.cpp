#include "app/CallControlWireCodec.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

QString callControlTypeToWire(CallControlType type)
{
    switch (type) {
    case CallControlType::Offer: return QStringLiteral("call-offer");
    case CallControlType::Answer: return QStringLiteral("call-answer");
    case CallControlType::Reject: return QStringLiteral("call-reject");
    case CallControlType::Busy: return QStringLiteral("call-busy");
    case CallControlType::Cancel: return QStringLiteral("call-cancel");
    case CallControlType::Hangup: return QStringLiteral("call-hangup");
    case CallControlType::MediaReady: return QStringLiteral("call-media-ready");
    case CallControlType::ScreenShareStart: return QStringLiteral("screen-share-start");
    case CallControlType::ScreenShareStop: return QStringLiteral("screen-share-stop");
    case CallControlType::RemoteControlRequest: return QStringLiteral("remote-control-request");
    case CallControlType::RemoteControlGrant: return QStringLiteral("remote-control-grant");
    case CallControlType::RemoteControlRevoke: return QStringLiteral("remote-control-revoke");
    case CallControlType::RemoteMouseInput: return QStringLiteral("remote-mouse-input");
    case CallControlType::RemoteKeyInput: return QStringLiteral("remote-key-input");
    }
    return QStringLiteral("call-offer");
}

bool callControlTypeFromWire(const QString& text, CallControlType* type)
{
    if (!type) {
        return false;
    }
    if (text == QStringLiteral("call-offer")) {
        *type = CallControlType::Offer;
    } else if (text == QStringLiteral("call-answer")) {
        *type = CallControlType::Answer;
    } else if (text == QStringLiteral("call-reject")) {
        *type = CallControlType::Reject;
    } else if (text == QStringLiteral("call-busy")) {
        *type = CallControlType::Busy;
    } else if (text == QStringLiteral("call-cancel")) {
        *type = CallControlType::Cancel;
    } else if (text == QStringLiteral("call-hangup")) {
        *type = CallControlType::Hangup;
    } else if (text == QStringLiteral("call-media-ready")) {
        *type = CallControlType::MediaReady;
    } else if (text == QStringLiteral("screen-share-start")) {
        *type = CallControlType::ScreenShareStart;
    } else if (text == QStringLiteral("screen-share-stop")) {
        *type = CallControlType::ScreenShareStop;
    } else if (text == QStringLiteral("remote-control-request")) {
        *type = CallControlType::RemoteControlRequest;
    } else if (text == QStringLiteral("remote-control-grant")) {
        *type = CallControlType::RemoteControlGrant;
    } else if (text == QStringLiteral("remote-control-revoke")) {
        *type = CallControlType::RemoteControlRevoke;
    } else if (text == QStringLiteral("remote-mouse-input")) {
        *type = CallControlType::RemoteMouseInput;
    } else if (text == QStringLiteral("remote-key-input")) {
        *type = CallControlType::RemoteKeyInput;
    } else {
        return false;
    }
    return true;
}

QString buildCallPayloadJson(const CallControlPayload& payload)
{
    QJsonObject json;
    json.insert(QStringLiteral("call_id"), QString::fromStdString(payload.callId));
    json.insert(QStringLiteral("media_flags"), payload.mediaFlags);
    json.insert(QStringLiteral("audio_udp_port"), static_cast<int>(payload.audioUdpPort));
    json.insert(QStringLiteral("screen_tcp_port"), static_cast<int>(payload.screenTcpPort));
    if (payload.type == CallControlType::RemoteMouseInput
        || payload.type == CallControlType::RemoteKeyInput) {
        json.insert(QStringLiteral("input_type"), payload.inputType);
        json.insert(QStringLiteral("input_x"), payload.inputX);
        json.insert(QStringLiteral("input_y"), payload.inputY);
        json.insert(QStringLiteral("input_button"), payload.inputButton);
        json.insert(QStringLiteral("input_modifiers"), payload.inputModifiers);
    }
    return QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact));
}

CallControlPayload callPayloadFromEnvelope(const MessageEnvelope& envelope)
{
    CallControlPayload payload;
    payload.senderId = envelope.senderId;
    payload.targetId = envelope.targetId;
    payload.reason = envelope.reason;

    const QString wireType = QString::fromStdString(envelope.controlType);
    CallControlType parsedType = CallControlType::Offer;
    if (callControlTypeFromWire(wireType, &parsedType)) {
        payload.type = parsedType;
    }

    if (!envelope.payloadJson.empty()) {
        const QJsonDocument jsonDoc = QJsonDocument::fromJson(
            QByteArray::fromStdString(envelope.payloadJson));
        if (jsonDoc.isObject()) {
            const QJsonObject obj = jsonDoc.object();
            payload.callId = obj.value(QStringLiteral("call_id")).toString().toStdString();
            payload.mediaFlags = obj.value(QStringLiteral("media_flags")).toInt(0);
            payload.audioUdpPort = static_cast<quint16>(
                obj.value(QStringLiteral("audio_udp_port")).toInt(0));
            payload.screenTcpPort = static_cast<quint16>(
                obj.value(QStringLiteral("screen_tcp_port")).toInt(0));
            payload.inputType = obj.value(QStringLiteral("input_type")).toInt(0);
            payload.inputX = obj.value(QStringLiteral("input_x")).toInt(0);
            payload.inputY = obj.value(QStringLiteral("input_y")).toInt(0);
            payload.inputButton = obj.value(QStringLiteral("input_button")).toInt(0);
            payload.inputModifiers = obj.value(QStringLiteral("input_modifiers")).toInt(0);
        }
    }

    if (payload.callId.empty()) {
        payload.callId = envelope.fileTaskId;
    }

    return payload;
}
