// T9 手柄九宫格输入法 —— 主程序
//
// 数据流：XInputPad(手柄) -> ImeController(开关/翻译) -> T9Engine -> PinyinIme(候选)
//
// 九宫格布局（5 键移到左上，中心留空），右摇杆拨动输入：
//     5 jkl │ 2 abc │ 3 def
//     4 ghi │  ·    │ 6 mno
//     7pqrs │ 8 tuv │ 9wxyz
//
// 启动行为：
//   - 以托盘图标静默运行，不显示界面
//   - 按手柄快捷键（config.ini toggle_hotkey）开启并显示界面
//   - 再次按快捷键关闭并隐藏界面
//   - 托盘左键点击：同快捷键切换
//   - 托盘右键菜单：开机启动（可勾选）、退出
//   - Ctrl+Alt+Q：退出程序

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#include "app/ally_hid_controller.h"
#include "app/config.h"
#include "app/desktop_controller.h"
#include "app/ime_controller.h"
#include "gamepad/xinput_pad.h"
#include "ime/pinyin_ime.h"
#include "t9/keypad.h"
#include "t9/t9_engine.h"
#include "t9/t9_keymap.h"
#include "ui/overlay.h"

namespace {

// ---- 通用辅助 ----

void EnableUtf8Console() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

bool FileExists(const std::string& p) {
    if (p.empty()) return false;
    FILE* f = std::fopen(p.c_str(), "rb");
    if (f) { std::fclose(f); return true; }
    return false;
}

std::string ExeDir() {
#if defined(_WIN32)
    char buf[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    std::string path(buf, n);
    size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? std::string{} : path.substr(0, slash + 1);
#else
    return {};
#endif
}

// 将诊断信息同时输出到 OutputDebugStringA 和日志文件
void WriteLog(const char* msg) {
#if defined(_WIN32)
    OutputDebugStringA(msg);
#endif
    std::string dir = ExeDir();
    if (!dir.empty()) {
        std::string path = dir + "t9ime.log";
        FILE* f = std::fopen(path.c_str(), "a");
        if (f) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            std::fprintf(f, "[%02d:%02d:%02d.%03d] %s",
                         st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
            std::fclose(f);
        }
    }
}

std::string ResolveDict(const std::string& given) {
    if (FileExists(given)) return given;
    std::string dir = ExeDir();
    if (!dir.empty()) {
        const char* rels[] = {"data/dict_pinyin.dat", "../data/dict_pinyin.dat",
                              "../../data/dict_pinyin.dat"};
        for (const char* r : rels) {
            std::string cand = dir + r;
            if (FileExists(cand)) return cand;
        }
    }
    return given;
}

// 在 exe 所在目录及上级目录查找 config.ini
std::string ResolveConfig(const std::string& given) {
    if (FileExists(given)) return given;
    std::string dir = ExeDir();
    if (!dir.empty()) {
        const char* rels[] = {"config.ini", "../config.ini", "../../config.ini"};
        for (const char* r : rels) {
            std::string cand = dir + r;
            if (FileExists(cand)) return cand;
        }
    }
    return given;
}

ui::OverlayState MakeOverlayState(const t9::T9Engine& engine,
                                  const app::ImeController& ime,
                                  const app::Config& cfg,
                                  bool pad_connected) {
    ui::OverlayState s;
    s.enabled        = ime.Enabled();
    s.pad_connected  = pad_connected;
    s.pointing       = ime.Pointing();
    s.letter_mode    = ime.LetterMode();
    s.letter_text    = ime.LetterText();
    s.digits         = engine.Digits();
    s.pinyin         = engine.PinyinCandidates(32);
    s.candidates     = ime.Candidates();
    s.selected       = ime.SelectedIndex();
    s.page_start     = ime.PageStart();
    s.page_size      = ime.PageSize();
    s.last_committed = ime.LastCommitted();
    s.hotkey_desc    = app::DescribeCombo(cfg.toggle_combo);
    return s;
}

// ---- 托盘图标 ----

constexpr UINT kTrayCallbackMsg = WM_APP + 1;
constexpr int kMenuAutoStart = 1001;
constexpr int kMenuQuit = 1002;
constexpr int kQuitHotkeyId = 0x9001;
constexpr int kToggleHotkeyId = 0x9002;  // Ctrl+Alt+E: 键盘切换热键（鼠标模式下备用）
constexpr const wchar_t* kAutoStartKey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kAutoStartName = L"T9GamepadIME";

// 应用图标资源 ID（与 assets/app.rc 中定义一致）
constexpr UINT kAppIconId = 101;

// 托盘 WndProc 需要访问的主循环状态（在 main() 中赋值）
NOTIFYICONDATAW g_nid = {};
bool g_auto_start = false;
app::ImeController* g_ime = nullptr;
app::DesktopController* g_desktop = nullptr;
app::AllyHidController* g_ally_hid = nullptr;
bool g_has_ally = false;
ui::Overlay* g_overlay = nullptr;
const t9::T9Engine* g_engine = nullptr;
const app::Config* g_cfg = nullptr;
bool* g_running = nullptr;
bool g_pad_connected = false;

// 检查开机启动是否已启用（注册表 HKCU\...\Run 下是否有 T9GamepadIME 值）
bool IsAutoStartEnabled() {
    HKEY hkey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAutoStartKey,
                      0, KEY_READ, &hkey) != ERROR_SUCCESS)
        return false;
    DWORD size = 0, type = 0;
    LONG result = RegQueryValueExW(hkey, kAutoStartName, nullptr,
                                   &type, nullptr, &size);
    RegCloseKey(hkey);
    return result == ERROR_SUCCESS && type == REG_SZ && size > 0;
}

