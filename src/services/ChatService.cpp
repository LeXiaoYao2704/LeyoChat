#include "services/ChatService.h"

#include "architecture/ResourceReferenceMessage.h"
#include "services/DirectConversationAddressing.h"
#include "services/MessageMutationService.h"
#include "storage/ConversationRepository.h"
#include "ui/StickerManager.h"

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QUuid>

namespace {
std::wstring toWide(const QString& value) {
    return value.toStdWString();
}

std::wstring toWideForOutgoingTrace(const char* variant, const char* field, const QString& value) {
    qInfo().noquote()
        << "[chat-create] stage=to-wide"
        << "variant=" << variant
        << "field=" << field
        << "len=" << value.size();
    return value.toStdWString();
}

void logOutgoingCreateStage(const char* variant,
                            const char* stage,
                            const QString& localClientId,
                            const QString& conversationId,
                            const QString& targetId,
                            const QString& body,
                            const QString& replyToMessageId = QString(),
                            const QString& replyToSenderId = QString(),
                            const QString& replyToBody = QString()) {
    qInfo().noquote()
        << "[chat-create] stage=" << stage
        << "variant=" << variant
        << "local=" << localClientId.left(8)
        << "conversation=" << conversationId
        << "target=" << targetId.left(8)
        << "bodyLen=" << body.size()
        << "replyMsgLen=" << replyToMessageId.size()
        << "replySenderLen=" << replyToSenderId.size()
        << "replyBodyLen=" << replyToBody.size();
}

std::string toUtf8(const QString& value) {
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

std::wstring utf8ToWide(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size())).toStdWString();
}

bool looksLikeImageName(const QString& name) {
    const QString suffix = QFileInfo(name).suffix().toLower();
    return suffix == QLatin1String("png") || suffix == QLatin1String("jpg")
        || suffix == QLatin1String("jpeg") || suffix == QLatin1String("gif")
        || suffix == QLatin1String("bmp") || suffix == QLatin1String("webp")
        || suffix == QLatin1String("svg") || suffix == QLatin1String("tiff")
        || suffix == QLatin1String("ico");
}

std::wstring incomingPreview(const MessageEnvelope& envelope) {
    if (envelope.type == MessageType::FileAttachment) {
        const QString attachmentName =
            QString::fromUtf8(envelope.attachmentName.data(), static_cast<int>(envelope.attachmentName.size()));
        const bool isImage = looksLikeImageName(attachmentName);
        const QString preview = isImage ? QStringLiteral("[\u56FE\u7247]") : QStringLiteral("[\u6587\u4EF6]");
        return preview.toStdWString();
    }

    if (envelope.type == MessageType::ResourceReference) {
        const auto payload = parseResourceReferenceEnvelope(envelope);
        const QString resourceLabel = payload.has_value()
                                          ? (!payload->resource.title.trimmed().isEmpty()
                                                 ? payload->resource.title.trimmed()
                                                 : payload->resource.resourceId.trimmed())
                                          : QString();
        const QString preview = resourceLabel.isEmpty()
                                    ? QStringLiteral("[共享资源]")
                                    : QStringLiteral("[共享资源] %1").arg(resourceLabel);
        return preview.toStdWString();
    }

    return utf8ToWide(envelope.body);
}
}

bool ChatService::shouldAutoActivateIncomingConversation(const QString& currentConversationId,
                                                         const QString& incomingConversationId,
                                                         bool isNudge) {
    if (isNudge) {
        return true;
    }

    const QString normalizedCurrentConversationId = currentConversationId.trimmed();
    const QString normalizedIncomingConversationId = incomingConversationId.trimmed();
    if (normalizedCurrentConversationId.isEmpty() || normalizedIncomingConversationId.isEmpty()) {
        return false;
    }

    return normalizedCurrentConversationId == normalizedIncomingConversationId;
}

