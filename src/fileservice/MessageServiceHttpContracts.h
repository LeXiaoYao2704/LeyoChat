#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

#include "MessageServiceDatabase.h"

namespace MessageServiceHttpContracts {

std::optional<StoreMessageRequest> parseStoreMessageRequest(
    const QJsonObject& object,
    const QString& senderId,
    QString* error);

QJsonObject storeMessageResultToJson(const StoreMessageResult& result);
QJsonObject storedMessageToJson(const StoredMessage& message);
QJsonObject messageListToJson(const QVector<StoredMessage>& messages);
QJsonObject conversationListToJson(const QVector<StoredConversation>& conversations);

std::optional<qint64> parseAckSeq(const QJsonObject& object,
                                  const QString& fieldName,
                                  QString* error);

}
