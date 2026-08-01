#include "app/config.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace app {

namespace {

// 去除首尾空白
std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// 大小写无关比较
bool IEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    }
    return true;
}

// 解析浮点数并校验：仅当整串为合法数值时才返回 true 并赋值。
bool ParseFloatStrict(const std::string& s, float* out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    double v = std::strtod(s.c_str(), &end);
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    *out = static_cast<float>(v);
    return true;
}

// 解析整数并校验：仅当整串为合法整数时才返回 true 并赋值。
bool ParseIntStrict(const std::string& s, int* out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(s.c_str(), &end, 10);
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    *out = static_cast<int>(v);
    return true;
}

struct NameMap {
    const char* name;
    gamepad::Button button;
};

const NameMap kNames[] = {
    {"A", gamepad::Button::kA},
    {"B", gamepad::Button::kB},
    {"X", gamepad::Button::kX},
    {"Y", gamepad::Button::kY},
    {"DpadUp", gamepad::Button::kDpadUp},
    {"DpadDown", gamepad::Button::kDpadDown},
    {"DpadLeft", gamepad::Button::kDpadLeft},
    {"DpadRight", gamepad::Button::kDpadRight},
    {"LB", gamepad::Button::kLB},
    {"RB", gamepad::Button::kRB},
    {"Start", gamepad::Button::kStart},
    {"Back", gamepad::Button::kBack},
};

// 解析形如 "Back+Start" 的组合键
std::vector<gamepad::Button> ParseCombo(const std::string& value) {
    std::vector<gamepad::Button> combo;
    std::stringstream ss(value);
    std::string part;
    while (std::getline(ss, part, '+')) {
        gamepad::Button b;
        if (ParseButton(Trim(part), &b)) combo.push_back(b);
    }
    return combo;
}

}  // namespace

const char* ButtonName(gamepad::Button b) {
    for (const auto& n : kNames) {
        if (n.button == b) return n.name;
    }
    return "?";
}

bool ParseButton(const std::string& name, gamepad::Button* out) {
    for (const auto& n : kNames) {
        if (IEquals(name, n.name)) {
            if (out) *out = n.button;
            return true;
        }
    }
    return false;
}

std::string DescribeCombo(const std::vector<gamepad::Button>& combo) {
    std::string s;
    for (size_t i = 0; i < combo.size(); ++i) {
        if (i) s += "+";
        s += ButtonName(combo[i]);
    }
    return s.empty() ? "(none)" : s;
}

bool Config::LoadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));

        if (IEquals(key, "toggle_hotkey")) {
            auto combo = ParseCombo(value);
            if (!combo.empty()) toggle_combo = combo;
        } else if (IEquals(key, "stick_activate")) {
            float v;
            if (ParseFloatStrict(value, &v)) {
                stick_activate = v;
                if (stick_activate < 0.0f) stick_activate = 0.0f;
                if (stick_activate > 1.0f) stick_activate = 1.0f;
            }
        } else if (IEquals(key, "stick_release")) {
            float v;
            if (ParseFloatStrict(value, &v)) {
                stick_release = v;
                if (stick_release < 0.0f) stick_release = 0.0f;
                if (stick_release > 1.0f) stick_release = 1.0f;
            }
        } else if (IEquals(key, "long_press_ms")) {
            int v;
            if (ParseIntStrict(value, &v) && v >= 0) long_press_ms = v;
        } else if (IEquals(key, "candidate_page")) {
            int v;
            if (ParseIntStrict(value, &v) && v >= 1) candidate_page = v;
        } else if (IEquals(key, "start_enabled")) {
            start_enabled = IEquals(value, "true") || value == "1";
        } else if (IEquals(key, "overlay_opacity")) {
            float v;
            if (ParseFloatStrict(value, &v)) {
                overlay_opacity = v;
                if (overlay_opacity < 0.0f) overlay_opacity = 0.0f;
                if (overlay_opacity > 1.0f) overlay_opacity = 1.0f;
            }
        }
    }
    return true;
}

}  // namespace app