QString ChatService::createOutgoingMessage(const QString& localClientId,
                                           ConversationRepository* repository,
                                           const QString& conversationId,
                                           const QString& targetId,
                                           const QString& body) {
    logOutgoingCreateStage("plain", "enter", localClientId, conversationId, targetId, body);
    if (!repository) {
        return {};
    }

    const QString trimmedBody = body.trimmed();
    logOutgoingCreateStage("plain", "trimmed", localClientId, conversationId, targetId, trimmedBody);
    if (conversationId.trimmed().isEmpty() || targetId.trimmed().isEmpty() || trimmedBody.isEmpty()) {
        return {};
    }

    const QString messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const qint64 createdAtMs = QDateTime::currentMSecsSinceEpoch();
    logOutgoingCreateStage("plain", "build-message-begin", localClientId, conversationId, targetId, trimmedBody);
    const std::wstring messageIdWide = toWideForOutgoingTrace("plain", "messageId", messageId);
    const std::wstring conversationIdWide = toWideForOutgoingTrace("plain", "conversationId", conversationId);
    const std::wstring localClientIdWide = toWideForOutgoingTrace("plain", "localClientId", localClientId);
    const std::wstring bodyWide = toWideForOutgoingTrace("plain", "body", trimmedBody);
    const ChatMessage message{
        messageIdWide,
        conversationIdWide,
        localClientIdWide,
        bodyWide,
        createdAtMs,
        MessageDeliveryState::Pending,
        {},
        {},
        L"text",
        {}
    };

    logOutgoingCreateStage("plain", "append-begin", localClientId, conversationId, targetId, trimmedBody);
    if (!repository->appendMessage(message)) {
        return {};
    }
    logOutgoingCreateStage("plain", "append-ok", localClientId, conversationId, targetId, trimmedBody);

    logOutgoingCreateStage("plain", "summary-begin", localClientId, conversationId, targetId, trimmedBody);
    upsertConversationSummary(repository,
                              ConversationSummary{
        toWideForOutgoingTrace("plain", "summary.conversationId", conversationId),
        toWideForOutgoingTrace("plain", "summary.title", targetId),
        toWideForOutgoingTrace("plain", "summary.preview", trimmedBody),
        createdAtMs
    },
                              QStringLiteral("direct"));
    logOutgoingCreateStage("plain", "done", localClientId, conversationId, targetId, trimmedBody);
    return messageId;
}

QString ChatService::createOutgoingMessage(const QString& localClientId,
                                           ConversationRepository* repository,
                                           const QString& conversationId,
                                           const QString& targetId,
                                           const QString& body,
                                           const QString& replyToMessageId,
                                           const QString& replyToSenderId,
                                           const QString& replyToBody) {
    logOutgoingCreateStage("reply", "enter", localClientId, conversationId, targetId, body,
                           replyToMessageId, replyToSenderId, replyToBody);
    if (!repository) {
        return {};
    }

    const QString trimmedBody = body.trimmed();
    logOutgoingCreateStage("reply", "trimmed", localClientId, conversationId, targetId, trimmedBody,
                           replyToMessageId, replyToSenderId, replyToBody);
    if (conversationId.trimmed().isEmpty() || targetId.trimmed().isEmpty() || trimmedBody.isEmpty()) {
        return {};
    }

    const QString messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const qint64 createdAtMs = QDateTime::currentMSecsSinceEpoch();
    logOutgoingCreateStage("reply", "build-message-begin", localClientId, conversationId, targetId, trimmedBody,
                           replyToMessageId, replyToSenderId, replyToBody);
    const std::wstring messageIdWide = toWideForOutgoingTrace("reply", "messageId", messageId);
    const std::wstring conversationIdWide = toWideForOutgoingTrace("reply", "conversationId", conversationId);
    const std::wstring localClientIdWide = toWideForOutgoingTrace("reply", "localClientId", localClientId);
    const std::wstring bodyWide = toWideForOutgoingTrace("reply", "body", trimmedBody);
    ChatMessage message{
        messageIdWide,
        conversationIdWide,
        localClientIdWide,
        bodyWide,
        createdAtMs,
        MessageDeliveryState::Pending,
        {},
        {},
        L"text",
        {}
    };
    logOutgoingCreateStage("reply", "reply-fields-begin", localClientId, conversationId, targetId, trimmedBody,
                           replyToMessageId, replyToSenderId, replyToBody);
    message.replyToMessageId = toWideForOutgoingTrace("reply", "replyToMessageId", replyToMessageId);
    message.replyToSenderId = toWideForOutgoingTrace("reply", "replyToSenderId", replyToSenderId);
    message.replyToBody = toWideForOutgoingTrace("reply", "replyToBody", replyToBody);

    logOutgoingCreateStage("reply", "append-begin", localClientId, conversationId, targetId, trimmedBody,
                           replyToMessageId, replyToSenderId, replyToBody);
    if (!repository->appendMessage(message)) {
        return {};
    }
    logOutgoingCreateStage("reply", "append-ok", localClientId, conversationId, targetId, trimmedBody,
                           replyToMessageId, replyToSenderId, replyToBody);

    logOutgoingCreateStage("reply", "summary-begin", localClientId, conversationId, targetId, trimmedBody,
                           replyToMessageId, replyToSenderId, replyToBody);
    upsertConversationSummary(repository,
                              ConversationSummary{
        toWideForOutgoingTrace("reply", "summary.conversationId", conversationId),
        toWideForOutgoingTrace("reply", "summary.title", targetId),
        toWideForOutgoingTrace("reply", "summary.preview", trimmedBody),
        createdAtMs
    },
                              QStringLiteral("direct"));
    logOutgoingCreateStage("reply", "done", localClientId, conversationId, targetId, trimmedBody,
                           replyToMessageId, replyToSenderId, replyToBody);
    return messageId;
}

