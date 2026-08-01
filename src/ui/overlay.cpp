#include "ui/overlay.h"

#if defined(_WIN32)

#include "t9/keypad.h"

#include <algorithm>

namespace ui {

namespace {

// ---- 窗口尺寸 ----
constexpr int kWinW = 440;
constexpr int kWinH = 560;

// ---- 颜色方案 ----
constexpr COLORREF kBg         = RGB(18, 20, 28);
constexpr COLORREF kPanelBg    = RGB(28, 31, 42);
constexpr COLORREF kCellBg     = RGB(38, 42, 58);
constexpr COLORREF kCellBgDim  = RGB(30, 32, 46);
constexpr COLORREF kCenterBg   = RGB(24, 26, 38);
constexpr COLORREF kActiveBg   = RGB(50, 120, 210);
constexpr COLORREF kActiveGlow = RGB(80, 160, 240);
constexpr COLORREF kBorder     = RGB(60, 65, 85);
constexpr COLORREF kSeparator  = RGB(48, 52, 68);
constexpr COLORREF kTextMain   = RGB(215, 220, 230);
constexpr COLORREF kTextBright = RGB(255, 255, 255);
constexpr COLORREF kTextDim    = RGB(140, 150, 170);
constexpr COLORREF kTextGreen  = RGB(80, 220, 120);
constexpr COLORREF kTextGray   = RGB(110, 115, 130);
constexpr COLORREF kTextAmber  = RGB(255, 200, 80);    // 标点琥珀色
constexpr COLORREF kSelectBg   = RGB(50, 120, 210);

// ---- 布局常量 ----
constexpr int kPadding     = 18;
constexpr int kCellW       = 100;
constexpr int kCellH       = 78;
constexpr int kGridW       = kCellW * 3;              // 300
constexpr int kGridH       = kCellH * 3;              // 234
constexpr int kGridOriginX = (kWinW - kGridW) / 2;    // 70
constexpr int kRowH        = 32;

// ---- 字体 ----
struct FontSet {
    HFONT title     = nullptr;  // 18px 状态行
    HFONT label     = nullptr;  // 20px 标签/正文
    HFONT digit     = nullptr;  // 32px 粗体 九宫格数字
    HFONT small     = nullptr;  // 17px 小字（字母、提示）
    HFONT candidate = nullptr;  // 22px 候选词
};

FontSet CreateFonts() {
    FontSet fs;
    const wchar_t* family = L"Microsoft YaHei";
    fs.title = CreateFontW(18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, family);
    fs.label = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, family);
    fs.digit = CreateFontW(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, family);
    fs.small = CreateFontW(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, family);
    fs.candidate = CreateFontW(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, family);
    return fs;
}

void DestroyFonts(FontSet& fs) {
    if (fs.title)     { DeleteObject(fs.title);     fs.title = nullptr; }
    if (fs.label)     { DeleteObject(fs.label);     fs.label = nullptr; }
    if (fs.digit)     { DeleteObject(fs.digit);     fs.digit = nullptr; }
    if (fs.small)     { DeleteObject(fs.small);     fs.small = nullptr; }
    if (fs.candidate) { DeleteObject(fs.candidate); fs.candidate = nullptr; }
}

// UTF-8 -> UTF-16
std::wstring W(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                  static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                        static_cast<int>(s.size()), ws.data(), len);
    return ws;
}

// 在 (x, y) 处用指定字体和颜色绘制文字（透明背景）。
void TextOutUtf8(HDC hdc, HFONT font, int x, int y,
                 const std::string& utf8, COLORREF color,
                 UINT align = TA_LEFT) {
    HFONT old = (HFONT)SelectObject(hdc, font);
    SetTextColor(hdc, color);
    SetTextAlign(hdc, align);
    SetBkMode(hdc, TRANSPARENT);
    std::wstring ws = W(utf8);
    TextOutW(hdc, x, y, ws.c_str(), static_cast<int>(ws.size()));
    SelectObject(hdc, old);
}

// 测量文字宽度
int TextWidth(HDC hdc, HFONT font, const std::string& utf8) {
    std::wstring ws = W(utf8);
    HFONT old = (HFONT)SelectObject(hdc, font);
    SIZE sz = {};
    GetTextExtentPoint32W(hdc, ws.c_str(),
                          static_cast<int>(ws.size()), &sz);
    SelectObject(hdc, old);
    return sz.cx;
}

