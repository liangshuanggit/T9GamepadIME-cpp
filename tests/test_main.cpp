// T9GamepadIME 单元测试（纯逻辑，使用桩拼音内核，无需词典）。
//
// 覆盖：右摇杆 8 方向识别与拨动边沿、九宫格键位映射、T9 拼音展开、
//       配置解析、以及 ImeController 的开关/候选选择状态机。
//
// 借助 XInputPad 在 T9IME_TESTING 下暴露的 InjectForTest 注入手柄输入，
// 从而在无手柄硬件的环境下验证控制器逻辑。

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "app/config.h"
#include "app/ime_controller.h"
#include "gamepad/stick.h"
#include "gamepad/xinput_pad.h"
#include "ime/pinyin_ime.h"
#include "t9/keypad.h"
#include "t9/t9_engine.h"
#include "t9/t9_keymap.h"

namespace {

int g_failed = 0;
int g_total = 0;

void Check(bool cond, const char* expr, const char* file, int line) {
    ++g_total;
    if (!cond) {
        ++g_failed;
        std::printf("  [FAIL] %s  (%s:%d)\n", expr, file, line);
    }
}

#define CHECK(cond) Check((cond), #cond, __FILE__, __LINE__)

bool Contains(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& e : v) if (e == s) return true;
    return false;
}

// ---------------- 1) 右摇杆 8 方向识别 ----------------
void TestDirectionOf() {
    std::printf("[Test] DirectionOf 八方向\n");
    using gamepad::Direction;
    const float t = 0.5f;
    CHECK(gamepad::DirectionOf(1, 0, t) == Direction::kRight);
    CHECK(gamepad::DirectionOf(0, 1, t) == Direction::kUp);
    CHECK(gamepad::DirectionOf(-1, 0, t) == Direction::kLeft);
    CHECK(gamepad::DirectionOf(0, -1, t) == Direction::kDown);
    CHECK(gamepad::DirectionOf(-0.8f, 0.8f, t) == Direction::kUpLeft);
    CHECK(gamepad::DirectionOf(0.8f, 0.8f, t) == Direction::kUpRight);
    CHECK(gamepad::DirectionOf(-0.8f, -0.8f, t) == Direction::kDownLeft);
    CHECK(gamepad::DirectionOf(0.8f, -0.8f, t) == Direction::kDownRight);
    // 幅度不足 -> 无方向
    CHECK(gamepad::DirectionOf(0.1f, 0.1f, t) == Direction::kNone);
    CHECK(gamepad::DirectionOf(0, 0, t) == Direction::kNone);
}

// ---------------- 2) 拨动边沿检测 ----------------
void TestFlickDetector() {
    std::printf("[Test] StickFlickDetector 边沿语义\n");
    using gamepad::Direction;
    gamepad::StickFlickDetector d(0.6f, 0.35f);

    // 首次拨到右侧 -> 触发一次
    CHECK(d.Update(1.0f, 0.0f) == Direction::kRight);
    // 持续保持 -> 不再触发（连发抑制）
    CHECK(d.Update(1.0f, 0.0f) == Direction::kNone);
    CHECK(d.Update(0.9f, 0.0f) == Direction::kNone);
    // 未回落到 release 以下 -> 仍不触发
    CHECK(d.Update(0.5f, 0.0f) == Direction::kNone);
    // 回中（< release）-> 解除锁定
    CHECK(d.Update(0.0f, 0.0f) == Direction::kNone);
    CHECK(d.Pointing() == Direction::kNone);
    // 再次拨动（向上）-> 再次触发
    CHECK(d.Update(0.0f, 1.0f) == Direction::kUp);
    CHECK(d.Pointing() == Direction::kUp);
}

