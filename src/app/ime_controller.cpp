#include "app/ime_controller.h"

#include <cstdio>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "app/text_injector.h"
#include "t9/keypad.h"
#include "t9/t9_keymap.h"
namespace app {

// 常用中文标点：无数字输入时作为候选词显示，可直接上屏
static const std::vector<std::string> kCommonPunctuation = {
    "，", "。", "、", "！", "？", "：", "；", "…",
    "—", "～", "（", "）", "「", "」", "《", "》"
};

// 调试日志：写入 OutputDebugStringA（同文件其它分支的既有方式）
static void ImeLog(const char* msg) {
#if defined(_WIN32)
    OutputDebugStringA(msg);
#else
    (void)msg;
#endif
}

ImeController::ImeController(t9::T9Engine* engine, const Config& cfg)
    : engine_(engine),
      cfg_(cfg),
      enabled_(cfg.start_enabled),
      flick_(cfg.stick_activate, cfg.stick_release),
      last_update_(std::chrono::steady_clock::now()) {
    // 初始化候选：无输入时显示常用标点
    RefreshCandidates();
}

int ImeController::PageStart() const {
    if (cfg_.candidate_page <= 0) return 0;
    return (selected_ / cfg_.candidate_page) * cfg_.candidate_page;
}

std::string ImeController::LetterText() const {
    return letter_digit_ ? t9::LetterLabelForDigit(letter_digit_) : std::string();
}

void ImeController::RefreshCandidates() {
    if (letter_mode_) {
        // 字母候选模式：显示该键的大写字母、数字、小写字母（"ABC2abc"）
        candidates_ = t9::LetterCandidatesForDigit(letter_digit_);
    } else if (!engine_->Digits().empty()) {
        // 有数字输入时，从引擎获取汉字候选
        candidates_ = engine_->HanziCandidates(kMaxCandidates);
    } else {
        // 无输入时显示常用标点，用户可直接选择上屏
        candidates_ = kCommonPunctuation;
    }
    ClampSelection();
}

void ImeController::ClampSelection() {
    if (candidates_.empty()) {
        selected_ = 0;
        return;
    }
    if (selected_ < 0) selected_ = 0;
    if (selected_ >= static_cast<int>(candidates_.size())) {
        selected_ = static_cast<int>(candidates_.size()) - 1;
    }
}

void ImeController::ResetState() {
    prev_pointing_ = gamepad::Direction::kNone;
    flick_.Reset();
    letter_mode_ = false;
    letter_digit_ = 0;
    long_press_dir_ = gamepad::Direction::kNone;
    long_press_elapsed_ms_ = 0;
    long_press_consumed_ = false;
    last_update_ = std::chrono::steady_clock::now();
    // 重置快捷键状态：设为 true 表示"上一帧组合键已按下"，
    // 防止模式切换后 XInput 恢复时因残留按键状态导致立即重新触发切换。
    // 用户需要松开快捷键后再次按下才能触发下一次切换。
    prev_combo_ = true;
    edit_highlight_ = 0;
    pending_face_ = false;
}

void ImeController::EnterLetterMode(gamepad::Direction dir) {
    char digit = t9::DigitForDirection(dir);
    if (digit == 0) return;
    // 长按进入字母候选：清空拼音数字串，显示该键的字母/数字候选
    engine_->Clear();
    letter_mode_ = true;
    letter_digit_ = digit;
    selected_ = 0;
    RefreshCandidates();
}

void ImeController::ExitLetterMode() {
    letter_mode_ = false;
    letter_digit_ = 0;
    selected_ = 0;
    RefreshCandidates();
}

bool ImeController::ToggleComboEdge(const gamepad::XInputPad& pad) {
    bool all_down = !cfg_.toggle_combo.empty();
    for (gamepad::Button b : cfg_.toggle_combo) {
        if (!pad.Down(b)) {
            all_down = false;
            break;
        }
    }
    bool edge = all_down && !prev_combo_;
    prev_combo_ = all_down;
    return edge;
}

char ImeController::EditHighlight() const {
    if (edit_highlight_ == 0) return 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - edit_highlight_at_).count();
    return elapsed <= kEditHighlightMs ? edit_highlight_ : 0;
}

