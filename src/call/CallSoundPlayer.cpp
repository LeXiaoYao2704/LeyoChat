#include "call/CallSoundPlayer.h"

#include <QUrl>

CallSoundPlayer::CallSoundPlayer(QObject* parent)
    : QObject(parent)
{
    m_ringback.setSource(QUrl(QStringLiteral("qrc:/sounds/ringback.wav")));
    m_ringback.setLoopCount(QSoundEffect::Infinite);
    m_ringback.setVolume(0.6);

    m_ringtone.setSource(QUrl(QStringLiteral("qrc:/sounds/ringtone.wav")));
    m_ringtone.setLoopCount(QSoundEffect::Infinite);
    m_ringtone.setVolume(0.8);

    m_hangup.setSource(QUrl(QStringLiteral("qrc:/sounds/hangup.wav")));
    m_hangup.setLoopCount(1);
    m_hangup.setVolume(0.5);
}

void CallSoundPlayer::playRingback()
{
    stopAll();
    m_ringback.play();
}

void CallSoundPlayer::playRingtone()
{
    stopAll();
    m_ringtone.play();
}

void CallSoundPlayer::playHangup()
{
    stopAll();
    m_hangup.play();
}

void CallSoundPlayer::stopAll()
{
    m_ringback.stop();
    m_ringtone.stop();
    m_hangup.stop();
}
