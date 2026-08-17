#include <QtTest>

#include "integrations/SharedFileResourceContracts.h"

class TestSharedFileResourceContracts : public QObject {
    Q_OBJECT

private slots:
    void payload_containsDownloadAndOpenTargets();
};

void TestSharedFileResourceContracts::payload_containsDownloadAndOpenTargets()
{
    const SharedFileResource resource{
        QStringLiteral("svc-share"),
        QStringLiteral("workspace-files"),
        QStringLiteral("shared-file-01"),
        QStringLiteral("Beta 安装包"),
        QStringLiteral("张小乐"),
        QStringLiteral("v0.1.3"),
        QStringLiteral("群共享文件"),
        QStringLiteral("shared://files/download/01"),
        QStringLiteral("shared://files/open/01"),
        1024 * 1024,
    };

    const ResourceRefPayload payload = SharedFileResourceContracts::makePayload(resource);
    QCOMPARE(payload.kind, QStringLiteral("shared_file"));
    QCOMPARE(payload.actions.size(), 2);
    QCOMPARE(payload.actions.front().label, QStringLiteral("下载"));
    QCOMPARE(payload.actions.back().label, QStringLiteral("打开"));
}

QTEST_MAIN(TestSharedFileResourceContracts)
#include "TestSharedFileResourceContracts.moc"
