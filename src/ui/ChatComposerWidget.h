#pragma once

#include <QPointer>
#include <QWidget>

#include "ui/ComposerRecoveryContext.h"

class QFrame;
class ElaFrame;
class QTextEdit;
class ElaTextEdit;
class ElaText;
class ElaToolButton;
class ElaPushButton;

class ChatComposerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChatComposerWidget(QWidget* parent = nullptr);

    static QStringList emojiChoicesForTesting();
    static QList<int> fontSizeChoicesForTesting();
    static QStringList fontFamilyChoicesForTesting();

    void setDraftMetaText(const QString& text);
    void refreshTheme();

    void enterEditMode(const QString& messageId, const QString& currentBody);
    void exitEditMode();
    QString editingMessageId() const;
    bool isInEditMode() const;

    void setReplyContext(const QString& messageId, const QString& senderId,
                         const QString& senderName, const QString& bodyPreview);
    void clearReplyContext();
    QString replyToMessageId() const;
    QString replyToSenderId() const;
    QString replyToSenderName() const;
    QString replyToBody() const;

    ComposerRecoveryContext recoveryContext() const;
    void restoreRecoveryContext(const ComposerRecoveryContext& context);

    QWidget* toolbarHost() const;
    ElaTextEdit* messageEditor() const;
    ElaPushButton* sendButton() const;
    ElaToolButton* fileButton() const;
    ElaToolButton* screenshotButton() const;
    ElaPushButton* sendModeButton() const;
    ElaToolButton* nudgeButton() const;
    QWidget* moreActionsButton() const;
    ElaToolButton* devOpsButton() const;
    ElaToolButton* boldButton() const;
    ElaToolButton* italicButton() const;
    ElaToolButton* underlineButton() const;
    ElaToolButton* fontButton() const;
    ElaToolButton* emojiButton() const;
    QStringList secondaryActionLabelsForTesting() const;

signals:
    void sendTriggered();
    void fileTriggered();
    void nudgeTriggered();
    void devOpsTriggered();
    void typingActivity();
    void stickerSelected(const QString& packId, const QString& stickerId);
    void recoveryContextChanged();
    void recoveryContextCommitted();

private:
    QWidget* m_toolbarHost = nullptr;
    ElaFrame* m_controlBand = nullptr;
    ElaText* m_metaChipLabel = nullptr;
    ElaTextEdit* m_messageEditor = nullptr;
    ElaToolButton* m_boldButton = nullptr;
    ElaToolButton* m_italicButton = nullptr;
    ElaToolButton* m_underlineButton = nullptr;
    ElaToolButton* m_fontButton = nullptr;
    ElaToolButton* m_emojiButton = nullptr;
    ElaPushButton* m_sendButton = nullptr;
    ElaToolButton* m_fileButton = nullptr;
    ElaToolButton* m_screenshotButton = nullptr;
    ElaPushButton* m_sendModeButton = nullptr;
    ElaToolButton* m_nudgeButton = nullptr;
    ElaToolButton* m_devOpsButton = nullptr;
    QWidget* m_replyPreviewBar = nullptr;
    ElaText* m_replyPreviewLabel = nullptr;
    QString m_editingMessageId;
    int m_lastEmojiMode = 0; // 0=表情 1=贴纸 2=自定义
    QPointer<ElaFrame> m_emojiPanel;
    void ensureEmojiPanel();
    void showEmojiPanel();
    QString m_replyToMessageId;
    QString m_replyToSenderId;
    QString m_replyToSenderName;
    QString m_replyToBody;
};
