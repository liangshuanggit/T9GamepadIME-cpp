#pragma once
// 桌面模式控制器：IME 关闭时，将手柄输入转换为鼠标/键盘操作，
// 使手柄可以操控 Windows 桌面。
//
// 控制映射：
//   左摇杆     → 鼠标光标移动（速度随摇杆幅度变化）
//   右摇杆     → 鼠标滚轮滚动
//   A          → 鼠标左键单击
//   B          → 鼠标右键单击
//   X          → 退格键
//   Y          → 回车键
//   DPad上/下  → 方向键 上/下
//   DPad左/右  → 方向键 左/右
//   LB         → PageUp
//   RB         → PageDown
//   Back       → Windows 键（打开开始菜单）
//   LT         → 鼠标滚轮向下（持续）
//   RT         → 鼠标滚轮向上（持续）
//
// 设计说明：
//   SetActive(false) 时，Update 仅跟踪按键状态（不发送任何输入），
//   确保模式切换瞬间不会因状态过期而产生误触发。

#include "gamepad/xinput_pad.h"

namespace app {

class DesktopController {
public:
    DesktopController() = default;

    // 设置是否活跃（发送输入）。非活跃时仅跟踪状态。
    void SetActive(bool active);

    // 处理一帧手柄输入。活跃时发送鼠标/键盘事件，非活跃时仅跟踪状态。
    void Update(const gamepad::XInputPad& pad);

    // 重置所有按键边沿状态（模式切换时调用，防止过期状态导致误触发）
    void ResetState();

private:
    // 鼠标移动
    void MoveMouse(float dx, float dy);

    // 鼠标滚轮
    void ScrollWheel(int delta);

    // 按键单击（按下 + 释放）
    void ClickKey(unsigned short vk);

    // 鼠标单击
    void ClickMouse(int button);  // 0=左, 1=右

    // —— 按键边沿状态（上一帧是否按住）——
    bool prev_a_ = false;
    bool prev_b_ = false;
    bool prev_x_ = false;
    bool prev_y_ = false;
    bool prev_dpad_up_ = false;
    bool prev_dpad_down_ = false;
    bool prev_dpad_left_ = false;
    bool prev_dpad_right_ = false;
    bool prev_lb_ = false;
    bool prev_rb_ = false;
    bool prev_back_ = false;

    bool active_ = false;  // 初始非活跃（等 IME 关闭后才激活）

    // 诊断：帧计数器（每 60 帧输出一次状态日志）
    int diag_frame_ = 0;

    // 摇杆死区
    static constexpr float kDeadZone = 0.15f;

    // 鼠标灵敏度：摇杆满偏时每帧移动的像素数
    static constexpr float kMouseSpeed = 28.0f;

    // 滚轮灵敏度：摇杆满偏时每帧滚动量
    static constexpr int kScrollSpeed = 3;

    // 扳机滚轮灵敏度
    static constexpr int kTriggerScrollSpeed = 2;
};

}  // namespace app
