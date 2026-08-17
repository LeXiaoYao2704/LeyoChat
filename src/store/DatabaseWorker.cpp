#include "store/DatabaseWorker.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

#include "storage/ConversationRepository.h"
#include "storage/GroupRepository.h"

DatabaseWorker::DatabaseWorker(const QString& dbPath, QObject* parent)
    : QObject(parent)
    , m_dbPath(dbPath)
    , m_connectionName(QStringLiteral("leyochat-worker-%1")
                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

DatabaseWorker::~DatabaseWorker() {
    m_conversationRepo.reset();
    m_groupRepo.reset();
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::database(m_connectionName, false).close();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

void DatabaseWorker::ensureConnection() {
    if (m_conversationRepo) return;
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        emit persistError(QStringLiteral("ensureConnection"), QStringLiteral("db.open() failed"));
        return;
    }
    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
    pragma.exec(QStringLiteral("PRAGMA wal_autocheckpoint = 0"));
    m_conversationRepo = std::make_unique<ConversationRepository>(m_connectionName);
    m_groupRepo = std::make_unique<GroupRepository>(m_connectionName);
}

void DatabaseWorker::loadAll() {
    ensureConnection();
    if (!m_conversationRepo) return;

    auto conversations = m_conversationRepo->loadConversationSummaries();
    auto knownPeers = m_conversationRepo->loadKnownPeers();
    QSet<QString> unreadIds = m_localClientId.isEmpty()
        ? QSet<QString>{}
        : m_conversationRepo->loadConversationsWithUnreadMessages(m_localClientId);

    QHash<QString, Group> groups;
    QHash<QString, std::vector<GroupMember>> groupMembers;

    QVector<ConversationSummary> convVec;
    convVec.reserve(static_cast<int>(conversations.size()));
    for (auto& c : conversations) {
        convVec.append(std::move(c));
    }
    QVector<PeerEndpoint> peerVec;
    peerVec.reserve(static_cast<int>(knownPeers.size()));
    for (auto& p : knownPeers) {
        peerVec.append(std::move(p));
    }

    emit allDataLoaded(std::move(convVec), std::move(unreadIds),
                       std::move(groups), std::move(groupMembers),
                       std::move(peerVec));
}

void DatabaseWorker::loadMessagesForConversation(const QString& conversationId) {
    ensureConnection();
    if (!m_conversationRepo) {
        emit messagesLoaded(conversationId, {});
        emit recentMessagesLoaded(conversationId, {}, false);
        return;
    }
    auto page = m_conversationRepo->loadRecentMessagesPage(conversationId.toStdWString());
    auto messages = page.messages;
    emit messagesLoaded(conversationId, messages);
    emit recentMessagesLoaded(conversationId, std::move(messages), page.hasMoreBefore);
}

void DatabaseWorker::loadMessagesBefore(const QString& conversationId,
                                        const QString& beforeMessageId,
                                        int limit) {
    ensureConnection();
    if (!m_conversationRepo) {
        emit olderMessagesLoaded(conversationId, beforeMessageId, {}, false);
        return;
    }
    auto page = m_conversationRepo->loadMessagesBeforePage(
        conversationId.toStdWString(), beforeMessageId, limit);
    emit olderMessagesLoaded(conversationId,
                             beforeMessageId,
                             std::move(page.messages),
                             page.hasMoreBefore);
}

void DatabaseWorker::persistMessage(const ChatMessage& msg) {
    ensureConnection();
    if (!m_conversationRepo) return;
    m_conversationRepo->appendMessage(msg, QDateTime::currentMSecsSinceEpoch());
}

void DatabaseWorker::persistConversation(const ConversationSummary& summary) {
    ensureConnection();
    if (!m_conversationRepo) return;
    m_conversationRepo->upsertConversation(summary);
}

void DatabaseWorker::persistDeliveryState(const QString& messageId, int state) {
    ensureConnection();
    if (!m_conversationRepo) return;
    m_conversationRepo->updateDeliveryState(messageId, static_cast<MessageDeliveryState>(state));
}

void DatabaseWorker::persistConversationFlag(const QString& conversationId, int flag, bool value) {
    ensureConnection();
    if (!m_conversationRepo) return;
    m_conversationRepo->setConversationFlag(conversationId, static_cast<ConversationFlag>(flag), value);
}

void DatabaseWorker::persistGroupMembers(const QString& groupId,
                                          const std::vector<GroupMember>& members) {
    ensureConnection();
    if (!m_groupRepo) return;
    m_groupRepo->replaceMembers(groupId.toStdWString(), members);
}

void DatabaseWorker::persistReadReceipt(const QString& messageId, const QString& readerId,
                                         qint64 readAtMs) {
    ensureConnection();
    if (!m_conversationRepo) return;
    m_conversationRepo->insertReadReceipt(messageId, readerId, readAtMs);
}

void DatabaseWorker::persistMessageRecall(const QString& messageId, qint64 recalledAtMs) {
    ensureConnection();
    if (!m_conversationRepo) return;
    m_conversationRepo->applyMessageRecall(messageId, QString(), recalledAtMs);
}

void DatabaseWorker::persistMessageEdit(const QString& messageId, const QString& newBody,
                                         qint64 editedAtMs, const QString& editorId) {
    ensureConnection();
    if (!m_conversationRepo) return;
    m_conversationRepo->applyMessageEdit(messageId, editorId, editedAtMs, newBody);
}

void DatabaseWorker::persistGroupReadCount(const QString& messageId, int count) {
    Q_UNUSED(messageId);
    Q_UNUSED(count);
}

void DatabaseWorker::runWalCheckpoint() {
    ensureConnection();
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
    if (q.next()) {
        qInfo() << "[wal-checkpoint-worker] TRUNCATE result: busy=" << q.value(0).toInt()
                << "log=" << q.value(1).toInt()
                << "checkpointed=" << q.value(2).toInt();
    }
}

void DatabaseWorker::flushReadReceipts(const QString& conversationId,
                                       const QString& localClientId,
                                       bool isGroup) {
    ensureConnection();
    if (!m_conversationRepo || conversationId.isEmpty()) {
        emit readReceiptsFlushed(conversationId, {}, false);
        return;
    }

    // 1. 加载消息，筛选出需要发送已读回执的目标
    const auto messages = m_conversationRepo->loadMessages(conversationId.toStdWString());
    QVector<QPair<QString, QString>> targets; // {messageId, senderId}
    for (const auto& message : messages) {
        const QString senderId = QString::fromStdWString(message.senderId);
        const QString messageId = QString::fromStdWString(message.messageId);
        if (senderId == localClientId || messageId.isEmpty()) {
            continue;
        }
        const bool eligibleState = isGroup
            ? (message.deliveryState == MessageDeliveryState::Sent
               || message.deliveryState == MessageDeliveryState::Received)
            : message.deliveryState == MessageDeliveryState::Received;
        if (eligibleState) {
            targets.append({messageId, senderId});
        }
    }

    // 2. 标记未读已消费（SQL UPDATE）
    const bool consumed = m_conversationRepo->consumeConversationUnread(
        conversationId, localClientId, isGroup);

    if (consumed) {
        const qint64 readSeq = m_conversationRepo->loadRemoteChatCursor(conversationId);
        for (const auto& target : targets) {
            const QString serverMessageId =
                m_conversationRepo->loadRemoteServerIdForLocalMessageId(target.first);
            if (!serverMessageId.isEmpty()) {
                m_conversationRepo->enqueuePendingRemoteReadAck(serverMessageId,
                                                                conversationId,
                                                                readSeq);
            }
        }
    }

    emit readReceiptsFlushed(conversationId, std::move(targets), consumed);
}