// 设置或取消开机启动
bool SetAutoStart(bool enable) {
    HKEY hkey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAutoStartKey,
                      0, KEY_SET_VALUE, &hkey) != ERROR_SUCCESS)
        return false;
    LONG result;
    if (enable) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring quoted = L"\"" + std::wstring(path) + L"\"";
        result = RegSetValueExW(hkey, kAutoStartName, 0, REG_SZ,
                                (const BYTE*)quoted.c_str(),
                                static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(hkey, kAutoStartName);
    }
    RegCloseKey(hkey);
    return result == ERROR_SUCCESS;
}

// 更新托盘提示文字
void UpdateTrayTooltip(bool enabled) {
    if (enabled)
        wcscpy_s(g_nid.szTip, L"T9 手柄输入法 - 开启");
    else
        wcscpy_s(g_nid.szTip, L"T9 手柄输入法 - 关闭");
    g_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// 切换 IME 开关并同步界面与硬件模式
void ToggleIme() {
    if (!g_ime) return;
    g_ime->ToggleEnabled();
    bool en = g_ime->Enabled();
    WriteLog(en ? "[Toggle] IME 开启\n" : "[Toggle] IME 关闭\n");

    if (g_overlay) {
        g_overlay->SetVisible(en);
        if (en && g_engine && g_cfg) {
            g_overlay->Refresh(
                MakeOverlayState(*g_engine, *g_ime, *g_cfg, g_pad_connected));
        }
    }
    // 硬件模式策略：
    //   ROG Ally 始终保持游戏手柄模式（XInput 可用），确保 Start 键始终可检测。
    //   IME 关闭时不切换到硬件鼠标模式，而是用软件 DesktopController 模拟桌面操控。
    //   （SetMouseMode 会导致 XInput 停止工作，Start 键无法检测，快捷键失效。）
    if (g_has_ally && g_ally_hid && en) {
        // 仅在开启时确保处于游戏手柄模式
        if (!g_ally_hid->SetGamepadMode()) {
            std::string err = std::string("[Toggle] SetGamepadMode 失败: ")
                            + g_ally_hid->LastError() + "\n";
            WriteLog(err.c_str());
        }
    }
    // 软件桌面控制器：IME 关闭时对所有设备激活（含 ROG Ally）
    if (g_desktop) g_desktop->SetActive(!en);
    // 关闭界面（IME 关闭）后向前台发送两次 Ctrl+Alt+C
    if (!en && g_desktop) g_desktop->PressCtrlAltCTwice();
    // 刷新两个控制器的内部状态，防止过期状态导致按键功能不生效
    if (g_desktop) g_desktop->ResetState();
    g_ime->ResetState();
    WriteLog(en ? "[Toggle] -> 游戏手柄输入模式\n"
                  : "[Toggle] -> 桌面操控模式（软件）\n");
    UpdateTrayTooltip(en);
}

// 托盘隐藏窗口的 WndProc
LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case kTrayCallbackMsg: {
            if (LOWORD(lp) == WM_RBUTTONUP) {
                // 右键：显示上下文菜单
                POINT pt;
                GetCursorPos(&pt);
                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING | (g_auto_start ? MF_CHECKED : MF_UNCHECKED),
                            kMenuAutoStart, L"开机启动");
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, kMenuQuit, L"退出");
                SetForegroundWindow(hwnd);
                TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN,
                               pt.x, pt.y, 0, hwnd, nullptr);
                PostMessageW(hwnd, WM_NULL, 0, 0);
                DestroyMenu(menu);
            } else if (LOWORD(lp) == WM_LBUTTONUP) {
                // 左键：切换 IME
                ToggleIme();
            }
            return 0;
        }
        case WM_COMMAND: {
            switch (LOWORD(wp)) {
                case kMenuAutoStart:
                    g_auto_start = !g_auto_start;
                    SetAutoStart(g_auto_start);
                    break;
                case kMenuQuit:
                    if (g_running) *g_running = false;
                    break;
            }
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

// 创建消息专用隐藏窗口（用于接收托盘消息）
HWND CreateTrayWindow(HINSTANCE hinst) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &TrayWndProc;
    wc.hInstance = hinst;
    wc.lpszClassName = L"T9TrayWindow";
    RegisterClassExW(&wc);
    return CreateWindowExW(0, wc.lpszClassName, L"T9Tray",
                           WS_OVERLAPPEDWINDOW, 0, 0, 0, 0,
                           HWND_MESSAGE, nullptr, hinst, nullptr);
}

