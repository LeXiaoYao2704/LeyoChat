#include <QApplication>
#include <QImage>
#include <QStyleOptionViewItem>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QPushButton>
#include <QtTest/QTest>

#include <algorithm>

#include "ui/MessageBubbleDelegate.h"
#include "ui/MessageBubbleWidget.h"
#include "ui/MessageListModel.h"

class TestMessageBubbleWidget : public QObject {
    Q_OBJECT

private slots:
    void pure_image_bubble_respects_available_width();
    void image_transfer_message_uses_image_style_without_file_chips();
    void image_size_hint_recomputes_when_file_becomes_readable();
};

void TestMessageBubbleWidget::pure_image_bubble_respects_available_width()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString imagePath = tempDir.filePath(QStringLiteral("preview.png"));
    QImage image(1600, 1200, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(QStringLiteral("#4A90E2")));
    QVERIFY(image.save(imagePath));

    QStandardItemModel model(1, 1);
    QModelIndex index = model.index(0, 0);
    model.setData(index, QStringLiteral("msg-1"), MessageListModel::MessageIdRole);
    model.setData(index, true, MessageListModel::OutgoingRole);
    model.setData(index, true, MessageListModel::FileMessageRole);
    model.setData(index, QStringLiteral("preview.png"), MessageListModel::AttachmentNameRole);
    model.setData(index, imagePath, MessageListModel::LocalFilePathRole);
    model.setData(index, QStringLiteral("图片消息"), MessageListModel::BodyRole);
    model.setData(index, QStringLiteral("自己"), MessageListModel::SenderNameRole);
    model.setData(index, QStringLiteral("10:24"), MessageListModel::TimeLabelRole);
    model.setData(index, static_cast<int>(MessageDeliveryState::Sent), MessageListModel::DeliveryStateRole);

    MessageBubbleWidget widget;
    widget.setAvailableWidth(260);
    widget.populateFromIndex(index);
    widget.adjustSize();

    QVERIFY2(widget.sizeHint().width() <= 260,
             qPrintable(QStringLiteral("sizeHint width = %1").arg(widget.sizeHint().width())));
}

void TestMessageBubbleWidget::image_transfer_message_uses_image_style_without_file_chips()
{
    QStandardItemModel model(1, 1);
    const QModelIndex index = model.index(0, 0);
    model.setData(index, QStringLiteral("msg-image-transfer"), MessageListModel::MessageIdRole);
    model.setData(index, true, MessageListModel::OutgoingRole);
    model.setData(index, true, MessageListModel::FileMessageRole);
    model.setData(index, QStringLiteral("photo.png"), MessageListModel::AttachmentNameRole);
    model.setData(index, QStringLiteral(""), MessageListModel::LocalFilePathRole);
    model.setData(index, QStringLiteral("图片上传中"), MessageListModel::BodyRole);
    model.setData(index, QStringLiteral("自己"), MessageListModel::SenderNameRole);
    model.setData(index, QStringLiteral("10:25"), MessageListModel::TimeLabelRole);
    model.setData(index, static_cast<int>(MessageDeliveryState::Pending), MessageListModel::DeliveryStateRole);
    model.setData(index, QStringLiteral("transfer-task-1"), MessageListModel::TransferTaskIdRole);
    model.setData(index, static_cast<int>(FileTransferState::Transferring), MessageListModel::TransferStateRole);
    model.setData(index, static_cast<qint64>(512 * 1024), MessageListModel::TransferBytesCompletedRole);
    model.setData(index, static_cast<qint64>(1024 * 1024), MessageListModel::TransferFileSizeRole);
    model.setData(index, true, MessageListModel::TransferCancelableRole);

    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 420, 220);
    option.font = QFont(QStringLiteral("Microsoft YaHei UI"), 10);

    MessageBubbleWidget widget;
    widget.setAvailableWidth(420);
    widget.populateFromIndex(index);
    const auto buttons = widget.findChildren<QPushButton*>();
    const bool hasCancelButton = std::any_of(
        buttons.cbegin(), buttons.cend(), [](const QPushButton* button) {
            return button && button->text() == QStringLiteral("取消");
    });
    QVERIFY2(!hasCancelButton,
             "image transfer message should not expose a file-card cancel button");

    const auto geometry = MessageBubbleDelegate::fileCardActionGeometryForTesting(option, index);
    QVERIFY2(!geometry.hasActionChips,
             "image transfer message should not expose file action chips");
    QVERIFY2(!geometry.hasTransferCancelChip,
             "image transfer message should not expose transfer cancel chip in file card geometry");
}

void TestMessageBubbleWidget::image_size_hint_recomputes_when_file_becomes_readable()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString imagePath = tempDir.filePath(QStringLiteral("delayed-tall-preview.png"));

    QStandardItemModel model(1, 1);
    const QModelIndex index = model.index(0, 0);
    model.setData(index,
                  QStringLiteral("msg-delayed-tall-preview"),
                  MessageListModel::MessageIdRole);
    model.setData(index, false, MessageListModel::OutgoingRole);
    model.setData(index, true, MessageListModel::FileMessageRole);
    model.setData(index,
                  QStringLiteral("delayed-tall-preview.png"),
                  MessageListModel::AttachmentNameRole);
    model.setData(index, imagePath, MessageListModel::LocalFilePathRole);
    model.setData(index, QStringLiteral("[图片]"), MessageListModel::BodyRole);
    model.setData(index, QStringLiteral("对方"), MessageListModel::SenderNameRole);
    model.setData(index, QStringLiteral("10:26"), MessageListModel::TimeLabelRole);
    model.setData(index,
                  static_cast<int>(MessageDeliveryState::Received),
                  MessageListModel::DeliveryStateRole);

    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 640, 220);
    option.font = QFont(QStringLiteral("Microsoft YaHei UI"), 10);

    MessageBubbleDelegate delegate;
    const QSize placeholderSize = delegate.sizeHint(option, index);

    QImage image(120, 960, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(QStringLiteral("#263238")));
    QVERIFY(image.save(imagePath));

    const QSize readySize = delegate.sizeHint(option, index);
    QVERIFY2(readySize.height() > placeholderSize.height() + 200,
             qPrintable(QStringLiteral("placeholder=%1 ready=%2")
                            .arg(placeholderSize.height())
                            .arg(readySize.height())));
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestMessageBubbleWidget tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestMessageBubbleWidget.moc"
