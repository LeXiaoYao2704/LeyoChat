#include <QCoreApplication>
#include <QtTest/QTest>

#include "domain/FileTransferTask.h"
#include "ui/TransferListModel.h"

class TestTransferListModel : public QObject {
    Q_OBJECT

private slots:
    void filtersTransfersByDirectionAndState()
    {
        TransferListModel model;

        QVector<TransferListItem> items{
            TransferListItem{
                QStringLiteral("outgoing-failed"),
                QStringLiteral("\u8BBE\u8BA1\u7A3F.png"),
                QStringLiteral("\u4F20\u8F93\u5931\u8D25"),
                QStringLiteral("\u53D1\u7ED9 \u5F20\u4E50"),
                QStringLiteral("\u5F20\u4E50"),
                QStringLiteral("C:/tmp/a.png"),
                FileTransferDirection::Outgoing,
                FileTransferState::Failed,
                false,
                false,
                true},
            TransferListItem{
                QStringLiteral("incoming-completed"),
                QStringLiteral("\u5468\u62A5.xlsx"),
                QStringLiteral("\u5DF2\u5B8C\u6210"),
                QStringLiteral("\u6765\u81EA \u674E\u56DB"),
                QStringLiteral("\u674E\u56DB"),
                QStringLiteral("C:/tmp/b.xlsx"),
                FileTransferDirection::Incoming,
                FileTransferState::Completed,
                true,
                true,
                false}
        };

        model.setItems(items);
        QCOMPARE(model.rowCount(), 2);

        model.setFilter(TransferListFilter::OutgoingOnly);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0, 0), TransferListModel::TaskIdRole).toString(),
                 QStringLiteral("outgoing-failed"));

        model.setFilter(TransferListFilter::IncomingOnly);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0, 0), TransferListModel::TaskIdRole).toString(),
                 QStringLiteral("incoming-completed"));

        model.setFilter(TransferListFilter::FailedOnly);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0, 0), TransferListModel::RetryableRole).toBool(), true);

        model.setFilter(TransferListFilter::CompletedOnly);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0, 0), TransferListModel::DirectionRole).toInt(),
                 static_cast<int>(FileTransferDirection::Incoming));
    }

    void exposesFileStatusCounterpartAndBadgeRoles()
    {
        TransferListModel model;
        model.setItems({
            TransferListItem{
                QStringLiteral("task-1"),
                QStringLiteral("\u53D1\u5E03\u5305.zip"),
                QStringLiteral("\u7B49\u5F85\u5BF9\u65B9\u63A5\u6536"),
                QStringLiteral("\u53D1\u7ED9 \u6D4B\u8BD5\u7EC4"),
                QStringLiteral("\u6D4B\u8BD5\u7EC4"),
                QString(),
                FileTransferDirection::Outgoing,
                FileTransferState::WaitingAccept,
                false,
                false,
                true}
        });

        const QModelIndex index = model.index(0, 0);
        QCOMPARE(model.data(index, Qt::DisplayRole).toString(), QStringLiteral("\u53D1\u5E03\u5305.zip"));
        QCOMPARE(model.data(index, TransferListModel::StatusTextRole).toString(),
                 QStringLiteral("\u7B49\u5F85\u5BF9\u65B9\u63A5\u6536"));
        QCOMPARE(model.data(index, TransferListModel::DetailTextRole).toString(),
                 QStringLiteral("\u53D1\u7ED9 \u6D4B\u8BD5\u7EC4"));
        QCOMPARE(model.data(index, TransferListModel::PeerLabelRole).toString(),
                 QStringLiteral("\u6D4B\u8BD5\u7EC4"));
        QCOMPARE(model.data(index, TransferListModel::FileBadgeRole).toString(),
                 QStringLiteral("ZIP"));
        QVERIFY(model.data(index, Qt::ToolTipRole).toString().contains(QStringLiteral("\u53D1\u7ED9 \u6D4B\u8BD5\u7EC4")));
        QVERIFY(model.data(index, Qt::ToolTipRole).toString().contains(QStringLiteral("\u53D1\u5E03\u5305.zip")));
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    TestTransferListModel tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestTransferListModel.moc"
