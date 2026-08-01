#pragma once
// 屏幕覆盖层（Overlay）：在桌面右下角显示扇形九宫格与候选。
//
// 使用 WS_EX_TOPMOST 置顶，WS_EX_NOACTIVATE 不抢焦点，
// WS_EX_TRANSPARENT 点击穿透（不影响前台应用光标状态）。
// 关闭态仅灰显扇形键区；开启态显示完整扇形键区 + 拼音 + 候选。
// 扇形布局：8 个方向键以弧形扇叶围绕中心静止位（右摇杆回中位）。

#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "gamepad/stick.h"

namespace ui {

// 需要渲染的状态快照（由 main 每帧填充）。
struct OverlayState {
    bool enabled = false;
    bool pad_connected = false;  // 手柄是否已连接
    gamepad::Direction pointing = gamepad::Direction::kNone;
    bool letter_mode = false;       // 是否处于长按字母候选模式
    std::string letter_text;        // 字母候选模式的紧凑描述，如 "ABC2abc"
    std::string digits;
    std::vector<std::string> pinyin;
    std::vector<std::string> candidates;
    int selected = 0;
    int page_start = 0;
    int page_size = 8;
    std::string last_committed;
    std::string hotkey_desc;
    char edit_highlight = 0;  // 最近触发的编辑快捷键（LB+面键）：'A'/'B'/'X'/'Y'，0=无
};

class Overlay {
public:
    Overlay();
    ~Overlay();

    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;

    // 创建窗口。
    bool Create(float opacity = 1.0f);
    void Destroy();

    bool IsValid() const { return hwnd_ != nullptr; }

    // 更新状态并触发重绘。
    void Refresh(const OverlayState& state);

    // 设置可见性。
    void SetVisible(bool visible);

private:
#if defined(_WIN32)
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    void OnPaint(HDC hdc);
    void DrawStatus(HDC hdc);
    void DrawGrid(HDC hdc, int ox, int oy);  // 扇形键区（中心静止位 + 8 方向扇叶）
    void DrawEditShortcuts(HDC hdc, int y);  // 编辑快捷键栏（LB+面键，触发高亮）
    void DrawInfoLines(HDC hdc, int y);
    void DrawCandidates(HDC hdc, int y);
    void DrawCommitted(HDC hdc, int y);
    void DrawPunctuation(HDC hdc, int y);

    struct FontSet {
        HFONT title     = nullptr;  // 24px 状态行
        HFONT label     = nullptr;  // 20px 标签/正文
        HFONT digit     = nullptr;  // 36px 粗体 九宫格数字
        HFONT small     = nullptr;  // 17px 小字（字母、提示）
        HFONT candidate = nullptr;  // 26px 候选词
        HFONT big       = nullptr;  // 40px 粗体 当前输入数字串
    };

    void EnsureFonts();
    void DestroyFonts();
    void EnsureBackBuffer(HDC hdc);

    HWND hwnd_ = nullptr;
    float opacity_ = 1.0f;
    int width_ = 500;
    int height_ = 640;  // 适配 720p 屏幕（工作区约 672px）
    OverlayState state_;
    FontSet fonts_;
    HDC mem_dc_ = nullptr;
    HBITMAP mem_bmp_ = nullptr;
    HGDIOBJ mem_old_ = nullptr;
#endif
};

}  // namespace ui
