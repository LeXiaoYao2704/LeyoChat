#pragma once

#include <ElaDialog.h>
#include "ClientPreferences.h"

class ElaCheckBox;

class CloseToTrayDialog : public ElaDialog
{
    Q_OBJECT

public:
    explicit CloseToTrayDialog(QWidget* parent = nullptr);

    ClientCloseAction selectedAction() const;
    bool dontAskAgain() const;

private:
    ElaCheckBox* m_rememberCheck{nullptr};
    ClientCloseAction m_selectedAction{ClientCloseAction::MinimizeToTray};
};
