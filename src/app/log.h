#pragma once
// 统一日志工具：同时输出到 OutputDebugString 与 exe 所在目录的 t9ime.log。
// 供 main / desktop_controller / ally_hid_controller 等模块复用，
// 避免各处重复实现"取 exe 目录 + 打开文件 + 写时间戳"的逻辑。

#include <string>

namespace app {

class Log {
public:
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;

    // 写一条日志（msg 应以 '\n' 结尾或不带，函数内部统一补时间戳前缀）。
    static void Write(const char* msg);
    // 便捷重载：std::string
    static void Write(const std::string& msg) { Write(msg.c_str()); }

private:
    Log() = default;
};

}  // namespace app
