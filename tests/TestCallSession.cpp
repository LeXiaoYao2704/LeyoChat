#include <QtTest/QTest>
#include <QtTest/QSignalSpy>

#include "call/CallSession.h"

class TestCallSession : public QObject {
    Q_OBJECT

private slots:
    void startCallTransitionsToOutgoingRing()
    {
        CallSession session;

        QSignalSpy signalSpy(&session, &CallSession::outgoingSignal);
        QVERIFY(session.startCall(QStringLiteral("call-1"), QStringLiteral("peer-1"), 1));

        QCOMPARE(session.state(), CallSession::State::OutgoingRing);
        QCOMPARE(signalSpy.count(), 1);
    }

    void incomingOfferAnswerTransitionsToConnecting()
    {
        CallSession session;
        QVERIFY(session.handleIncomingOffer(QStringLiteral("call-2"), QStringLiteral("peer-2"), 1));
        QCOMPARE(session.state(), CallSession::State::IncomingRing);

        session.answerCall();
        QCOMPARE(session.state(), CallSession::State::Connecting);
    }

    void busySignalReturnsToIdle()
    {
        CallSession session;
        QVERIFY(session.startCall(QStringLiteral("call-3"), QStringLiteral("peer-3"), 1));

        CallControlPayload payload;
        payload.type = CallControlType::Busy;
        payload.callId = "call-3";
        session.handleCallControl(payload);

        QCOMPARE(session.state(), CallSession::State::Idle);
    }
};

int main(int argc, char** argv)
{
    TestCallSession tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestCallSession.moc"
