#pragma once

#include <ElaDialog.h>

#include "domain/ChatMessage.h"

#include <QDate>
#include <QHash>
#include <QSet>
#include <vector>

class ElaComboBox;
class ElaLineEdit;
class ElaPushButton;
class QListWidget;
class QListWidgetItem;
class ElaListWidget;
class ElaPivot;
class ConversationHistoryCalendarWidget;
class QWidget;

class ConversationHistoryDialog : public ElaDialog {
    Q_OBJECT

public:
    explicit ConversationHistoryDialog(const QString& conversationTitle,
                                       const std::vector<ChatMessage>& records,
                                       const QString& localDisplayName,
                                       const QHash<QString, QString>& senderDisplayNames = {},
                                       QWidget* parent = nullptr);

signals:
    void messageJumpRequested(const QString& conversationId, const QString& messageId);

private slots:
    void applyFilters();
    void onPivotClicked(int index);
    void onDateSelected(const QDate& date);
    void onResultActivated(QListWidgetItem* item);

private:
    void populateResults(const std::vector<const ChatMessage*>& filtered);
    void populateMemberCombo();
    QSet<QDate> collectActiveDates() const;
    bool matchesTypeFilter(const ChatMessage& msg, int filterIndex) const;
    bool isImageExtension(const QString& name) const;
    bool isImageMessage(const ChatMessage& msg) const;
    bool isDocumentMessage(const ChatMessage& msg) const;
    bool isLinkMessage(const ChatMessage& msg) const;
    QString attachmentDisplayName(const ChatMessage& msg) const;
    QString messagePreviewText(const ChatMessage& msg) const;
    QString messageSearchText(const ChatMessage& msg) const;
    QWidget* createResultRowWidget(const ChatMessage& msg) const;
    QString senderDisplayName(const ChatMessage& msg) const;
    void exportToTxt(const QString& filePath) const;
    void exportToHtml(const QString& filePath) const;
    void onExportClicked();

    std::vector<ChatMessage> m_records;
    QString m_conversationTitle;
    QString m_localDisplayName;
    QHash<QString, QString> m_senderDisplayNames;

    ElaLineEdit* m_searchEdit = nullptr;
    ElaPivot* m_typePivot = nullptr;
    ElaListWidget* m_resultList = nullptr;
    ElaComboBox* m_memberCombo = nullptr;
    ConversationHistoryCalendarWidget* m_calendar = nullptr;

    int m_currentTypeFilter = 0;   // 0=全部, 1=文本, 2=文档, 3=图片, 4=链接, 5=其他
    QDate m_selectedDate;
    std::vector<const ChatMessage*> m_filteredResults;
};
