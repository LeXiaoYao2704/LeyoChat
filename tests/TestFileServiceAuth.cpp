#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QUuid>

#include "fileservice/FileServiceAuth.h"
#include "fileservice/FileServiceDatabase.h"

namespace {
QString uniqueConn()
{
    return QStringLiteral("test-auth-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString jsonScope(std::initializer_list<const char*> workspaces)
{
    QJsonArray array;
    for (const char* workspace : workspaces) {
        array.append(QString::fromLatin1(workspace));
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}
}

class TestFileServiceAuth : public QObject {
    Q_OBJECT

private slots:
    void seedOrUpdateTokenScope_insertsWhenEmpty();
    void seedOrUpdateTokenScope_skipsWhenTokenExistsAndScopeMatches();
    void seedOrUpdateTokenScope_updatesWhenScopeChanges();
    void seedOrUpdateTokenScope_failsWhenAllowedWorkspacesIsEmpty();
    void seedOrUpdateTokenSecurity_persistsRoleAndScopes();
    void canAccessWorkspace_acceptsPersistedJsonArrayScopes();
    void canUseScope_acceptsWildcardAndJsonArrayScopes();
    void resolveRequestClient_allowsAdminSharedTokenIdentity();
    void resolveRequestClient_rejectsMemberImpersonation();
};

void TestFileServiceAuth::seedOrUpdateTokenScope_insertsWhenEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("a1.db")), conn);
    QVERIFY(db.open());

    FileServiceAuth auth(&db);
    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-1"), QStringLiteral("client-1"),
        QStringLiteral("Admin"), jsonScope({"group-a"})));

    const auto found = db.findToken(QStringLiteral("tok-1"));
    QVERIFY(found.has_value());
    QCOMPARE(found->allowedWorkspaces, jsonScope({"group-a"}));
}

void TestFileServiceAuth::seedOrUpdateTokenScope_skipsWhenTokenExistsAndScopeMatches()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("a2.db")), conn);
    QVERIFY(db.open());

    FileServiceAuth auth(&db);
    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-2"), QStringLiteral("client-2"),
        QStringLiteral("Admin"), jsonScope({"group-b"})));
    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-2"), QStringLiteral("client-2"),
        QStringLiteral("Admin"), jsonScope({"group-b"})));

    const auto found = db.findToken(QStringLiteral("tok-2"));
    QVERIFY(found.has_value());
    QCOMPARE(found->allowedWorkspaces, jsonScope({"group-b"}));
}

void TestFileServiceAuth::seedOrUpdateTokenScope_updatesWhenScopeChanges()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("a3.db")), conn);
    QVERIFY(db.open());

    ServiceToken token;
    token.token = QStringLiteral("tok-3");
    token.clientId = QStringLiteral("client-3");
    token.displayName = QStringLiteral("Admin");
    token.createdAtMs = 1;
    token.allowedWorkspaces = QStringLiteral("*");
    QVERIFY(db.insertToken(token));

    FileServiceAuth auth(&db);
    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-3"), QStringLiteral("client-3"),
        QStringLiteral("Admin"), jsonScope({"group-c", "group-d"})));

    const auto found = db.findToken(QStringLiteral("tok-3"));
    QVERIFY(found.has_value());
    QCOMPARE(found->allowedWorkspaces, jsonScope({"group-c", "group-d"}));
}

void TestFileServiceAuth::seedOrUpdateTokenScope_failsWhenAllowedWorkspacesIsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("a4.db")), conn);
    QVERIFY(db.open());

    FileServiceAuth auth(&db);
    const bool result = auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-4"), QStringLiteral("client-4"),
        QStringLiteral("Admin"), QStringLiteral(""));
    QVERIFY(!result);
}

void TestFileServiceAuth::seedOrUpdateTokenSecurity_persistsRoleAndScopes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("a-security.db")), conn);
    QVERIFY(db.open());

    FileServiceAuth auth(&db);
    QVERIFY(auth.seedOrUpdateTokenSecurity(
        QStringLiteral("tok-admin"),
        QStringLiteral("admin-1"),
        QStringLiteral("Admin"),
        QStringLiteral("*"),
        QStringLiteral("admin"),
        jsonScope({"admin:read", "admin:write"})));

    const auto found = db.findToken(QStringLiteral("tok-admin"));
    QVERIFY(found.has_value());
    QCOMPARE(found->role, QStringLiteral("admin"));
    QCOMPARE(found->scopes, jsonScope({"admin:read", "admin:write"}));

    const auto client = auth.validate(QStringLiteral("Bearer tok-admin"));
    QVERIFY(client.has_value());
    QVERIFY(auth.canUseScope(*client, QStringLiteral("admin:read")));
    QVERIFY(!auth.canUseScope(*client, QStringLiteral("message:write")));
}

