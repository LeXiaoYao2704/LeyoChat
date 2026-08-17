#pragma once

#include <ElaDialog.h>

class ElaText;
class QTimer;

class IncomingCallDialog : public ElaDialog {
    Q_OBJECT

public:
    explicit IncomingCallDialog(const QString& callerName,
                                const QString& callId,
                                QWidget* parent = nullptr);

signals:
    void answered(const QString& callId);
    void rejected(const QString& callId);

private:
    QString m_callId;
    QTimer* m_autoRejectTimer = nullptr;
};
