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

static const std::vector<std::string> kCommonPunctuation = {
    "，", "。", "、", "！", "？", "：", "；", "…",
    "—", "～", "（", "）", "「", "」", "《", "》"
};

static void ImeLog(const char* msg) {
#if defined(_WIN32)
    OutputDebugStringA(msg);
#else
    (void)msg;
#endif
}

ImeController::ImeController(t9::T9Engine* engine, const Config& cfg)
    : engine_(engine), cfg_(cfg), enabled_(cfg.start_enabled),
      flick_(cfg.stick_activate, cfg.stick_release),
      last_update_(std::chrono::steady_clock::now()) {
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
    if (letter_mode_) candidates_ = t9::LetterCandidatesForDigit(letter_digit_);
    else if (!engine_->Digits().empty()) candidates_ = engine_->HanziCandidates(kMaxCandidates);
    else candidates_ = kCommonPunctuation;
    ClampSelection();
}

void ImeController::ClampSelection() {
    if (candidates_.empty()) { selected_ = 0; return; }
    if (selected_ < 0) selected_ = 0;
    if (selected_ >= static_cast<int>(candidates_.size())) selected_ = static_cast<int>(candidates_.size()) - 1;
}

void ImeController::ResetState() {
    prev_pointing_ = gamepad::Direction::kNone;
    flick_.Reset(); letter_mode_ = false; letter_digit_ = 0;
    long_press_dir_ = gamepad::Direction::kNone; long_press_elapsed_ms_ = 0;
    long_press_consumed_ = false; last_update_ = std::chrono::steady_clock::now();
    prev_combo_ = true; edit_highlight_ = 0; pending_face_ = false;
}

void ImeController::EnterLetterMode(gamepad::Direction dir) {
    char digit = t9::DigitForDirection(dir); if (digit == 0) return;
    engine_->Clear(); letter_mode_ = true; letter_digit_ = digit; selected_ = 0; RefreshCandidates();
}

void ImeController::ExitLetterMode() {
    letter_mode_ = false; letter_digit_ = 0; selected_ = 0; RefreshCandidates();
}

bool ImeController::ToggleComboEdge(const gamepad::XInputPad& pad) {
    bool all_down = !cfg_.toggle_combo.empty();
    for (gamepad::Button b : cfg_.toggle_combo) if (!pad.Down(b)) { all_down = false; break; }
    bool edge = all_down && !prev_combo_; prev_combo_ = all_down; return edge;
}

char ImeController::EditHighlight() const {
    if (edit_highlight_ == 0) return 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - edit_highlight_at_).count();
    return elapsed <= kEditHighlightMs ? edit_highlight_ : 0;
}

bool ImeController::Update(const gamepad::XInputPad& pad) {
    bool changed = false;
    if (ToggleComboEdge(pad)) { enabled_ = !enabled_; std::string msg = "[IME] 快捷键触发 -> " + std::string(enabled_ ? "开启" : "关闭") + "\n"; ImeLog(msg.c_str()); return true; }
    if (!enabled_) return changed;
    bool lb = pad.Down(gamepad::Button::kLB); just_injected_ = false;
    if (pending_face_) {
        gamepad::Button btn = pending_btn_; pending_face_ = false;
        changed |= lb ? TriggerEditShortcut(btn) : HandleFaceNormal(btn);
    }
    const gamepad::Button kFaces[] = {gamepad::Button::kA, gamepad::Button::kB, gamepad::Button::kX, gamepad::Button::kY};
    for (gamepad::Button btn : kFaces) {
        if (!pad.Pressed(btn)) continue;
        if (lb) changed |= TriggerEditShortcut(btn);
        else if (!pending_face_) { pending_face_ = true; pending_btn_ = btn; }
        break;
    }
    gamepad::Direction fired = flick_.Update(pad.RightStickX(), pad.RightStickY());
    if (!lb && fired != gamepad::Direction::kNone) {
        if (letter_mode_) ExitLetterMode();
        char digit = t9::DigitForDirection(fired);
        if (digit) { engine_->PushKey(digit); selected_ = 0; RefreshCandidates(); changed = true; }
    }
    if (flick_.Pointing() != prev_pointing_) { prev_pointing_ = flick_.Pointing(); changed = true; }
    {
        auto now = std::chrono::steady_clock::now();
        int dt = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_).count());
        last_update_ = now;
        if (!lb && !letter_mode_ && flick_.Pointing() != gamepad::Direction::kNone) {
            if (long_press_dir_ == flick_.Pointing()) {
                long_press_elapsed_ms_ += dt;
                if (!long_press_consumed_ && long_press_elapsed_ms_ >= cfg_.long_press_ms) { long_press_consumed_ = true; EnterLetterMode(flick_.Pointing()); changed = true; }
            } else { long_press_dir_ = flick_.Pointing(); long_press_elapsed_ms_ = 0; long_press_consumed_ = false; }
        } else { long_press_dir_ = gamepad::Direction::kNone; long_press_elapsed_ms_ = 0; long_press_consumed_ = false; }
    }
    if (!candidates_.empty()) {
        if (pad.Pressed(gamepad::Button::kDpadLeft) && selected_ > 0) { --selected_; changed = true; }
        if (pad.Pressed(gamepad::Button::kDpadRight) && selected_ + 1 < static_cast<int>(candidates_.size())) { ++selected_; changed = true; }
        if (pad.Pressed(gamepad::Button::kDpadUp)) { selected_ -= cfg_.candidate_page; ClampSelection(); changed = true; }
        if (pad.Pressed(gamepad::Button::kDpadDown)) { selected_ += cfg_.candidate_page; ClampSelection(); changed = true; }
    }
    return changed;
}

bool ImeController::TriggerEditShortcut(gamepad::Button btn) {
    char key = 0, letter = 0; const char* action = ""; const char* combo = "";
    switch (btn) {
        case gamepad::Button::kA: key='A'; letter='A'; action="全选"; combo="Ctrl+A"; break;
        case gamepad::Button::kX: key='X'; letter='X'; action="剪切"; combo="Ctrl+X"; break;
        case gamepad::Button::kY: key='Y'; letter='C'; action="复制"; combo="Ctrl+C"; break;
        case gamepad::Button::kB: key='B'; letter='V'; action="粘贴"; combo="Ctrl+V"; break;
        default: return false;
    }
    InjectCtrlKey(letter); edit_highlight_ = key; edit_highlight_at_ = std::chrono::steady_clock::now();
    char buf[96]; std::snprintf(buf, sizeof(buf), "[IME] LB+%c -> %s %s\n", key, action, combo); ImeLog(buf); return true;
}

bool ImeController::HandleFaceNormal(gamepad::Button btn) {
    if (btn == gamepad::Button::kA) {
        if (!candidates_.empty()) {
            last_committed_ = candidates_[selected_];
            // 只有真正的中文/字母候选确认才学习；标点仍不进入个人词库。
            if (engine_ && !engine_->Digits().empty() && !letter_mode_) {
                engine_->RecordSelection(last_committed_);
            }
            InjectText(last_committed_); just_injected_ = true; engine_->Clear(); selected_ = 0;
            if (letter_mode_) ExitLetterMode(); else RefreshCandidates();
            return true;
        }
    } else if (btn == gamepad::Button::kB) {
        if (letter_mode_) { ExitLetterMode(); engine_->Clear(); return true; }
        if (!engine_->Digits().empty()) { engine_->PopKey(); selected_ = 0; RefreshCandidates(); return true; }
        InjectBackspace();
    }
    return false;
}

}  // namespace app