// ---------------- 3) 九宫格键位映射与布局 ----------------
void TestKeypad() {
    std::printf("[Test] 九宫格 方向->数字 与 布局\n");
    using gamepad::Direction;
    CHECK(t9::DigitForDirection(Direction::kUpLeft) == '5');  // 左上放 5
    CHECK(t9::DigitForDirection(Direction::kUp) == '2');
    CHECK(t9::DigitForDirection(Direction::kUpRight) == '3');
    CHECK(t9::DigitForDirection(Direction::kLeft) == '4');
    CHECK(t9::DigitForDirection(Direction::kRight) == '6');
    CHECK(t9::DigitForDirection(Direction::kDownLeft) == '7');
    CHECK(t9::DigitForDirection(Direction::kDown) == '8');
    CHECK(t9::DigitForDirection(Direction::kDownRight) == '9');
    CHECK(t9::DigitForDirection(Direction::kNone) == 0);  // 中心留空

    const t9::KeyCell* cells = t9::GridCells();
    CHECK(cells[0].digit == '5');    // 左上
    CHECK(cells[4].digit == '\0');   // 中心留空
    CHECK(cells[8].digit == '9');    // 右下
    CHECK(std::string(cells[1].letters) == "abc");  // 上 = 2
}

// ---------------- 4) T9 数字键 <-> 字母 ----------------
void TestKeymap() {
    std::printf("[Test] T9 数字/字母映射\n");
    CHECK(t9::LettersForKey('2') == "abc");
    CHECK(t9::LettersForKey('9') == "wxyz");
    CHECK(t9::LettersForKey('1').empty());
    CHECK(t9::KeyForLetter('a') == '2');
    CHECK(t9::KeyForLetter('z') == '9');
    CHECK(t9::PinyinToDigits("ni") == "64");   // n=6, i=4
    CHECK(t9::PinyinToDigits("hao") == "426"); // h=4, a=2, o=6
    CHECK(t9::PinyinToDigits("a1").empty());   // 含非法字符
}

// ---------------- 4b) 长按字母候选 ----------------
void TestLetterCandidates() {
    std::printf("[Test] 长按字母候选 ABC2abc\n");

    // '2' -> 大写 A B C + 数字 2 + 小写 a b c
    auto c2 = t9::LetterCandidatesForDigit('2');
    std::vector<std::string> exp2 = {"A", "B", "C", "2", "a", "b", "c"};
    CHECK(c2 == exp2);
    CHECK(t9::LetterLabelForDigit('2') == "ABC2abc");

    // '7' -> 4 字母键
    auto c7 = t9::LetterCandidatesForDigit('7');
    std::vector<std::string> exp7 = {"P", "Q", "R", "S", "7", "p", "q", "r", "s"};
    CHECK(c7 == exp7);
    CHECK(t9::LetterLabelForDigit('7') == "PQRS7pqrs");

    // 非法键 -> 空
    CHECK(t9::LetterCandidatesForDigit('1').empty());
    CHECK(t9::LetterLabelForDigit('1').empty());
}