bool ImeController::Update(const gamepad::XInputPad& pad) {
    bool changed = false;

    // 1) 开关快捷键（无论开启与否都要检测）
    if (ToggleComboEdge(pad)) {
        enabled_ = !enabled_;
        std::string msg = "[IME] 快捷键触发 -> " + std::string(enabled_ ? "开启" : "关闭") + "\n";
#if defined(_WIN32)
        OutputDebugStringA(msg.c_str());
#endif
        return true;  // 状态变化，重绘；本帧不再处理其它输入
    }

    // 2) 关闭态：不处理任何输入，按键保持原功能
    if (!enabled_) return changed;

    // 3) LB 编辑修饰键：LB + 面键 -> 全选/剪切/复制/粘贴。
    //    组合判定含一帧回溯：面键若先于 LB 被检测到按下（同帧按下或手柄
    //    扫描导致 LB 晚一帧上报），先挂起该按下沿，下一帧 LB 到位时回溯为
    //    组合，避免面键常规功能（上屏/退格）抢先触发。
    bool lb = pad.Down(gamepad::Button::kLB);

    // 每帧重置文本注入标记（A 常规上屏时置位）
    just_injected_ = false;

    // 3a) 消费上一帧挂起的面键按下沿
    if (pending_face_) {
        gamepad::Button btn = pending_btn_;
        pending_face_ = false;
        if (lb) {
            changed |= TriggerEditShortcut(btn);  // LB 已到位 -> 回溯为组合
        } else {
            changed |= HandleFaceNormal(btn);     // 无 LB -> 按常规功能处理
        }
    }

    // 3b) 本帧面键按下沿：LB 已按住 -> 立即组合；否则挂起一帧等待确认。
    //     每帧只处理优先级最高的一个面键（A > B > X > Y，保持原有 else-if 语义），
    //     避免同帧多键互相覆盖。
    {
        const gamepad::Button kFaces[] = {
            gamepad::Button::kA, gamepad::Button::kB,
            gamepad::Button::kX, gamepad::Button::kY,
        };
        for (gamepad::Button btn : kFaces) {
            if (!pad.Pressed(btn)) continue;
            if (lb) {
                changed |= TriggerEditShortcut(btn);
            } else if (!pending_face_) {
                pending_face_ = true;
                pending_btn_ = btn;
            }
            break;  // 一帧一个面键按下沿
        }
    }

    // 3b) 右摇杆拨动 -> 触发九宫格键位
    //     始终调用 flick_.Update() 保持拨动检测状态同步，
    //     但 LB 按住期间不提交数字（避免编辑快捷键操作时误输入）。
    gamepad::Direction fired = flick_.Update(pad.RightStickX(), pad.RightStickY());
    if (!lb && fired != gamepad::Direction::kNone) {
        // 新的拨动：先退出字母候选模式
        if (letter_mode_) {
            ExitLetterMode();
        }
        char digit = t9::DigitForDirection(fired);
        if (digit) {
            engine_->PushKey(digit);
            selected_ = 0;
            RefreshCandidates();
            changed = true;
        }
    }

    // 指向变化也需重绘（更新九宫格高亮）
    if (flick_.Pointing() != prev_pointing_) {
        prev_pointing_ = flick_.Pointing();
        changed = true;
    }

    // 3b) 长按检测：保持锁定方向不回中超过阈值 -> 进入该键的字母候选模式
    //     （LB 按住期间不进入字母模式，避免编辑操作时误触发）
    {
        auto now = std::chrono::steady_clock::now();
        int dt = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_update_).count());
        last_update_ = now;

        if (!lb && !letter_mode_ && flick_.Pointing() != gamepad::Direction::kNone) {
            if (long_press_dir_ == flick_.Pointing()) {
                long_press_elapsed_ms_ += dt;
                if (!long_press_consumed_ &&
                    long_press_elapsed_ms_ >= cfg_.long_press_ms) {
                    long_press_consumed_ = true;
                    EnterLetterMode(flick_.Pointing());
                    changed = true;
                }
            } else {
                long_press_dir_ = flick_.Pointing();
                long_press_elapsed_ms_ = 0;
                long_press_consumed_ = false;
            }
        } else {
            long_press_dir_ = gamepad::Direction::kNone;
            long_press_elapsed_ms_ = 0;
            long_press_consumed_ = false;
        }
    }

    // 4) 十字键：左右切换候选，上下翻页
    if (!candidates_.empty()) {
        if (pad.Pressed(gamepad::Button::kDpadLeft)) {
            if (selected_ > 0) { --selected_; changed = true; }
        }
        if (pad.Pressed(gamepad::Button::kDpadRight)) {
            if (selected_ + 1 < static_cast<int>(candidates_.size())) { ++selected_; changed = true; }
        }
        if (pad.Pressed(gamepad::Button::kDpadUp)) {
            selected_ -= cfg_.candidate_page;
            ClampSelection();
            changed = true;
        }
        if (pad.Pressed(gamepad::Button::kDpadDown)) {
            selected_ += cfg_.candidate_page;
            ClampSelection();
            changed = true;
        }
    }

    // 5) A/B 常规功能（上屏/退格）已并入 HandleFaceNormal：
    //    面键按下沿统一在 3a/3b 处处理（组合或挂起消费）。

    return changed;
}

