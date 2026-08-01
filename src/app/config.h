#pragma once
// 应用配置：开关快捷键、右摇杆阈值、候选分页等，可从文本文件加载。

#include <string>
#include <vector>

#include "gamepad/xinput_pad.h"

namespace app {

struct Config {
    // 开关快捷键：需“同时按住”的按钮组合（默认 Back + Start）。
    std::vector<gamepad::Button> toggle_combo{
        gamepad::Button::kBack, gamepad::Button::kStart};

    float stick_activate = 0.6f;   // 触发拨动的幅度阈值 (0~1)
    float stick_release = 0.35f;   // 回中阈值，低于此才能再次触发 (0~1)
    int long_press_ms = 500;       // 右摇杆长按触发字母候选的持续时间 (毫秒)
    int candidate_page = 8;        // 每页候选数
    bool start_enabled = false;    // 启动时是否已开启输入法功能
    float overlay_opacity = 0.7f;  // 九宫格覆盖层不透明度 (0~1)

    // 从 key=value 文本文件加载（缺省项保留默认值）。
    // 返回是否成功打开文件。
    bool LoadFromFile(const std::string& path);
};

// 按钮名 <-> 枚举 互转（用于配置解析与显示）。
const char* ButtonName(gamepad::Button b);
bool ParseButton(const std::string& name, gamepad::Button* out);

// 将组合键渲染为可读字符串，如 "Back+Start"。
std::string DescribeCombo(const std::vector<gamepad::Button>& combo);

}  // namespace app
