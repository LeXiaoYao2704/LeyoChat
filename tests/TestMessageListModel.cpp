#include <QtTest/QTest>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <cstdlib>

#include "domain/ChatMessage.h"
#include "domain/FileTransferTask.h"
#include "ui/MessageListModel.h"
#include "store/ChatDataStore.h"

class TestMessageListModel : public QObject {
    Q_OBJECT

private slots:
    void bodyRoleReturnsRawBody() {
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");

        ChatMessage msg;
        msg.messageId   = L"m1";
        msg.senderId    = L"client-a";
        msg.body        = L"hello world";
        msg.createdAtMs = 1712000000000LL;
        msg.deliveryState = MessageDeliveryState::Sent;
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QCOMPARE(idx.data(MessageListModel::BodyRole).toString(),
                 QStringLiteral("hello world"));
    }

    void senderNameRoleReturnsLocalAlias() {
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");

        ChatMessage msg;
        msg.messageId   = L"m2";
        msg.senderId    = L"client-a";
        msg.body        = L"hi";
        msg.createdAtMs = 1712000000000LL;
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QCOMPARE(idx.data(MessageListModel::SenderNameRole).toString(),
                 QStringLiteral("\u6211")); // "我"
    }

    void senderNameRoleReturnsPeerName() {
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob", "client-b");

        ChatMessage msg;
        msg.messageId   = L"m3";
        msg.senderId    = L"client-b";
        msg.body        = L"hey";
        msg.createdAtMs = 1712000000000LL;
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QCOMPARE(idx.data(MessageListModel::SenderNameRole).toString(),
                 QStringLiteral("Bob"));
    }

    void directConversation_unknownSenderDoesNotRenderAsSelf() {
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob", "client-b");

        ChatMessage msg;
        msg.messageId   = L"m3b";
        msg.senderId    = L"legacy-client-a";
        msg.body        = L"unknown sender message";
        msg.createdAtMs = 1712000000000LL;
        msg.deliveryState = MessageDeliveryState::Sent;
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QVERIFY(!idx.data(MessageListModel::OutgoingRole).toBool());
        QCOMPARE(idx.data(MessageListModel::SenderNameRole).toString(),
                 QStringLiteral("Bob"));
    }

    void timeLabelRoleReturnsHHmm() {
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");

        ChatMessage msg;
        msg.messageId   = L"m4";
        msg.senderId    = L"client-a";
        msg.body        = L"time test";
        msg.createdAtMs = QDateTime::currentMSecsSinceEpoch();
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        const QString label = idx.data(MessageListModel::TimeLabelRole).toString();
        // 格式必须是 "HH:mm"，长度 5，第三个字符是冒号
        QCOMPARE(label.length(), 5);
        QCOMPARE(label.at(2), QChar(':'));
    }

    void tooltipRoleIsEmptyForChatBubble() {
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");

        ChatMessage msg;
        msg.messageId = L"m4-tip";
        msg.senderId = L"client-a";
        msg.body = L"tooltip test";
        msg.createdAtMs = 1712024000000LL;
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        const QVariant tooltip = idx.data(Qt::ToolTipRole);
        QVERIFY(!tooltip.isValid() || tooltip.toString().isEmpty());
    }

    void exposesResourceReferenceRoles() {
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");

        ChatMessage msg;
        msg.messageId = L"m5";
        msg.senderId = L"client-b";
        msg.body = L"[共享资源] Spec Board";
        msg.createdAtMs = 1712000000000LL;
        msg.messageType = L"resource_ref";
        msg.payloadJson = LR"({"service_id":"svc-001","workspace_id":"ws-001","origin":"service","kind":"shared_file","resource_id":"res-001","title":"Spec Board","subtitle":"共享规格文件"})";
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QVERIFY(idx.data(MessageListModel::ResourceReferenceRole).toBool());
        QCOMPARE(idx.data(MessageListModel::MessageTypeRole).toString(), QStringLiteral("resource_ref"));
        QVERIFY(idx.data(MessageListModel::PayloadJsonRole).toString().contains(QStringLiteral("Spec Board")));
    }

    void directConversation_exposesAvatarPathsForBothSides()
    {
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("client-a"),
                                QStringLiteral("Bob"),
                                QStringLiteral("client-b"));
        model.setAvatarContext(QStringLiteral("C:/avatars/self.png"),
                               QStringLiteral("C:/avatars/bob.png"));

        ChatMessage mine;
        mine.messageId = L"self-1";
        mine.senderId = L"client-a";
        mine.body = L"hello";
        mine.createdAtMs = 1712000000000LL;