void TestFileServiceAuth::canAccessWorkspace_acceptsPersistedJsonArrayScopes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("a5.db")), conn);
    QVERIFY(db.open());

    FileServiceAuth auth(&db);
    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-5"), QStringLiteral("client-5"),
        QStringLiteral("Admin"), jsonScope({"group-a", "group-c"})));

    const auto client = auth.validate(QStringLiteral("Bearer tok-5"));
    QVERIFY(client.has_value());
    QVERIFY(auth.canAccessWorkspace(*client, QStringLiteral("group-a")));
    QVERIFY(auth.canAccessWorkspace(*client, QStringLiteral("group-c")));
    QVERIFY(!auth.canAccessWorkspace(*client, QStringLiteral("group-b")));
}

void TestFileServiceAuth::canUseScope_acceptsWildcardAndJsonArrayScopes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("a-scope.db")), conn);
    QVERIFY(db.open());

    ServiceToken wildcard;
    wildcard.token = QStringLiteral("tok-wildcard");
    wildcard.clientId = QStringLiteral("client-wildcard");
    wildcard.displayName = QStringLiteral("Wildcard");
    wildcard.allowedWorkspaces = QStringLiteral("*");
    wildcard.scopes = QStringLiteral("*");
    QVERIFY(db.insertToken(wildcard));

    ServiceToken scoped;
    scoped.token = QStringLiteral("tok-scoped");
    scoped.clientId = QStringLiteral("client-scoped");
    scoped.displayName = QStringLiteral("Scoped");
    scoped.allowedWorkspaces = QStringLiteral("*");
    scoped.scopes = jsonScope({"message:read"});
    QVERIFY(db.insertToken(scoped));

    FileServiceAuth auth(&db);
    const auto wildcardClient = auth.validate(QStringLiteral("Bearer tok-wildcard"));
    QVERIFY(wildcardClient.has_value());
    QVERIFY(auth.canUseScope(*wildcardClient, QStringLiteral("admin:write")));

    const auto scopedClient = auth.validate(QStringLiteral("Bearer tok-scoped"));
    QVERIFY(scopedClient.has_value());
    QVERIFY(auth.canUseScope(*scopedClient, QStringLiteral("message:read")));
    QVERIFY(!auth.canUseScope(*scopedClient, QStringLiteral("message:write")));
}

void TestFileServiceAuth::resolveRequestClient_allowsAdminSharedTokenIdentity()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("request-admin.db")), conn);
    QVERIFY(db.open());

    FileServiceAuth auth(&db);
    const AuthenticatedClient tokenClient{
        QStringLiteral("admin"),
        QStringLiteral("*"),
        QStringLiteral("admin"),
        QStringLiteral("*")
    };

    const auto resolved = auth.resolveRequestClient(
        tokenClient, QStringLiteral(" workstation-client-a "));

    QVERIFY(resolved.has_value());
    QCOMPARE(resolved->clientId, QStringLiteral("workstation-client-a"));
    QCOMPARE(resolved->role, QStringLiteral("admin"));
}

void TestFileServiceAuth::resolveRequestClient_rejectsMemberImpersonation()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("request-member.db")), conn);
    QVERIFY(db.open());

    FileServiceAuth auth(&db);
    const AuthenticatedClient tokenClient{
        QStringLiteral("client-a"),
        QStringLiteral("*"),
        QStringLiteral("member"),
        QStringLiteral("*")
    };

    QVERIFY(!auth.resolveRequestClient(
        tokenClient, QStringLiteral("client-b")).has_value());
    const auto sameClient = auth.resolveRequestClient(
        tokenClient, QStringLiteral("client-a"));
    QVERIFY(sameClient.has_value());
    QCOMPARE(sameClient->clientId, QStringLiteral("client-a"));
}

QTEST_MAIN(TestFileServiceAuth)
#include "TestFileServiceAuth.moc"