bool ChatService::markMessageSent(ConversationRepository* repository, const QString& messageId) {
    return updateMessageState(repository, messageId, MessageDeliveryState::Sent);
}

bool ChatService::markMessageServerAcked(ConversationRepository* repository,
                                         const QString& messageId) {
    return updateMessageState(repository, messageId, MessageDeliveryState::ServerAcked);
}

bool ChatService::markMessageReceived(ConversationRepository* repository, const QString& messageId) {
    return updateMessageState(repository, messageId, MessageDeliveryState::Received);
}

bool ChatService::markMessageRead(ConversationRepository* repository, const QString& messageId) {
    return updateMessageState(repository, messageId, MessageDeliveryState::Read);
}

bool ChatService::buildEnvelope(const QString& localClientId,
                                ConversationRepository* repository,
                                const QString& messageId,
                                const QString& targetId,
                                MessageEnvelope* outEnvelope,
                                QString* errorMessage) {
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!repository || !outEnvelope) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("repository and output envelope are required");
        }
        return false;
    }

    QString conversationId;
    QString body;
    qint64 createdAtMs = 0;
    QString attachmentName;
    QString localFilePath;
    const bool foundMessage = repository->findMessageStorageRecordById(messageId,
                                                                       &conversationId,
                                                                       &body,
                                                                       &createdAtMs,
                                                                       &attachmentName,
                                                                       &localFilePath);
    if (!foundMessage) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("persisted message record was not found");
        }
        return false;
    }

    if (!attachmentName.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "attachment message cannot be rebuilt as direct text");
        }
        return false;
    }

    outEnvelope->messageId = toUtf8(messageId);
    outEnvelope->type = MessageType::ChatText;
    outEnvelope->senderId = toUtf8(localClientId);
    outEnvelope->targetId = toUtf8(targetId);
    outEnvelope->conversationId = toUtf8(conversationId);
    outEnvelope->body = toUtf8(body);
    outEnvelope->attachmentName = {};
    outEnvelope->createdAtMs = createdAtMs;

    // 贴纸消息重发时需要还原 messageSubtype + payloadJson（含 gif_base64）
    ChatMessage fullMsg;
    if (repository->findMessageById(messageId, &fullMsg)
        && QString::fromStdWString(fullMsg.messageType) == QStringLiteral("sticker")) {
        outEnvelope->messageSubtype = "sticker";
        outEnvelope->contentType = "plain";
        // DB 中 payloadJson 只有 pack_id / sticker_id，网络传输需要附带 gif_base64
        const QJsonObject dbObj = QJsonDocument::fromJson(
            QString::fromStdWString(fullMsg.payloadJson).toUtf8()).object();
        const QString packId = dbObj.value(QStringLiteral("pack_id")).toString();
        const QString stickerId = dbObj.value(QStringLiteral("sticker_id")).toString();
        QJsonObject netObj = dbObj;
        const QByteArray gifData = StickerManager::instance().readStickerData(packId, stickerId);
        if (!gifData.isEmpty()) {
            netObj.insert(QStringLiteral("gif_base64"),
                          QString::fromLatin1(gifData.toBase64()));
        }
        const QByteArray netBytes = QJsonDocument(netObj).toJson(QJsonDocument::Compact);
        outEnvelope->payloadJson = std::string(netBytes.constData(),
                                               static_cast<std::size_t>(netBytes.size()));
    }
    if (repository->findMessageById(messageId, &fullMsg)
        && QString::fromStdWString(fullMsg.messageType) == QStringLiteral("nudge")) {
        outEnvelope->contentType = "nudge";
    }
    if (repository->findMessageById(messageId, &fullMsg)
        && QString::fromStdWString(fullMsg.messageType) == QStringLiteral("forward_package")) {
        outEnvelope->messageSubtype = "forward_package";
        outEnvelope->contentType = "plain";
        const QByteArray payloadBytes =
            QString::fromStdWString(fullMsg.payloadJson).toUtf8();
        outEnvelope->payloadJson =
            std::string(payloadBytes.constData(),
                        static_cast<std::size_t>(payloadBytes.size()));
    }

    return true;
}

