#include <QApplication>
#include <QAbstractButton>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTextEdit>
#include <QtTest/QTest>

#include <ElaPushButton.h>
#include <ElaTextEdit.h>
#include <ElaToolButton.h>

#include "ui/ChatComposerWidget.h"

class TestChatComposerWidget : public QObject {
    Q_OBJECT

private slots:
    void exposesToolbarEditorAndSendSurface()
    {
        ChatComposerWidget widget;

        QVERIFY(widget.toolbarHost() != nullptr);
        QVERIFY(widget.messageEditor() != nullptr);
        QVERIFY(widget.sendButton() != nullptr);
        QVERIFY(widget.fileButton() != nullptr);
        QVERIFY(widget.screenshotButton() != nullptr);
    }

    void removesRetryDraftActionFromDefaultToolbar()
    {
        ChatComposerWidget widget;

        const auto buttons = widget.findChildren<QPushButton*>();
        for (QPushButton* button : buttons) {
            QVERIFY(button->text() != QStringLiteral("\u91CD\u8BD5\u5F85\u53D1"));
        }
    }

    void usesIconStyleForFileAndScreenshotActions()
    {
        ChatComposerWidget widget;

        QVERIFY(!widget.fileButton()->text().contains(QStringLiteral("\u6587\u4EF6")));
        QVERIFY(!widget.screenshotButton()->text().contains(QStringLiteral("\u622A\u56FE")));
        QVERIFY(!widget.fileButton()->toolTip().trimmed().isEmpty());
        QVERIFY(!widget.screenshotButton()->toolTip().trimmed().isEmpty());
    }

    void exposesNudgeReminderAction()
    {
        ChatComposerWidget widget;

        QVERIFY(widget.nudgeButton() != nullptr);
        QCOMPARE(widget.nudgeButton()->toolTip(), QStringLiteral("\u7A97\u53E3\u6296\u52A8\u63D0\u9192"));
        QVERIFY(widget.nudgeButton()->text().trimmed().isEmpty());
    }

    void hidesDevOpsInsertActionByDefault()
    {
        ChatComposerWidget widget;

        QVERIFY(widget.devOpsButton() == nullptr);
        QVERIFY(widget.findChild<QPushButton*>(QStringLiteral("composerDevOpsButton")) == nullptr);
    }

    void exposesRichFormattingAndEmojiActions()
    {
        ChatComposerWidget widget;

        QVERIFY(widget.findChild<QWidget*>(QStringLiteral("composerBoldButton")) != nullptr);
        QVERIFY(widget.findChild<QWidget*>(QStringLiteral("composerItalicButton")) != nullptr);
        QVERIFY(widget.findChild<QWidget*>(QStringLiteral("composerUnderlineButton")) != nullptr);
        QVERIFY(widget.findChild<QWidget*>(QStringLiteral("composerFontButton")) != nullptr);
        QVERIFY(widget.findChild<QWidget*>(QStringLiteral("composerEmojiButton")) != nullptr);
    }

    void usesExpandedEmojiPaletteInsteadOfTinyFallbackMenu()
    {
        QVERIFY(ChatComposerWidget::emojiChoicesForTesting().size() >= 30);
    }

    void usesPresetFontFamilyAndSizeChoices()
    {
        const QStringList families = ChatComposerWidget::fontFamilyChoicesForTesting();
        QVERIFY(families.contains(QStringLiteral("Microsoft YaHei UI")));
        QVERIFY(families.contains(QStringLiteral("Segoe UI")));

        const QList<int> sizes = ChatComposerWidget::fontSizeChoicesForTesting();
        QVERIFY(sizes.contains(10));
        QVERIFY(sizes.contains(12));
        QVERIFY(sizes.contains(16));
        QVERIFY(sizes.contains(24));
    }

    void usesReadableChineseComposerStrings()
    {
        ChatComposerWidget widget;

        auto* fontButton = widget.findChild<QWidget*>(QStringLiteral("composerFontButton"));
        auto* emojiButton = widget.findChild<QWidget*>(QStringLiteral("composerEmojiButton"));
        QVERIFY(fontButton != nullptr);
        QVERIFY(emojiButton != nullptr);

        QCOMPARE(widget.messageEditor()->placeholderText(),
                 QStringLiteral("输入消息，Enter 发送，Shift+Enter 换行"));
        QCOMPARE(fontButton->toolTip(),
                 QStringLiteral("字体与字号"));
        QCOMPARE(emojiButton->toolTip(),
                 QStringLiteral("插入表情"));
    }

    void keepsPrimaryAndSecondaryActionsSeparate()
    {
        ChatComposerWidget widget;

        QVERIFY(widget.sendButton() != nullptr);
        QVERIFY(widget.fileButton() != nullptr);
        QVERIFY(widget.sendModeButton() != nullptr);
        QVERIFY(widget.sendButton()->text().contains(QStringLiteral("\u53D1\u9001")));
    }

