#pragma once
// ROG Ally 硬件级手柄模式切换
//
// 通过 HID Feature Report 与 ROG Ally 的 MCU 配置接口通信，
// 在游戏手柄模式（XInput）和鼠标模式（桌面操控）之间切换。
//
// HID 协议（逆向自 ASUS Armoury Crate）：
//   Report ID:  0x5A
//   Code Page:  0xD1
//   命令格式:   [0x5A, 0xD1, 0x01, 0x01, mode]
//   mode 值:    0x01=游戏手柄, 0x02=WASD, 0x03=鼠标
//
// 当设备不是 ROG Ally 时，IsSupported() 返回 false，
// 上层应回退到软件级 DesktopController 进行鼠标/键盘模拟。
//
// 参考: Linux kernel hid-asus-ally 驱动、NeroReflex 逆向工程文档

#include <windows.h>
#include <cstddef>

namespace app {

class AllyHidController {
public:
    AllyHidController();
    ~AllyHidController();

    AllyHidController(const AllyHidController&) = delete;
    AllyHidController& operator=(const AllyHidController&) = delete;

    // 检测是否已成功打开 ROG Ally 配置接口
    bool IsSupported() const;

    // 切换到游戏手柄模式（XInput）
    bool SetGamepadMode();

    // 切换到鼠标模式（桌面操控）
    bool SetMouseMode();

    // 获取最后一次诊断信息
    const char* LastError() const;

private:
    // 枚举 HID 设备，查找 ROG Ally 配置接口
    bool FindDevice();

    // 发送 Feature Report
    bool SendFeatureReport(const unsigned char* report, size_t size);

    HANDLE hid_device_ = INVALID_HANDLE_VALUE;
    ULONG  report_size_ = 0;  // 从 HID CAPS 获取的实际 Feature Report 大小

    // ASUS Vendor ID
    static constexpr unsigned short kAsusVid = 0x0B05;

    // ROG Ally HID 协议常量
    static constexpr unsigned char kReportId   = 0x5A;
    static constexpr unsigned char kCodePage   = 0xD1;
    static constexpr unsigned char kCmdSetMode = 0x01;

    // 模式值
    static constexpr unsigned char kModeGamepad = 0x01;
    static constexpr unsigned char kModeMouse   = 0x03;

    // 诊断信息
    char last_error_[256] = {};
};

}  // namespace app
