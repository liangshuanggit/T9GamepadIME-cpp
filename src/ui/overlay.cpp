#include "ui/overlay.h"

#if defined(_WIN32)

#include "t9/keypad.h"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {

// ---- 窗口尺寸 ----
constexpr int kWinW = 500;
constexpr int kWinH = 640;  // 适配 720p 屏幕（工作区约 672px），大字体布局

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
constexpr COLORREF kInputBg    = RGB(44, 52, 74);      // 当前输入数字串底色

// ---- 布局常量 ----
constexpr int kPadding = 20;
constexpr int kRowH    = 34;   // 候选行高（大字体）

// ---- 纵向布局（自上而下分区）----
constexpr int kStatusY   = 12;    // 状态行（● 开启 / 手柄 / 退出）
constexpr int kSep1      = 46;    // 分隔线 1
constexpr int kDigitsY   = 56;    // [数字] 行
constexpr int kPinyinY   = 108;   // [拼音] 行
constexpr int kSep2      = 142;   // 分隔线 2
constexpr int kCandTitle = 152;   // 候选标题
constexpr int kCommitted = 300;   // 已上屏（条件显示）
constexpr int kSep3      = 330;   // 分隔线 3（键区上方）
constexpr int kSep4      = 612;   // 分隔线 4（键区下方）
constexpr int kEditY     = 618;   // 编辑快捷键栏（单行文字）

// ---- 扇形键区（中心静止位 + 8 方向扇叶）----
constexpr int   kFanCx     = kWinW / 2;          // 键区中心 X（水平居中）
constexpr int   kFanCy     = 470;                // 键区中心 Y（覆盖层内相对位置）
constexpr int   kFanRInner = 32;                 // 内环半径（中心静止位半径）
constexpr int   kFanROuter = 135;                // 外环半径（扇叶外缘）
constexpr int   kFanGap    = 3;                  // 扇叶之间/与内环的间隙(px)

// ---- 字体 ----
struct FontDesc {
    int size;
    int weight;
};

// 按名称创建单个字体
HFONT CreateOneFont(const wchar_t* family, int size, int weight) {
    return CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, family);
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

// 绘制水平分隔线
void DrawSeparator(HDC hdc, int y) {
    HPEN pen = CreatePen(PS_SOLID, 1, kSeparator);
    HGDIOBJ old = SelectObject(hdc, pen);
    MoveToEx(hdc, kPadding, y, nullptr);
    LineTo(hdc, kWinW - kPadding, y);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

// 极坐标 -> 屏幕坐标（角度弧度制，X 右为正，Y 上为正）
POINT PolarToPoint(int cx, int cy, double radius, double angle_rad) {
    POINT p{};
    p.x = cx + static_cast<int>(std::lround(radius * std::cos(angle_rad)));
    p.y = cy - static_cast<int>(std::lround(radius * std::sin(angle_rad)));
    return p;
}

// 构造扇形（圆环扇叶）的顶点序列：
// 内缘圆弧（inner_radius -> outer_radius，a0 -> a1）再逆序描回。
// gap：收缩半径产生的间隙（沿径向向中心收缩）。
std::vector<POINT> BuildFanPoints(int cx, int cy, double r_in, double r_out,
                                  double a0, double a1, double gap) {
    const int kArcPts = 10;  // 每条弧的采样点数
    std::vector<POINT> pts;
    pts.reserve(kArcPts * 2);
    for (int i = 0; i < kArcPts; ++i) {
        double a = a0 + (a1 - a0) * i / (kArcPts - 1);
        pts.push_back(PolarToPoint(cx, cy, r_out - gap, a));
    }
    for (int i = kArcPts - 1; i >= 0; --i) {
        double a = a0 + (a1 - a0) * i / (kArcPts - 1);
        pts.push_back(PolarToPoint(cx, cy, r_in + gap, a));
    }
    return pts;
}

// 绘制实心扇形键（圆环扇叶），用 Polygon 填充并描边
void FillFanColor(HDC hdc, int cx, int cy, double r_in, double r_out,
                  double a0, double a1, COLORREF c, COLORREF border,
                  int border_width = 1) {
    std::vector<POINT> pts =
        BuildFanPoints(cx, cy, r_in, r_out, a0, a1, kFanGap);
    HBRUSH br = CreateSolidBrush(c);
    HPEN pen = CreatePen(PS_SOLID, border_width, border);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    Polygon(hdc, pts.data(), static_cast<int>(pts.size()));
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
    DeleteObject(br);
}

// 绘制实心圆（中心静止位）
void FillCircleColor(HDC hdc, int cx, int cy, int r, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c);
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    Ellipse(hdc, cx - r, cy - r, cx + r + 1, cy + r + 1);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
    DeleteObject(br);
}