// 添加托盘图标
void AddTrayIcon(HWND hwnd) {
    HINSTANCE hinst = GetModuleHandleW(nullptr);
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = kTrayCallbackMsg;
    // 从 exe 资源加载自定义图标；失败时回退到系统默认图标
    g_nid.hIcon = LoadIconW(hinst, MAKEINTRESOURCEW(kAppIconId));
    if (!g_nid.hIcon) {
        g_nid.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    }
    wcscpy_s(g_nid.szTip, L"T9 手柄输入法 - 关闭");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

// ---- 首次运行标记（HKCU 注册表，持久化，避免每次启动都提示） ----

constexpr const wchar_t* kFirstRunKey = L"Software\\T9GamepadIME";
constexpr const wchar_t* kFirstRunValue = L"FirstRunDone";

bool IsFirstRun() {
    HKEY hkey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kFirstRunKey,
                      0, KEY_READ, &hkey) != ERROR_SUCCESS)
        return true;  // 注册表键不存在 -> 首次运行
    DWORD type = 0, size = 0;
    LONG result = RegQueryValueExW(hkey, kFirstRunValue, nullptr,
                                   &type, nullptr, &size);
    RegCloseKey(hkey);
    return result != ERROR_SUCCESS;
}

void MarkFirstRunDone() {
    HKEY hkey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kFirstRunKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &hkey, nullptr) == ERROR_SUCCESS) {
        DWORD one = 1;
        RegSetValueExW(hkey, kFirstRunValue, 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&one), sizeof(one));
        RegCloseKey(hkey);
    }
}

// 在托盘图标上弹出一个气泡提示
void ShowTrayBalloon(const wchar_t* title, const wchar_t* text) {
    g_nid.uFlags = NIF_INFO;
    wcscpy_s(g_nid.szInfo, text);
    wcscpy_s(g_nid.szInfoTitle, title);
    g_nid.dwInfoFlags = NIIF_INFO;
    g_nid.uTimeout = 10000;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

}  // namespace

int main(int argc, char** argv) {
    // 解析参数：--selftest[=拼音] 非交互自检，其余为词典路径。
    std::string selftest_py;
    bool selftest = false;
    std::vector<std::string> pos;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--selftest") {
            selftest = true;
        } else if (a.rfind("--selftest=", 0) == 0) {
            selftest = true;
            selftest_py = a.substr(11);
        } else {
            pos.push_back(a);
        }
    }

#if defined(_WIN32)
    if (selftest) {
        AllocConsole();
        EnableUtf8Console();
        FILE* fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
    }