        ChatMessage peer;
        peer.messageId = L"peer-1";
        peer.senderId = L"client-b";
        peer.body = L"hi";
        peer.createdAtMs = 1712000001000LL;

        model.setItems({mine, peer});

        QCOMPARE(model.index(0, 0).data(MessageListModel::SenderAvatarPathRole).toString(),
                 QStringLiteral("C:/avatars/self.png"));
        QCOMPARE(model.index(1, 0).data(MessageListModel::SenderAvatarPathRole).toString(),
                 QStringLiteral("C:/avatars/bob.png"));
    }

    void groupConversation_exposesMemberAvatarPath()
    {
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("client-a"), QStringLiteral("项目群"));
        model.setAvatarContext(QStringLiteral("C:/avatars/self.png"), QString());
        model.setGroupMemberNames({{QStringLiteral("client-a"), QStringLiteral("我")},
                                   {QStringLiteral("client-c"), QStringLiteral("陈宇")}});
        model.setGroupMemberAvatars({{QStringLiteral("client-a"), QStringLiteral("C:/avatars/self.png")},
                                     {QStringLiteral("client-c"), QStringLiteral("C:/avatars/chen.png")}});

        ChatMessage peer;
        peer.messageId = L"group-1";
        peer.senderId = L"client-c";
        peer.body = L"group hi";
        peer.createdAtMs = 1712000000000LL;
        model.setItems({peer});

        QCOMPARE(model.index(0, 0).data(MessageListModel::SenderAvatarPathRole).toString(),
                 QStringLiteral("C:/avatars/chen.png"));
    }

    void fileMessage_exposesTransferVisualState()
    {
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("client-a"), QStringLiteral("Bob"));

        ChatMessage fileMessage;
        fileMessage.messageId = L"task-1";
        fileMessage.senderId = L"client-a";
        fileMessage.body = L"[File] demo.iso";
        fileMessage.attachmentName = L"demo.iso";
        fileMessage.localFilePath = L"C:/files/demo.iso";
        fileMessage.createdAtMs = 1712000000000LL;
        model.setItems({fileMessage});

        MessageListModel::TransferVisualState transferState;
        transferState.taskId = QStringLiteral("task-1");
        transferState.state = FileTransferState::Transferring;
        transferState.bytesCompleted = 64 * 1024 * 1024;
        transferState.fileSize = 256 * 1024 * 1024;
        transferState.cancelable = true;
        model.setTransferStates({{QStringLiteral("task-1"), transferState}});

        const QModelIndex idx = model.index(0, 0);
        QCOMPARE(idx.data(MessageListModel::TransferTaskIdRole).toString(),
                 QStringLiteral("task-1"));
        QCOMPARE(idx.data(MessageListModel::TransferStateRole).toInt(),
                 static_cast<int>(FileTransferState::Transferring));
        QCOMPARE(idx.data(MessageListModel::TransferBytesCompletedRole).toLongLong(),
                 64ll * 1024 * 1024);
        QCOMPARE(idx.data(MessageListModel::TransferFileSizeRole).toLongLong(),
                 256ll * 1024 * 1024);
        QVERIFY(idx.data(MessageListModel::TransferCancelableRole).toBool());
    }

    void localFilePathRole_fallsBackToGroupFileCardLocalPath()
    {
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("client-a"), QStringLiteral("Bob"), QStringLiteral("client-b"));

        QJsonObject fileCard;
        fileCard[QStringLiteral("channel")] = QStringLiteral("fileservice");
        fileCard[QStringLiteral("file_name")] = QStringLiteral("design.pdf");
        fileCard[QStringLiteral("local_path")] = QStringLiteral("C:/Downloads/LeyoChat/Received/Bob/design.pdf");

        ChatMessage msg;
        msg.messageId = L"gfc-local-path";
        msg.senderId = L"client-b";
        msg.body = L"[群文件] design.pdf";
        msg.createdAtMs = 1712000000000LL;
        msg.deliveryState = MessageDeliveryState::Received;
        msg.messageType = L"group_file_card";
        msg.fileCardJson = QString::fromUtf8(QJsonDocument(fileCard).toJson(QJsonDocument::Compact)).toStdWString();
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QCOMPARE(idx.data(MessageListModel::LocalFilePathRole).toString(),
                 QStringLiteral("C:/Downloads/LeyoChat/Received/Bob/design.pdf"));
    }

    void localFilePathRole_outgoingGroupFileCardFallsBackToSenderFilePath()
    {
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("client-a"), QStringLiteral("Bob"), QStringLiteral("client-b"));

        QJsonObject fileCard;
        fileCard[QStringLiteral("channel")] = QStringLiteral("fileservice");
        fileCard[QStringLiteral("file_name")] = QStringLiteral("LeyoChat.exe");
        fileCard[QStringLiteral("sender_file_path")] = QStringLiteral("D:/build/LeyoChat.exe");

        ChatMessage msg;
        msg.messageId = L"gfc-sender-path";
        msg.senderId = L"client-a";
        msg.body = L"[群文件] LeyoChat.exe";
        msg.createdAtMs = 1712000000000LL;
        msg.deliveryState = MessageDeliveryState::Sent;
        msg.messageType = L"group_file_card";
        msg.fileCardJson = QString::fromUtf8(QJsonDocument(fileCard).toJson(QJsonDocument::Compact)).toStdWString();
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QCOMPARE(idx.data(MessageListModel::LocalFilePathRole).toString(),
                 QStringLiteral("D:/build/LeyoChat.exe"));
    }

    void recalledRole_reflectsIsRecalled()
    {
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");

        ChatMessage msg;
        msg.messageId     = L"m-recalled";
        msg.senderId      = L"client-b";
        msg.body          = L"this message was recalled";
        msg.createdAtMs   = 1712000000000LL;
        msg.isRecalled    = true;
        msg.recalledAtMs  = 1712000001000LL;
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QVERIFY(idx.data(MessageListModel::RecalledRole).toBool());
    }

    void editedRole_trueWhenEditedAtMsNonZero()
    {
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");

        ChatMessage msg;
        msg.messageId   = L"m-edited";
        msg.senderId    = L"client-b";
        msg.body        = L"edited content";
        msg.createdAtMs = 1712000000000LL;
        msg.editedAtMs  = 1712000001000LL;
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QVERIFY(idx.data(MessageListModel::EditedRole).toBool());
    }

    void editedRole_falseWhenEditedAtMsZero()
    {
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");

        ChatMessage msg;
        msg.messageId   = L"m-not-edited";
        msg.senderId    = L"client-b";
        msg.body        = L"normal message";
        msg.createdAtMs = 1712000000000LL;
        msg.editedAtMs  = 0;
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QVERIFY(!idx.data(MessageListModel::EditedRole).toBool());
    }

    void createdAtRole_returnsCreatedAtMs()
    {
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");

        ChatMessage msg;
        msg.messageId   = L"m-ts";
        msg.senderId    = L"client-b";
        msg.body        = L"timestamp test";
        msg.createdAtMs = 1712345678901LL;
        model.setItems({msg});

        const QModelIndex idx = model.index(0, 0);
        QCOMPARE(idx.data(MessageListModel::CreatedAtRole).toLongLong(),
                 static_cast<long long>(1712345678901LL));
    }

    void testBindToStoreAndSwitchConversation()
    {
        ChatDataStore store;
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");
        model.bindToStore(&store);

        ChatMessage msg;
        msg.messageId   = L"s1";
        msg.senderId    = L"client-a";
        msg.body        = L"store hello";
        msg.createdAtMs = 1712000000000LL;
        store.appendMessage(QStringLiteral("conv-1"), msg);

        ChatMessage msg2;
        msg2.messageId   = L"s2";
        msg2.senderId    = L"client-b";
        msg2.body        = L"store reply";
        msg2.createdAtMs = 1712000001000LL;
        store.appendMessage(QStringLiteral("conv-1"), msg2);

        model.switchToConversation(QStringLiteral("conv-1"));
        QCOMPARE(model.rowCount(), 2);

        model.switchToConversation(QStringLiteral("conv-nonexistent"));
        QCOMPARE(model.rowCount(), 0);
    }

    void testIncrementalInsert()
    {
        ChatDataStore store;
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");
        model.bindToStore(&store);

        ChatMessage msg;
        msg.messageId   = L"s1";
        msg.senderId    = L"client-a";
        msg.body        = L"first";
        msg.createdAtMs = 1712000000000LL;
        store.appendMessage(QStringLiteral("conv-1"), msg);

        model.switchToConversation(QStringLiteral("conv-1"));
        QCOMPARE(model.rowCount(), 1);

        QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

        ChatMessage msg2;
        msg2.messageId   = L"s2";
        msg2.senderId    = L"client-b";
        msg2.body        = L"second";
        msg2.createdAtMs = 1712000001000LL;
        store.appendMessage(QStringLiteral("conv-1"), msg2);

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(insertSpy.count(), 1);
        QCOMPARE(resetSpy.count(), 0);
    }

    void testIncrementalUpdate()
    {
        ChatDataStore store;
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");
        model.bindToStore(&store);

        ChatMessage msg;
        msg.messageId     = L"s1";
        msg.senderId      = L"client-a";
        msg.body          = L"pending";
        msg.createdAtMs   = 1712000000000LL;
        msg.deliveryState = MessageDeliveryState::Pending;
        store.appendMessage(QStringLiteral("conv-1"), msg);

        model.switchToConversation(QStringLiteral("conv-1"));

        QSignalSpy changeSpy(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

        store.updateDeliveryState(QStringLiteral("conv-1"),
                                  QStringLiteral("s1"),
                                  MessageDeliveryState::Sent);

        QCOMPARE(changeSpy.count(), 1);
        QCOMPARE(resetSpy.count(), 0);
    }

    void testAttachmentPathUpdateRequestsSizeHintRefresh()
    {
        ChatDataStore store;
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("client-a"), QStringLiteral("Bob"));
        model.bindToStore(&store);

        ChatMessage message;
        message.messageId = L"image-path-update";
        message.senderId = L"client-b";
        message.body = L"[图片]";
        message.attachmentName = L"delayed-preview.png";
        message.createdAtMs = 1712000000000LL;
        message.deliveryState = MessageDeliveryState::Received;
        store.appendMessage(QStringLiteral("conv-image"), message);
        model.switchToConversation(QStringLiteral("conv-image"));

        QSignalSpy changeSpy(&model, &QAbstractItemModel::dataChanged);

        message.localFilePath = L"C:/received/delayed-preview.png";
        store.updateMessage(QStringLiteral("conv-image"), message);

        bool requestedSizeHintRefresh = false;
        for (const QList<QVariant>& emission : changeSpy) {
            const QList<int> roles = emission.at(2).value<QList<int>>();
            if (roles.contains(Qt::SizeHintRole)) {
                requestedSizeHintRefresh = true;
                break;
            }
        }
        QVERIFY2(requestedSizeHintRefresh,
                 "attachment path changes must request a QListView row-height refresh");
    }

    void testIgnoreOtherConversation()
    {
        ChatDataStore store;
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob");
        model.bindToStore(&store);

        ChatMessage msg;
        msg.messageId   = L"s1";
        msg.senderId    = L"client-a";
        msg.body        = L"conv1 msg";
        msg.createdAtMs = 1712000000000LL;
        store.appendMessage(QStringLiteral("conv-1"), msg);

        model.switchToConversation(QStringLiteral("conv-1"));
        QCOMPARE(model.rowCount(), 1);

        QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);

        ChatMessage msg2;
        msg2.messageId   = L"other-1";
        msg2.senderId    = L"client-c";
        msg2.body        = L"other conv";
        msg2.createdAtMs = 1712000002000LL;
        store.appendMessage(QStringLiteral("conv-2"), msg2);

        QCOMPARE(insertSpy.count(), 0);
        QCOMPARE(model.rowCount(), 1);
    }

    void testSetDisplayContextNoopDoesNotReset()
    {
        MessageListModel model;
        model.setDisplayContext("client-a", "Bob", "client-b");

        ChatMessage msg;
        msg.messageId = L"m-ctx-1";
        msg.senderId = L"client-a";
        msg.body = L"same";
        msg.createdAtMs = 1712000000000LL;
        model.setItems({msg});

        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

        model.setDisplayContext("client-a", "Bob", "client-b");
        model.setItems({msg});

        QCOMPARE(resetSpy.count(), 0);
    }

    void testUnknownDirectSenderIsNotTreatedAsLocalMessage()
    {
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("client-a"),
                                QStringLiteral("Bob"),
                                QStringLiteral("client-b"));

        ChatMessage msg;
        msg.messageId = L"m-unknown-sender";
        msg.senderId = L"shared-service-identity";
        msg.body = L"incoming";
        msg.createdAtMs = 1712000000000LL;
        model.setItems({msg});

        QCOMPARE(model.index(0, 0).data(MessageListModel::OutgoingRole).toBool(),
                 false);
    }
};

// 使用堆分配避免 Qt 对象析构时的崩溃（本环境已知问题）
int main(int argc, char** argv) {
    auto* app = new QCoreApplication(argc, argv);
    auto* tc  = new TestMessageListModel();
    const int result = QTest::qExec(tc, argc, argv);
    Q_UNUSED(app)
    std::_Exit(result);
}

#include "TestMessageListModel.moc"