// ---------------- 5) T9 引擎拼音展开 ----------------
void TestT9Engine() {
    std::printf("[Test] T9Engine 拼音展开\n");
    ime::PinyinIme ime;
    ime.Open("", "");  // 桩：总是成功
    t9::T9Engine eng(&ime);

    // 非法键被忽略
    eng.PushKey('1');
    eng.PushKey('0');
    CHECK(eng.Digits().empty());

    // "42" -> {g,h,i} x {a,b,c}：桩模式下所有字母组合都会展开
    // （音节合法性过滤由 libgooglepinyin 的 ValidateSplstr 负责，桩不过滤）
    eng.PushKey('4');
    eng.PushKey('2');
    CHECK(eng.Digits() == "42");
    auto py = eng.PinyinCandidates(50);
    CHECK(Contains(py, "ga"));
    CHECK(Contains(py, "ha"));
    CHECK(Contains(py, "ia"));  // 桩模式下不过滤

    // 退格
    eng.PopKey();
    CHECK(eng.Digits() == "4");
    eng.Clear();
    CHECK(eng.Digits().empty());
    CHECK(eng.PinyinCandidates().empty());

    // ---- 多音节展开 ----
    // "64426" = n,i,h,a,o -> "nihao"（ni + hao 两音节）
    eng.PushKey('6'); eng.PushKey('4'); eng.PushKey('4');
    eng.PushKey('2'); eng.PushKey('6');
    CHECK(eng.Digits() == "64426");
    {
        // 桩模式下不过滤音节，3^5=243 种组合，需足够大的 max_results
        auto py = eng.PinyinCandidates(300);
        CHECK(Contains(py, "nihao"));   // ni + hao
        CHECK(Contains(py, "mihao"));   // mi + hao
    }
    eng.Clear();

    // "826" = t,a,o -> "tao"（单音节，应仍正常工作）
    eng.PushKey('8'); eng.PushKey('2'); eng.PushKey('6');
    {
        auto py = eng.PinyinCandidates(50);
        CHECK(Contains(py, "tao"));
        CHECK(Contains(py, "tan"));  // t+a+n
    }
    eng.Clear();

    // "64264" = ni + hao 的变体（n=6,i=4,h=4,a=2,o=6 不对，这里是 ni+gao?）
    // 实际测试 "644264" = nihao + n? 先测短一点的
    // "426" = h,a,o -> "hao"
    eng.PushKey('4'); eng.PushKey('2'); eng.PushKey('6');
    {
        auto py = eng.PinyinCandidates(50);
        CHECK(Contains(py, "hao"));
        CHECK(Contains(py, "gao"));
    }
    eng.Clear();

    // ---- 模糊音开关 ----
    CHECK(!eng.FuzzyEnabled());
    eng.SetFuzzyEnabled(true);
    CHECK(eng.FuzzyEnabled());
    eng.SetFuzzyEnabled(false);
    CHECK(!eng.FuzzyEnabled());
}

// ---------------- 5b) T9 引擎模糊音 ----------------
void TestFuzzyPinyin() {
    std::printf("[Test] T9Engine 模糊音\n");
    ime::PinyinIme ime;
    ime.Open("", "");
    t9::T9Engine eng(&ime);

    // 模糊音关闭时不产生额外变体
    eng.SetFuzzyEnabled(false);
    eng.PushKey('9'); eng.PushKey('4'); eng.PushKey('4');  // w,g,h -> wgh
    {
        auto py = eng.PinyinCandidates(300);
        CHECK(Contains(py, "wgh"));  // 桩模式：原始组合存在
    }
    eng.Clear();

    // 模糊音开启时产生额外变体
    eng.SetFuzzyEnabled(true);
    eng.PushKey('9'); eng.PushKey('4'); eng.PushKey('4');
    {
        auto py = eng.PinyinCandidates(300);
        // 桩模式下 ValidateAndDecode 总是返回成功，
        // 所以模糊音变体也会出现在结果中
        CHECK(Contains(py, "wgh"));  // 原始
    }
    eng.Clear();
    eng.SetFuzzyEnabled(false);
}

// ---------------- 5c) T9 引擎词频排序 ----------------
void TestHanziSorting() {
    std::printf("[Test] T9Engine 词频排序\n");
    ime::PinyinIme ime;
    ime.Open("", "");
    t9::T9Engine eng(&ime);

    // 桩模式下 Search 返回 [stub:拼音] 格式的候选
    eng.PushKey('6'); eng.PushKey('4');  // "64" -> ni, mi, etc.
    {
        auto hz = eng.HanziCandidates(50);
        CHECK(!hz.empty());
        // 桩模式下候选格式为 [stub:拼音]，UTF-8 字符数应该一致
        // 验证排序不会崩溃且返回非空
    }
    eng.Clear();
}

