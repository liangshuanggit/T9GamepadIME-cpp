#include "app/desktop_controller.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace app {

#if defined(_WIN32)

// 诊断日志输出到 OutputDebugStringA 和日志文件
static void DcLog(const char* msg) {
    OutputDebugStringA(msg);
    char dir[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    std::string d(dir);
    size_t slash = d.find_last_of("\\/");
    if (slash != std::string::npos) {
        d = d.substr(0, slash + 1);
        FILE* f = std::fopen((d + "t9ime.log").c_str(), "a");
        if (f) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            std::fprintf(f, "[%02d:%02d:%02d.%03d] %s",
                         st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
            std::fclose(f);
        }
    }
}

void DesktopController::SetActive(bool active) {
    if (active_ != active) {
        active_ = active;
        DcLog(active ? "[Desktop] 激活（桌面操控模式）\n"
                      : "[Desktop] 停用（游戏手柄模式）\n");
    }
}

void DesktopController::MoveMouse(float dx, float dy) {
    if (dx == 0.0f && dy == 0.0f) return;
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>(std::round(dx));
    input.mi.dy = static_cast<LONG>(std::round(dy));
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
}

void DesktopController::ScrollWheel(int delta) {
    if (delta == 0) return;
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.mouseData = static_cast<DWORD>(delta);
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    SendInput(1, &input, sizeof(INPUT));
}

void DesktopController::ClickKey(unsigned short vk) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[0].ki.dwFlags = 0;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void DesktopController::ClickMouse(int button) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[1].type = INPUT_MOUSE;
    if (button == 0) {
        // 左键
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    } else {
        // 右键
        inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    }
    SendInput(2, inputs, sizeof(INPUT));
}

void DesktopController::PressCtrlAltCTwice() {
    // 连续发送两次完整的 Ctrl+Alt+C（按下组合 -> 释放 -> 再次按下组合 -> 释放），
    // 用于触发某些软件/设备的快捷键动作（如手柄与键鼠模式的切换）。
    for (int i = 0; i < 2; ++i) {
        INPUT inputs[6] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_CONTROL;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_MENU;
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 'C';
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = 'C';
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[4].type = INPUT_KEYBOARD;
        inputs[4].ki.wVk = VK_MENU;
        inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[5].type = INPUT_KEYBOARD;
        inputs[5].ki.wVk = VK_CONTROL;
        inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(6, inputs, sizeof(INPUT));
        // 两次之间留出间隔，避免被目标程序合并为一次
        Sleep(100);
    }
}

#else  // 非 Windows 桩实现

void DesktopController::SetActive(bool active) { active_ = active; }
void DesktopController::MoveMouse(float, float) {}
void DesktopController::ScrollWheel(int) {}
void DesktopController::ClickKey(unsigned short) {}
void DesktopController::ClickMouse(int) {}
void DesktopController::PressCtrlAltCTwice() {}

#endif

void DesktopController::ResetState() {
    prev_a_ = false;
    prev_b_ = false;
    prev_x_ = false;
    prev_y_ = false;
    prev_dpad_up_ = false;
    prev_dpad_down_ = false;
    prev_dpad_left_ = false;
    prev_dpad_right_ = false;
    prev_lb_ = false;
    prev_rb_ = false;
    prev_back_ = false;
}

