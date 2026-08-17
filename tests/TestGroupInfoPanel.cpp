#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QtTest/QTest>

#include "domain/PeerEndpoint.h"
#include "integrations/RemoteFileServiceSettings.h"
#include "ui/CreateGroupDialog.h"
#include "ui/GroupInfoPanel.h"

namespace {
GroupMemberListEntries makeGroupMembers(std::initializer_list<GroupMemberListEntry> members)
{
    GroupMemberListEntries entries;
    entries.reserve(static_cast<qsizetype>(members.size()));
    for (const GroupMemberListEntry& member : members) {
        entries.push_back(member);
    }
    return entries;
}
}

class TestGroupInfoPanel : public QObject {
    Q_OBJECT

private slots:
    void showsAnnouncementAndMembers()
    {
        GroupInfoPanel panel;
        panel.setGroupSummary(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                              QStringLiteral("\u672C\u5468\u4E94\u4E0B\u5348\u63D0\u4EA4\u8BC4\u5BA1\u6750\u6599"),
                              makeGroupMembers({
                                  {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                                  {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
                              }),
                              true);

        QVERIFY(panel.groupTitleText() == QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));
        QVERIFY(panel.announcementText()
                == QStringLiteral("\u672C\u5468\u4E94\u4E0B\u5348\u63D0\u4EA4\u8BC4\u5BA1\u6750\u6599"));
        QVERIFY(panel.memberCount() == 2);
    }

    void announcementReminderButtonEmitsContextSnapshot()
    {
        GroupInfoPanel panel;
        panel.setGroupId(QStringLiteral("group-001"));
        panel.setGroupSummary(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                              QStringLiteral("\u672C\u5468\u4E94\u4E0B\u5348\u63D0\u4EA4\u8BC4\u5BA1\u6750\u6599"),
                              makeGroupMembers({
                                  {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                              }),
                              true);

        auto* remindButton = panel.findChild<QPushButton*>(QStringLiteral("groupAnnouncementReminderButton"));
        QVERIFY(remindButton != nullptr);
        QVERIFY(remindButton->isVisibleTo(&panel));

        QSignalSpy spy(&panel, SIGNAL(groupAnnouncementReminderRequested(QString,QString,QString)));
        QVERIFY(spy.isValid());

        remindButton->click();

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("group-001"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));
        QCOMPARE(args.at(2).toString(),
                 QStringLiteral("\u672C\u5468\u4E94\u4E0B\u5348\u63D0\u4EA4\u8BC4\u5BA1\u6750\u6599"));
    }

    void exposesTopBandForConsoleStyleGroupPanel()
    {
        GroupInfoPanel panel;
        panel.setGroupSummary(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                              QStringLiteral("\u6682\u65E0\u516C\u544A"),
                              makeGroupMembers({
                                  {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                              }),
                              true);

        auto* topBand = panel.findChild<QWidget*>(QStringLiteral("groupInfoTopBand"));
        QVERIFY(topBand != nullptr);

        auto* modeChip = panel.findChild<QLabel*>(QStringLiteral("groupInfoModeChip"));
        QVERIFY(modeChip != nullptr);
        QVERIFY(!modeChip->text().trimmed().isEmpty());
    }

    void removesPseudoActionChips()
    {
        GroupInfoPanel panel;
        panel.setGroupSummary(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                              QStringLiteral("\u672C\u5468\u4E94\u4E0B\u5348\u63D0\u4EA4\u8BC4\u5BA1\u6750\u6599"),
                              makeGroupMembers({
                                  {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                                  {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
                                  {QStringLiteral("wangwu"), QStringLiteral("\u738B\u4E94"), false, false, false},
                              }),
                              true);

        QVERIFY(panel.findChildren<QLabel*>(QStringLiteral("entryChip")).isEmpty());
    }

    void wrapsMembersInsideDedicatedSurfaceCard()
    {
        GroupInfoPanel panel;
        panel.setGroupSummary(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                              QStringLiteral("\u6682\u65E0\u516C\u544A"),
                              makeGroupMembers({
                                  {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                                  {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
                                  {QStringLiteral("wangwu"), QStringLiteral("\u738B\u4E94"), false, false, false},
                              }),
                              true);

        auto* membersCard = panel.findChild<QWidget*>(QStringLiteral("groupMembersCard"));
        auto* membersList = panel.findChild<QWidget*>(QStringLiteral("groupMembersList"));
        QVERIFY(membersCard != nullptr);
        QVERIFY(membersList != nullptr);
    }

    void exposes_context_sections_without_fixed_third_column_container()
    {
        GroupInfoPanel panel;
        panel.setGroupSummary(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                              QStringLiteral("\u6682\u65E0\u516C\u544A"),
                              makeGroupMembers({
                                  {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                                  {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
                              }),
                              true);

        QVERIFY(panel.findChild<QWidget*>(QStringLiteral("groupOverviewSection")) != nullptr);
        QVERIFY(panel.findChild<QWidget*>(QStringLiteral("groupAnnouncementSection")) != nullptr);
        QVERIFY(panel.findChild<QWidget*>(QStringLiteral("groupMembersSection")) != nullptr);
        QVERIFY(panel.findChild<QWidget*>(QStringLiteral("groupFilesSection")) != nullptr);
        QVERIFY(panel.findChild<QWidget*>(QStringLiteral("groupInfoThirdColumnContainer")) == nullptr);
    }

    void rendersRichMemberRowsInsteadOfBulletText()
    {
        GroupInfoPanel panel;
        panel.setGroupSummary(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                              QStringLiteral("\u6682\u65E0\u516C\u544A"),
                              makeGroupMembers({
                                  {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                                  {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, true},
                                  {QStringLiteral("wangwu"), QStringLiteral("\u738B\u4E94"), false, false, false},
                              }),
                              true);

        auto* membersList = panel.findChild<QListWidget*>(QStringLiteral("groupMembersList"));
        QVERIFY(membersList != nullptr);
        QVERIFY(membersList->count() > 0);

        QWidget* rowWidget = membersList->itemWidget(membersList->item(0));
        QVERIFY(rowWidget != nullptr);
        QVERIFY(rowWidget->findChild<QWidget*>(QStringLiteral("groupMemberAvatar")) != nullptr);
        QVERIFY(rowWidget->findChild<QLabel*>(QStringLiteral("groupMemberName")) != nullptr);
        QVERIFY(rowWidget->findChild<QLabel*>(QStringLiteral("groupMemberMeta")) != nullptr);
    }

    void showsHybridRuntimeSummary()
    {
        GroupInfoPanel panel;
        panel.setGroupSummary(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                              QStringLiteral("\u6682\u65E0\u516C\u544A"),
                              makeGroupMembers({
                                  {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                              }),
                              true);
        panel.setHybridRuntimeSummary(QStringLiteral("\u5DF2\u7ED1\u5B9A\u670D\u52A1"),
                                      QStringLiteral("\u5F53\u524D\u670D\u52A1 LeyoChat Service 路 3 \u4E2A\u5171\u4EAB\u8D44\u6E90"));

        auto* runtimeChip = panel.findChild<QLabel*>(QStringLiteral("groupRuntimeChip"));
        auto* runtimeDetail = panel.findChild<QLabel*>(QStringLiteral("groupRuntimeDetail"));
        QVERIFY(runtimeChip != nullptr);
        QVERIFY(runtimeDetail != nullptr);
        QCOMPARE(runtimeChip->text(), QStringLiteral("\u5DF2\u7ED1\u5B9A\u670D\u52A1"));
        QVERIFY(runtimeDetail->text().contains(QStringLiteral("LeyoChat Service")));
    }

    void emptyRuntimeSummary_usesUserFacingCopy()
    {
        GroupInfoPanel panel;

        auto* subtitle = panel.findChild<QLabel*>(QStringLiteral("groupInfoSubtitle"));
        QVERIFY(subtitle != nullptr);
        QCOMPARE(subtitle->text(), QStringLiteral("\u7FA4\u516C\u544A\u3001\u5171\u4EAB\u6587\u4EF6\u4E0E\u6210\u5458\u6982\u89C8"));

        panel.setHybridRuntimeSummary(QString(), QString());

        auto* runtimeChip = panel.findChild<QLabel*>(QStringLiteral("groupRuntimeChip"));
        auto* runtimeDetail = panel.findChild<QLabel*>(QStringLiteral("groupRuntimeDetail"));
        QVERIFY(runtimeChip != nullptr);
        QVERIFY(runtimeDetail != nullptr);
        QCOMPARE(runtimeChip->text(), QStringLiteral("\u5C1A\u672A\u5F00\u542F\u7FA4\u6587\u4EF6\u670D\u52A1"));
        QVERIFY(!runtimeDetail->text().contains(QStringLiteral("P2P")));
        QVERIFY(!runtimeDetail->text().contains(QStringLiteral("\u6DF7\u5408\u67B6\u6784")));
    }

    void ownerAndSelfRoles_areRenderedFromMemberFlags()
    {
        GroupInfoPanel panel;
        panel.setGroupSummary(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                              QStringLiteral("\u6682\u65E0\u516C\u544A"),
                              makeGroupMembers({
                                  {QStringLiteral("owner-001"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                                  {QStringLiteral("self-001"), QStringLiteral("\u6211"), false, false, true},
                              }),
                              true);

        auto* membersList = panel.findChild<QListWidget*>(QStringLiteral("groupMembersList"));
        QVERIFY(membersList != nullptr);
        QCOMPARE(membersList->count(), 2);

        QWidget* ownerRow = membersList->itemWidget(membersList->item(0));
        QWidget* selfRow = membersList->itemWidget(membersList->item(1));
        QVERIFY(ownerRow != nullptr);
        QVERIFY(selfRow != nullptr);

        auto* ownerMeta = ownerRow->findChild<QLabel*>(QStringLiteral("groupMemberMeta"));
        auto* selfMeta = selfRow->findChild<QLabel*>(QStringLiteral("groupMemberMeta"));
        QVERIFY(ownerMeta != nullptr);
        QVERIFY(selfMeta != nullptr);
        QVERIFY(ownerMeta->text().contains(QStringLiteral("\u7FA4\u4E3B")));
        QVERIFY(selfMeta->text().contains(QStringLiteral("\u6211")));
    }

    void removesStandaloneSettingsTabs()
    {
        GroupInfoPanel panel;
        QVERIFY(panel.findChild<QPushButton*>("groupInfoTabDetail") == nullptr);
        QVERIFY(panel.findChild<QPushButton*>("groupInfoTabSettings") == nullptr);
        QVERIFY(panel.findChild<QStackedWidget*>("groupInfoStackedWidget") != nullptr);
    }

    void fileServiceSettingsView_isNoLongerExposed()
    {
        GroupInfoPanel panel;

        panel.showFileServiceSettingsView();

        QVERIFY(!panel.isShowingFileServiceSettingsView());
        QVERIFY(panel.findChild<QPushButton*>("groupSettingsBackButton") == nullptr);
        QVERIFY(panel.findChild<QPushButton*>("groupFsSaveBtn") == nullptr);
        QVERIFY(panel.findChild<QLineEdit*>("groupFsBaseUrlEdit") == nullptr);
    }

    void settingGroupFileServiceConfig_doesNotCreateLegacyForm()
    {
        GroupInfoPanel panel;
        GroupFileServiceConfig cfg;
        cfg.groupId = QStringLiteral("grp-1");
        cfg.enabled = true;
        cfg.baseUrl = QStringLiteral("http://srv:8765");
        panel.setGroupId(cfg.groupId);
        panel.setGroupFileServiceConfig(cfg, /*canEdit=*/true);

        QVERIFY(panel.findChild<QPushButton*>("groupFsSaveBtn") == nullptr);
        QVERIFY(panel.findChild<QLabel*>("groupFsReadonlyNotice") == nullptr);
        QVERIFY(panel.findChild<QLineEdit*>("groupFsBaseUrlEdit") == nullptr);
    }

    void identicalGroupSummary_doesNotRebuildMemberRows()
    {
        GroupInfoPanel panel;
        const GroupMemberListEntries members = makeGroupMembers({
            {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
            {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
        });

        panel.setGroupSummary(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                              QStringLiteral("\u6682\u65E0\u516C\u544A"),
                              members,
                              true);

        auto* membersList = panel.findChild<QListWidget*>(QStringLiteral("groupMembersList"));
        QVERIFY(membersList != nullptr);
        QCOMPARE(membersList->count(), 2);
        QWidget* firstWidget = membersList->itemWidget(membersList->item(0));
        QVERIFY(firstWidget != nullptr);

        panel.setGroupSummary(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                              QStringLiteral("\u6682\u65E0\u516C\u544A"),
                              members,
                              true);

        QCOMPARE(membersList->count(), 2);
        QCOMPARE(membersList->itemWidget(membersList->item(0)), firstWidget);
    }

    void groupDialogFilteringAndConfirmationStayInSync()
    {
        const QList<PeerEndpoint> peers = {
            PeerEndpoint{QStringLiteral("zhangsan").toStdString(),
                         QStringLiteral("\u5F20\u4E09").toStdString(),
                         QStringLiteral("192.0.2.3").toStdString()},
            PeerEndpoint{QStringLiteral("lisi").toStdString(),
                         QStringLiteral("\u674E\u56DB").toStdString(),
                         QStringLiteral("192.0.2.8").toStdString()},
        };

        CreateGroupDialog dialog(peers);

        const auto lineEdits = dialog.findChildren<QLineEdit*>();
        QCOMPARE(lineEdits.size(), 2);

        QLineEdit* nameEdit = nullptr;
        QLineEdit* searchEdit = nullptr;
        for (QLineEdit* edit : lineEdits) {
            if (edit->placeholderText().contains(QStringLiteral("\u7FA4\u540D"))) {
                nameEdit = edit;
            } else if (edit->placeholderText().contains(QStringLiteral("\u641C\u7D22"))) {
                searchEdit = edit;
            }
        }

        QVERIFY(nameEdit != nullptr);
        QVERIFY(searchEdit != nullptr);

        QCOMPARE(dialog.visibleMemberCount(), 2);
        QVERIFY(!dialog.isConfirmEnabled());

        searchEdit->setText(QStringLiteral("\u4E0D\u5B58\u5728"));
        QCOMPARE(dialog.visibleMemberCount(), 0);

        searchEdit->setText(QStringLiteral("\u5F20\u4E09"));
        QCOMPARE(dialog.visibleMemberCount(), 1);

        const auto checkboxes = dialog.findChildren<QCheckBox*>();
        QCOMPARE(checkboxes.size(), 2);
        checkboxes.front()->setChecked(true);
        QVERIFY(!dialog.isConfirmEnabled());

        nameEdit->setText(QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));
        QVERIFY(dialog.isConfirmEnabled());
        QCOMPARE(dialog.selectedCount(), 1);
        QVERIFY(dialog.selectionSummaryText().contains(QStringLiteral("\u5F20\u4E09")));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestGroupInfoPanel tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestGroupInfoPanel.moc"
