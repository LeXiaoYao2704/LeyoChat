#pragma once

#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QObject>

/// 全局截图热键管理器（Windows 专用）
/// 通过 Win32 RegisterHotKey 注册系统级热键，
/// 拦截 WM_HOTKEY 消息并发出 hotkeyTriggered 信号。
class GlobalHotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit GlobalHotkeyManager(QObject* parent = nullptr);
    ~GlobalHotkeyManager() override;

    /// 注册全局热键。若已注册则先注销再重新注册。
    /// 返回 true 表示注册成功，false 表示被占用或无效。
    bool registerHotkey(const QKeySequence& keySequence);

    /// 注销当前已注册的热键。
    void unregisterHotkey();

    /// 检测指定快捷键是否可用（尝试注册后立即注销）。
    /// 返回 true 表示可用，false 表示已被占用。
    static bool testHotkeyAvailable(const QKeySequence& keySequence);

    /// 当前已注册的快捷键，未注册时返回空。
    QKeySequence currentHotkey() const;

    /// 是否已成功注册。
    bool isRegistered() const;

signals:
    /// 用户按下已注册的全局热键时发出。
    void hotkeyTriggered();

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;
#else
    bool nativeEventFilter(const QByteArray& eventType, void* message, long* result) override;
#endif

private:
    static constexpr int kHotkeyId = 0x4843; // "HC" in hex
    static constexpr int kTestHotkeyId = 0x4844;

    /// 将 QKeySequence 拆分为 Win32 修饰键和虚拟键码。
    static bool toWin32Hotkey(const QKeySequence& seq, unsigned int& modifiers, unsigned int& vk);

    QKeySequence m_currentSequence;
    bool m_registered = false;
};
