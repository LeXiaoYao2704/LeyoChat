#include <QtTest>

#include "app/ApplicationInfo.h"
#include "AppBuildInfo.h"

class TestApplicationInfo : public QObject {
    Q_OBJECT

private slots:
    void currentVersion_matchesConfiguredProjectVersion()
    {
        QCOMPARE(ApplicationInfo::currentVersion(), QStringLiteral(LEYOCHAT_APP_VERSION));
    }

    void releaseNotesText_isEmbeddedAndReadable()
    {
        const QString notes = ApplicationInfo::releaseNotesText();
        QVERIFY(!notes.isEmpty());
        QVERIFY(notes.contains(QStringLiteral("LeyoChat")));
    }

    void shouldShowReleaseNotesOnStartup_falseForFreshInstall()
    {
        QVERIFY(!ApplicationInfo::shouldShowReleaseNotesOnStartup(QString(), QStringLiteral("0.1.3")));
    }

    void shouldShowReleaseNotesOnStartup_trueForVersionUpgrade()
    {
        QVERIFY(ApplicationInfo::shouldShowReleaseNotesOnStartup(QStringLiteral("0.1.2"),
                                                                 QStringLiteral("0.1.3")));
    }

    void shouldShowReleaseNotesOnStartup_falseWhenVersionUnchanged()
    {
        QVERIFY(!ApplicationInfo::shouldShowReleaseNotesOnStartup(QStringLiteral("0.1.3"),
                                                                  QStringLiteral("0.1.3")));
    }
};

QTEST_MAIN(TestApplicationInfo)
#include "TestApplicationInfo.moc"
