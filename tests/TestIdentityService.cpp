#include <QtTest/QTest>

#include "services/IdentityService.h"

class TestIdentityService : public QObject {
    Q_OBJECT

private slots:
    void rejectsEmptyDisplayName() {
        IdentityService service;
        const auto result = service.validateInput(QString(), QStringLiteral("testuser"));
        QVERIFY(!result.isValid);
        QCOMPARE(result.errorMessage, QStringLiteral("\u6635\u79F0\u4E0D\u80FD\u4E3A\u7A7A"));
    }

    void rejectsEmptyEmployeeCode() {
        IdentityService service;
        const auto result = service.validateInput(QStringLiteral("\u5F20\u4E50"), QString());
        QVERIFY(!result.isValid);
        QCOMPARE(result.errorMessage, QStringLiteral("\u5DE5\u53F7\u4E0D\u80FD\u4E3A\u7A7A"));
    }

    void trimsWhitespaceInput() {
        IdentityService service;
        const auto result = service.validateInput(QStringLiteral("   "), QStringLiteral("testuser"));
        QVERIFY(!result.isValid);
        QCOMPARE(result.errorMessage, QStringLiteral("\u6635\u79F0\u4E0D\u80FD\u4E3A\u7A7A"));
    }

    void createProfile_initializesExtendedFieldsEmpty() {
        IdentityService service;
        const Profile profile = service.createProfile(QStringLiteral("\u5F20\u4E50"),
                                                      QStringLiteral("testuser"),
                                                      45454);
        QCOMPARE(profile.department, std::wstring());
        QCOMPARE(profile.jobTitle, std::wstring());
        QCOMPARE(profile.phoneNumber, std::wstring());
        QCOMPARE(profile.gender, std::wstring());
        QCOMPARE(profile.email, std::wstring());
    }
};

QTEST_MAIN(TestIdentityService)
#include "TestIdentityService.moc"
