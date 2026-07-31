#include "gamepad/stick.h"

#include <cmath>

namespace gamepad {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}  // namespace

Direction DirectionOf(float x, float y, float threshold) {
    float mag = std::sqrt(x * x + y * y);
    if (mag < threshold) return Direction::kNone;

    // atan2 返回 (-180, 180]，0 度为右，90 度为上；转为 [0, 360)
    float ang = std::atan2(y, x) * 180.0f / kPi;
    if (ang < 0.0f) ang += 360.0f;

    // 每 45 度一个扇区，以正方向为中心（右=0、上=90 ...）
    int sector = static_cast<int>((ang + 22.5f) / 45.0f) % 8;
    switch (sector) {
        case 0: return Direction::kRight;
        case 1: return Direction::kUpRight;
        case 2: return Direction::kUp;
        case 3: return Direction::kUpLeft;
        case 4: return Direction::kLeft;
        case 5: return Direction::kDownLeft;
        case 6: return Direction::kDown;
        case 7: return Direction::kDownRight;
    }
    return Direction::kNone;
}

StickFlickDetector::StickFlickDetector(float activate, float release)
    : activate_(activate), release_(release) {}

Direction StickFlickDetector::Update(float x, float y) {
    float mag = std::sqrt(x * x + y * y);

    // 回到中心区：解锁，清除锁定方向和高亮
    if (mag < release_) {
        armed_ = true;
        locked_dir_ = Direction::kNone;
        pointing_ = Direction::kNone;
        return Direction::kNone;
    }

    // 方向锁定：触发后，摇杆未回中之前，保持锁定方向不变。
    // 即使摇杆漂移到相邻方向，也不会触发新方向或改变高亮。
    if (!armed_) {
        // 仍处于锁定状态，pointing_ 保持 locked_dir_
        return Direction::kNone;
    }

    // 已解锁（armed_==true）：检测新方向
    Direction dir = DirectionOf(x, y, activate_);
    if (dir != Direction::kNone) {
        // 触发：锁定该方向，直到回中才能再次触发
        armed_ = false;
        locked_dir_ = dir;
        pointing_ = dir;
        return dir;
    }

    // 幅度超过 release 但未到 activate：中间过渡区，不触发
    // pointing_ 保持 kNone（未锁定状态）
    return Direction::kNone;
}

void StickFlickDetector::Reset() {
    armed_ = true;
    locked_dir_ = Direction::kNone;
    pointing_ = Direction::kNone;
}

}  // namespace gamepad