    void keepsComposerSurfaceCompactForChatDensity()
    {
        ChatComposerWidget widget;

        auto* surface = widget.findChild<QFrame*>(QStringLiteral("composerSurface"));
        QVERIFY(surface != nullptr);

        auto* band = widget.findChild<QFrame*>(QStringLiteral("composerControlBand"));
        QVERIFY(band != nullptr);
        QVERIFY(band->isHidden());

        auto* modeChip = widget.findChild<QLabel*>(QStringLiteral("composerModeChip"));
        QVERIFY(modeChip != nullptr);
        QVERIFY(!modeChip->text().trimmed().isEmpty());
    }

    void allowsUpdatingComposerMetaChip()
    {
        ChatComposerWidget widget;

        widget.setDraftMetaText(QStringLiteral("2 \u4E2A\u9644\u4EF6\u5F85\u53D1"));

        auto* metaChip = widget.findChild<QLabel*>(QStringLiteral("composerMetaChip"));
        QVERIFY(metaChip != nullptr);
        QCOMPARE(metaChip->text(), QStringLiteral("2 \u4E2A\u9644\u4EF6\u5F85\u53D1"));
    }

    void keepsComposerCompactForDenseChatLayouts()
    {
        ChatComposerWidget widget;

        QVERIFY(widget.messageEditor() != nullptr);
        QVERIFY(widget.messageEditor()->minimumHeight() >= 82);
        QVERIFY(widget.messageEditor()->minimumHeight() <= 120);
        QVERIFY(widget.sendButton()->height() <= 34);
    }

    void keeps_primary_actions_visible()
    {
        ChatComposerWidget widget;

        QVERIFY(widget.sendButton() != nullptr);
        QVERIFY(widget.fileButton() != nullptr);
        QVERIFY(widget.screenshotButton() != nullptr);
        QVERIFY(widget.nudgeButton() != nullptr);
        QVERIFY(widget.moreActionsButton() == nullptr);
    }

    void keeps_secondary_integrations_out_of_default_composer()
    {
        ChatComposerWidget widget;

        QVERIFY(widget.devOpsButton() == nullptr);
        QVERIFY(widget.secondaryActionLabelsForTesting().isEmpty());
    }

    void keepsEmojiPopupHeightBoundedAcrossModes()
    {
        ChatComposerWidget widget;
        widget.resize(900, 420);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        auto* emojiButton = widget.findChild<QAbstractButton*>(QStringLiteral("composerEmojiButton"));
        QVERIFY(emojiButton != nullptr);
        QTest::mouseClick(emojiButton, Qt::LeftButton);

        auto* panel = widget.findChild<QFrame*>(QStringLiteral("emojiPopupPanel"));
        QTRY_VERIFY(panel != nullptr);
        QTRY_VERIFY(panel->isVisible());

        auto* stickerModeBtn = panel->findChild<QAbstractButton*>(QString(), Qt::FindChildrenRecursively);
        Q_UNUSED(stickerModeBtn);

        QList<QAbstractButton*> modeButtons = panel->findChildren<QAbstractButton*>();
        QAbstractButton* stickerBtn = nullptr;
        QAbstractButton* emojiModeBtn = nullptr;
        for (QAbstractButton* btn : modeButtons) {
            if (btn->text().contains(QStringLiteral("贴纸"))) {
                stickerBtn = btn;
            }
            if (btn->text().contains(QStringLiteral("表情"))) {
                emojiModeBtn = btn;
            }
        }
        QVERIFY(stickerBtn != nullptr);
        QVERIFY(emojiModeBtn != nullptr);

        const int firstHeight = panel->height();
        QTest::mouseClick(stickerBtn, Qt::LeftButton);
        QTRY_VERIFY(panel->height() <= 430);

        QTest::mouseClick(emojiModeBtn, Qt::LeftButton);
        QTRY_VERIFY(panel->height() <= 430);
        QVERIFY(panel->height() <= firstHeight + 80);
    }

    void editModeProvidesCloseButton()
    {
        ChatComposerWidget widget;

        widget.enterEditMode(QStringLiteral("msg-1"), QStringLiteral("old text"));

        auto* closeButton = widget.findChild<QAbstractButton*>(
            QStringLiteral("composerEditCloseButton"));
        QVERIFY(closeButton != nullptr);
        QVERIFY(!closeButton->isHidden());
        QVERIFY(widget.isInEditMode());
        QCOMPARE(widget.sendButton()->text(), QStringLiteral("保存"));

        closeButton->click();

        QVERIFY(!widget.isInEditMode());
        QCOMPARE(widget.sendButton()->text(), QStringLiteral("发送"));
    }

