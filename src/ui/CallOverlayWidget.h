#pragma once

#include <QWidget>

class ElaText;
class ElaPushButton;
class QTimer;

class CallOverlayWidget : public QWidget {
    Q_OBJECT

public:
    explicit CallOverlayWidget(const QString& peerName, QWidget* parent = nullptr);

    void setAudioMuted(bool muted);
    void setScreenShareVisible(bool visible);
    void setRemoteControlVisible(bool visible);

signals:
    void hangupClicked();
    void muteToggled(bool muted);
    void screenShareClicked();
    void remoteControlClicked();

private:
    void updateDuration();

    ElaText* m_durationLabel = nullptr;
    ElaPushButton* m_muteButton = nullptr;
    ElaPushButton* m_screenShareButton = nullptr;
    ElaPushButton* m_remoteControlButton = nullptr;
    bool m_muted = false;
    qint64 m_startedAtMs = 0;
    QTimer* m_durationTimer = nullptr;
};