bool ChatService::storeIncomingEnvelope(const QString& localClientId,
                                        ConversationRepository* repository,
                                        const MessageEnvelope& envelope) {
    if (!repository) {
        return false;
    }

    if (envelope.type == MessageType::MessageMutation) {
        return MessageMutationService::applyIncomingMutation(repository, envelope);
    }

    if (envelope.type == MessageType::ReceiptReceived) {
        const QString msgId =
            QString::fromUtf8(envelope.messageId.data(), static_cast<int>(envelope.messageId.size())).trimmed();
        const QString senderId =
            QString::fromUtf8(envelope.senderId.data(), static_cast<int>(envelope.senderId.size())).trimmed();
        const QString targetId =
            QString::fromUtf8(envelope.targetId.data(), static_cast<int>(envelope.targetId.size())).trimmed();
        const QString receiptConversationId =
            QString::fromUtf8(envelope.conversationId.data(),
                              static_cast<int>(envelope.conversationId.size())).trimmed();
        if (msgId.isEmpty() || senderId.isEmpty() || targetId != localClientId) {
            return false;
        }

        ChatMessage existing;
        if (!repository->findMessageById(msgId, &existing)) {
            return repository->enqueuePendingDeliveryReceipt(msgId,
                                                             senderId,
                                                             targetId,
                                                             receiptConversationId,
                                                             envelope.createdAtMs);
        }

        const QString expectedDirectConversationId =
            DirectConversationAddressing::conversationIdForPeers(localClientId, senderId);
        const QString existingConversationId = QString::fromStdWString(existing.conversationId).trimmed();
        const bool existingConversationLooksDirect =
            DirectConversationAddressing::otherParticipant(localClientId, existingConversationId) == senderId;
        const bool receiptConversationLooksDirect =
            receiptConversationId.isEmpty()
            || DirectConversationAddressing::otherParticipant(localClientId, receiptConversationId) == senderId;
        const bool conversationMatches =
            existingConversationLooksDirect && receiptConversationLooksDirect;
        if (expectedDirectConversationId.isEmpty()
            || QString::fromStdWString(existing.senderId) != localClientId
            || !conversationMatches) {
            return false;
        }

        qInfo().noquote() << "[receipt-recv] msgId="
                          << msgId.left(8)
                          << "from=" << senderId.left(8);
        return updateMessageState(repository, msgId, MessageDeliveryState::Received);
    }

    if (envelope.type == MessageType::ReceiptRead) {
        const QString msgId = QString::fromUtf8(envelope.messageId.data(),
                                                static_cast<int>(envelope.messageId.size())).trimmed();
        const QString readerId = QString::fromUtf8(envelope.senderId.data(),
                                                   static_cast<int>(envelope.senderId.size())).trimmed();
        if (msgId.isEmpty() || readerId.isEmpty()) {
            return false;
        }
        if (!repository->insertReadReceipt(msgId, readerId, envelope.createdAtMs)) {
            return false;
        }

        ChatMessage existing;
        if (repository->findMessageById(msgId, &existing)
            && repository->isKnownActiveGroupConversation(QString::fromStdWString(existing.conversationId))) {
            return true;
        }

        return updateMessageState(repository, msgId, MessageDeliveryState::Read);
    }

    if (envelope.type != MessageType::ChatText
        && envelope.type != MessageType::FileAttachment
        && envelope.type != MessageType::ResourceReference) {
        return false;
    }

    const QString senderId = QString::fromUtf8(envelope.senderId.data(), static_cast<int>(envelope.senderId.size()));
    const QString conversationId = DirectConversationAddressing::conversationIdForPeers(localClientId, senderId);
    const QString body = QString::fromStdWString(incomingPreview(envelope));
    if (senderId.isEmpty() || conversationId.isEmpty()) {
        return false;
    }
    // 防护检查：严禁 senderId 等于 localClientId（防止竞态条件导致自己的消息被对方数据覆盖）
    // INSERT OR REPLACE 语义下这会导致严重数据损坏
    if (senderId == localClientId) {
        qWarning().noquote() << "[storeIncomingEnvelope-GUARD] CRITICAL: Rejected message from self!"
                             << "msgId=" << QString::fromUtf8(envelope.messageId.data(),
                                                               static_cast<int>(envelope.messageId.size()))
                             << "senderId=" << senderId
                             << "localClientId=" << localClientId;
        return false;
    }

    // 去重检查：如果该 messageId 已存在于本地数据库，说明是已发消息的网络回显，
    // 必须跳过，否则会用对端 senderId 覆盖本端的原始记录
    {
        const QString incomingMsgId = QString::fromUtf8(envelope.messageId.data(),
                                                         static_cast<int>(envelope.messageId.size()));
        ChatMessage existing;
        if (repository->findMessageById(incomingMsgId, &existing)) {
            const QString existingSenderId = QString::fromStdWString(existing.senderId);
            qInfo().noquote() << "[storeIncomingEnvelope-DEDUP] Skipped duplicate msgId=" << incomingMsgId
                              << "existingSender=" << existingSenderId
                              << "incomingSender=" << senderId;
            return existingSenderId == senderId;
        }
        // 诊断日志：记录 dedup 检查通过（消息不存在）的关键信息，
        // 如果此消息的 senderId 后续被异常覆盖，可通过此日志追踪根因
        qInfo().noquote() << "[storeIncomingEnvelope-PASS] msgId=" << incomingMsgId.left(8)
                           << "sender=" << senderId.left(8)
                           << "type=" << static_cast<int>(envelope.type);
    }

    if (envelope.type == MessageType::ChatText && body.trimmed().isEmpty()) {
        // Ignore empty text envelopes so presence/heartbeat-like traffic does not
        // keep refreshing conversation timestamps without real chat content.
        return false;
    }

    ChatMessage message{
        utf8ToWide(envelope.messageId),
        conversationId.toStdWString(),
        utf8ToWide(envelope.senderId),
        body.toStdWString(),
        envelope.createdAtMs,
        MessageDeliveryState::Received,
        QString::fromUtf8(envelope.attachmentName.data(), static_cast<int>(envelope.attachmentName.size())).toStdWString(),
        {},
        envelope.type == MessageType::ResourceReference ? std::wstring(L"resource_ref") : std::wstring(L"text"),
        envelope.type == MessageType::ResourceReference
            ? QString::fromUtf8(envelope.payloadJson.data(), static_cast<int>(envelope.payloadJson.size())).toStdWString()
            : std::wstring()
    };
    // 贴纸消息：保留 payloadJson 和 messageType
    if (envelope.messageSubtype == "sticker" && !envelope.payloadJson.empty()) {
        message.messageType = L"sticker";
        message.payloadJson = QString::fromUtf8(envelope.payloadJson.data(),
                                                static_cast<int>(envelope.payloadJson.size())).toStdWString();
    }
    // 合并转发卡片
    if (envelope.messageSubtype == "forward_package" && !envelope.payloadJson.empty()) {
        message.messageType = L"forward_package";
        message.payloadJson = QString::fromUtf8(envelope.payloadJson.data(),
                                                static_cast<int>(envelope.payloadJson.size())).toStdWString();
    }
    message.replyToMessageId = utf8ToWide(envelope.replyToMessageId);
    message.replyToSenderId = utf8ToWide(envelope.replyToSenderId);
    message.replyToBody = utf8ToWide(envelope.replyToBody);

    // 将 appendMessage + upsertConversation + setFlag 包在事务中，
    // 保证原子性：要么三步全成功，要么全回滚，避免出现消息入库但会话列表不一致的状态。
    // 若外层已有事务则跳过内部事务管理
    QSqlDatabase db = QSqlDatabase::database(repository->connectionName(), false);
    const bool ownTransaction = db.transaction();

    if (!repository->appendMessage(message, QDateTime::currentMSecsSinceEpoch())) {
        qWarning().noquote() << "[storeIncomingEnvelope] appendMessage failed msgId="
                             << QString::fromUtf8(envelope.messageId.data(),
                                                  static_cast<int>(envelope.messageId.size()))
                             << "sender=" << QString::fromUtf8(envelope.senderId.data(),
                                                               static_cast<int>(envelope.senderId.size()))
                             << "subtype=" << QString::fromUtf8(envelope.messageSubtype.data(),
                                                                static_cast<int>(envelope.messageSubtype.size()));
        if (ownTransaction) db.rollback();
        return false;
    }

    const bool ok = upsertConversationSummary(repository,
                                              ConversationSummary{
        toWide(conversationId),
        toWide(senderId),
        toWide(body),
        envelope.createdAtMs
    },
                                              QStringLiteral("direct"));
    if (!ok) {
        qWarning().noquote() << "[storeIncomingEnvelope] upsertConversation failed msgId="
                             << QString::fromUtf8(envelope.messageId.data(),
                                                  static_cast<int>(envelope.messageId.size()))
                             << "conversationId=" << conversationId;
        if (ownTransaction) db.rollback();
        return false;
    }
    // 收到新消息时重置已完成标记，让对话重新出现在列表
    repository->setConversationFlag(conversationId, ConversationFlag::Done, false);

    if (ownTransaction) {
        if (!db.commit()) {
            qWarning() << "[storeIncomingEnvelope] commit failed";
            db.rollback();
            return false;
        }
    }
    return true;
}