void DesktopController::Update(const gamepad::XInputPad& pad) {
#ifdef T9IME_TESTING
    (void)pad;
    return;
#endif

    // ---- 活跃时：发送鼠标/键盘输入 ----
    if (active_) {
        // 诊断：每 60 帧（约 1 秒）输出一次手柄状态
        if (++diag_frame_ >= 60) {
            diag_frame_ = 0;
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "[Desktop] alive lx=%.2f ly=%.2f rx=%.2f ry=%.2f "
                "A=%d B=%d X=%d Y=%d LT=%.2f RT=%.2f\n",
                pad.LeftStickX(), pad.LeftStickY(),
                pad.RightStickX(), pad.RightStickY(),
                pad.Down(gamepad::Button::kA), pad.Down(gamepad::Button::kB),
                pad.Down(gamepad::Button::kX), pad.Down(gamepad::Button::kY),
                pad.LeftTrigger(), pad.RightTrigger());
            DcLog(buf);
        }

        // ---- 左摇杆 → 鼠标移动 ----
        float lx = pad.LeftStickX();
        float ly = pad.LeftStickY();
        float lmag = std::sqrt(lx * lx + ly * ly);
        if (lmag > kDeadZone) {
            // 重映射：死区外重新映射到 [0, 1]
            float scale = (lmag - kDeadZone) / (1.0f - kDeadZone);
            scale = std::min(1.0f, scale) / lmag;
            float dx = lx * scale * kMouseSpeed;
            float dy = -ly * scale * kMouseSpeed;  // Y 轴反转
            MoveMouse(dx, dy);
        }

        // ---- 右摇杆 → 滚轮 ----
        float rx = pad.RightStickX();
        float ry = pad.RightStickY();
        float rmag = std::sqrt(rx * rx + ry * ry);
        if (rmag > kDeadZone && std::abs(ry) > kDeadZone) {
            int scroll = static_cast<int>(-ry * kScrollSpeed * WHEEL_DELTA / 3);
            ScrollWheel(scroll);
        }

        // ---- 扳机键 → 持续滚轮 ----
        if (pad.LeftTrigger() > 0.3f) {
            ScrollWheel(-kTriggerScrollSpeed * WHEEL_DELTA / 120);
        }
        if (pad.RightTrigger() > 0.3f) {
            ScrollWheel(kTriggerScrollSpeed * WHEEL_DELTA / 120);
        }
    }

    // ---- 按键边沿检测（无论是否活跃都跟踪状态）----
    // 活跃时发送事件，非活跃时仅更新 prev_* 以保持状态同步

    bool a = pad.Down(gamepad::Button::kA);
    if (active_ && a && !prev_a_) { ClickMouse(0); DcLog("[Desktop] A -> 左键单击\n"); }
    prev_a_ = a;

    bool b = pad.Down(gamepad::Button::kB);
    if (active_ && b && !prev_b_) { ClickMouse(1); DcLog("[Desktop] B -> 右键单击\n"); }
    prev_b_ = b;

    bool x = pad.Down(gamepad::Button::kX);
    if (active_ && x && !prev_x_) { ClickKey(VK_BACK); DcLog("[Desktop] X -> 退格\n"); }
    prev_x_ = x;

    bool y = pad.Down(gamepad::Button::kY);
    if (active_ && y && !prev_y_) { ClickKey(VK_RETURN); DcLog("[Desktop] Y -> 回车\n"); }
    prev_y_ = y;

    bool du = pad.Down(gamepad::Button::kDpadUp);
    if (active_ && du && !prev_dpad_up_) ClickKey(VK_UP);
    prev_dpad_up_ = du;

    bool dd = pad.Down(gamepad::Button::kDpadDown);
    if (active_ && dd && !prev_dpad_down_) ClickKey(VK_DOWN);
    prev_dpad_down_ = dd;

    bool dl = pad.Down(gamepad::Button::kDpadLeft);
    if (active_ && dl && !prev_dpad_left_) ClickKey(VK_LEFT);
    prev_dpad_left_ = dl;

    bool dr = pad.Down(gamepad::Button::kDpadRight);
    if (active_ && dr && !prev_dpad_right_) ClickKey(VK_RIGHT);
    prev_dpad_right_ = dr;

    bool lb = pad.Down(gamepad::Button::kLB);
    if (active_ && lb && !prev_lb_) ClickKey(VK_PRIOR);
    prev_lb_ = lb;

    bool rb = pad.Down(gamepad::Button::kRB);
    if (active_ && rb && !prev_rb_) ClickKey(VK_NEXT);
    prev_rb_ = rb;

    bool back = pad.Down(gamepad::Button::kBack);
    if (active_ && back && !prev_back_) {
        // Ctrl+Esc 等效于 Win 键
        INPUT inputs[4] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_CONTROL;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_ESCAPE;
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = VK_ESCAPE;
        inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = VK_CONTROL;
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(4, inputs, sizeof(INPUT));
    }
    prev_back_ = back;
}

}  // namespace app
