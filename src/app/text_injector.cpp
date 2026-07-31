#include "app/text_injector.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <vector>

namespace app {

#if defined(_WIN32)

namespace {

// UTF-8 -> UTF-16
std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                  static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                        static_cast<int>(s.size()), ws.data(), len);
    return ws;
}

// 检查前台窗口是否可接收输入（非桌面、非任务栏等系统窗口）
bool IsForegroundAcceptable() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;

    // 排除 Shell 窗口（Progman、Shell_TrayWnd 等）
    wchar_t cls[64] = {};
    GetClassNameW(fg, cls, 63);
    if (wcscmp(cls, L"Progman") == 0 ||
        wcscmp(cls, L"Shell_TrayWnd") == 0 ||
        wcscmp(cls, L"WorkerW") == 0) {
        return false;
    }
    return true;
}

// 确保 Overlay 窗口不会在前台（否则 SendInput 会发给 Overlay）
// Overlay 使用 WS_EX_NOACTIVATE，正常不会抢焦点，
// 但在某些边缘情况下仍需检查。
bool EnsureNotOverlayFocused() {
    HWND fg = GetForegroundWindow();
    if (!fg) return true;
    wchar_t cls[64] = {};
    GetClassNameW(fg, cls, 63);
    // Overlay 窗口类名（见 overlay.cpp 中的注册）
    if (wcscmp(cls, L"T9GamepadOverlay") == 0) {
        return false;  // Overlay 在前台，不注入
    }
    return true;
}

}  // namespace

bool InjectText(const std::string& utf8_text) {
    if (utf8_text.empty()) return false;

#ifdef T9IME_TESTING
    // 测试模式：不实际注入，仅返回成功
    return true;
#endif

    std::wstring text = Utf8ToWide(utf8_text);
    if (text.empty()) return false;

    // 前台窗口检查：确保有可接收输入的窗口
    if (!IsForegroundAcceptable() || !EnsureNotOverlayFocused()) {
        // 没有合适的前台窗口，不注入
        return false;
    }

    // 为每个字符生成 KEYDOWN + KEYUP 事件
    std::vector<INPUT> inputs;
    inputs.reserve(text.size() * 2);

    for (wchar_t ch : text) {
        INPUT down = {};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = ch;
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(down);

        INPUT up = {};
        up.type = INPUT_KEYBOARD;
        up.ki.wScan = ch;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }

    // 短暂延时，确保前台窗口已准备好接收输入
    Sleep(10);

    UINT sent = SendInput(static_cast<UINT>(inputs.size()),
                          inputs.data(), sizeof(INPUT));
    return sent == static_cast<UINT>(inputs.size());
}

bool InjectBackspace() {
#ifdef T9IME_TESTING
    return true;
#endif
    if (!IsForegroundAcceptable() || !EnsureNotOverlayFocused()) {
        return false;
    }

    // 发送 VK_BACK (退格键) 的 KEYDOWN + KEYUP
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_BACK;
    inputs[0].ki.dwFlags = 0;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_BACK;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    Sleep(10);
    UINT sent = SendInput(2, inputs, sizeof(INPUT));
    return sent == 2;
}

#else  // 非 Windows 平台：桩实现

bool InjectText(const std::string& utf8_text) {
    (void)utf8_text;
    return false;
}

bool InjectBackspace() {
    return false;
}

#endif

}  // namespace app
