#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QtTest>

#include "integrations/AzureDevOpsSettings.h"
#include "ui/AzureDevOpsInsertDialog.h"

class TestAzureDevOpsInsertDialog : public QObject {
    Q_OBJECT

private slots:
    void recognizesWorkItemLinkAndEnablesInsert()
    {
        AzureDevOpsInsertDialog dialog;
        dialog.setLinkText(
            QStringLiteral("https://dev.azure.com/leyochat/LeyoChat/_workitems/edit/123"));

        const auto locator = dialog.parsedLocator();
        QVERIFY(locator.has_value());
        QCOMPARE(locator->kind, AzureDevOpsResourceKind::WorkItem);
        QCOMPARE(locator->resourceId, QStringLiteral("123"));

        auto* insertButton = dialog.findChild<QPushButton*>(
            QStringLiteral("azureDevOpsInsertButton"));
        QVERIFY(insertButton != nullptr);
        QVERIFY(insertButton->isEnabled());
    }

    void supportsManualBuildLocatorFromSettings()
    {
        AzureDevOpsConnectionSettings settings;
        settings.baseUrl = QStringLiteral("https://dev.azure.com");
        settings.organization = QStringLiteral("leyochat");
        settings.project = QStringLiteral("LeyoChat");

        AzureDevOpsInsertDialog dialog(settings);
        auto* typeCombo =
            dialog.findChild<QComboBox*>(QStringLiteral("azureDevOpsTypeCombo"));
        auto* resourceIdEdit =
            dialog.findChild<QLineEdit*>(QStringLiteral("azureDevOpsResourceIdEdit"));
        auto* insertButton = dialog.findChild<QPushButton*>(
            QStringLiteral("azureDevOpsInsertButton"));

        QVERIFY(typeCombo != nullptr);
        QVERIFY(resourceIdEdit != nullptr);
        QVERIFY(insertButton != nullptr);

        typeCombo->setCurrentIndex(2);
        resourceIdEdit->setText(QStringLiteral("456"));

        const auto locator = dialog.parsedLocator();
        QVERIFY(locator.has_value());
        QCOMPARE(locator->kind, AzureDevOpsResourceKind::Build);
        QCOMPARE(locator->organization, QStringLiteral("leyochat"));
        QCOMPARE(locator->project, QStringLiteral("LeyoChat"));
        QCOMPARE(locator->resourceId, QStringLiteral("456"));
        QVERIFY(locator->webUrl.contains(QStringLiteral("buildId=456")));
        QVERIFY(insertButton->isEnabled());
    }

    void requiresRepositoryForManualPullRequest()
    {
        AzureDevOpsConnectionSettings settings;
        settings.baseUrl = QStringLiteral("https://dev.azure.com");
        settings.organization = QStringLiteral("leyochat");
        settings.project = QStringLiteral("LeyoChat");

        AzureDevOpsInsertDialog dialog(settings);
        auto* typeCombo =
            dialog.findChild<QComboBox*>(QStringLiteral("azureDevOpsTypeCombo"));
        auto* repositoryEdit =
            dialog.findChild<QLineEdit*>(QStringLiteral("azureDevOpsRepositoryEdit"));
        auto* resourceIdEdit =
            dialog.findChild<QLineEdit*>(QStringLiteral("azureDevOpsResourceIdEdit"));
        auto* insertButton = dialog.findChild<QPushButton*>(
            QStringLiteral("azureDevOpsInsertButton"));

        QVERIFY(typeCombo != nullptr);
        QVERIFY(repositoryEdit != nullptr);
        QVERIFY(resourceIdEdit != nullptr);
        QVERIFY(insertButton != nullptr);

        typeCombo->setCurrentIndex(1);
        resourceIdEdit->setText(QStringLiteral("789"));
        QVERIFY(!dialog.parsedLocator().has_value());
        QVERIFY(!insertButton->isEnabled());

        repositoryEdit->setText(QStringLiteral("LeyoChat"));
        const auto locator = dialog.parsedLocator();
        QVERIFY(locator.has_value());
        QCOMPARE(locator->kind, AzureDevOpsResourceKind::PullRequest);
        QCOMPARE(locator->repository, QStringLiteral("LeyoChat"));
        QCOMPARE(locator->resourceId, QStringLiteral("789"));
        QVERIFY(insertButton->isEnabled());
    }

    void rejectsUnknownLinkAndKeepsInsertDisabled()
    {
        AzureDevOpsInsertDialog dialog;
        dialog.setLinkText(QStringLiteral("https://example.com/not-devops"));

        QVERIFY(!dialog.parsedLocator().has_value());
        auto* insertButton = dialog.findChild<QPushButton*>(
            QStringLiteral("azureDevOpsInsertButton"));
        QVERIFY(insertButton != nullptr);
        QVERIFY(!insertButton->isEnabled());
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestAzureDevOpsInsertDialog tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestAzureDevOpsInsertDialog.moc"
