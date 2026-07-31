#pragma once
// XInput 手柄轮询封装：检测按键的“边沿”（本帧刚按下）。

#include <cstdint>

#ifdef T9IME_TESTING
#include <vector>
#endif

namespace gamepad {

// 抽象的手柄按键，供上层逻辑使用（与具体后端解耦）
enum class Button {
    kA, kB, kX, kY,
    kDpadUp, kDpadDown, kDpadLeft, kDpadRight,
    kLB, kRB,
    kStart, kBack,
};

class XInputPad {
public:
    explicit XInputPad(int user_index = 0);

    // 轮询一次，刷新内部按键状态。返回手柄是否已连接。
    bool Poll();

    // 本帧是否“刚按下”（上一帧未按，本帧按下）
    bool Pressed(Button b) const;
    // 当前是否处于按住状态
    bool Down(Button b) const;

    // 右摇杆归一化坐标：范围 [-1, 1]，X 右为正，Y 上为正
    float RightStickX() const;
    float RightStickY() const;

    // 左摇杆归一化坐标：范围 [-1, 1]，X 右为正，Y 上为正
    float LeftStickX() const;
    float LeftStickY() const;

    // 左/右扳机键值：范围 [0, 1]
    float LeftTrigger() const;
    float RightTrigger() const;

    bool Connected() const { return connected_; }

#ifdef T9IME_TESTING
    // 仅测试：直接注入本帧状态，模拟一次 Poll。
    // down 为当前按住的按钮集合；rx/ry 为归一化右摇杆坐标 [-1,1]。
    // 连续两次调用即可模拟按键“边沿”（上一帧 -> 本帧）。
    void InjectForTest(const std::vector<Button>& down, float rx = 0.0f,
                       float ry = 0.0f);
#endif

private:
    int user_index_;
    bool connected_ = false;
    uint16_t buttons_ = 0;       // 本帧按键位掩码
    uint16_t prev_buttons_ = 0;  // 上一帧按键位掩码
    int16_t thumb_rx_ = 0;       // 右摇杆原始 X
    int16_t thumb_ry_ = 0;       // 右摇杆原始 Y
    int16_t thumb_lx_ = 0;       // 左摇杆原始 X
    int16_t thumb_ly_ = 0;       // 左摇杆原始 Y
    uint8_t lt_ = 0;             // 左扳机原始值
    uint8_t rt_ = 0;             // 右扳机原始值
};

}  // namespace gamepad
