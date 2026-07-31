#pragma once
// 输入法控制器：把一帧手柄输入翻译为 T9 操作，并维护开关状态与候选选择。
//
// 开关逻辑：
//   - 按下配置的快捷键组合切换开启/关闭。
//   - 关闭时本控制器不处理右摇杆/十字键/A/B，手柄按键保持其原功能
//     （本程序仅轮询 XInput，不拦截，故游戏仍可正常读取；若需真正屏蔽输入
//      到前台程序，需要 ViGEm 等更底层方案，属后续扩展）。
//
// 开启时：
//   - 右摇杆 8 方向拨动 -> 触发对应九宫格键位（T9 数字）。
//   - 十字键 左/右 切换候选，上/下 翻页。
//   - A 确认上屏当前候选，B 退格。

#include <string>
#include <vector>

#include "app/config.h"
#include "gamepad/stick.h"
#include "gamepad/xinput_pad.h"
#include "t9/t9_engine.h"

namespace app {

class ImeController {
public:
    ImeController(t9::T9Engine* engine, const Config& cfg);

    // 处理一帧输入；返回 true 表示界面状态有变化，需要重绘。
    bool Update(const gamepad::XInputPad& pad);

    bool Enabled() const { return enabled_; }

    // 直接切换开关状态（供托盘点击等非手柄触发场景使用）
    void ToggleEnabled() { enabled_ = !enabled_; }

    // 重置内部状态（模式切换时调用，清除摇杆锁定与指向状态）
    void ResetState();

    // —— 渲染所需访问器 ——
    const std::vector<std::string>& Candidates() const { return candidates_; }
    int SelectedIndex() const { return selected_; }
    int PageSize() const { return cfg_.candidate_page; }
    int PageStart() const;
    gamepad::Direction Pointing() const { return flick_.Pointing(); }
    const std::string& LastCommitted() const { return last_committed_; }

    // 上一帧是否执行了文本注入（供 main 决定是否需要额外处理）
    bool JustInjected() const { return just_injected_; }
    void ClearInjectedFlag() { just_injected_ = false; }

private:
    void RefreshCandidates();
    void ClampSelection();
    bool ToggleComboEdge(const gamepad::XInputPad& pad);

    t9::T9Engine* engine_;  // 不持有
    Config cfg_;
    bool enabled_;
    bool prev_combo_ = false;
    gamepad::StickFlickDetector flick_;
    gamepad::Direction prev_pointing_ = gamepad::Direction::kNone;

    std::vector<std::string> candidates_;
    int selected_ = 0;
    std::string last_committed_;
    bool just_injected_ = false;

    static constexpr size_t kMaxCandidates = 100;
};

}  // namespace app
