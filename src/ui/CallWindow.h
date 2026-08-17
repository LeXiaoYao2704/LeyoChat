#pragma once

#include "call/CallSession.h"

#include <QWidget>

class ElaText;
class QTimer;
class QVBoxLayout;
class ElaToolButton;
class ElaPushButton;

// 独立通话弹窗，统一处理呼出、来电、通话中、结束全部状态
class CallWindow : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Outgoing, Incoming };

    explicit CallWindow(Mode mode, const QString& peerName,
                        const QString& avatarPath, QWidget* parent = nullptr);

    void updateState(CallSession::State state);
    void setAudioMuted(bool muted);
    void setScreenSharing(bool sharing);
    void setRemoteControlActive(bool active);

signals:
    void cancelClicked();       // 呼出方取消
    void answerClicked();       // 来电方接听
    void rejectClicked();       // 来电方拒绝
    void hangupClicked();       // 通话中挂断
    void muteToggled(bool muted);
    void screenShareClicked();
    void remoteControlClicked();

protected:
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void buildUi();
    void applyState();
    void updateDuration();

    Mode m_mode;
    QString m_peerName;
    CallSession::State m_state = CallSession::State::Idle;

    // 拖动
    bool m_dragging = false;
    QPoint m_dragStart;

    // UI 元素
    ElaText* m_avatarLabel = nullptr;
    ElaText* m_nameLabel = nullptr;
    ElaText* m_statusLabel = nullptr;
    ElaText* m_durationLabel = nullptr;
    ElaText* m_dotIndicator = nullptr;

    ElaToolButton* m_closeButton = nullptr;
    ElaToolButton* m_minimizeButton = nullptr;
    ElaToolButton* m_muteButton = nullptr;
    ElaToolButton* m_screenShareButton = nullptr;
    ElaToolButton* m_remoteControlButton = nullptr;
    ElaPushButton* m_primaryButton = nullptr;     // 取消/挂断/拒绝
    ElaPushButton* m_answerButton = nullptr;       // 来电时的接听按钮

    QWidget* m_toolRow = nullptr;

    QTimer* m_durationTimer = nullptr;
    qint64 m_activeStartMs = 0;
    bool m_muted = false;
};