#endif

    std::string sys_dict = ResolveDict(pos.size() > 0 ? pos[0] : "data/dict_pinyin.dat");
    std::string user_dict = pos.size() > 1 ? pos[1] : "data/user_dict.dat";

    app::Config cfg;
    std::string cfg_path = ResolveConfig("config.ini");
    bool cfg_ok = cfg.LoadFromFile(cfg_path);

    ime::PinyinIme pinyin;
    bool dict_ok = pinyin.Open(sys_dict, user_dict);

    t9::T9Engine engine(&pinyin);

    // 自检模式
    if (selftest) {
        std::string py = selftest_py.empty() ? "ni" : selftest_py;
        std::string digits = t9::PinyinToDigits(py);
        std::printf("[自检] 词典: %s (%s)\n", sys_dict.c_str(), dict_ok ? "OK" : "FAILED");
        std::printf("[自检] 拼音=%s -> T9数字=%s\n", py.c_str(), digits.c_str());
        for (char d : digits) engine.PushKey(d);
        std::printf("[自检] 拼音展开: ");
        for (const auto& s : engine.PinyinCandidates(20)) std::printf("%s ", s.c_str());
        std::printf("\n");
        auto cands = engine.HanziCandidates(50);
        std::printf("[自检] 汉字候选(%zu): ", cands.size());
        for (const auto& c : cands) std::printf("%s ", c.c_str());
        std::printf("\n");
        return (dict_ok && !cands.empty()) ? 0 : 1;
    }

    app::ImeController ime(&engine, cfg);
    app::DesktopController desktop;

    // 清空旧日志（必须在 AllyHidController 创建之前，以保留探测日志）
    {
        std::string dir = ExeDir();
        if (!dir.empty()) {
            std::string path = dir + "t9ime.log";
            FILE* f = std::fopen(path.c_str(), "w");
            if (f) std::fclose(f);
        }
    }
    WriteLog("=== T9 IME 启动 ===\n");

    // 记录配置加载状态
    if (cfg_ok) {
        std::string msg = "[Init] 配置已加载: " + cfg_path
                        + " | 快捷键: " + app::DescribeCombo(cfg.toggle_combo)
                        + " | 不透明度: " + std::to_string(cfg.overlay_opacity)
                        + " | 启动开启: " + (cfg.start_enabled ? "true" : "false") + "\n";
        WriteLog(msg.c_str());
    } else {
        WriteLog("[Init] 配置文件未找到，使用默认值（快捷键: Back+Start）\n");
    }

    app::AllyHidController ally_hid;
    bool has_ally = ally_hid.IsSupported();

    // 初始模式设置
    // 启动时不切换硬件模式，保持设备当前状态。
    // ROG Ally 始终保持游戏手柄模式（XInput 可用），IME 关闭时用软件 DesktopController。
    // 非 ROG Ally：始终使用软件 DesktopController 模拟桌面操控。
    if (has_ally) {
        WriteLog("[Init] 检测到 ROG Ally，启动时不切换模式（保持当前状态）\n");
    } else {
        std::string err = std::string("[Init] 未检测到 ROG Ally: ")
                        + ally_hid.LastError() + "\n";
        WriteLog(err.c_str());
    }
    // IME 关闭时对所有设备激活软件桌面控制器（含 ROG Ally）
    desktop.SetActive(!ime.Enabled());
    gamepad::XInputPad pad(0);

    // 创建覆盖层（初始隐藏）
    ui::Overlay overlay;
    if (!overlay.Create(cfg.overlay_opacity)) {
        OutputDebugStringA("[错误] 无法创建 Overlay 窗口。\n");
        return 1;
    }
    overlay.SetVisible(false);

    if (!dict_ok) {
        std::string msg = "[警告] 拼音词典加载失败（" + sys_dict + "），候选将为空。\n";
        OutputDebugStringA(msg.c_str());
    }

    // 创建托盘图标
    HINSTANCE hinst = GetModuleHandleW(nullptr);
    HWND tray_hwnd = CreateTrayWindow(hinst);
    AddTrayIcon(tray_hwnd);
    g_auto_start = IsAutoStartEnabled();

    // 首次启动：在托盘上弹出气泡提示（只提示一次）
    if (IsFirstRun()) {
        MarkFirstRunDone();
        ShowTrayBalloon(L"T9 手柄输入法",
            L"要先在Armoury Crate SE 中设置打开控制中心的快捷键为CTRL+ALT+C才能实现功能");
    }

    // 设置托盘 WndProc 需要的全局指针
    g_ime = &ime;
    g_desktop = &desktop;
    g_ally_hid = &ally_hid;
    g_has_ally = has_ally;
    g_overlay = &overlay;
    g_engine = &engine;
    g_cfg = &cfg;

    // 注册退出热键（Ctrl+Alt+Q）和切换热键（Ctrl+Alt+E）
