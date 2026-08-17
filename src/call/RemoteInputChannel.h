#pragma once

#include <QObject>
#include <QByteArray>

// 远程控制输入事件定义 + 收发
// 事件通过现有 PeerConnection TCP 通道传输（复用 CallControl 消息）
class RemoteInputChannel : public QObject {
    Q_OBJECT
public:
    explicit RemoteInputChannel(QObject* parent = nullptr);

    // 接收端：注入鼠标/键盘事件到本地系统
    void injectMouseEvent(int type, int x, int y, int button);
    void injectKeyEvent(int type, int key, int modifiers);

    // 是否启用输入注入（安全开关）
    void setEnabled(bool enabled);
    bool isEnabled() const;

signals:
    void error(const QString& message);

private:
    bool m_enabled = false;
};