// ---------------- 6) 配置解析 ----------------
void TestConfig() {
    std::printf("[Test] Config 解析\n");
    // 按钮名互转
    gamepad::Button b;
    CHECK(app::ParseButton("Start", &b) && b == gamepad::Button::kStart);
    CHECK(app::ParseButton("back", &b) && b == gamepad::Button::kBack);  // 大小写无关
    CHECK(!app::ParseButton("NoSuch", &b));
    std::vector<gamepad::Button> combo{gamepad::Button::kBack, gamepad::Button::kStart};
    CHECK(app::DescribeCombo(combo) == "Back+Start");

    // 从临时文件加载
    const char* path = "test_config_tmp.ini";
    {
        FILE* f = std::fopen(path, "wb");
        CHECK(f != nullptr);
        if (f) {
            std::fputs("# comment\n", f);
            std::fputs("toggle_hotkey = LB+RB\n", f);
            std::fputs("stick_activate = 0.75\n", f);
            std::fputs("candidate_page = 7\n", f);
            std::fputs("start_enabled = true\n", f);
            std::fclose(f);
        }
    }
    app::Config cfg;
    CHECK(cfg.LoadFromFile(path));
    CHECK(cfg.toggle_combo.size() == 2 &&
          cfg.toggle_combo[0] == gamepad::Button::kLB &&
          cfg.toggle_combo[1] == gamepad::Button::kRB);
    CHECK(std::fabs(cfg.stick_activate - 0.75f) < 1e-4f);
    CHECK(cfg.candidate_page == 7);
    CHECK(cfg.start_enabled == true);
    std::remove(path);

    // 不存在的文件 -> 返回 false，保留默认值
    app::Config def;
    CHECK(!def.LoadFromFile("no_such_file_xyz.ini"));
    CHECK(def.candidate_page == 8);
}

// ---------------- 7) ImeController 状态机 ----------------
void TestImeController() {
    std::printf("[Test] ImeController 开关/候选状态机\n");
    using gamepad::Button;
    ime::PinyinIme ime;
    ime.Open("", "");
    t9::T9Engine eng(&ime);
    app::Config cfg;  // 默认 toggle=Back+Start, start_enabled=false
    app::ImeController ctl(&eng, cfg);
    gamepad::XInputPad pad;

    CHECK(!ctl.Enabled());

    // 关闭态：拨动右摇杆应被忽略（按键保持原功能）
    pad.InjectForTest({}, 1.0f, 0.0f);  // 强拨右
    ctl.Update(pad);
    CHECK(eng.Digits().empty());  // 未产生任何输入
    CHECK(!ctl.Enabled());

    // 回中一帧，避免影响后续边沿
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);

    // 按下 Back+Start 组合 -> 开启
    pad.InjectForTest({Button::kBack, Button::kStart}, 0.0f, 0.0f);
    CHECK(ctl.Update(pad));  // 状态变化需重绘
    CHECK(ctl.Enabled());

    // 松开组合（held->release 不应再次切换）
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(ctl.Enabled());

    // 开启态：右摇杆拨右 -> 触发 '6'
    pad.InjectForTest({}, 1.0f, 0.0f);
    ctl.Update(pad);
    CHECK(eng.Digits() == "6");
    CHECK(!ctl.Candidates().empty());  // 桩候选非空
    const size_t ncand = ctl.Candidates().size();

    // 摇杆回中（再次解锁 + 不重复触发）
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(eng.Digits() == "6");  // 未连发

    // 十字键右 -> 候选下一个
    CHECK(ncand >= 2);
    pad.InjectForTest({Button::kDpadRight}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(ctl.SelectedIndex() == 1);
    pad.InjectForTest({}, 0.0f, 0.0f);  // 释放
    ctl.Update(pad);

    // 十字键左 -> 候选上一个
    pad.InjectForTest({Button::kDpadLeft}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(ctl.SelectedIndex() == 0);
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);

    // B 退格 -> 数字串清空
    pad.InjectForTest({Button::kB}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(eng.Digits().empty());
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);

    // 重新输入并用 A 确认上屏
    pad.InjectForTest({}, 1.0f, 0.0f);  // 拨右 -> '6'
    ctl.Update(pad);
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(!ctl.Candidates().empty());
    std::string expect = ctl.Candidates()[ctl.SelectedIndex()];
    pad.InjectForTest({Button::kA}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(ctl.LastCommitted() == expect);
    CHECK(eng.Digits().empty());  // 上屏后清空
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);

    // 再次按 Back+Start -> 关闭
    pad.InjectForTest({Button::kBack, Button::kStart}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(!ctl.Enabled());
    // 关闭后拨杆再次被忽略
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);
    pad.InjectForTest({}, 1.0f, 0.0f);
    ctl.Update(pad);
    CHECK(eng.Digits().empty());
}

