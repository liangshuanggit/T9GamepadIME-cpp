#include "app/ime_controller.h"

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

namespace app {

// 常用中文标点：无数字输入时作为候选词显示，可直接上屏
static const std::vector<std::string> kCommonPunctuation = {
    "，", "。", "、", "！", "？", "：", "；", "…",
    "—", "～", "（", "）", "「", "」", "《", "》"
};

ImeController::ImeController(t9::T9Engine* engine, const Config& cfg)
    : engine_(engine),
      cfg_(cfg),
      enabled_(cfg.start_enabled),
      flick_(cfg.stick_activate, cfg.stick_release) {
    // 初始化候选：无输入时显示常用标点
    RefreshCandidates();
}

int ImeController::PageStart() const {
    if (cfg_.candidate_page <= 0) return 0;
    return (selected_ / cfg_.candidate_page) * cfg_.candidate_page;
}

void ImeController::RefreshCandidates() {
    // 有数字输入时，从引擎获取汉字候选
    if (!engine_->Digits().empty()) {
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

    // 3) 右摇杆拨动 -> 触发九宫格键位
    gamepad::Direction fired = flick_.Update(pad.RightStickX(), pad.RightStickY());
    if (fired != gamepad::Direction::kNone) {
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

    // 5) A 确认上屏当前候选 -> 注入到前台窗口光标处
    just_injected_ = false;
    if (pad.Pressed(gamepad::Button::kA)) {
        if (!candidates_.empty()) {
            last_committed_ = candidates_[selected_];
            // 将候选词以 Unicode 事件注入到当前前台窗口
            InjectText(last_committed_);
            just_injected_ = true;
            engine_->Clear();
            selected_ = 0;
            RefreshCandidates();
            changed = true;
        }
    }

    // 6) B 退格
    if (pad.Pressed(gamepad::Button::kB)) {
        if (!engine_->Digits().empty()) {
            // 有数字输入：删除最后一位数字
            engine_->PopKey();
            selected_ = 0;
            RefreshCandidates();
            changed = true;
        } else {
            // 无数字输入（标点模式）：向前台窗口发送退格键，
            // 删除光标处的内容
            InjectBackspace();
        }
    }

    return changed;
}

}  // namespace app
