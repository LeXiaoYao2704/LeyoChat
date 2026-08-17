#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>
#include "ui/GroupFileTableModel.h"

class TestGroupFileTableModel : public QObject {
    Q_OBJECT
private slots:
    void setFiles_updatesRowCount();
    void data_displaysFormattedSize();
    void setSearchFilter_filtersByFileName();
    void setSortColumn_sortsByDate();
    void setFolderFilter_filtersByFolderId();
    void fileAt_returnsCorrectRecord();
    void clearFilter_restoresAll();
};

static QJsonObject makeFile(const QString& name, qint64 size,
                            const QString& uploader, qint64 dateMs,
                            const QString& folderId = {})
{
    QJsonObject o;
    o["file_name"] = name;
    o["file_size"] = size;
    o["uploaded_by_name"] = uploader;
    o["updated_at_ms"] = dateMs;
    o["folder_id"] = folderId;
    o["file_id"] = name;
    return o;
}

void TestGroupFileTableModel::setFiles_updatesRowCount()
{
    GroupFileTableModel model;
    QCOMPARE(model.rowCount(), 0);
    QJsonArray files;
    files.append(makeFile("a.pdf", 1024, "Alice", 1000));
    files.append(makeFile("b.doc", 2048, "Bob", 2000));
    model.setFiles(files);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.columnCount(), int(GroupFileTableModel::ColumnCount));
}

void TestGroupFileTableModel::data_displaysFormattedSize()
{
    GroupFileTableModel model;
    QJsonArray files;
    files.append(makeFile("a.pdf", 1536, "Alice", 1000));
    model.setFiles(files);
    const auto idx = model.index(0, GroupFileTableModel::FileSize);
    const QString display = model.data(idx, Qt::DisplayRole).toString();
    QVERIFY(display.contains("KB") || display.contains("1"));
}

void TestGroupFileTableModel::setSearchFilter_filtersByFileName()
{
    GroupFileTableModel model;
    QJsonArray files;
    files.append(makeFile("report.pdf", 100, "Alice", 1000));
    files.append(makeFile("photo.jpg", 200, "Bob", 2000));
    files.append(makeFile("report_v2.pdf", 300, "Carol", 3000));
    model.setFiles(files);
    model.setSearchFilter("report");
    QCOMPARE(model.rowCount(), 2);
    model.setSearchFilter("photo");
    QCOMPARE(model.rowCount(), 1);
}

void TestGroupFileTableModel::setSortColumn_sortsByDate()
{
    GroupFileTableModel model;
    QJsonArray files;
    files.append(makeFile("old.pdf", 100, "Alice", 1000));
    files.append(makeFile("new.pdf", 100, "Bob", 3000));
    files.append(makeFile("mid.pdf", 100, "Carol", 2000));
    model.setFiles(files);
    model.setSortColumn(GroupFileTableModel::UploadDate, Qt::AscendingOrder);
    QCOMPARE(model.fileAt(0)["file_name"].toString(), QStringLiteral("old.pdf"));
    QCOMPARE(model.fileAt(2)["file_name"].toString(), QStringLiteral("new.pdf"));
    model.setSortColumn(GroupFileTableModel::UploadDate, Qt::DescendingOrder);
    QCOMPARE(model.fileAt(0)["file_name"].toString(), QStringLiteral("new.pdf"));
}

void TestGroupFileTableModel::setFolderFilter_filtersByFolderId()
{
    GroupFileTableModel model;
    QJsonArray files;
    files.append(makeFile("a.pdf", 100, "Alice", 1000, "f1"));
    files.append(makeFile("b.pdf", 100, "Bob", 2000, "f2"));
    files.append(makeFile("c.pdf", 100, "Carol", 3000, "f1"));
    files.append(makeFile("d.pdf", 100, "Dave", 4000));
    model.setFiles(files);
    model.setFolderFilter("f1");
    QCOMPARE(model.rowCount(), 2);
    model.setFolderFilter("f2");
    QCOMPARE(model.rowCount(), 1);
    model.setFolderFilter("");
    QCOMPARE(model.rowCount(), 4);
}

void TestGroupFileTableModel::fileAt_returnsCorrectRecord()
{
    GroupFileTableModel model;
    QJsonArray files;
    files.append(makeFile("a.pdf", 100, "Alice", 1000));
    model.setFiles(files);
    const auto obj = model.fileAt(0);
    QCOMPARE(obj["file_name"].toString(), QStringLiteral("a.pdf"));
    QVERIFY(model.fileAt(99).isEmpty());
}

void TestGroupFileTableModel::clearFilter_restoresAll()
{
    GroupFileTableModel model;
    QJsonArray files;
    files.append(makeFile("a.pdf", 100, "Alice", 1000, "f1"));
    files.append(makeFile("b.pdf", 100, "Bob", 2000));
    model.setFiles(files);
    model.setFolderFilter("f1");
    QCOMPARE(model.rowCount(), 1);
    model.setSearchFilter("xyz");
    QCOMPARE(model.rowCount(), 0);
    model.setSearchFilter("");
    model.setFolderFilter("");
    QCOMPARE(model.rowCount(), 2);
}

QTEST_GUILESS_MAIN(TestGroupFileTableModel)
#include "TestGroupFileTableModel.moc"
