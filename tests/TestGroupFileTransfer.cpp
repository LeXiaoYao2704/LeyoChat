// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增382行/修改10行/删除0行; 总行数382行
// @AI-LastModified: 2026-04-16 09:17:10

#include <QtTest/QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QUuid>

#include "domain/ChatMessage.h"
#include "fileservice/FileServiceDatabase.h"
#include "fileservice/FileStorageManager.h"
#include "services/GroupService.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"
#include "storage/GroupRepository.h"
#include "ui/MessageListModel.h"

// ─── 测试类 ──────────────────────────────────────────────────────────

class TestGroupFileTransfer : public QObject {
    Q_OBJECT

    static QString uniqueConn()
    {
        return QStringLiteral("gft-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

private slots:

    //
    // ===== 1. FileServiceDatabase — chat_files 表 =====
    //

    void chatFiles_insertAndFind()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString conn = uniqueConn();
        FileServiceDatabase db(dir.filePath(QStringLiteral("cf.db")), conn);
        QVERIFY(db.open());

        ChatFileRecord rec;
        rec.chatFileId   = QStringLiteral("cf-001");
        rec.workspaceId  = QStringLiteral("ws-alpha");
        rec.fileName     = QStringLiteral("设计文档.pdf");
        rec.fileHash     = QStringLiteral("abc123");
        rec.uploaderId   = QStringLiteral("user-a");
        rec.uploaderName = QStringLiteral("张三");
        rec.fileSize     = 1024 * 100;
        rec.createdAtMs  = 1712800000000LL;
        rec.storagePath  = QStringLiteral("chat-files/cf-001/设计文档.pdf");
        QVERIFY(db.insertChatFile(rec));

        const auto found = db.findChatFileById(QStringLiteral("cf-001"));
        QVERIFY(found.has_value());
        QCOMPARE(found->chatFileId,   QStringLiteral("cf-001"));
        QCOMPARE(found->workspaceId,  QStringLiteral("ws-alpha"));
        QCOMPARE(found->fileName,     QStringLiteral("设计文档.pdf"));
        QCOMPARE(found->fileHash,     QStringLiteral("abc123"));
        QCOMPARE(found->uploaderId,   QStringLiteral("user-a"));
        QCOMPARE(found->uploaderName, QStringLiteral("张三"));
        QCOMPARE(found->fileSize,     qint64(1024 * 100));
        QCOMPARE(found->createdAtMs,  qint64(1712800000000LL));
        QCOMPARE(found->storagePath,  QStringLiteral("chat-files/cf-001/设计文档.pdf"));
    }

    void chatFiles_findReturnsNulloptWhenMissing()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileServiceDatabase db(dir.filePath(QStringLiteral("cf2.db")), uniqueConn());
        QVERIFY(db.open());

        const auto found = db.findChatFileById(QStringLiteral("nonexistent"));
        QVERIFY(!found.has_value());
    }

    void chatFiles_duplicateInsertFails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileServiceDatabase db(dir.filePath(QStringLiteral("cf3.db")), uniqueConn());
        QVERIFY(db.open());

