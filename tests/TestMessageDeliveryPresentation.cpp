#include <QtTest>

#include "ui/MessageDeliveryPresentation.h"

class TestMessageDeliveryPresentation : public QObject {
    Q_OBJECT

private slots:
    void serverAckedIsPresentedAsSent()
    {
        QCOMPARE(messageDeliveryStateText(MessageDeliveryState::ServerAcked),
                 QStringLiteral("已发送"));
        QCOMPARE(messageDeliveryIndicatorText(MessageDeliveryState::ServerAcked,
                                              true,
                                              0,
                                              0),
                 QStringLiteral("已发送"));
    }

    void unknownStateIsNotPresentedAsSending()
    {
        QCOMPARE(messageDeliveryStateText(static_cast<MessageDeliveryState>(999)),
                 QStringLiteral("状态未知"));
    }
};

QTEST_MAIN(TestMessageDeliveryPresentation)
#include "TestMessageDeliveryPresentation.moc"
