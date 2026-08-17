#pragma once

#include <ElaDialog.h>

#include "domain/PeerEndpoint.h"

class ElaCheckBox;
class ElaLineEdit;
class ElaPushButton;
class ElaText;
class QWidget;

class CreateGroupDialog : public ElaDialog {
    Q_OBJECT

public:
    explicit CreateGroupDialog(const QList<PeerEndpoint>& peers, QWidget* parent = nullptr);

    void preSelectMembers(const QStringList& clientIds);

    QString groupName() const;
    QStringList selectedMemberIds() const;
    int selectedCount() const;
    int visibleMemberCount() const;
    QString selectionSummaryText() const;
    bool isConfirmEnabled() const;

private:
    void refreshSelectionSummary();
    void refreshConfirmState();
    void applySearchFilter(const QString& text);

    ElaLineEdit* m_nameEdit = nullptr;
    ElaLineEdit* m_searchEdit = nullptr;
    ElaText* m_selectionSummaryLabel = nullptr;
    ElaText* m_visibleCountLabel = nullptr;
    ElaText* m_emptyStateLabel = nullptr;
    QList<ElaCheckBox*> m_checkboxes;
    ElaPushButton* m_okButton = nullptr;
    QWidget* m_membersWidget = nullptr;
};
