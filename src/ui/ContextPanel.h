#pragma once

#include <QFrame>
#include <QSet>

class ElaText;
class QVBoxLayout;

class ContextPanel : public QFrame {
    Q_OBJECT

public:
    explicit ContextPanel(QWidget* parent = nullptr);

    void showPrivateProfile(const QString& peerId);
    void showGroupContext(const QString& groupId);
    bool isPinnedOpen() const;
    bool hasSection(const QString& key) const;

private:
    void refreshSummary();

    QVBoxLayout* m_layout = nullptr;
    ElaText* m_titleLabel = nullptr;
    ElaText* m_summaryLabel = nullptr;
    bool m_pinnedOpen = false;
    QString m_contextId;
    QSet<QString> m_sections;
};
