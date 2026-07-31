#pragma once
// 文本注入器：将候选词以 Unicode 字符事件注入到前台窗口的光标位置。
//
// 使用 SendInput + KEYEVENTF_UNICODE，无需经过键盘布局，
// 可直接输入中文、英文及任意 Unicode 字符。
// 适用于大多数接受键盘输入的 Windows 应用（记事本、浏览器输入框、游戏聊天框等）。

#include <string>

namespace app {

// 将 UTF-8 文本注入到当前前台窗口的光标位置。
// 返回是否成功（SendInput 返回值等于发送的事件数）。
bool InjectText(const std::string& utf8_text);

// 向前台窗口发送一次退格键（删除光标前一个字符）。
bool InjectBackspace();

}  // namespace app