// 绘制圆环描边（空心圆）
void FrameCircleColor(HDC hdc, int cx, int cy, int r, COLORREF c) {
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HBRUSH br = (HBRUSH)GetStockObject(NULL_BRUSH);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    Ellipse(hdc, cx - r, cy - r, cx + r + 1, cy + r + 1);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

}  // namespace

// =====================================================================
// Overlay 实现
// =====================================================================

Overlay::Overlay() = default;
Overlay::~Overlay() { Destroy(); }

void Overlay::EnsureFonts() {
    if (fonts_.title) return;  // 已创建
    const wchar_t* family = L"Microsoft YaHei";
    fonts_.title     = CreateOneFont(family, 24, FW_SEMIBOLD);
    fonts_.label     = CreateOneFont(family, 20, FW_NORMAL);
    fonts_.digit     = CreateOneFont(family, 36, FW_BOLD);
    fonts_.small     = CreateOneFont(family, 17, FW_NORMAL);
    fonts_.candidate = CreateOneFont(family, 26, FW_NORMAL);
    fonts_.big       = CreateOneFont(family, 40, FW_BOLD);
}

void Overlay::DestroyFonts() {
    if (fonts_.title)     { DeleteObject(fonts_.title);     fonts_.title = nullptr; }
    if (fonts_.label)     { DeleteObject(fonts_.label);     fonts_.label = nullptr; }
    if (fonts_.digit)     { DeleteObject(fonts_.digit);     fonts_.digit = nullptr; }
    if (fonts_.small)     { DeleteObject(fonts_.small);     fonts_.small = nullptr; }
    if (fonts_.candidate) { DeleteObject(fonts_.candidate); fonts_.candidate = nullptr; }
    if (fonts_.big)       { DeleteObject(fonts_.big);       fonts_.big = nullptr; }
}

bool Overlay::Create(float opacity) {
    opacity_ = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);

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

    // 定位到前台窗口所在显示器的右下角（多显示器时跟随当前使用中的屏幕），
    // 无前台窗口时回退到主显示器工作区。
    RECT wa = {};
    HMONITOR mon = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (mon && GetMonitorInfoW(mon, &mi)) {
        wa = mi.rcWork;
    } else {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    }
    int x = wa.right  - width_  - 20;
    int y = wa.bottom - height_ - 20;
    if (y < 0) y = 0;  // 屏幕太矮时贴顶，避免窗口部分移出屏幕

    DWORD ex_style = WS_EX_TRANSPARENT | WS_EX_TOPMOST |
                     WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    // 半透明时启用分层窗口，否则保持纯 OPAQUE（性能更好）
    if (opacity_ < 1.0f) ex_style |= WS_EX_LAYERED;

    hwnd_ = CreateWindowExW(
        ex_style,
        wc.lpszClassName,
        L"T9 Gamepad IME",
        WS_POPUP,
        x, y, width_, height_,
        nullptr, nullptr, hinst, this);

    if (!hwnd_) return false;

    // 应用整体透明度
    if (opacity_ < 1.0f) {
        BYTE alpha = static_cast<BYTE>(opacity_ * 255.0f);
        SetLayeredWindowAttributes(hwnd_, 0, alpha, LWA_ALPHA);
    }

    // 初始隐藏，由调用方通过 SetVisible() 控制显示时机
    ShowWindow(hwnd_, SW_HIDE);
    UpdateWindow(hwnd_);
    return true;
}