std::vector<ChatMessage> ChatService::loadMessages(ConversationRepository* repository, const QString& conversationId) {
    if (!repository) {
        return {};
    }

    return repository->loadMessages(toWide(conversationId));
}

std::vector<ConversationSummary> ChatService::loadConversationSummaries(ConversationRepository* repository) {
    if (!repository) {
        return {};
    }

    return repository->loadConversationSummaries();
}

bool ChatService::applyReaction(ConversationRepository* repository,
                                const QString& messageId,
                                const QString& reactorClientId,
                                const QString& emoji)
{
    if (!repository) return false;
    return repository->applyReaction(messageId, reactorClientId, emoji);
}

bool ChatService::storeIncomingGroupEnvelope(ConversationRepository* repository,
                                              const MessageEnvelope& envelope,
                                              const QString& groupId,
                                              const QString& title) {
    if (!repository || groupId.isEmpty()) {
        return false;
    }

    if (envelope.type == MessageType::MessageMutation) {
        return MessageMutationService::applyIncomingMutation(repository, envelope);
    }

    const QString messageId = QString::fromUtf8(envelope.messageId.data(),
                                                 static_cast<int>(envelope.messageId.size()));
    const QString senderId  = QString::fromUtf8(envelope.senderId.data(),
                                                 static_cast<int>(envelope.senderId.size()));
    if (messageId.isEmpty()) {
        return false;
    }

    // 去重检查：如果该 messageId 已存在于本地数据库，跳过入库防止覆盖
    {
        ChatMessage existing;
        if (repository->findMessageById(messageId, &existing)) {
            const QString existingConversationId = QString::fromStdWString(existing.conversationId);
            const QString existingSenderId = QString::fromStdWString(existing.senderId);
            qInfo().noquote() << "[storeIncomingGroupEnvelope-DEDUP] Skipped duplicate msgId=" << messageId
                              << "existingSender=" << existingSenderId
                              << "incomingSender=" << senderId;
            return existingConversationId == groupId && existingSenderId == senderId;
        }
    }

    // Parse JSON body: {"group_id":"...","message_kind":"text","content_type":"html","text":"..."}
    const QByteArray rawBody(envelope.body.data(), static_cast<int>(envelope.body.size()));
    const QJsonObject jsonBody = QJsonDocument::fromJson(rawBody).object();
    const QString messageKind = jsonBody.value(QStringLiteral("message_kind")).toString();
    QString attachmentName =
        QString::fromUtf8(envelope.attachmentName.data(), static_cast<int>(envelope.attachmentName.size())).trimmed();
    if (attachmentName.isEmpty()) {
        attachmentName = jsonBody.value(QStringLiteral("attachment_name")).toString().trimmed();
    }

    QString body = jsonBody.contains(QStringLiteral("text"))
                       ? jsonBody.value(QStringLiteral("text")).toString()
                       : QString::fromUtf8(envelope.body.data(), static_cast<int>(envelope.body.size()));
    if (messageKind == QStringLiteral("attachment")) {
        if (body.trimmed().isEmpty()) {
            body = attachmentName.isEmpty()
                ? QStringLiteral("[图片]")
                : QStringLiteral("[图片] %1").arg(attachmentName);
        }
    }

    if (envelope.type == MessageType::ResourceReference
        || messageKind == QStringLiteral("resource_reference")) {
        const auto payload = parseResourceReferenceEnvelope(envelope);
        const QString resourceLabel = payload.has_value()
                                          ? (!payload->resource.title.trimmed().isEmpty()
                                                 ? payload->resource.title.trimmed()
                                                 : payload->resource.resourceId.trimmed())
                                          : QString();
        body = resourceLabel.isEmpty()
                   ? QStringLiteral("[共享资源]")
                   : QStringLiteral("[共享资源] %1").arg(resourceLabel);
    }

    ChatMessage message{
        messageId.toStdWString(),
        groupId.toStdWString(),
        senderId.toStdWString(),
        body.toStdWString(),
        envelope.createdAtMs,
        MessageDeliveryState::Received,
        attachmentName.toStdWString(),
        {},
        (envelope.type == MessageType::ResourceReference
         || messageKind == QStringLiteral("resource_reference"))
            ? std::wstring(L"resource_ref")
            : std::wstring(L"text"),
        (envelope.type == MessageType::ResourceReference
         || messageKind == QStringLiteral("resource_reference"))
            ? QString::fromUtf8(envelope.payloadJson.data(), static_cast<int>(envelope.payloadJson.size())).toStdWString()
            : std::wstring()
    };
    message.replyToMessageId = utf8ToWide(envelope.replyToMessageId);
    message.replyToSenderId = utf8ToWide(envelope.replyToSenderId);
    message.replyToBody = utf8ToWide(envelope.replyToBody);

    // 贴纸消息：保留 payloadJson 和 messageType
    if (envelope.messageSubtype == "sticker" && !envelope.payloadJson.empty()) {
        message.messageType = L"sticker";
        message.payloadJson = QString::fromUtf8(envelope.payloadJson.data(),
                                                static_cast<int>(envelope.payloadJson.size())).toStdWString();
    }
    // 合并转发卡片
    if (envelope.messageSubtype == "forward_package" && !envelope.payloadJson.empty()) {
        message.messageType = L"forward_package";
        message.payloadJson = QString::fromUtf8(envelope.payloadJson.data(),
                                                static_cast<int>(envelope.payloadJson.size())).toStdWString();
    }

    // 将 mentionedIds 序列化为 JSON 数组字符串存储
    if (!envelope.mentionedIds.empty()) {
        QJsonArray arr;
        for (const auto& id : envelope.mentionedIds) {
            arr.append(QString::fromStdString(id));
        }
        message.mentionedIds = QString::fromUtf8(
            QJsonDocument(arr).toJson(QJsonDocument::Compact)).toStdWString();
    }

    // 将 appendMessage + upsert + setFlag 包在事务中，保证原子性
    // 若外层调用方已开启事务（如群 fan-out 批量入队），跳过内部事务管理
    QSqlDatabase db = QSqlDatabase::database(repository->connectionName(), false);
    const bool ownTransaction = db.transaction();

    if (!repository->appendMessage(message, QDateTime::currentMSecsSinceEpoch())) {
        if (ownTransaction) db.rollback();
        return false;
    }

    const bool ok = repository->upsertConversation(ConversationSummary{
        groupId.toStdWString(),
        title.toStdWString(),
        body.toStdWString(),
        envelope.createdAtMs
    });
    if (!ok) {
        if (ownTransaction) db.rollback();
        return false;
    }
    // 收到群消息时重置已完成标记
    repository->setConversationFlag(groupId, ConversationFlag::Done, false);

    if (ownTransaction) {
        if (!db.commit()) {
            qWarning() << "[storeIncomingGroupEnvelope] commit failed";
            db.rollback();
            return false;
        }
    }
    return true;
}