// 绘制填充矩形
void FillRectColor(HDC hdc, int x, int y, int w, int h, COLORREF c) {
    RECT rc = {x, y, x + w, y + h};
    HBRUSH br = CreateSolidBrush(c);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
}

// 绘制矩形边框
void FrameRectColor(HDC hdc, int x, int y, int w, int h, COLORREF c) {
    RECT rc = {x, y, x + w, y + h};
    HBRUSH br = CreateSolidBrush(c);
    FrameRect(hdc, &rc, br);
    DeleteObject(br);
}

// 绘制粗边框（用于高亮格子）
void DrawThickBorder(HDC hdc, int x, int y, int w, int h,
                     COLORREF c, int thickness) {
    HPEN pen = CreatePen(PS_SOLID, thickness, c);
    HGDIOBJ old = SelectObject(hdc, pen);
    HBRUSH br = (HBRUSH)GetStockObject(NULL_BRUSH);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, old);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
}

// 绘制水平分隔线
void DrawSeparator(HDC hdc, int y) {
    HPEN pen = CreatePen(PS_SOLID, 1, kSeparator);
    HGDIOBJ old = SelectObject(hdc, pen);
    MoveToEx(hdc, kPadding, y, nullptr);
    LineTo(hdc, kWinW - kPadding, y);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

}  // namespace

// =====================================================================
// Overlay 实现
// =====================================================================

Overlay::Overlay() = default;
Overlay::~Overlay() { Destroy(); }

bool Overlay::Create(float opacity) {
    (void)opacity;  // 不再使用透明度

    HINSTANCE hinst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &Overlay::WndProc;
    wc.hInstance     = hinst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(kPanelBg);
    // 从 exe 资源加载自定义图标（ID 101），失败时无图标（Overlay 为弹出窗口，图标非关键）
    wc.hIcon         = LoadIconW(hinst, MAKEINTRESOURCEW(101));
    wc.hIconSm       = wc.hIcon;
    wc.lpszClassName = L"T9GamepadOverlay";

    RegisterClassExW(&wc);

    // 定位到屏幕右下角，留出任务栏空间
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int margin_right  = 20;
    int margin_bottom = 64;
    int x = sw - width_  - margin_right;
    int y = sh - height_ - margin_bottom;

    hwnd_ = CreateWindowExW(
        WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"T9 Gamepad IME",
        WS_POPUP,
        x, y, width_, height_,
        nullptr, nullptr, hinst, this);

    if (!hwnd_) return false;

    // 初始隐藏，由调用方通过 SetVisible() 控制显示时机
    ShowWindow(hwnd_, SW_HIDE);
    UpdateWindow(hwnd_);
    return true;
}

void Overlay::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void Overlay::Refresh(const OverlayState& s) {
    state_ = s;
    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);
    }
}

