#pragma once

#include <string>

#include <QtGlobal>

enum class CallControlType {
    Offer,
    Answer,
    Reject,
    Busy,
    Cancel,
    Hangup,
    MediaReady,
    ScreenShareStart,
    ScreenShareStop,
    RemoteControlRequest,
    RemoteControlGrant,
    RemoteControlRevoke,
    RemoteMouseInput,
    RemoteKeyInput
};

enum class CallMediaFlag : int {
    None = 0,
    Audio = 1,
    ScreenShare = 2,
    RemoteControl = 4
};

inline CallMediaFlag operator|(CallMediaFlag left, CallMediaFlag right)
{
    return static_cast<CallMediaFlag>(static_cast<int>(left) | static_cast<int>(right));
}

inline bool hasCallMediaFlag(int flags, CallMediaFlag flag)
{
    return (flags & static_cast<int>(flag)) != 0;
}

struct CallControlPayload {
    CallControlType type = CallControlType::Offer;
    std::string callId;
    std::string senderId;
    std::string targetId;
    std::string reason;
    int mediaFlags = 0;
    quint16 audioUdpPort = 0;
    quint16 screenTcpPort = 0;
    // 远程输入事件字段
    int inputType = 0;       // 鼠标: 0=press,1=release,2=move,3=wheel; 键盘: 0=press,1=release
    int inputX = 0;
    int inputY = 0;
    int inputButton = 0;     // 鼠标按钮 / 键盘 key
    int inputModifiers = 0;  // 键盘修饰符
};
