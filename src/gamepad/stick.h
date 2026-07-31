#pragma once
// 右摇杆方向识别与“拨动”检测。
// 摇杆静止在中心时无方向；拨向 8 个方向之一时识别为对应方向。

namespace gamepad {

// 8 方向（外加无方向）。角度约定：X 右为正，Y 上为正。
enum class Direction {
    kNone,
    kUp,
    kUpRight,
    kRight,
    kDownRight,
    kDown,
    kDownLeft,
    kLeft,
    kUpLeft,
};

// 将归一化摇杆坐标 (x, y) 转为 8 方向；幅度小于 threshold 时返回 kNone。
Direction DirectionOf(float x, float y, float threshold);

// 摇杆"拨动"检测（方向锁定 + 边沿语义）：
// 从中心区拨向某方向时产生一次触发；触发后锁定该方向，
// 必须完全回到中心（幅度 < release）之后才能解锁并触发新方向。
// 方向锁定防止回中路径经过相邻方向时误触。
class StickFlickDetector {
public:
    explicit StickFlickDetector(float activate = 0.8f, float release = 0.15f);

    // 传入本帧归一化坐标，返回"本帧刚触发"的方向；未触发返回 kNone。
    Direction Update(float x, float y);

    // 当前锁定的方向（用于界面高亮）：触发后保持锁定方向，
    // 直到摇杆回中才清零。与原始摇杆方向解耦，避免高亮跳变。
    Direction Pointing() const { return pointing_; }

    // 重置内部状态（模式切换时调用，清除锁定方向并重新武装）
    void Reset();

private:
    float activate_;
    float release_;
    bool armed_ = true;                        // 是否已回中，允许下一次触发
    Direction locked_dir_ = Direction::kNone;  // 触发后锁定的方向
    Direction pointing_ = Direction::kNone;    // 当前高亮方向（锁定方向或无）
};

}  // namespace gamepad
