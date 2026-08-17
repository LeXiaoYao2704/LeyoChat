#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include "update/UpdateChecker.h"
#include "app/ApplicationInfo.h"

class TestUpdateChecker : public QObject {
    Q_OBJECT
private slots:
    void parseValidLatestJson();
    void parseInvalidJson_emitsCheckFailed();
    void sameVersion_noSignal();
    void newerVersion_emitsUpdateAvailable();
    void olderVersion_noSignal();
    void missingFile_emitsCheckFailed();
    void manualCheck_noUpdate_emitsNoUpdateAvailable();
};

void TestUpdateChecker::parseValidLatestJson()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QJsonObject obj;
    obj["version"] = "99.0.0";
    obj["file"] = "LeyoChat-99.0.0-setup.exe";
    obj["sha256"] = "abc123";
    obj["releaseNotes"] = "测试更新";
    QFile f(dir.filePath("latest.json"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(obj).toJson());
    f.close();

    UpdateChecker checker;
    checker.setUpdateSourcePath(dir.path());
    QSignalSpy spy(&checker, &UpdateChecker::updateAvailable);
    checker.checkNow();
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);
    auto info = spy.at(0).at(0).value<UpdateChecker::UpdateInfo>();
    QCOMPARE(info.version, QStringLiteral("99.0.0"));
    QCOMPARE(info.fileName, QStringLiteral("LeyoChat-99.0.0-setup.exe"));
    QCOMPARE(info.sha256, QStringLiteral("abc123"));
}

void TestUpdateChecker::parseInvalidJson_emitsCheckFailed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile f(dir.filePath("latest.json"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not json");
    f.close();

    UpdateChecker checker;
    checker.setUpdateSourcePath(dir.path());
    QSignalSpy spy(&checker, &UpdateChecker::checkFailed);
    checker.checkNow();
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);
}

void TestUpdateChecker::sameVersion_noSignal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QJsonObject obj;
    obj["version"] = ApplicationInfo::currentVersion();
    obj["file"] = "LeyoChat-current-setup.exe";
    obj["sha256"] = "abc";
    QFile f(dir.filePath("latest.json"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(obj).toJson());
    f.close();

    UpdateChecker checker;
    checker.setUpdateSourcePath(dir.path());
    QSignalSpy updateSpy(&checker, &UpdateChecker::updateAvailable);
    QSignalSpy noUpdateSpy(&checker, &UpdateChecker::noUpdateAvailable);
    checker.checkNow();
    QVERIFY(noUpdateSpy.wait(5000));
    QCOMPARE(updateSpy.count(), 0);
}

void TestUpdateChecker::newerVersion_emitsUpdateAvailable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QJsonObject obj;
    obj["version"] = "99.0.0";
    obj["file"] = "LeyoChat-99.0.0-setup.exe";
    obj["sha256"] = "def";
    QFile f(dir.filePath("latest.json"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(obj).toJson());
    f.close();

    UpdateChecker checker;
    checker.setUpdateSourcePath(dir.path());
    QSignalSpy spy(&checker, &UpdateChecker::updateAvailable);
    checker.checkNow();
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);
}

void TestUpdateChecker::olderVersion_noSignal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QJsonObject obj;
    obj["version"] = "0.0.1";
    obj["file"] = "old.exe";
    obj["sha256"] = "x";
    QFile f(dir.filePath("latest.json"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(obj).toJson());
    f.close();

    UpdateChecker checker;
    checker.setUpdateSourcePath(dir.path());
    QSignalSpy updateSpy(&checker, &UpdateChecker::updateAvailable);
    QSignalSpy noUpdateSpy(&checker, &UpdateChecker::noUpdateAvailable);
    checker.checkNow();
    QVERIFY(noUpdateSpy.wait(5000));
    QCOMPARE(updateSpy.count(), 0);
}

void TestUpdateChecker::missingFile_emitsCheckFailed()
{
    UpdateChecker checker;
    checker.setUpdateSourcePath(QStringLiteral("C:\\nonexistent\\path"));
    QSignalSpy spy(&checker, &UpdateChecker::checkFailed);
    checker.checkNow();
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);
}

void TestUpdateChecker::manualCheck_noUpdate_emitsNoUpdateAvailable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QJsonObject obj;
    obj["version"] = ApplicationInfo::currentVersion();
    obj["file"] = "current.exe";
    obj["sha256"] = "x";
    QFile f(dir.filePath("latest.json"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(obj).toJson());
    f.close();

    UpdateChecker checker;
    checker.setUpdateSourcePath(dir.path());
    QSignalSpy spy(&checker, &UpdateChecker::noUpdateAvailable);
    checker.checkNow();
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestUpdateChecker)
#include "TestUpdateChecker.moc"
