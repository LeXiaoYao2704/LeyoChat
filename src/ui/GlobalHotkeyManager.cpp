#include "ui/GlobalHotkeyManager.h"

#include <QApplication>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

GlobalHotkeyManager::GlobalHotkeyManager(QObject* parent)
    : QObject(parent)
{
    qApp->installNativeEventFilter(this);
}

GlobalHotkeyManager::~GlobalHotkeyManager()
{
    unregisterHotkey();
    qApp->removeNativeEventFilter(this);
}

bool GlobalHotkeyManager::registerHotkey(const QKeySequence& keySequence)
{
#ifdef Q_OS_WIN
    unregisterHotkey();
    if (keySequence.isEmpty()) {
        return false;
    }
    unsigned int modifiers = 0;
    unsigned int vk = 0;
    if (!toWin32Hotkey(keySequence, modifiers, vk)) {
        qWarning() << "[GlobalHotkey] cannot parse key sequence:" << keySequence.toString();
        return false;
    }
    if (!RegisterHotKey(nullptr, kHotkeyId, modifiers | MOD_NOREPEAT, vk)) {
        qWarning() << "[GlobalHotkey] RegisterHotKey failed for" << keySequence.toString()
                    << "error:" << GetLastError();
        return false;
    }
    m_currentSequence = keySequence;
    m_registered = true;
    qInfo() << "[GlobalHotkey] registered:" << keySequence.toString();
    return true;
#else
    Q_UNUSED(keySequence)
    return false;
#endif
}

void GlobalHotkeyManager::unregisterHotkey()
{
#ifdef Q_OS_WIN
    if (m_registered) {
        UnregisterHotKey(nullptr, kHotkeyId);
        qInfo() << "[GlobalHotkey] unregistered:" << m_currentSequence.toString();
        m_registered = false;
        m_currentSequence = QKeySequence();
    }
#endif
}

bool GlobalHotkeyManager::testHotkeyAvailable(const QKeySequence& keySequence)
{
#ifdef Q_OS_WIN
    if (keySequence.isEmpty()) {
        return false;
    }
    unsigned int modifiers = 0;
    unsigned int vk = 0;
    if (!toWin32Hotkey(keySequence, modifiers, vk)) {
        return false;
    }
    if (!RegisterHotKey(nullptr, kTestHotkeyId, modifiers | MOD_NOREPEAT, vk)) {
        return false;
    }
    UnregisterHotKey(nullptr, kTestHotkeyId);
    return true;
#else
    Q_UNUSED(keySequence)
    return false;
#endif
}

QKeySequence GlobalHotkeyManager::currentHotkey() const
{
    return m_currentSequence;
}

bool GlobalHotkeyManager::isRegistered() const
{
    return m_registered;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool GlobalHotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
#else
bool GlobalHotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message, long* result)
#endif
{
    Q_UNUSED(result)
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        const auto* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY && static_cast<int>(msg->wParam) == kHotkeyId) {
            emit hotkeyTriggered();
            return true;
        }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
#endif
    return false;
}

bool GlobalHotkeyManager::toWin32Hotkey(const QKeySequence& seq, unsigned int& modifiers, unsigned int& vk)
{
#ifdef Q_OS_WIN
    if (seq.isEmpty()) {
        return false;
    }
    const auto combination = seq[0];
    const Qt::KeyboardModifiers qtMods = combination.keyboardModifiers();
    const int key = combination.key();

    modifiers = 0;
    if (qtMods & Qt::ControlModifier) modifiers |= MOD_CONTROL;
    if (qtMods & Qt::AltModifier)     modifiers |= MOD_ALT;
    if (qtMods & Qt::ShiftModifier)   modifiers |= MOD_SHIFT;
    if (qtMods & Qt::MetaModifier)    modifiers |= MOD_WIN;

    // 字母 A-Z
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        vk = static_cast<unsigned int>('A' + (key - Qt::Key_A));
        return true;
    }
    // 数字 0-9
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        vk = static_cast<unsigned int>('0' + (key - Qt::Key_0));
        return true;
    }
    // F1-F24
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        vk = static_cast<unsigned int>(VK_F1 + (key - Qt::Key_F1));
        return true;
    }
    // 常用特殊键
    switch (key) {
    case Qt::Key_Space:     vk = VK_SPACE; return true;
    case Qt::Key_Return:    vk = VK_RETURN; return true;
    case Qt::Key_Enter:     vk = VK_RETURN; return true;
    case Qt::Key_Escape:    vk = VK_ESCAPE; return true;
    case Qt::Key_Tab:       vk = VK_TAB; return true;
    case Qt::Key_Backspace: vk = VK_BACK; return true;
    case Qt::Key_Delete:    vk = VK_DELETE; return true;
    case Qt::Key_Insert:    vk = VK_INSERT; return true;
    case Qt::Key_Home:      vk = VK_HOME; return true;
    case Qt::Key_End:       vk = VK_END; return true;
    case Qt::Key_PageUp:    vk = VK_PRIOR; return true;
    case Qt::Key_PageDown:  vk = VK_NEXT; return true;
    case Qt::Key_Print:     vk = VK_SNAPSHOT; return true;
    default: break;
    }
    qWarning() << "[GlobalHotkey] unsupported key:" << key;
    return false;
#else
    Q_UNUSED(seq)
    Q_UNUSED(modifiers)
    Q_UNUSED(vk)
    return false;
#endif
}