void Overlay::Destroy() {
    DestroyFonts();
    if (mem_dc_) {
        if (mem_old_) SelectObject(mem_dc_, mem_old_);
        mem_old_ = nullptr;
        DeleteDC(mem_dc_);
        mem_dc_ = nullptr;
    }
    if (mem_bmp_) { DeleteObject(mem_bmp_); mem_bmp_ = nullptr; }
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

// ---- 绘制总入口（双缓冲，缓冲持久复用） ----

void Overlay::EnsureBackBuffer(HDC hdc) {
    if (mem_dc_ && mem_bmp_ && mem_old_) return;  // 已就绪
    if (!mem_dc_) mem_dc_ = CreateCompatibleDC(hdc);
    if (!mem_bmp_) mem_bmp_ = CreateCompatibleBitmap(hdc, width_, height_);
    if (mem_dc_ && mem_bmp_ && !mem_old_) {
        mem_old_ = SelectObject(mem_dc_, mem_bmp_);
    }
    // 位图创建失败：释放已创建的 DC，避免泄漏
    if (mem_dc_ && !mem_bmp_) {
        DeleteDC(mem_dc_);
        mem_dc_ = nullptr;
    }
}

void Overlay::OnPaint(HDC hdc) {
    EnsureBackBuffer(hdc);
    if (!mem_dc_ || !mem_bmp_) return;

    EnsureFonts();

    // 背景
    FillRectColor(mem_dc_, 0, 0, width_, height_, kPanelBg);

    // 状态行（顶部）
    DrawStatus(mem_dc_);

    // 信息行（数字/拼音）
    DrawSeparator(mem_dc_, kSep1);
    DrawInfoLines(mem_dc_, kDigitsY);

    // 候选区（无输入时显示标点候选）
    DrawSeparator(mem_dc_, kSep2);
    DrawCandidates(mem_dc_, kCandTitle);

    // 已上屏（紧接候选区）
    if (!state_.last_committed.empty()) {
        DrawCommitted(mem_dc_, kCommitted);
    }

    // 扇形键区（中心静止位 + 8 方向扇叶）
    DrawSeparator(mem_dc_, kSep3);
    DrawGrid(mem_dc_, kFanCx, kFanCy);

    // 编辑快捷键栏（底部，扇形键区下方；触发时对应项高亮）
    DrawSeparator(mem_dc_, kSep4);
    DrawEditShortcuts(mem_dc_, kEditY);

    BitBlt(hdc, 0, 0, width_, height_, mem_dc_, 0, 0, SRCCOPY);
}

// ---- 状态行 ----

void Overlay::DrawStatus(HDC hdc) {
    // 开关状态（大号加粗，直观）
    std::string status = state_.enabled ? "● 开启" : "○ 关闭";
    COLORREF sc = state_.enabled ? kTextGreen : kTextGray;
    TextOutUtf8(hdc, fonts_.title, kPadding, kStatusY, status, sc);

    // 手柄连接状态
    std::string pad = state_.pad_connected ? "手柄已连接" : "手柄未连接";
    COLORREF pc = state_.pad_connected ? kTextGreen : RGB(220, 80, 80);
    TextOutUtf8(hdc, fonts_.small, 150, kStatusY + 5, pad, pc);

    // 退出提示
    TextOutUtf8(hdc, fonts_.small, kWinW - kPadding, kStatusY + 5,
                "Ctrl+Alt+Q 退出", kTextDim, TA_RIGHT);
}

// ---- 扇形键区 ----

// 在极坐标位置绘制并居中（横向纵向都居中）的文字
static void DrawFanText(HDC hdc, HFONT font, int cx, int cy,
                        double r, double a, const std::string& text,
                        COLORREF color) {
    std::wstring ws = W(text);
    HFONT old = (HFONT)SelectObject(hdc, font);
    SIZE sz = {};
    GetTextExtentPoint32W(hdc, ws.c_str(), static_cast<int>(ws.size()), &sz);
    SelectObject(hdc, old);
    POINT p = PolarToPoint(cx, cy, r, a);
    TextOutUtf8(hdc, font, p.x - sz.cx / 2, p.y - sz.cy / 2, text, color,
                TA_LEFT);
}

void Overlay::DrawGrid(HDC hdc, int ox, int oy) {
    const t9::KeyCell* cells = t9::GridCells();
    char active = state_.enabled ? t9::DigitForDirection(state_.pointing) : 0;

    static const double kPi         = 3.14159265358979323846;
    static const double kHalfSector = kPi / 8.0;        // 45°/2 = 22.5°
    static const double kAngGap     = 0.06;             // 扇叶间角间隙（弧度，约 3.4°）
    static const double kDrawHalf   = kHalfSector - kAngGap / 2.0;

    // 各方向中线角（行优先，与 GridCells() 顺序一致），0 rad = 右，逆时针增大。
    static const double kSectorMid[9] = {
        3 * kPi / 4,   // 0 左上(5)
        kPi / 2,       // 1 上(2)
        kPi / 4,       // 2 右上(3)
        kPi,           // 3 左(4)
        0,             // 4 中心（未用）
        0,             // 5 右(6)
        5 * kPi / 4,   // 6 左下(7)
        3 * kPi / 2,   // 7 下(8)
        7 * kPi / 4,   // 8 右下(9)
    };

    // 径向文字位置：数字偏内，字母偏外
    const double kDigitR   = kFanRInner + (kFanROuter - kFanRInner) * 0.40;
    const double kLettersR = kFanRInner + (kFanROuter - kFanRInner) * 0.72;

    for (int i = 0; i < 9; ++i) {
        const t9::KeyCell& c = cells[i];
        if (c.digit == '\0') continue;  // 中心静止位单独绘制

        bool is_active = (c.digit == active);
        double aMid = kSectorMid[i];
        double a0 = aMid - kDrawHalf;
        double a1 = aMid + kDrawHalf;

        // 扇叶背景与描边
        COLORREF bg = is_active ? kActiveBg :
                      (state_.enabled ? kCellBg : kCellBgDim);
        COLORREF border = is_active ? kActiveGlow : kBorder;
        int bw = is_active ? 3 : 1;
        FillFanColor(hdc, ox, oy, kFanRInner, kFanROuter,
                     a0, a1, bg, border, bw);

        // 数字（大）
        COLORREF dc = is_active ? kTextBright :
                      (state_.enabled ? kTextMain : kTextGray);
        std::string d(1, c.digit);
        DrawFanText(hdc, fonts_.digit, ox, oy, kDigitR, aMid, d, dc);

        // 字母（小）
        COLORREF lc = is_active ? kTextBright :
                      (state_.enabled ? kTextDim : kTextGray);
        DrawFanText(hdc, fonts_.small, ox, oy, kLettersR, aMid, c.letters, lc);
    }

    // 中心静止位（右摇杆回中区）
    FillCircleColor(hdc, ox, oy, kFanRInner,
                    state_.enabled ? kCenterBg : kCellBgDim);
    FrameCircleColor(hdc, ox, oy, kFanRInner, kBorder);
    DrawFanText(hdc, fonts_.small, ox, oy, 0, 0, "·", kTextDim);
}

// ---- 编辑快捷键栏 ----

void Overlay::DrawEditShortcuts(HDC hdc, int y) {
    // 四项：与 ImeController 的触发逻辑一一对应（编辑快捷键仅在 IME 开启时生效）
    struct Item {
        char key;         // 高亮标识（与控制器 EditHighlight() 返回值一致）
        const char* combo;  // 组合键描述
        const char* name;   // 功能名
    };
    static const Item kItems[] = {
        {'A', "LB+A", "全选"},
        {'X', "LB+X", "剪切"},
        {'Y', "LB+Y", "复制"},
        {'B', "LB+B", "粘贴"},
    };

    constexpr int kItemW   = 112;
    constexpr int kItemGap = 10;

    for (int i = 0; i < 4; ++i) {
        int x = kPadding + i * (kItemW + kItemGap);
        bool hl = state_.edit_highlight == kItems[i].key;
        std::string text = std::string(kItems[i].combo) + " " + kItems[i].name;
        int tw = TextWidth(hdc, fonts_.small, text);

        if (hl) {
            // 触发高亮：蓝底亮字
            FillRectColor(hdc, x - 4, y - 3, kItemW + 8, 22, kActiveBg);
            TextOutUtf8(hdc, fonts_.small, x + (kItemW - tw) / 2, y,
                        text, kTextBright);
        } else {
            TextOutUtf8(hdc, fonts_.small, x + (kItemW - tw) / 2, y,
                        text, kTextDim);
        }
    }
}

// ---- 数字 / 拼音行 ----

void Overlay::DrawInfoLines(HDC hdc, int y) {
    COLORREF lc = state_.enabled ? kTextDim : kTextGray;

    // [数字] 标签 + 大号输入数字串（带底色条，直观醒目）
    TextOutUtf8(hdc, fonts_.label, kPadding, y, "[数字]", lc);
    std::string digits = state_.digits.empty() ? "-" : state_.digits;
    constexpr int kBarW = 230;
    constexpr int kBarH = 50;
    FillRectColor(hdc, 110, y - 6, kBarW, kBarH, kInputBg);
    HPEN pen = CreatePen(PS_SOLID, 1, kBorder);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HBRUSH br = (HBRUSH)GetStockObject(NULL_BRUSH);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    Rectangle(hdc, 110, y - 6, 110 + kBarW, y - 6 + kBarH);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    // 数字串过长时截断，避免超出底色条
    if (TextWidth(hdc, fonts_.big, digits) > kBarW - 20) {
        while (digits.size() > 1 &&
               TextWidth(hdc, fonts_.big, digits) > kBarW - 20) {
            digits.pop_back();
        }
    }
    TextOutUtf8(hdc, fonts_.big, 122, y - 4, digits,
                state_.enabled ? kTextBright : kTextGray);

    // [拼音] — 显示前 10 个拼音展开（已按音节数排序，少音节优先）
    int y2 = y + 52;
    TextOutUtf8(hdc, fonts_.label, kPadding, y2, "[拼音]", lc);
    std::string py;
    size_t py_max = std::min<size_t>(state_.pinyin.size(), 10);
    for (size_t i = 0; i < py_max; ++i) {
        if (i) py += " ";
        py += state_.pinyin[i];
    }
    if (state_.pinyin.size() > py_max) py += " ...";
    if (py.empty()) py = "-";
    TextOutUtf8(hdc, fonts_.label, 110, y2 + 2, py,
                state_.enabled ? kTextMain : kTextGray);
}

// ---- 候选区 ----

void Overlay::DrawCandidates(HDC hdc, int y) {
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
    TextOutUtf8(hdc, fonts_.label, kPadding, y, title,
                state_.letter_mode ? kTextBright : lc);

    // 开关快捷键提示（候选标题行右侧）
    std::string hk = "开关键: " + state_.hotkey_desc;
    TextOutUtf8(hdc, fonts_.small, kWinW - kPadding, y + 3, hk, kTextDim,
                TA_RIGHT);

    // 候选网格：每行 4 项，最多 3 行（大字号，选中项整格高亮）
    const auto& cands = state_.candidates;
    int start = state_.page_start;
    int psize = state_.page_size > 0 ? state_.page_size : 5;
    int end = std::min(start + psize, total);

    constexpr int kCols   = 4;
    constexpr int kCellW  = 109;
    constexpr int kGap    = 8;
    constexpr int kRowGap = 2;
    constexpr int kMaxRow = 3;
    int list_y = y + 36;
    int col = 0;
    int row = 0;

    for (int i = start; i < end && row < kMaxRow; ++i) {
        int x = kPadding + col * (kCellW + kGap);
        bool sel = (i == state_.selected);
        std::string entry = std::to_string(i - start + 1) + "." + cands[i];

        if (sel) {
            // 选中项：整格高亮背景 + 亮字
            FillRectColor(hdc, x, list_y - 4, kCellW, kRowH, kSelectBg);
            TextOutUtf8(hdc, fonts_.candidate, x + 8, list_y - 4,
                        entry, kTextBright);
        } else {
            TextOutUtf8(hdc, fonts_.candidate, x + 8, list_y - 4, entry,
                        state_.enabled ? kTextMain : kTextGray);
        }

        if (++col >= kCols) {
            col = 0;
            list_y += kRowH + kRowGap;
            ++row;
        }
    }
}

// ---- 已上屏 ----

void Overlay::DrawCommitted(HDC hdc, int y) {
    TextOutUtf8(hdc, fonts_.label, kPadding, y, "[已上屏]", kTextGreen);
    TextOutUtf8(hdc, fonts_.candidate, 130, y - 2,
                state_.last_committed, kTextBright);
}

// ---- 标点栏 ----

void Overlay::DrawPunctuation(HDC hdc, int y) {
    // 常用中文标点（无输入时作为候选可直接选）
    static const char* kPuncts[] = {
        "，", "。", "、", "！", "？", "：", "；", "…",
        "—", "～", "（", "）", "「", "」", "《", "》"
    };
    constexpr int kPunctCount = sizeof(kPuncts) / sizeof(kPuncts[0]);

    // 标签
    TextOutUtf8(hdc, fonts_.label, kPadding, y, "[标点]", kTextAmber);

    // 标点列表：两行排列
    int x = kPadding + 70;
    int row_y = y - 2;
    COLORREF pc = state_.enabled ? kTextMain : kTextGray;

    for (int i = 0; i < kPunctCount; ++i) {
        std::string entry = std::to_string(i + 1) + "." + kPuncts[i];
        TextOutUtf8(hdc, fonts_.candidate, x, row_y, entry, pc);
        int tw = TextWidth(hdc, fonts_.candidate, entry);
        x += tw + 12;

        // 第 8 个后换行
        if (i == 7) {
            x = kPadding + 70;
            row_y += kRowH;
        }
    }
}

}  // namespace ui

#endif  // _WIN32