bool ChatService::persistOutgoingGroupFanOut(
    ConversationRepository* repository,
    const QString& groupId,
    const QString& title,
    const std::vector<PendingGroupFanOutEnvelope>& pendingEnvelopes,
    const MessageEnvelope& selfEnvelope) {
    const QString trimmedGroupId = groupId.trimmed();
    if (!repository || trimmedGroupId.isEmpty() || pendingEnvelopes.empty() || selfEnvelope.messageId.empty()) {
        return false;
    }

    for (const auto& pending : pendingEnvelopes) {
        if (pending.targetId.trimmed().isEmpty()
            || pending.envelopeBlob.isEmpty()
            || pending.createdAtMs <= 0) {
            return false;
        }
    }

    QSqlDatabase db = QSqlDatabase::database(repository->connectionName(), false);
    if (!db.isValid() || !db.transaction()) {
        qWarning() << "[persistOutgoingGroupFanOut] failed to start transaction";
        return false;
    }

    for (const auto& pending : pendingEnvelopes) {
        if (!repository->enqueuePendingGroupEnvelope(
                pending.targetId.trimmed(), trimmedGroupId, pending.envelopeBlob, pending.createdAtMs)) {
            qWarning().noquote() << "[persistOutgoingGroupFanOut] enqueue pending failed target="
                                 << pending.targetId.left(8)
                                 << "group=" << trimmedGroupId.left(8);
            db.rollback();
            return false;
        }
    }

    if (!storeIncomingGroupEnvelope(repository, selfEnvelope, trimmedGroupId, title)) {
        qWarning().noquote() << "[persistOutgoingGroupFanOut] self message store failed msgId="
                             << QString::fromUtf8(selfEnvelope.messageId.data(),
                                                  static_cast<int>(selfEnvelope.messageId.size())).left(8);
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        qWarning() << "[persistOutgoingGroupFanOut] commit failed";
        db.rollback();
        return false;
    }
    return true;
}