void Overlay::SetVisible(bool visible) {
    if (hwnd_) {
        ShowWindow(hwnd_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
}

LRESULT CALLBACK Overlay::WndProc(HWND hwnd, UINT msg,
                                   WPARAM wp, LPARAM lp) {
    Overlay* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = static_cast<Overlay*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Overlay*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_SETCURSOR:
            // 禁止窗口改变光标，保持前台应用的光标状态不变
            return TRUE;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (self) self->OnPaint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

// ---- 绘制总入口（双缓冲） ----

void Overlay::OnPaint(HDC hdc) {
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, width_, height_);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);

    // 背景
    FillRectColor(mem, 0, 0, width_, height_, kPanelBg);

    FontSet fonts = CreateFonts();

    // 状态行（顶部）
    DrawStatus(mem);

    // 信息行（数字/拼音）— 紧接状态行
    int info_y = kPadding + 32;
    DrawSeparator(mem, info_y);
    DrawInfoLines(mem, info_y + 10);

    // 候选区（无输入时显示标点候选）
    int cand_y = info_y + 10 + kRowH * 2 + 10;
    DrawSeparator(mem, cand_y);
    DrawCandidates(mem, cand_y + 10);

    // 已上屏（紧接候选区，无额外预留空间）
    int commit_y = cand_y + 10 + kRowH * 3;
    if (!state_.last_committed.empty()) {
        DrawCommitted(mem, commit_y);
    }

    // 九宫格（紧跟内容区，无空隙）
    int grid_y = commit_y + kRowH;
    DrawGrid(mem, kGridOriginX, grid_y);

    DestroyFonts(fonts);

    BitBlt(hdc, 0, 0, width_, height_, mem, 0, 0, SRCCOPY);

    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

// ---- 状态行 ----

void Overlay::DrawStatus(HDC hdc) {
    FontSet fonts = CreateFonts();

    // 状态文字
    std::string status = state_.enabled ? "● 开启" : "○ 关闭";
    COLORREF sc = state_.enabled ? kTextGreen : kTextGray;
    TextOutUtf8(hdc, fonts.title, kPadding, kPadding, status, sc);

    // 手柄连接状态
    std::string pad = state_.pad_connected ? "手柄已连接" : "手柄未连接";
    COLORREF pc = state_.pad_connected ? kTextGreen : RGB(220, 80, 80);
    TextOutUtf8(hdc, fonts.small, 90, kPadding + 3, pad, pc);

    // 快捷键
    std::string hk = "键: " + state_.hotkey_desc;
    TextOutUtf8(hdc, fonts.small, 200, kPadding + 3, hk, kTextDim);

    // 退出提示
    TextOutUtf8(hdc, fonts.small, kWinW - kPadding, kPadding + 3,
                "Ctrl+Alt+Q 退出", kTextDim, TA_RIGHT);

    DestroyFonts(fonts);
}

// ---- 九宫格 ----

void Overlay::DrawGrid(HDC hdc, int ox, int oy) {
    FontSet fonts = CreateFonts();
    const t9::KeyCell* cells = t9::GridCells();
    char active = state_.enabled ? t9::DigitForDirection(state_.pointing) : 0;

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            const t9::KeyCell& c = cells[row * 3 + col];
            int cx = ox + col * kCellW;
            int cy = oy + row * kCellH;
            bool is_center = (c.digit == '\0');
            bool is_active = (c.digit != '\0' && c.digit == active);

            // 格子背景
            COLORREF bg;
            if (is_active)      bg = kActiveBg;
            else if (is_center) bg = kCenterBg;
            else if (state_.enabled) bg = kCellBg;
            else                bg = kCellBgDim;
            FillRectColor(hdc, cx + 2, cy + 2, kCellW - 4, kCellH - 4, bg);

            // 边框：高亮格子用粗边框 + 发光色
            if (is_active) {
                DrawThickBorder(hdc, cx + 1, cy + 1, kCellW - 2, kCellH - 2,
                                kActiveGlow, 3);
            } else {
                FrameRectColor(hdc, cx, cy, kCellW, kCellH, kBorder);
            }

            if (is_center) {
                // 中心点
                TextOutUtf8(hdc, fonts.digit,
                            cx + kCellW / 2, cy + kCellH / 2 - 18,
                            "·", kTextDim, TA_CENTER);
            } else {
                // 数字（大）
                COLORREF dc = is_active ? kTextBright :
                              (state_.enabled ? kTextMain : kTextGray);
                std::string d(1, c.digit);
                TextOutUtf8(hdc, fonts.digit,
                            cx + kCellW / 2, cy + 6,
                            d, dc, TA_CENTER);
                // 字母（小）
                COLORREF lc = is_active ? kTextBright :
                              (state_.enabled ? kTextDim : kTextGray);
                TextOutUtf8(hdc, fonts.small,
                            cx + kCellW / 2, cy + kCellH - 24,
                            c.letters, lc, TA_CENTER);
            }
        }
    }

    DestroyFonts(fonts);
}

// ---- 数字 / 拼音行 ----

void Overlay::DrawInfoLines(HDC hdc, int y) {
    FontSet fonts = CreateFonts();
    COLORREF lc = state_.enabled ? kTextDim : kTextGray;
    COLORREF vc = state_.enabled ? kTextMain : kTextGray;

    // [数字]
    TextOutUtf8(hdc, fonts.label, kPadding, y, "[数字]", lc);
    TextOutUtf8(hdc, fonts.candidate, kPadding + 70, y - 2,
                state_.digits.empty() ? "-" : state_.digits, vc);

    // [拼音] — 用小字体显示，带音节分隔符（如 ni'hao'ma）
    // 显示前 10 个拼音展开（已按音节数排序，少音节优先）
    int y2 = y + kRowH;
    TextOutUtf8(hdc, fonts.label, kPadding, y2, "[拼音]", lc);
    std::string py;
    size_t py_max = std::min<size_t>(state_.pinyin.size(), 10);
    for (size_t i = 0; i < py_max; ++i) {
        if (i) py += " ";
        py += state_.pinyin[i];
    }
    if (state_.pinyin.size() > py_max) py += " ...";
    if (py.empty()) py = "-";
    TextOutUtf8(hdc, fonts.small, kPadding + 70, y2 + 2, py, vc);

    DestroyFonts(fonts);
}

