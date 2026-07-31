#pragma once
// 屏幕覆盖层（Overlay）：在桌面右下角显示九宫格与候选。
//
// 使用 WS_EX_TOPMOST 置顶，WS_EX_NOACTIVATE 不抢焦点，
// WS_EX_TRANSPARENT 点击穿透（不影响前台应用光标状态）。
// 关闭态仅灰显九宫格；开启态显示完整九宫格 + 拼音 + 候选。

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
    std::string digits;
    std::vector<std::string> pinyin;
    std::vector<std::string> candidates;
    int selected = 0;
    int page_start = 0;
    int page_size = 5;
    std::string last_committed;
    std::string hotkey_desc;
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
    void DrawGrid(HDC hdc, int ox, int oy);
    void DrawInfoLines(HDC hdc, int y);
    void DrawCandidates(HDC hdc, int y);
    void DrawCommitted(HDC hdc, int y);
    void DrawPunctuation(HDC hdc, int y);

    HWND hwnd_ = nullptr;
    float opacity_ = 1.0f;
    int width_ = 440;
    int height_ = 560;
    OverlayState state_;
#endif
};

}  // namespace ui
