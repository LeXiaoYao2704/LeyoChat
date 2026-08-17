#pragma once

#include <QWidget>

class ElaText;
class ElaToolButton;
class QStackedWidget;
class ElaStackedWidget;
class QFrame;
class ElaFrame;
class QTimer;

class ChatHeaderWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChatHeaderWidget(QWidget* parent = nullptr);
    void refreshTheme();

    void setDirectChatState(const QString& name,
                            const QString& status,
                            const QString& signature,
                            const QString& avatarImagePath = QString());
    void setGroupChatState(const QString& groupName, const QString& memberSummary);
    void setGroupRuntimeState(const QString& statusValue,
                              const QString& consoleHint,
                              const QString& contextChip = QString());
    void clearGroupRuntimeState();

    QString titleText() const;
    QString subtitleText() const;
    QString secondaryText() const;
    bool isGroupMode() const;

    void showTypingIndicator();
    void clearTypingIndicator();

    ElaToolButton* directHistoryButton() const;
    ElaToolButton* directVoiceCallButton() const;
    ElaToolButton* directCreateGroupButton() const;
    ElaToolButton* groupAnnouncementButton() const;
    ElaToolButton* groupAddMemberButton() const;
    ElaToolButton* groupHistoryButton() const;
    ElaToolButton* groupInfoButton() const;
    ElaToolButton* groupSettingsButton() const;
    ElaToolButton* groupFileServiceSettingsButton() const;
    ElaToolButton* groupFileManagerButton() const;
    void setGroupFileManagerVisible(bool visible);
    ElaToolButton* directCloseButton() const;
    ElaToolButton* groupCloseButton() const;

signals:
    void directHistoryRequested();
    void directVoiceCallRequested();
    void directCreateGroupRequested();
    void groupAnnouncementRequested();
    void groupInfoPanelRequested();
    void groupAddMemberRequested();
    void groupHistoryRequested();
    void groupSettingsRequested();
    void groupFileServiceSettingsRequested();
    void groupFileManagerRequested();
    void closeRequested();

private:
    ElaFrame* m_controlBand = nullptr;
    ElaFrame* m_actionTray = nullptr;
    ElaFrame* m_statusStrip = nullptr;
    ElaText* m_modeChipLabel = nullptr;
    ElaText* m_contextChipLabel = nullptr;
    ElaText* m_consoleHintLabel = nullptr;
    ElaText* m_statusValueLabel = nullptr;

    ElaStackedWidget* m_stack = nullptr;

    QWidget* m_directPage = nullptr;
    ElaText* m_directAvatarLabel = nullptr;
    ElaText* m_directTitleLabel = nullptr;
    ElaText* m_directSubtitleLabel = nullptr;
    ElaText* m_directSecondaryLabel = nullptr;
    ElaToolButton* m_directHistoryButton = nullptr;
    ElaToolButton* m_directVoiceCallButton = nullptr;
    ElaToolButton* m_directCreateGroupButton = nullptr;
    ElaToolButton* m_directCloseButton = nullptr;

    QWidget* m_groupPage = nullptr;
    ElaText* m_groupAvatarLabel = nullptr;
    ElaText* m_groupTitleLabel = nullptr;
    ElaText* m_groupSubtitleLabel = nullptr;
    ElaToolButton* m_groupAnnouncementButton = nullptr;
    ElaToolButton* m_groupAddMemberButton = nullptr;
    ElaToolButton* m_groupHistoryButton = nullptr;
    ElaToolButton* m_groupSettingsButton = nullptr;
    ElaToolButton* m_groupFileServiceSettingsButton = nullptr;
    ElaToolButton* m_groupFileManagerButton = nullptr;
    ElaToolButton* m_groupCloseButton = nullptr;
    ElaToolButton* m_groupMoreButton = nullptr;
    QString m_groupRuntimeStatusValue;
    QString m_groupRuntimeConsoleHint;
    QString m_groupRuntimeContextChip;
    QString m_savedDirectStatus;
    QTimer* m_typingTimer = nullptr;
};
