#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QTextEdit>
#include <QTest>
#include <vector>

#include "ui/ContactListModel.h"
#include "ui/ConversationListModel.h"
#include "ui/MainWindow.h"
#include "ui/MessageListModel.h"
#include "ui/TransferListModel.h"

namespace {

QPushButton* findNavButton(MainWindow& window, const QString& label)
{
    const auto buttons = window.findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {
        if (button->objectName() == QStringLiteral("navBtn") && button->text().contains(label)) {
            return button;
        }
    }
    return nullptr;
}

bool saveWidgetShot(QWidget& widget, const QString& path)
{
    QImage image(widget.size() * widget.devicePixelRatioF(),
                 QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(widget.devicePixelRatioF());
    image.fill(Qt::transparent);

    QPainter painter(&image);
    widget.render(&painter);
    painter.end();

    QDir().mkpath(QFileInfo(path).absolutePath());
    return image.save(path);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    const QString outputDir =
        argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("visual-review");

    MainWindow window;
    ContactListModel contactModel;
    ConversationListModel conversationModel;
    MessageListModel messageModel;
    TransferListModel transferModel;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    window.resize(1366, 860);
    window.setAvatarText(QStringLiteral("H"));
    window.setNavUnreadCount(12);
    window.setNavGroupUnreadCount(3);
    QVector<ConversationSummary> conversations = {
        {L"group:project",
         L"\u9879\u76EE\u8BA8\u8BBA\u7EC4",
         L"\u7B54\u8FA9\u7A3F\u548C\u6F14\u793A\u8282\u594F\u5DF2\u540C\u6B65",
         nowMs - 45000,
         true,
         true,
         false,
         false,
         true},
        {L"direct:lisi",
         L"\u674E\u56DB",
         L"\u6211\u4E0B\u5348\u628A\u6700\u540E\u4E00\u7248\u7A3F\u5B50\u53D1\u4F60",
         nowMs - 240000,
         false,
         false,
         false,
         false,
         false},
        {L"group:ops",
         L"\u53D1\u5E03\u5BF9\u63A5\u7FA4",
         L"\u665A\u4E0A 8 \u70B9\u8FDB\u884C\u4E00\u6B21\u8FDE\u901A\u9A8C\u8BC1",
         nowMs - 7200000,
         false,
         false,
         true,
         false,
         false},
    };
    conversationModel.setItems(std::move(conversations));
    conversationModel.setUnreadConversationIds(QSet<QString>{QStringLiteral("group:project")});
    window.setConversationModel(&conversationModel);
    window.setSelectedConversationId(QStringLiteral("group:project"));

    QVector<PeerEndpoint> contacts = {
        {std::string("zhangsan"), std::string("\xE5\xBC\xA0\xE4\xB8\x89"), std::string("192.0.2.3"), 9527, true},
        {std::string("lisi"), std::string("\xE6\x9D\x8E\xE5\x9B\x9B"), std::string("192.0.2.8"), 9527, true},
        {std::string("wangwu"), std::string("\xE7\x8E\x8B\xE4\xBA\x94"), std::string("192.0.2.12"), 9527, false},
    };
    contactModel.setItems(std::move(contacts));
    window.setContactModel(&contactModel);
    window.setSelectedContactId(QStringLiteral("lisi"));

    QVector<TransferListItem> transfers = {
        {QStringLiteral("task-001"),
         QStringLiteral("leyochat-demo-walkthrough-v3.pdf"),
         QStringLiteral("\u4F20\u8F93\u4E2D"),
         QStringLiteral("\u53D1\u9001\u7ED9 \u674E\u56DB  \u00B7  72%"),
         QStringLiteral("\u674E\u56DB"),
         QStringLiteral("C:/demo/leyochat-demo-walkthrough-v3.pdf"),
         FileTransferDirection::Outgoing,
         FileTransferState::Transferring,
         false,
         true,
         false},
        {QStringLiteral("task-002"),
         QStringLiteral("industrial-site-layout.png"),
         QStringLiteral("\u5DF2\u5B8C\u6210"),
         QStringLiteral("\u6765\u81EA \u5F20\u4E09  \u00B7  14.2 MB"),
         QStringLiteral("\u5F20\u4E09"),
         QStringLiteral("C:/demo/industrial-site-layout.png"),
         FileTransferDirection::Incoming,
         FileTransferState::Completed,
         true,
         true,
         false},
        {QStringLiteral("task-003"),
         QStringLiteral("review-notes.docx"),
         QStringLiteral("\u5DF2\u4E2D\u65AD"),
         QStringLiteral("\u53D1\u9001\u7ED9 \u8D75\u516D  \u00B7  \u53EF\u91CD\u8BD5"),
         QStringLiteral("\u8D75\u516D"),
         QStringLiteral("C:/demo/review-notes.docx"),
         FileTransferDirection::Outgoing,
         FileTransferState::Interrupted,
         false,
         true,
         true},
    };
    transferModel.setItems(std::move(transfers));
    window.setTransferModel(&transferModel);
    window.setSelectedTransferId(QStringLiteral("task-001"));
    messageModel.setDisplayContext(QStringLiteral("local"), QString());
    messageModel.setGroupMemberNames({{QStringLiteral("local"), QStringLiteral("\u6211")},
                                      {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09")},
                                      {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB")}});

    std::vector<ChatMessage> sampleMessages = {
        {L"msg-001",
         L"group:project",
         L"zhangsan",
         L"\u7B54\u8FA9\u7A3F\u6211\u5DF2\u7ECF\u6574\u7406\u597D\u4E86\uFF0C\u4F60\u4EEC\u53EF\u4EE5\u76F4\u63A5\u5728\u8FD9\u4E2A\u7248\u672C\u4E0A\u7EE7\u7EED\u8865\u5145\u3002",
         nowMs - 180000,
         MessageDeliveryState::Read,
         L"",
         L""},
        {L"msg-002",
         L"group:project",
         L"local",
         L"\u6211\u628A\u9884\u6F14\u6240\u9700\u7684\u6750\u6599\u5305\u8FDB\u6765\u4E86\uFF0C\u4F60\u4EEC\u76F4\u63A5\u770B\u6587\u4EF6\u5361\u7247\u5373\u53EF\u3002",
         nowMs - 120000,
         MessageDeliveryState::Sent,
         L"leyochat-demo-walkthrough-v3.pdf",
         L"C:\\demo\\leyochat-demo-walkthrough-v3.pdf"},
        {L"msg-003",
         L"group:project",
         L"lisi",
         L"\u597D\u7684\uFF0C\u6211\u4F1A\u5728 15:00 \u524D\u628A\u6700\u540E\u4E00\u7248\u64AD\u7A3F\u548C\u6F14\u793A\u8282\u594F\u518D\u8FC7\u4E00\u904D\u3002",
         nowMs - 45000,
         MessageDeliveryState::Received,
         L"",
         L""},
    };
    messageModel.setItems(std::move(sampleMessages));
    window.setMessageModel(&messageModel);
    window.show();
    window.raise();
    QTest::qWait(420);

    if (!saveWidgetShot(window, QDir(outputDir).filePath(QStringLiteral("welcome.png")))) {
        return 1;
    }

    const GroupMemberListEntries reviewGroupMembers = {
        {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false, QString()},
        {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false, QString()},
        {QStringLiteral("wangwu"), QStringLiteral("\u738B\u4E94"), false, false, false, QString()},
        {QStringLiteral("zhaoliu"), QStringLiteral("\u8D75\u516D"), false, false, false, QString()},
    };
    window.setGroupMembers(reviewGroupMembers, false);
    window.setGroupInfoPanel(QStringLiteral("\u672C\u5468\u4E94 15:00 \u8FDB\u884C\u8054\u8C03\u6F14\u793A"),
                             reviewGroupMembers,
                             false);
    window.showGroupConversation(QStringLiteral("group:project"),
                                 QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));
    QImage draftAttachment(220, 112, QImage::Format_ARGB32_Premultiplied);
    draftAttachment.fill(QColor(QStringLiteral("#DDE8FF")));
    {
        QPainter draftPainter(&draftAttachment);
        draftPainter.fillRect(QRect(16, 16, 56, 80), QColor(QStringLiteral("#5B7BE7")));
        draftPainter.fillRect(QRect(88, 24, 108, 14), QColor(QStringLiteral("#7092F3")));
        draftPainter.fillRect(QRect(88, 50, 92, 10), QColor(QStringLiteral("#A1B7F7")));
        draftPainter.fillRect(QRect(88, 70, 68, 10), QColor(QStringLiteral("#C0D0FB")));
        draftPainter.end();
    }
    window.importScreenshotPreview(draftAttachment);
    if (QTextEdit* editor = window.findChild<QTextEdit*>()) {
        editor->append(QStringLiteral("\u6F14\u793A\u7A3F\u6240\u9700\u7684\u622A\u56FE\u6211\u4E5F\u9884\u7F6E\u5230 composer \u91CC\u4E86\u3002"));
    }
    QTest::qWait(260);
    if (!saveWidgetShot(window, QDir(outputDir).filePath(QStringLiteral("group-console.png")))) {
        return 2;
    }

    if (QPushButton* groupButton = findNavButton(window, QStringLiteral("\u7FA4\u804A"))) {
        QTest::mouseClick(groupButton, Qt::LeftButton);
        QTest::qWait(220);
        if (!saveWidgetShot(window, QDir(outputDir).filePath(QStringLiteral("group-nav.png")))) {
            return 3;
        }
    } else {
        return 4;
    }

    if (QPushButton* contactButton = findNavButton(window, QStringLiteral("\u8054\u7CFB\u4EBA"))) {
        QTest::mouseClick(contactButton, Qt::LeftButton);
        QTest::qWait(220);
        if (!saveWidgetShot(window, QDir(outputDir).filePath(QStringLiteral("contacts-nav.png")))) {
            return 5;
        }
    } else {
        return 6;
    }

    if (QPushButton* transferButton = findNavButton(window, QStringLiteral("\u4F20\u8F93"))) {
        QTest::mouseClick(transferButton, Qt::LeftButton);
        QTest::qWait(220);
        if (!saveWidgetShot(window, QDir(outputDir).filePath(QStringLiteral("transfers-nav.png")))) {
            return 7;
        }
    } else {
        return 8;
    }

    return 0;
}
