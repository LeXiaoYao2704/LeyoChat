#include "app/AttachmentPayloadHelpers.h"

#include "app/AppPathHelpers.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

QString saveIncomingAttachment(const MessageEnvelope& envelope, const QString& senderName) {
    const QByteArray payload = QByteArray::fromStdString(envelope.body);
    const QByteArray decodedBytes = QByteArray::fromBase64(payload);
    if (decodedBytes.isEmpty() && !payload.isEmpty()) {
        return {};
    }

    const QString attachmentName =
        QString::fromUtf8(envelope.attachmentName.data(), static_cast<int>(envelope.attachmentName.size()));
    const QString directoryPath = ensureIncomingFilesDirectoryForSender(senderName);
    const QString filePath = uniqueFilePath(directoryPath, attachmentName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    if (file.write(decodedBytes) != decodedBytes.size()) {
        return {};
    }
    file.close();
    return filePath;
}

QString saveIncomingAttachmentPayload(const QByteArray& payload,
                                      const QString& attachmentName,
                                      const QString& senderName) {
    const QByteArray decodedBytes = QByteArray::fromBase64(payload);
    if (decodedBytes.isEmpty() && !payload.isEmpty()) {
        return {};
    }

    const QString directoryPath = ensureIncomingFilesDirectoryForSender(senderName);
    const QString filePath = uniqueFilePath(directoryPath, attachmentName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    if (file.write(decodedBytes) != decodedBytes.size()) {
        return {};
    }
    file.close();
    return filePath;
}

bool tryExtractInlineGroupAttachment(const MessageEnvelope& envelope,
                                     QString* outAttachmentName,
                                     QByteArray* outBase64Payload,
                                     QString* outPreviewText) {
    if (envelope.type != MessageType::GroupMessage) {
        return false;
    }

    const QJsonObject bodyJson =
        QJsonDocument::fromJson(QByteArray::fromStdString(envelope.body)).object();
    if (bodyJson.value(QStringLiteral("message_kind")).toString() != QStringLiteral("attachment")) {
        return false;
    }

    const QString attachmentName = !envelope.attachmentName.empty()
        ? QString::fromUtf8(envelope.attachmentName.data(), static_cast<int>(envelope.attachmentName.size())).trimmed()
        : bodyJson.value(QStringLiteral("attachment_name")).toString().trimmed();
    const QString base64Payload = bodyJson.value(QStringLiteral("base64")).toString();
    if (attachmentName.isEmpty() || base64Payload.isEmpty()) {
        return false;
    }

    if (outAttachmentName) {
        *outAttachmentName = attachmentName;
    }
    if (outBase64Payload) {
        *outBase64Payload = base64Payload.toUtf8();
    }
    if (outPreviewText) {
        const QString preview = bodyJson.value(QStringLiteral("text")).toString().trimmed();
        *outPreviewText = preview.isEmpty()
            ? QStringLiteral("[图片] %1").arg(attachmentName)
            : preview;
    }
    return true;
}