// LB+面键 组合触发：注入 Ctrl+字母，并记录高亮供界面显示
bool ImeController::TriggerEditShortcut(gamepad::Button btn) {
    char key = 0;     // 高亮标识（'A'/'B'/'X'/'Y'）
    char letter = 0;  // 注入的 Ctrl 组合字母
    const char* action = "";  // 功能名
    const char* combo = "";   // 快捷键描述
    switch (btn) {
        case gamepad::Button::kA:
            key = 'A'; letter = 'A'; action = "全选"; combo = "Ctrl+A"; break;
        case gamepad::Button::kX:
            key = 'X'; letter = 'X'; action = "剪切"; combo = "Ctrl+X"; break;
        case gamepad::Button::kY:
            key = 'Y'; letter = 'C'; action = "复制"; combo = "Ctrl+C"; break;
        case gamepad::Button::kB:
            key = 'B'; letter = 'V'; action = "粘贴"; combo = "Ctrl+V"; break;
        default:
            return false;
    }
    InjectCtrlKey(letter);
    edit_highlight_ = key;
    edit_highlight_at_ = std::chrono::steady_clock::now();
    char buf[96];
    std::snprintf(buf, sizeof(buf), "[IME] LB+%c -> %s %s\n", key, action, combo);
    ImeLog(buf);
    return true;
}

// 面键常规功能（无 LB 时）：A 确认上屏候选；B 退格/取消字母模式
bool ImeController::HandleFaceNormal(gamepad::Button btn) {
    if (btn == gamepad::Button::kA) {
        if (!candidates_.empty()) {
            last_committed_ = candidates_[selected_];
            // 将候选词以 Unicode 事件注入到当前前台窗口
            InjectText(last_committed_);
            just_injected_ = true;
            engine_->Clear();
            selected_ = 0;
            if (letter_mode_) {
                // 字母候选模式下屏后退出字母模式
                ExitLetterMode();
            } else {
                RefreshCandidates();
            }
            return true;
        }
    } else if (btn == gamepad::Button::kB) {
        if (letter_mode_) {
            // 字母候选模式：取消并退出
            ExitLetterMode();
            engine_->Clear();
            return true;
        } else if (!engine_->Digits().empty()) {
            // 有数字输入：删除最后一位数字
            engine_->PopKey();
            selected_ = 0;
            RefreshCandidates();
            return true;
        } else {
            // 无数字输入（标点模式）：向前台窗口发送退格键，
            // 删除光标处的内容
            InjectBackspace();
        }
    }
    return false;
}

}  // namespace app