#if defined(_WIN32)
    RegisterHotKey(nullptr, kQuitHotkeyId, MOD_CONTROL | MOD_ALT, 'Q');
    RegisterHotKey(nullptr, kToggleHotkeyId, MOD_CONTROL | MOD_ALT, 'E');
#endif

    // 首次轮询
    pad.Poll();
    bool prev_connected = pad.Connected();
    g_pad_connected = prev_connected;

    bool running = true;
    g_running = &running;

    while (running) {
        // 处理 Win32 消息（托盘、退出热键、Overlay 重绘等）
#if defined(_WIN32)
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            if (msg.message == WM_HOTKEY && msg.wParam == kQuitHotkeyId) {
                running = false;
                break;
            }
            if (msg.message == WM_HOTKEY && msg.wParam == kToggleHotkeyId) {
                ToggleIme();
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
#endif

        pad.Poll();
        g_pad_connected = pad.Connected();

        bool was_enabled = ime.Enabled();
        bool changed = ime.Update(pad);

        // 手柄快捷键切换 IME：同步显示/隐藏界面 + 模式切换
        if (ime.Enabled() != was_enabled) {
            bool en = ime.Enabled();
            WriteLog(en ? "[Hotkey] IME 开启\n" : "[Hotkey] IME 关闭\n");
            overlay.SetVisible(en);
            if (en) {
                overlay.Refresh(MakeOverlayState(engine, ime, cfg, pad.Connected()));
            }
            // 硬件模式策略：
            //   ROG Ally 始终保持游戏手柄模式（XInput 可用），确保 Start 键始终可检测。
            //   IME 关闭时不切换到硬件鼠标模式，而是用软件 DesktopController 模拟桌面操控。
            //   （SetMouseMode 会导致 XInput 停止工作，Start 键无法检测，快捷键失效。）
            if (has_ally && en) {
                if (!ally_hid.SetGamepadMode()) {
                    // 游戏手柄模式切换失败时降级为软件模式
                    has_ally = false;
                    g_has_ally = false;
                    std::string err = std::string("[Hotkey] SetGamepadMode 失败: ")
                                    + ally_hid.LastError() + "\n";
                    WriteLog(err.c_str());
                }
            }
            // 软件桌面控制器：IME 关闭时对所有设备激活（含 ROG Ally）
            desktop.SetActive(!en);
            // 关闭界面（IME 关闭）后向前台发送两次 Ctrl+Alt+C
            if (!en) desktop.PressCtrlAltCTwice();
            // 刷新两个控制器的内部状态，防止过期状态导致按键功能不生效
            desktop.ResetState();
            ime.ResetState();
            WriteLog(en ? "[Hotkey] -> 游戏手柄输入模式\n"
                          : "[Hotkey] -> 桌面操控模式（软件）\n");
            UpdateTrayTooltip(en);
            changed = true;
        }

        // 每帧都调用桌面控制器（活跃时发送输入，非活跃时仅跟踪状态）
        if (pad.Connected()) {
            desktop.Update(pad);
        }

        // 手柄连接状态变化
        if (pad.Connected() != prev_connected) {
            prev_connected = pad.Connected();
            changed = true;
        }

        // 仅在界面可见且状态有变化时刷新
        if (changed && ime.Enabled()) {
            overlay.Refresh(MakeOverlayState(engine, ime, cfg, pad.Connected()));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

#if defined(_WIN32)
    UnregisterHotKey(nullptr, kQuitHotkeyId);
    UnregisterHotKey(nullptr, kToggleHotkeyId);
#endif
    RemoveTrayIcon();
    overlay.Destroy();
    OutputDebugStringA("T9 IME 已退出。\n");
    return 0;
}