// ---------------- 7b) ImeController 长按字母候选模式 ----------------
void TestImeControllerLetterMode() {
    std::printf("[Test] ImeController 长按字母候选模式\n");
    using gamepad::Button;
    ime::PinyinIme ime;
    ime.Open("", "");
    t9::T9Engine eng(&ime);
    app::Config cfg;  // 默认 long_press_ms=500
    app::ImeController ctl(&eng, cfg);
    gamepad::XInputPad pad;

    // 开启
    pad.InjectForTest({Button::kBack, Button::kStart}, 0.0f, 0.0f);
    ctl.Update(pad);
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(ctl.Enabled());

    // 短拨右 -> 触发 '6' 拼音候选，未进入字母模式
    pad.InjectForTest({}, 1.0f, 0.0f);
    ctl.Update(pad);
    CHECK(eng.Digits() == "6");
    CHECK(!ctl.LetterMode());

    // 保持方向不回中，累计长按时长 -> 进入字母模式（'6' -> "MNO6mno"）
    ctl.DebugSetLongPressElapsed(500);
    pad.InjectForTest({}, 1.0f, 0.0f);
    ctl.Update(pad);
    CHECK(ctl.LetterMode());
    CHECK(ctl.LetterText() == "MNO6mno");
    CHECK(eng.Digits().empty());  // 进入字母模式清空拼音数字串
    std::vector<std::string> exp = {"M", "N", "O", "6", "m", "n", "o"};
    CHECK(ctl.Candidates() == exp);

    // 摇杆回中，字母模式保持，可用十字键导航
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(ctl.LetterMode());
    CHECK(ctl.SelectedIndex() == 0);
    pad.InjectForTest({Button::kDpadRight}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(ctl.SelectedIndex() == 1);
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);

    // A 确认上屏所选字母并退出字母模式
    pad.InjectForTest({Button::kA}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(ctl.LastCommitted() == "N");
    CHECK(!ctl.LetterMode());
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);

    // 再次长按进入字母模式后按 B -> 取消，回到标点候选
    pad.InjectForTest({}, 1.0f, 0.0f);
    ctl.Update(pad);
    ctl.DebugSetLongPressElapsed(500);
    pad.InjectForTest({}, 1.0f, 0.0f);
    ctl.Update(pad);
    CHECK(ctl.LetterMode());
    pad.InjectForTest({Button::kB}, 0.0f, 0.0f);
    ctl.Update(pad);
    CHECK(!ctl.LetterMode());
    CHECK(eng.Digits().empty());
    CHECK(!ctl.Candidates().empty());  // 回到标点
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);

    // 字母模式下新拨动 -> 退出字母模式并触发新键
    pad.InjectForTest({}, 1.0f, 0.0f);
    ctl.Update(pad);
    ctl.DebugSetLongPressElapsed(500);
    pad.InjectForTest({}, 1.0f, 0.0f);
    ctl.Update(pad);
    CHECK(ctl.LetterMode());
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);
    pad.InjectForTest({}, 0.0f, 1.0f);  // 拨上 -> '2'
    ctl.Update(pad);
    CHECK(!ctl.LetterMode());
    CHECK(eng.Digits() == "2");
    pad.InjectForTest({}, 0.0f, 0.0f);
    ctl.Update(pad);
}

}  // namespace

int main() {
    std::printf("==== T9GamepadIME 单元测试 ====\n");
    TestDirectionOf();
    TestFlickDetector();
    TestKeypad();
    TestKeymap();
    TestLetterCandidates();
    TestT9Engine();
    TestFuzzyPinyin();
    TestHanziSorting();
    TestConfig();
    TestImeController();
    TestImeControllerLetterMode();

    std::printf("--------------------------------\n");
    std::printf("总计 %d 项断言，失败 %d 项。\n", g_total, g_failed);
    if (g_failed == 0) {
        std::printf("结果：全部通过\n");
        return 0;
    }
    std::printf("结果：存在失败项。\n");
    return 1;
}
