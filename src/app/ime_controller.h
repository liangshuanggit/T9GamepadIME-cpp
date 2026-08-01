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
//   - 右摇杆长按（保持方向不回中超过 long_press_ms）-> 进入该键的字母候选模式，
//     候选显示为大写字母、数字、小写字母（如 '2' -> "ABC2abc"）。
//   - 十字键 左/右 切换候选，上/下 翻页。
//   - A 确认上屏当前候选，B 退格 / 取消字母模式。
//   - LB 作为"编辑修饰键"：
//       LB + A → 全选 (Ctrl+A)
//       LB + X → 剪切 (Ctrl+X)
//       LB + Y → 复制 (Ctrl+C)
//       LB + B → 粘贴 (Ctrl+V)
//     仅在 IME 开启时生效；按住 LB 期间不处理 A/B 的常规功能（不误上屏/退格）。

#include <chrono>
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

    // 是否处于字母候选模式（长按触发），以及当前键的紧凑描述，如 "ABC2abc"
    bool LetterMode() const { return letter_mode_; }
    std::string LetterText() const;

    // 上一帧是否执行了文本注入（供 main 决定是否需要额外处理）
    bool JustInjected() const { return just_injected_; }
    void ClearInjectedFlag() { just_injected_ = false; }

    // 最近触发的编辑快捷键（LB+面键）：'A'/'B'/'X'/'Y'，0 表示无。
    // 仅在触发后的短暂窗口期内返回非 0，供界面高亮显示。
    char EditHighlight() const;

#ifdef T9IME_TESTING
    // 仅测试：直接设定长按累计时长，便于触发字母候选模式
    void DebugSetLongPressElapsed(int ms) { long_press_elapsed_ms_ = ms; }
#endif

private:
    void RefreshCandidates();
    void ClampSelection();
    bool ToggleComboEdge(const gamepad::XInputPad& pad);
    void EnterLetterMode(gamepad::Direction dir);
    void ExitLetterMode();

    // LB+面键 组合触发（注入 Ctrl+字母 + 记录高亮）
    bool TriggerEditShortcut(gamepad::Button btn);
    // 面键常规功能（无 LB 时）：A 上屏候选 / B 退格
    bool HandleFaceNormal(gamepad::Button btn);

    t9::T9Engine* engine_;  // 不持有
    Config cfg_;
    bool enabled_;
    bool prev_combo_ = false;
    gamepad::StickFlickDetector flick_;
    gamepad::Direction prev_pointing_ = gamepad::Direction::kNone;

    // 长按检测：摇杆锁定方向后累计按住时长
    gamepad::Direction long_press_dir_ = gamepad::Direction::kNone;
    int long_press_elapsed_ms_ = 0;
    bool long_press_consumed_ = false;
    std::chrono::steady_clock::time_point last_update_;

    // 字母候选模式：长按某键后，候选为该键的字母/数字
    bool letter_mode_ = false;
    char letter_digit_ = 0;

    // 编辑快捷键（LB+面键）触发反馈：记录触发时刻，供界面短暂高亮
    char edit_highlight_ = 0;
    std::chrono::steady_clock::time_point edit_highlight_at_;

    // 面键按下沿挂起：面键先于 LB 被检测到按下时（同帧按下或手柄扫描
    // 导致 LB 晚一帧上报），挂起该按下沿一帧，下一帧 LB 到位时回溯为组合，
    // 避免面键常规功能（上屏/退格）抢先触发。
    bool pending_face_ = false;
    gamepad::Button pending_btn_ = gamepad::Button::kA;

    std::vector<std::string> candidates_;
    int selected_ = 0;
    std::string last_committed_;
    bool just_injected_ = false;

    static constexpr size_t kMaxCandidates = 100;
    // 编辑快捷键高亮的显示时长（毫秒）
    static constexpr int kEditHighlightMs = 800;
};

}  // namespace app
