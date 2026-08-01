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
#include "app/log.h"
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
    app::Log::Write(msg);
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
    // 编辑快捷键高亮：仅 IME 开启（界面启用）时由 ImeController 触发
    s.edit_highlight = ime.EditHighlight();
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

// 在托盘图标上弹出一个气泡提示
void ShowTrayBalloon(const wchar_t* title, const wchar_t* text) {
    g_nid.uFlags = NIF_INFO;
    wcscpy_s(g_nid.szInfo, text);
    wcscpy_s(g_nid.szInfoTitle, title);
    g_nid.dwInfoFlags = NIIF_INFO;
    g_nid.uTimeout = 10000;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

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
    if (!Shell_NotifyIconW(NIM_MODIFY, &g_nid)) {
        WriteLog("[错误] 更新托盘图标失败\n");
    }
}

// 切换 IME 开关并同步界面与硬件模式
// 应用 IME 开关状态：同步界面、硬件/软件模式、快捷键与工具提示。
// 托盘与手柄快捷键两条触发路径共用，避免逻辑分叉导致状态漂移。
void ApplyImeEnabled(bool en) {
    WriteLog(en ? "[Toggle] IME 开启\n" : "[Toggle] IME 关闭\n");

    if (g_overlay) {
        g_overlay->SetVisible(en);
        if (en && g_engine && g_cfg) {
            g_overlay->Refresh(
                MakeOverlayState(*g_engine, *g_ime, *g_cfg, g_pad_connected));
        }
    }
    // 硬件模式策略：
    //   ROG Ally：IME 开启 -> 硬件游戏手柄模式（XInput 输入可用）；
    //             IME 关闭 -> 硬件桌面（鼠标）模式，由 Ally 固件接管
    //             鼠标/按键（桌面模式下 XInput 通常断开，手柄 Start 键
    //             快捷键失效，需用托盘图标或 Ctrl+Alt+E 重新开启 IME）。
    //   硬件切换失败（或非 ROG Ally）时降级为软件 DesktopController 模拟桌面操控。
    bool hw_ok = false;
    if (g_has_ally && g_ally_hid) {
        bool ok = en ? g_ally_hid->SetGamepadMode()
                     : g_ally_hid->SetMouseMode();
        if (!ok) {
            // 本次切换失败：降级为软件模式，但不永久禁用硬件路径（
            // 下次切换仍会重试，避免一次临时失败导致本会话硬件失效）
            std::string err = std::string("[Toggle] 硬件模式切换失败: ")
                            + g_ally_hid->LastError() + "\n";
            WriteLog(err.c_str());
        }
        hw_ok = ok;
    }
    // 软件桌面控制器：仅当 IME 关闭且硬件桌面模式未接管时激活
    // （非 ROG Ally 或切换失败 -> 软件模拟；硬件桌面模式成功 -> 由固件接管）
    bool need_sw_desktop = !en && !hw_ok;
    if (g_desktop) g_desktop->SetActive(need_sw_desktop);
    // 关闭界面（IME 关闭）后向前台发送两次 Ctrl+Alt+C
    if (!en && g_desktop) g_desktop->PressCtrlAltCTwice();
    // 刷新两个控制器的内部状态，防止过期状态导致按键功能不生效
    if (g_desktop) g_desktop->ResetState();
    if (g_ime) g_ime->ResetState();
    if (en) {
        WriteLog("[Toggle] -> 游戏手柄输入模式\n");
    } else {
        WriteLog(need_sw_desktop ? "[Toggle] -> 桌面操控模式（软件）\n"
                                 : "[Toggle] -> 桌面操控模式（硬件）\n");
    }
    UpdateTrayTooltip(en);
}

void ToggleIme() {
    if (!g_ime) return;
    g_ime->ToggleEnabled();
    ApplyImeEnabled(g_ime->Enabled());
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
    if (!Shell_NotifyIconW(NIM_ADD, &g_nid)) {
        WriteLog("[错误] 添加托盘图标失败\n");
    }
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

}  // namespace

