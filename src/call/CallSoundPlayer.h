#pragma once

#include <QObject>
#include <QSoundEffect>

// 通话音效管理器：管理呼出回铃音、来电铃声、挂断提示音
class CallSoundPlayer : public QObject {
    Q_OBJECT
public:
    explicit CallSoundPlayer(QObject* parent = nullptr);

    void playRingback();   // 呼出等待音，循环
    void playRingtone();   // 来电铃声，循环
    void playHangup();     // 挂断提示音，单次
    void stopAll();        // 停止所有播放

private:
    QSoundEffect m_ringback;
    QSoundEffect m_ringtone;
    QSoundEffect m_hangup;
};
