#include "call/RemoteInputChannel.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

RemoteInputChannel::RemoteInputChannel(QObject* parent)
    : QObject(parent)
{
}

void RemoteInputChannel::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool RemoteInputChannel::isEnabled() const
{
    return m_enabled;
}

void RemoteInputChannel::injectMouseEvent(int type, int x, int y, int button)
{
    if (!m_enabled) return;

#ifdef Q_OS_WIN
    // 使用虚拟屏幕（所有显示器合并区域），支持多显示器
    const int virtualLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int virtualTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (screenWidth <= 0 || screenHeight <= 0) return;

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>((x - virtualLeft) * 65535 / screenWidth);
    input.mi.dy = static_cast<LONG>((y - virtualTop) * 65535 / screenHeight);
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE;

    switch (type) {
    case 0: // press
        if (button == 1) input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
        else if (button == 2) input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
        else if (button == 4) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
        break;
    case 1: // release
        if (button == 1) input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
        else if (button == 2) input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
        else if (button == 4) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
        break;
    case 2: // move only
        break;
    case 3: // wheel
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = static_cast<DWORD>(y);  // y = angleDelta.y()
        break;
    default:
        return;
    }

    SendInput(1, &input, sizeof(INPUT));
#else
    Q_UNUSED(type)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(button)
    emit error(QStringLiteral("远程控制仅支持 Windows"));
#endif
}

void RemoteInputChannel::injectKeyEvent(int type, int key, int modifiers)
{
    if (!m_enabled) return;

#ifdef Q_OS_WIN
    Q_UNUSED(modifiers)

    // 将 Qt key 映射到 Windows 虚拟键码（基本映射）
    WORD vk = 0;
    if (key >= 0x20 && key <= 0x7E) {
        // 可打印 ASCII 字符
        SHORT scanResult = VkKeyScanW(static_cast<WCHAR>(key));
        if (scanResult == -1) return;  // 当前键盘布局无法映射
        vk = static_cast<WORD>(scanResult & 0xFF);
    } else {
        switch (key) {
        case Qt::Key_Return:    vk = VK_RETURN; break;
        case Qt::Key_Enter:     vk = VK_RETURN; break;
        case Qt::Key_Escape:    vk = VK_ESCAPE; break;
        case Qt::Key_Tab:       vk = VK_TAB; break;
        case Qt::Key_Backspace: vk = VK_BACK; break;
        case Qt::Key_Delete:    vk = VK_DELETE; break;
        case Qt::Key_Insert:    vk = VK_INSERT; break;
        case Qt::Key_Home:      vk = VK_HOME; break;
        case Qt::Key_End:       vk = VK_END; break;
        case Qt::Key_PageUp:    vk = VK_PRIOR; break;
        case Qt::Key_PageDown:  vk = VK_NEXT; break;
        case Qt::Key_Left:      vk = VK_LEFT; break;
        case Qt::Key_Right:     vk = VK_RIGHT; break;
        case Qt::Key_Up:        vk = VK_UP; break;
        case Qt::Key_Down:      vk = VK_DOWN; break;
        case Qt::Key_Shift:     vk = VK_SHIFT; break;
        case Qt::Key_Control:   vk = VK_CONTROL; break;
        case Qt::Key_Alt:       vk = VK_MENU; break;
        case Qt::Key_Space:     vk = VK_SPACE; break;
        case Qt::Key_F1:        vk = VK_F1; break;
        case Qt::Key_F2:        vk = VK_F2; break;
        case Qt::Key_F3:        vk = VK_F3; break;
        case Qt::Key_F4:        vk = VK_F4; break;
        case Qt::Key_F5:        vk = VK_F5; break;
        case Qt::Key_F6:        vk = VK_F6; break;
        case Qt::Key_F7:        vk = VK_F7; break;
        case Qt::Key_F8:        vk = VK_F8; break;
        case Qt::Key_F9:        vk = VK_F9; break;
        case Qt::Key_F10:       vk = VK_F10; break;
        case Qt::Key_F11:       vk = VK_F11; break;
        case Qt::Key_F12:       vk = VK_F12; break;
        default:
            return;
        }
    }

    if (vk == 0) return;

    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = (type == 1) ? KEYEVENTF_KEYUP : 0;

    SendInput(1, &input, sizeof(INPUT));
#else
    Q_UNUSED(type)
    Q_UNUSED(key)
    Q_UNUSED(modifiers)
    emit error(QStringLiteral("远程控制仅支持 Windows"));
#endif
}
