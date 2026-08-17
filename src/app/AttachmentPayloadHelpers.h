#pragma once

#include "domain/MessageEnvelope.h"

#include <QByteArray>
#include <QString>

QString saveIncomingAttachment(const MessageEnvelope& envelope, const QString& senderName);

QString saveIncomingAttachmentPayload(const QByteArray& payload,
                                      const QString& attachmentName,
                                      const QString& senderName);

bool tryExtractInlineGroupAttachment(const MessageEnvelope& envelope,
                                     QString* outAttachmentName,
                                     QByteArray* outBase64Payload,
                                     QString* outPreviewText);