        ChatFileRecord rec;
        rec.chatFileId   = QStringLiteral("cf-dup");
        rec.workspaceId  = QStringLiteral("ws-1");
        rec.fileName     = QStringLiteral("test.bin");
        rec.fileHash     = QStringLiteral("hash-dup");
        rec.uploaderId   = QStringLiteral("u1");
        rec.uploaderName = QStringLiteral("测试用户");
        rec.fileSize     = 100;
        rec.createdAtMs  = 1000;
        rec.storagePath  = QStringLiteral("chat-files/cf-dup/test.bin");
        QVERIFY(db.insertChatFile(rec));
        QVERIFY(!db.insertChatFile(rec)); // 重复主键应失败
    }

    //
    // ===== 2. FileStorageManager — readFileRange / fileSize =====
    //

    void fileRange_fullRead()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileStorageManager mgr(dir.path());

        // 写入一个测试文件
        const QByteArray content = QByteArrayLiteral("Hello, LeyoChat World! 你好世界");
        const auto relPath = mgr.saveFile(QStringLiteral("f1"), QStringLiteral("v1"), content);
        QVERIFY(relPath.has_value());

        // 完整读取
        const auto result = mgr.readFileRange(*relPath, 0, -1);
        QVERIFY(result.has_value());
        QCOMPARE(result->data, content);
        QCOMPARE(result->totalSize, static_cast<qint64>(content.size()));
        QCOMPARE(result->rangeStart, qint64(0));
        QCOMPARE(result->rangeEnd, static_cast<qint64>(content.size() - 1));
    }

    void fileRange_partialRead()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileStorageManager mgr(dir.path());

        const QByteArray content = QByteArrayLiteral("ABCDEFGHIJ");
        const auto relPath = mgr.saveFile(QStringLiteral("f2"), QStringLiteral("v1"), content);
        QVERIFY(relPath.has_value());

        // 读取 offset=3, length=4 → "DEFG"
        const auto result = mgr.readFileRange(*relPath, 3, 4);
        QVERIFY(result.has_value());
        QCOMPARE(result->data, QByteArrayLiteral("DEFG"));
        QCOMPARE(result->rangeStart, qint64(3));
        QCOMPARE(result->rangeEnd, qint64(6));
        QCOMPARE(result->totalSize, qint64(10));
    }

    void fileRange_offsetExceedsSize_returnsEmptyOrNullopt()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileStorageManager mgr(dir.path());

        const QByteArray content = QByteArrayLiteral("ABC");
        const auto relPath = mgr.saveFile(QStringLiteral("f3"), QStringLiteral("v1"), content);
        QVERIFY(relPath.has_value());

        const auto result = mgr.readFileRange(*relPath, 100, 10);
        // 实现可能返回 nullopt 或返回空 data，两者都可接受
        if (result.has_value()) {
            QVERIFY(result->data.isEmpty());
        }
    }

    void fileSize_returnsCorrectSize()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileStorageManager mgr(dir.path());

        const QByteArray content(512, 'X');
        const auto relPath = mgr.saveFile(QStringLiteral("f4"), QStringLiteral("v1"), content);
        QVERIFY(relPath.has_value());

        QCOMPARE(mgr.fileSize(*relPath), qint64(512));
    }

    void fileSize_nonexistent_returnsNegative()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileStorageManager mgr(dir.path());

        QVERIFY(mgr.fileSize(QStringLiteral("no-such-file")) < 0);
    }

    //
    // ===== 3. GroupService — buildGroupFileOfferFanOut =====
    //

    void fileOfferFanOut_broadcastsToAllRecipients()
    {
        const QString conn = uniqueConn();
        DatabaseManager databaseManager(QStringLiteral(":memory:"), conn);
        QVERIFY(databaseManager.open());

        GroupRepository groupRepo(conn);
        ConversationRepository convRepo(conn);
        GroupService service(&groupRepo, &convRepo);

        Group group;
        QVERIFY(service.createGroup(QStringLiteral("sender-001"),
                                    QStringLiteral("文件测试群"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003"),
                                     QStringLiteral("user-004")},
                                    &group));

        // 创建临时文件来模拟发送
        QTemporaryFile tmpFile;
        tmpFile.setFileTemplate(QStringLiteral("TestFile_XXXXXX.zip"));
        QVERIFY(tmpFile.open());
        tmpFile.write(QByteArray(2048, 'A'));
        tmpFile.flush();

        const auto envelopes = service.buildGroupFileOfferFanOut(
            QStringLiteral("sender-001"),
            QString::fromStdWString(group.groupId),
            tmpFile.fileName(),
            QStringLiteral("王五"));

        // 应广播给 3 个成员（不含发送者）
        QCOMPARE(envelopes.size(), static_cast<std::size_t>(3));

        // 验证每个 envelope 的结构
        for (const auto& env : envelopes) {
            QCOMPARE(env.type, MessageType::GroupMessage);
            QCOMPARE(QString::fromStdString(env.messageSubtype), QStringLiteral("group_file_card"));
            QCOMPARE(QString::fromStdString(env.senderId), QStringLiteral("sender-001"));
            QVERIFY(!env.payloadJson.empty());

            // 解析 payloadJson
            const QJsonObject card = QJsonDocument::fromJson(
                QByteArray::fromStdString(env.payloadJson)).object();
            QCOMPARE(card.value(QStringLiteral("channel")).toString(), QStringLiteral("p2p"));
            QVERIFY(!card.value(QStringLiteral("file_name")).toString().isEmpty());
            QCOMPARE(card.value(QStringLiteral("file_size")).toInteger(), qint64(2048));
            QCOMPARE(card.value(QStringLiteral("sender_id")).toString(), QStringLiteral("sender-001"));
            QCOMPARE(card.value(QStringLiteral("sender_file_path")).toString(), tmpFile.fileName());
            QCOMPARE(card.value(QStringLiteral("uploader_name")).toString(), QStringLiteral("王五"));
            QCOMPARE(card.value(QStringLiteral("download_state")).toString(), QStringLiteral("none"));
        }

        // 验证消息体包含 [群文件] 前缀
        QVERIFY(QString::fromStdString(envelopes.front().body).contains(QStringLiteral("[群文件]")));

        // 验证所有 envelope 共享同一个 messageId（同一条群消息）
        QCOMPARE(envelopes[0].messageId, envelopes[1].messageId);
        QCOMPARE(envelopes[1].messageId, envelopes[2].messageId);

        // 验证 targetId 各不相同
        QSet<QString> targets;
        for (const auto& env : envelopes) {
            targets.insert(QString::fromStdString(env.targetId));
        }
        QCOMPARE(targets.size(), 3);
        QVERIFY(!targets.contains(QStringLiteral("sender-001")));
    }

    void fileOfferFanOut_emptyGroupReturnsEmpty()
    {
        const QString conn = uniqueConn();
        DatabaseManager databaseManager(QStringLiteral(":memory:"), conn);
        QVERIFY(databaseManager.open());

        GroupRepository groupRepo(conn);
        ConversationRepository convRepo(conn);
        GroupService service(&groupRepo, &convRepo);

        // 只有创建者一人的"群" — activeRecipients 排除自己 → 空
        Group group;
        QVERIFY(service.createGroup(QStringLiteral("lone-user"),
                                    QStringLiteral("孤独群"),
                                    {}, // 无其他成员
                                    &group));

        QTemporaryFile tmpFile;
        QVERIFY(tmpFile.open());
        tmpFile.write("x");
        tmpFile.flush();

        const auto envelopes = service.buildGroupFileOfferFanOut(
            QStringLiteral("lone-user"),
            QString::fromStdWString(group.groupId),
            tmpFile.fileName(),
            QStringLiteral("独行者"));

        QVERIFY(envelopes.empty());
    }

    //
    // ===== 4. ConversationRepository — fileCardJson 读写 =====
    //

    void fileCardJson_appendAndLoad()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString conn = uniqueConn();
        DatabaseManager mgr(dir.filePath(QStringLiteral("fcj.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-x", L"群聊A", L"last", 1000}));

        QJsonObject card;
        card[QStringLiteral("channel")]   = QStringLiteral("fileservice");
        card[QStringLiteral("file_id")]   = QStringLiteral("fid-001");
        card[QStringLiteral("file_name")] = QStringLiteral("report.pdf");
        const QString cardStr = QString::fromUtf8(
            QJsonDocument(card).toJson(QJsonDocument::Compact));

        ChatMessage msg;
        msg.messageId      = L"msg-fc1";
        msg.conversationId = L"conv-x";
        msg.senderId       = L"user-a";
        msg.body           = L"[群文件] report.pdf";
        msg.createdAtMs    = 2000;
        msg.deliveryState  = MessageDeliveryState::Received;
        msg.messageType    = L"group_file_card";
        msg.fileCardJson   = cardStr.toStdWString();
        QVERIFY(repo.appendMessage(msg));

        // 通过 findMessageById 加载并验证
        ChatMessage loaded;
        QVERIFY(repo.findMessageById(QStringLiteral("msg-fc1"), &loaded));
        QCOMPARE(QString::fromStdWString(loaded.fileCardJson), cardStr);
    }

    void fileCardJson_updateAndReload()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString conn = uniqueConn();
        DatabaseManager mgr(dir.filePath(QStringLiteral("fcj2.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-y", L"群聊B", L"last", 1000}));

        ChatMessage msg;
        msg.messageId      = L"msg-fc2";
        msg.conversationId = L"conv-y";
        msg.senderId       = L"user-b";
        msg.body           = L"[群文件] data.csv";
        msg.createdAtMs    = 3000;
        msg.deliveryState  = MessageDeliveryState::Received;
        msg.fileCardJson   = L"{\"download_state\":\"none\"}";
        QVERIFY(repo.appendMessage(msg));

        // 更新 file_card_json
        const QString newCardStr = QStringLiteral(R"({"download_state":"completed","local_path":"C:/temp/data.csv"})");
        QVERIFY(repo.updateMessageFileCardJson(QStringLiteral("msg-fc2"), newCardStr));

        ChatMessage reloaded;
        QVERIFY(repo.findMessageById(QStringLiteral("msg-fc2"), &reloaded));
        const QJsonObject reloadedCard = QJsonDocument::fromJson(
            QString::fromStdWString(reloaded.fileCardJson).toUtf8()).object();
        QCOMPARE(reloadedCard.value(QStringLiteral("download_state")).toString(),
                 QStringLiteral("completed"));
        QCOMPARE(reloadedCard.value(QStringLiteral("local_path")).toString(),
                 QStringLiteral("C:/temp/data.csv"));
    }

    void fileCardJson_emptyByDefault()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString conn = uniqueConn();
        DatabaseManager mgr(dir.filePath(QStringLiteral("fcj3.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-z", L"群聊C", L"last", 1000}));

        ChatMessage msg;
        msg.messageId      = L"msg-fc3";
        msg.conversationId = L"conv-z";
        msg.senderId       = L"user-c";
        msg.body           = L"普通消息";
        msg.createdAtMs    = 4000;
        msg.deliveryState  = MessageDeliveryState::Received;
        // fileCardJson 不设置（空默认）
        QVERIFY(repo.appendMessage(msg));

        ChatMessage loaded;
        QVERIFY(repo.findMessageById(QStringLiteral("msg-fc3"), &loaded));
        QVERIFY(QString::fromStdWString(loaded.fileCardJson).trimmed().isEmpty());
    }

    //
    // ===== 5. MessageListModel — FileCardJsonRole =====
    //

    void fileCardJsonRole_returnsCardJson()
    {
        MessageListModel model;
        model.setDisplayContext("user-a", "Bob");

        QJsonObject card;
        card[QStringLiteral("channel")]   = QStringLiteral("p2p");
        card[QStringLiteral("file_name")] = QStringLiteral("test.zip");
        const QString cardStr = QString::fromUtf8(
            QJsonDocument(card).toJson(QJsonDocument::Compact));

        ChatMessage msg;
        msg.messageId    = L"m-fc1";
        msg.senderId     = L"user-a";
        msg.body         = L"[群文件] test.zip";
        msg.createdAtMs  = 1712000000000LL;
        msg.fileCardJson = cardStr.toStdWString();
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QCOMPARE(idx.data(MessageListModel::FileCardJsonRole).toString(), cardStr);
    }

    void fileCardJsonRole_emptyWhenNotSet()
    {
        MessageListModel model;
        model.setDisplayContext("user-a", "Bob");

        ChatMessage msg;
        msg.messageId   = L"m-fc2";
        msg.senderId    = L"user-a";
        msg.body        = L"普通消息";
        msg.createdAtMs = 1712000000000LL;
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QVERIFY(idx.data(MessageListModel::FileCardJsonRole).toString().isEmpty());
    }

    //
    // ===== 6. FileStorageManager — saveFileFromPath =====
    //

    void saveFileFromPath_copiesFile()
    {
        QTemporaryDir storageDir;
        QVERIFY(storageDir.isValid());
        FileStorageManager mgr(storageDir.path());

        // 创建临时源文件
        QTemporaryFile srcFile;
        QVERIFY(srcFile.open());
        const QByteArray content(4096, 'Z');
        srcFile.write(content);
        srcFile.flush();

        const auto relPath = mgr.saveFileFromPath(QStringLiteral("cf-copy-001"), srcFile.fileName());
        QVERIFY(relPath.has_value());

        // 验证通过存储路径可读回相同内容
        const auto readBack = mgr.readFile(*relPath);
        QVERIFY(readBack.has_value());
        QCOMPARE(*readBack, content);
    }
};

QTEST_MAIN(TestGroupFileTransfer)
#include "TestGroupFileTransfer.moc"
