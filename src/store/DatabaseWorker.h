#pragma once

#include <memory>
#include <vector>

#include <QHash>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QVector>

#include "domain/ChatMessage.h"
#include "domain/ConversationSummary.h"
#include "domain/Group.h"
#include "domain/GroupMember.h"
#include "domain/PeerEndpoint.h"

class ConversationRepository;
class GroupRepository;

class DatabaseWorker : public QObject {
    Q_OBJECT
public:
    explicit DatabaseWorker(const QString& dbPath, QObject* parent = nullptr);
    ~DatabaseWorker() override;

    void setLocalClientId(const QString& id) { m_localClientId = id; }

public slots:
    void loadAll();
    void loadMessagesForConversation(const QString& conversationId);
    void loadMessagesBefore(const QString& conversationId,
                            const QString& beforeMessageId,
                            int limit);

    void persistMessage(const ChatMessage& msg);
    void persistConversation(const ConversationSummary& summary);
    void persistDeliveryState(const QString& messageId, int state);
    void persistConversationFlag(const QString& conversationId, int flag, bool value);
    void persistGroupMembers(const QString& groupId, const std::vector<GroupMember>& members);
    void persistReadReceipt(const QString& messageId, const QString& readerId, qint64 readAtMs);
    void persistMessageRecall(const QString& messageId, qint64 recalledAtMs);
    void persistMessageEdit(const QString& messageId, const QString& newBody,
                            qint64 editedAtMs, const QString& editorId);
    void persistGroupReadCount(const QString& messageId, int count);

    void runWalCheckpoint();

    void flushReadReceipts(const QString& conversationId,
                           const QString& localClientId,
                           bool isGroup);

signals:
    void allDataLoaded(QVector<ConversationSummary> conversations,
                       QSet<QString> unreadIds,
                       QHash<QString, Group> groups,
                       QHash<QString, std::vector<GroupMember>> groupMembers,
                       QVector<PeerEndpoint> knownPeers);
    void messagesLoaded(const QString& conversationId, std::vector<ChatMessage> messages);
    void recentMessagesLoaded(const QString& conversationId,
                              std::vector<ChatMessage> messages,
                              bool hasMoreBefore);
    void olderMessagesLoaded(const QString& conversationId,
                             QString beforeMessageId,
                             std::vector<ChatMessage> messages,
                             bool hasMoreBefore);
    void persistError(const QString& operation, const QString& detail);
    void readReceiptsFlushed(const QString& conversationId,
                             QVector<QPair<QString, QString>> targets,
                             bool unreadConsumed);

private:
    QString m_dbPath;
    QString m_connectionName;
    QString m_localClientId;
    std::unique_ptr<ConversationRepository> m_conversationRepo;
    std::unique_ptr<GroupRepository> m_groupRepo;

    void ensureConnection();
};