// ---- 候选区 ----

void Overlay::DrawCandidates(HDC hdc, int y) {
    FontSet fonts = CreateFonts();
    COLORREF lc = state_.enabled ? kTextDim : kTextGray;

    // 标题行：字母模式显示"[字母 ABC2abc]"，无输入时显示"[标点]"，否则显示"[候选]"
    int total = static_cast<int>(state_.candidates.size());
    std::string title;
    if (state_.letter_mode) {
        title = "[字母 " + state_.letter_text + "]";
    } else {
        bool is_punct = state_.digits.empty() && total > 0;
        title = is_punct ? "[标点]" : "[候选]";
    }
    if (total > 0) {
        title += " (" + std::to_string(state_.selected + 1) + "/" +
                 std::to_string(total) + ")";
    }
    TextOutUtf8(hdc, fonts.label, kPadding, y, title,
                state_.letter_mode ? kTextBright : lc);

    const auto& cands = state_.candidates;
    int start = state_.page_start;
    int psize = state_.page_size > 0 ? state_.page_size : 5;
    int end = std::min(start + psize, total);

    int list_y = y + kRowH + 2;
    int col_x = kPadding;
    int row = 0;

    for (int i = start; i < end; ++i) {
        bool sel = (i == state_.selected);
        std::string entry = std::to_string(i - start + 1) + "." + cands[i];

        if (sel) {
            // 选中项背景
            int tw = TextWidth(hdc, fonts.candidate, entry);
            int pad = 6;
            FillRectColor(hdc, col_x - pad, list_y - 3,
                          tw + pad * 2, kRowH + 2, kSelectBg);
            TextOutUtf8(hdc, fonts.candidate, col_x, list_y - 2,
                        entry, kTextBright);
            col_x += tw + pad * 2 + 14;
        } else {
            TextOutUtf8(hdc, fonts.candidate, col_x, list_y - 2, entry,
                        state_.enabled ? kTextMain : kTextGray);
            int tw = TextWidth(hdc, fonts.candidate, entry);
            col_x += tw + 14;
        }

        // 换行检查
        if (col_x > kWinW - kPadding - 80 && i + 1 < end) {
            col_x = kPadding;
            list_y += kRowH;
            ++row;
            // 最多显示 5 行候选
            if (row >= 5) break;
        }
    }

    DestroyFonts(fonts);
}

// ---- 已上屏 ----

void Overlay::DrawCommitted(HDC hdc, int y) {
    FontSet fonts = CreateFonts();
    TextOutUtf8(hdc, fonts.label, kPadding, y, "[已上屏]", kTextGreen);
    TextOutUtf8(hdc, fonts.candidate, kPadding + 80, y - 2,
                state_.last_committed, kTextBright);
    DestroyFonts(fonts);
}

// ---- 标点栏 ----

void Overlay::DrawPunctuation(HDC hdc, int y) {
    FontSet fonts = CreateFonts();

    // 常用中文标点（无输入时作为候选可直接选）
    static const char* kPuncts[] = {
        "，", "。", "、", "！", "？", "：", "；", "…",
        "—", "～", "（", "）", "「", "」", "《", "》"
    };
    constexpr int kPunctCount = sizeof(kPuncts) / sizeof(kPuncts[0]);

    // 标签
    TextOutUtf8(hdc, fonts.label, kPadding, y, "[标点]", kTextAmber);

    // 标点列表：两行排列
    int x = kPadding + 70;
    int row_y = y - 2;
    COLORREF pc = state_.enabled ? kTextMain : kTextGray;

    for (int i = 0; i < kPunctCount; ++i) {
        std::string entry = std::to_string(i + 1) + "." + kPuncts[i];
        TextOutUtf8(hdc, fonts.candidate, x, row_y, entry, pc);
        int tw = TextWidth(hdc, fonts.candidate, entry);
        x += tw + 12;

        // 第 8 个后换行
        if (i == 7) {
            x = kPadding + 70;
            row_y += kRowH;
        }
    }

    DestroyFonts(fonts);
}

}  // namespace ui

#endif  // _WIN32
