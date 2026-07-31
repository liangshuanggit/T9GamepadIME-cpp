#include "gamepad/xinput_pad.h"

#include <algorithm>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX  // 避免 windows.h 的 min/max 宏破坏 std::min/std::max
#endif
#include <windows.h>
#include <xinput.h>
#endif

namespace gamepad {

namespace {
#if defined(_WIN32)
// 把抽象 Button 映射到 XInput 的 XINPUT_GAMEPAD_* 位标志
uint16_t MaskOf(Button b) {
    switch (b) {
        case Button::kA:          return XINPUT_GAMEPAD_A;
        case Button::kB:          return XINPUT_GAMEPAD_B;
        case Button::kX:          return XINPUT_GAMEPAD_X;
        case Button::kY:          return XINPUT_GAMEPAD_Y;
        case Button::kDpadUp:     return XINPUT_GAMEPAD_DPAD_UP;
        case Button::kDpadDown:   return XINPUT_GAMEPAD_DPAD_DOWN;
        case Button::kDpadLeft:   return XINPUT_GAMEPAD_DPAD_LEFT;
        case Button::kDpadRight:  return XINPUT_GAMEPAD_DPAD_RIGHT;
        case Button::kLB:         return XINPUT_GAMEPAD_LEFT_SHOULDER;
        case Button::kRB:         return XINPUT_GAMEPAD_RIGHT_SHOULDER;
        case Button::kStart:      return XINPUT_GAMEPAD_START;
        case Button::kBack:       return XINPUT_GAMEPAD_BACK;
    }
    return 0;
}
#endif
}  // namespace

XInputPad::XInputPad(int user_index) : user_index_(user_index) {}

bool XInputPad::Poll() {
    prev_buttons_ = buttons_;
#if defined(_WIN32)
    XINPUT_STATE state{};
    if (XInputGetState(static_cast<DWORD>(user_index_), &state) == ERROR_SUCCESS) {
        connected_ = true;
        buttons_ = state.Gamepad.wButtons;
        thumb_rx_ = state.Gamepad.sThumbRX;
        thumb_ry_ = state.Gamepad.sThumbRY;
        thumb_lx_ = state.Gamepad.sThumbLX;
        thumb_ly_ = state.Gamepad.sThumbLY;
        lt_ = state.Gamepad.bLeftTrigger;
        rt_ = state.Gamepad.bRightTrigger;
    } else {
        connected_ = false;
        buttons_ = 0;
        thumb_rx_ = 0;
        thumb_ry_ = 0;
        thumb_lx_ = 0;
        thumb_ly_ = 0;
        lt_ = 0;
        rt_ = 0;
    }
#else
    connected_ = false;
    buttons_ = 0;
    thumb_rx_ = 0;
    thumb_ry_ = 0;
    thumb_lx_ = 0;
    thumb_ly_ = 0;
    lt_ = 0;
    rt_ = 0;
#endif
    return connected_;
}

bool XInputPad::Pressed(Button b) const {
#if defined(_WIN32)
    uint16_t m = MaskOf(b);
    return (buttons_ & m) && !(prev_buttons_ & m);
#else
    (void)b;
    return false;
#endif
}

bool XInputPad::Down(Button b) const {
#if defined(_WIN32)
    uint16_t m = MaskOf(b);
    return (buttons_ & m) != 0;
#else
    (void)b;
    return false;
#endif
}

float XInputPad::RightStickX() const {
    float v = thumb_rx_ / 32767.0f;
    return std::max(-1.0f, std::min(1.0f, v));
}

float XInputPad::RightStickY() const {
    float v = thumb_ry_ / 32767.0f;
    return std::max(-1.0f, std::min(1.0f, v));
}

float XInputPad::LeftStickX() const {
    float v = thumb_lx_ / 32767.0f;
    return std::max(-1.0f, std::min(1.0f, v));
}

float XInputPad::LeftStickY() const {
    float v = thumb_ly_ / 32767.0f;
    return std::max(-1.0f, std::min(1.0f, v));
}

float XInputPad::LeftTrigger() const {
    return lt_ / 255.0f;
}

float XInputPad::RightTrigger() const {
    return rt_ / 255.0f;
}

#ifdef T9IME_TESTING
void XInputPad::InjectForTest(const std::vector<Button>& down, float rx, float ry) {
    prev_buttons_ = buttons_;  // 上一帧状态，用于边沿检测
    uint16_t mask = 0;
#if defined(_WIN32)
    for (Button b : down) mask |= MaskOf(b);
#else
    (void)down;
#endif
    buttons_ = mask;
    connected_ = true;
    float cx = std::max(-1.0f, std::min(1.0f, rx));
    float cy = std::max(-1.0f, std::min(1.0f, ry));
    thumb_rx_ = static_cast<int16_t>(cx * 32767.0f);
    thumb_ry_ = static_cast<int16_t>(cy * 32767.0f);
}
#endif

}  // namespace gamepad