// 设置进程 DPI awareness：避免高 DPI 屏上 Overlay 字体/坐标被系统虚拟化缩放而模糊。
void EnableDpiAwareness() {
#if defined(_WIN32)
    // 优先使用每显示器 DPI aware V2（Windows 10 1703+）；
    // 动态链接调用以避免在旧系统上加载失败。
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using Fn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto setDpi = reinterpret_cast<Fn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setDpi &&
            setDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
    }
    // 回退：兼容旧系统
    HMODULE shcore = GetModuleHandleW(L"shcore.dll");
    if (shcore) {
        using Fn = HRESULT(WINAPI*)(int);
        auto setAware = reinterpret_cast<Fn>(
            GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (setAware) setAware(1);  // PROCESS_PER_MONITOR_DPI_AWARE
    } else {
        SetProcessDPIAware();
    }
#endif
}

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

    // 自检模式或正常启动前都先设置 DPI awareness（影响 GetSystemMetrics 等返回值）
    EnableDpiAwareness();

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
        // 自检输出同时写文件（selftest_out.txt），
        // 因 AllocConsole 打开的独立控制台无法被管道/批处理捕获。
        FILE* of = std::fopen("selftest_out.txt", "w");

        auto emit = [&](const std::string& line) {
            std::printf("%s", line.c_str());
            std::fflush(stdout);
            if (of) {
                std::fputs(line.c_str(), of);
                std::fputc('\n', of);
            }
        };

        std::string py = selftest_py.empty() ? "ni" : selftest_py;
        std::string digits = t9::PinyinToDigits(py);
        emit("[自检] 词典: " + sys_dict + " (" + (dict_ok ? "OK" : "FAILED") + ")");
        emit("[自检] 拼音=" + py + " -> T9数字=" + digits);

        engine.Clear();
        for (char d : digits) engine.PushKey(d);
        std::string pyline = "[自检] 拼音展开: ";
        for (const auto& s : engine.PinyinCandidates(20)) pyline += s + " ";
        emit(pyline);
        auto cands = engine.HanziCandidates(50);
        std::string hzline = "[自检] 汉字候选(" + std::to_string(cands.size()) + "): ";
        for (const auto& c : cands) hzline += c + " ";
        emit(hzline);

        if (of) std::fclose(of);
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
    // 启动即开启 IME（start_enabled=true）时确保处于硬件游戏手柄模式；
    // 启动时 IME 关闭则保持设备当前状态，不自动切换（首次切换 IME 时生效）。
    // 非 ROG Ally：始终使用软件 DesktopController 模拟桌面操控。
    if (has_ally) {
        if (ime.Enabled()) {
            if (!ally_hid.SetGamepadMode()) {
                std::string err = std::string("[Init] SetGamepadMode 失败: ")
                                + ally_hid.LastError() + "\n";
                WriteLog(err.c_str());
            }
            WriteLog("[Init] 检测到 ROG Ally，启动即开启 IME -> 游戏手柄模式\n");
        } else {
            WriteLog("[Init] 检测到 ROG Ally，启动时 IME 关闭，保持当前模式\n");
        }
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
    if (!tray_hwnd) {
        WriteLog("[错误] 无法创建托盘窗口\n");
        return 1;
    }
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
    if (!RegisterHotKey(nullptr, kQuitHotkeyId, MOD_CONTROL | MOD_ALT, 'Q')) {
        WriteLog("[Init] 注册退出热键 Ctrl+Alt+Q 失败\n");
    }
    if (!RegisterHotKey(nullptr, kToggleHotkeyId, MOD_CONTROL | MOD_ALT, 'E')) {
        WriteLog("[Init] 注册切换热键 Ctrl+Alt+E 失败\n");
    }
#endif

    // 首次轮询
    pad.Poll();
    bool prev_connected = pad.Connected();
    g_pad_connected = prev_connected;

    bool running = true;
    g_running = &running;

    // 编辑快捷键高亮状态（触发时高亮、窗口期结束后熄灭，变化需重绘）
    char prev_edit_hl = 0;

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

        // 编辑快捷键高亮状态变化（触发/熄灭）也需刷新界面
        char edit_hl = ime.EditHighlight();
        if (edit_hl != prev_edit_hl) {
            prev_edit_hl = edit_hl;
            changed = true;
        }

        // 手柄快捷键切换 IME：同步显示/隐藏界面 + 模式切换
        if (ime.Enabled() != was_enabled) {
            ApplyImeEnabled(ime.Enabled());
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