bool ChatService::persistPendingGroupFanOutOnly(
    ConversationRepository* repository,
    const QString& groupId,
    const std::vector<PendingGroupFanOutEnvelope>& pendingEnvelopes) {
    const QString trimmedGroupId = groupId.trimmed();
    if (!repository || trimmedGroupId.isEmpty() || pendingEnvelopes.empty()) {
        return false;
    }

    for (const auto& pending : pendingEnvelopes) {
        if (pending.targetId.trimmed().isEmpty()
            || pending.envelopeBlob.isEmpty()
            || pending.createdAtMs <= 0) {
            return false;
        }
    }

    QSqlDatabase db = QSqlDatabase::database(repository->connectionName(), false);
    if (!db.isValid() || !db.transaction()) {
        qWarning() << "[persistPendingGroupFanOutOnly] failed to start transaction";
        return false;
    }

    for (const auto& pending : pendingEnvelopes) {
        if (!repository->enqueuePendingGroupEnvelope(
                pending.targetId.trimmed(), trimmedGroupId, pending.envelopeBlob, pending.createdAtMs)) {
            qWarning().noquote() << "[persistPendingGroupFanOutOnly] enqueue pending failed target="
                                 << pending.targetId.left(8)
                                 << "group=" << trimmedGroupId.left(8);
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        qWarning() << "[persistPendingGroupFanOutOnly] commit failed";
        db.rollback();
        return false;
    }
    return true;
}

bool ChatService::updateMessageState(ConversationRepository* repository,
                                     const QString& messageId,
                                     MessageDeliveryState state) {
    if (!repository) {
        return false;
    }

    if (messageId.trimmed().isEmpty()) {
        return false;
    }

    return repository->updateDeliveryState(messageId, state);
}

bool ChatService::upsertConversationSummary(ConversationRepository* repository,
                                            const ConversationSummary& summary,
                                            const QString& conversationType) {
    if (!repository) {
        return false;
    }

    return repository->upsertConversationWithType(summary, conversationType);
}