    void recoveryContextRoundTripsReplyDraftWithoutSending()
    {
        ChatComposerWidget source;
        source.messageEditor()->setHtml(QStringLiteral("<b>draft</b>"));
        source.setReplyContext(QStringLiteral("reply-1"), QStringLiteral("sender-1"),
                               QStringLiteral("Alice"), QStringLiteral("reply body"));

        const ComposerRecoveryContext context = source.recoveryContext();
        QCOMPARE(context.replyMessageId, QStringLiteral("reply-1"));
        QCOMPARE(context.replySenderId, QStringLiteral("sender-1"));
        QCOMPARE(context.replySenderName, QStringLiteral("Alice"));
        QCOMPARE(context.replyBody, QStringLiteral("reply body"));
        QVERIFY(context.composerHtml.contains(QStringLiteral("draft")));

        ChatComposerWidget restored;
        QSignalSpy sendSpy(&restored, &ChatComposerWidget::sendTriggered);
        QSignalSpy fileSpy(&restored, &ChatComposerWidget::fileTriggered);
        restored.restoreRecoveryContext(context);

        QCOMPARE(sendSpy.count(), 0);
        QCOMPARE(fileSpy.count(), 0);
        QCOMPARE(restored.replyToMessageId(), QStringLiteral("reply-1"));
        QCOMPARE(restored.replyToSenderName(), QStringLiteral("Alice"));
        QVERIFY(restored.messageEditor()->toHtml().contains(QStringLiteral("draft")));
    }

    void recoveryContextRoundTripsEditWithoutSending()
    {
        ChatComposerWidget source;
        source.enterEditMode(QStringLiteral("edit-1"), QStringLiteral("edited body"));

        const ComposerRecoveryContext context = source.recoveryContext();
        QCOMPARE(context.editingMessageId, QStringLiteral("edit-1"));
        QVERIFY(context.editingBody.contains(QStringLiteral("edited body")));

        ChatComposerWidget restored;
        QSignalSpy sendSpy(&restored, &ChatComposerWidget::sendTriggered);
        restored.restoreRecoveryContext(context);

        QCOMPARE(sendSpy.count(), 0);
        QCOMPARE(restored.editingMessageId(), QStringLiteral("edit-1"));
        QVERIFY(restored.messageEditor()->toHtml().contains(QStringLiteral("edited body")));
    }

    void recoveryContextChangeSignalCoversDraftReplyAndEditState()
    {
        ChatComposerWidget widget;
        QSignalSpy spy(&widget, &ChatComposerWidget::recoveryContextChanged);

        widget.messageEditor()->setPlainText(QStringLiteral("draft"));
        QTRY_VERIFY(spy.count() >= 1);
        const int afterDraft = spy.count();

        widget.setReplyContext(QStringLiteral("reply-1"), QStringLiteral("sender-1"),
                               QStringLiteral("Alice"), QStringLiteral("body"));
        QVERIFY(spy.count() > afterDraft);
        const int afterReply = spy.count();

        widget.clearReplyContext();
        QVERIFY(spy.count() > afterReply);
        const int afterClear = spy.count();

        widget.enterEditMode(QStringLiteral("edit-1"), QStringLiteral("body"));
        QVERIFY(spy.count() > afterClear);
        const int afterEdit = spy.count();

        widget.exitEditMode();
        QVERIFY(spy.count() > afterEdit);
    }

    void destructiveComposerTransitionsRequestImmediatePersistence()
    {
        ChatComposerWidget widget;
        QSignalSpy committedSpy(&widget, &ChatComposerWidget::recoveryContextCommitted);

        widget.messageEditor()->setPlainText(QStringLiteral("draft"));
        widget.messageEditor()->clear();
        QVERIFY(committedSpy.count() >= 1);
        const int afterClear = committedSpy.count();

        widget.setReplyContext(QStringLiteral("reply-1"), QStringLiteral("sender-1"),
                               QStringLiteral("Alice"), QStringLiteral("body"));
        widget.clearReplyContext();
        QVERIFY(committedSpy.count() > afterClear);
        const int afterReply = committedSpy.count();

        widget.enterEditMode(QStringLiteral("edit-1"), QStringLiteral("body"));
        widget.exitEditMode();
        QVERIFY(committedSpy.count() > afterReply);
    }

    void editCloseButtonDoesNotExtendComposerObjectLayout()
    {
        QFile header(QDir(QStringLiteral(LEYOCHAT_SOURCE_DIR))
                         .filePath(QStringLiteral("src/ui/ChatComposerWidget.h")));
        QVERIFY2(header.open(QIODevice::ReadOnly | QIODevice::Text),
                 "ChatComposerWidget.h must be readable for the source contract check");

        const QString source = QString::fromUtf8(header.readAll());
        QVERIFY2(!source.contains(QStringLiteral("m_editCloseButton")),
                 "The edit close button must stay as a child widget, not a ChatComposerWidget "
                 "data member. Adding private members here changes the object layout and can "
                 "break partially rebuilt installer packages.");
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestChatComposerWidget tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestChatComposerWidget.moc"
