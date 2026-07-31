#include "app/ally_hid_controller.h"

#include <hidsdi.h>
#include <setupapi.h>
#include <vector>
#include <cstring>
#include <cstdio>
#include <string>

namespace app {

// 已知的 ROG Ally Product ID
//   0x1ABE = ROG Ally (初代)
//   0x1B4C = ROG Ally X
static const unsigned short kKnownPids[] = {0x1ABE, 0x1B4C};
static const int kNumPids = sizeof(kKnownPids) / sizeof(kKnownPids[0]);

// 最小 Feature Report 长度（ROG Ally 配置接口特征）
static constexpr ULONG kMinFeatureReportLen = 64;

// 将诊断信息同时输出到 OutputDebugStringA 和日志文件
static void WriteHidLog(const char* msg) {
    OutputDebugStringA(msg);
    char path[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    std::string p(path, n);
    size_t slash = p.find_last_of("\\/");
    if (slash == std::string::npos) return;
    p = p.substr(0, slash + 1) + "t9ime.log";
    FILE* f = std::fopen(p.c_str(), "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        std::fprintf(f, "[%02d:%02d:%02d.%03d] %s",
                     st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
        std::fclose(f);
    }
}

AllyHidController::AllyHidController() {
    FindDevice();
}

AllyHidController::~AllyHidController() {
    if (hid_device_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hid_device_);
        hid_device_ = INVALID_HANDLE_VALUE;
    }
}

bool AllyHidController::IsSupported() const {
    return hid_device_ != INVALID_HANDLE_VALUE;
}

const char* AllyHidController::LastError() const {
    return last_error_;
}

bool AllyHidController::SetGamepadMode() {
    if (hid_device_ == INVALID_HANDLE_VALUE) {
        std::snprintf(last_error_, sizeof(last_error_), "设备未打开");
        return false;
    }
    std::vector<unsigned char> report(report_size_, 0);
    report[0] = kReportId;      // 0x5A
    report[1] = kCodePage;      // 0xD1
    report[2] = kCmdSetMode;    // 0x01 (SetGamepadMode)
    report[3] = 0x01;           // 数据长度: 1 字节
    report[4] = kModeGamepad;   // 0x01 = 游戏手柄模式

    bool ok = SendFeatureReport(report.data(), report.size());
    char dbg[256];
    if (ok) {
        std::snprintf(dbg, sizeof(dbg),
            "[AllyHID] SetGamepadMode 成功 (report_size=%lu)\n", report_size_);
        WriteHidLog(dbg);
    } else {
        DWORD err = GetLastError();
        std::snprintf(last_error_, sizeof(last_error_),
                      "SetGamepadMode 失败, GLE=%lu", err);
        std::snprintf(dbg, sizeof(dbg),
            "[AllyHID] SetGamepadMode 失败, GLE=%lu, report_size=%lu\n",
            err, report_size_);
        WriteHidLog(dbg);
    }
    return ok;
}

bool AllyHidController::SetMouseMode() {
    if (hid_device_ == INVALID_HANDLE_VALUE) {
        std::snprintf(last_error_, sizeof(last_error_), "设备未打开");
        return false;
    }
    std::vector<unsigned char> report(report_size_, 0);
    report[0] = kReportId;      // 0x5A
    report[1] = kCodePage;      // 0xD1
    report[2] = kCmdSetMode;    // 0x01 (SetGamepadMode)
    report[3] = 0x01;           // 数据长度: 1 字节
    report[4] = kModeMouse;     // 0x03 = 鼠标模式

    bool ok = SendFeatureReport(report.data(), report.size());
    char dbg[256];
    if (ok) {
        std::snprintf(dbg, sizeof(dbg),
            "[AllyHID] SetMouseMode 成功 (report_size=%lu)\n", report_size_);
        WriteHidLog(dbg);
    } else {
        DWORD err = GetLastError();
        std::snprintf(last_error_, sizeof(last_error_),
                      "SetMouseMode 失败, GLE=%lu", err);
        std::snprintf(dbg, sizeof(dbg),
            "[AllyHID] SetMouseMode 失败, GLE=%lu, report_size=%lu\n",
            err, report_size_);
        WriteHidLog(dbg);
    }
    return ok;
}

bool AllyHidController::FindDevice() {
    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);

    HDEVINFO dev_info = SetupDiGetClassDevs(&hid_guid, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (dev_info == INVALID_HANDLE_VALUE) {
        std::snprintf(last_error_, sizeof(last_error_), "SetupDiGetClassDevs 失败");
        return false;
    }

    SP_DEVICE_INTERFACE_DATA iface_data = {};
    iface_data.cbSize = sizeof(iface_data);

    int ally_iface_count = 0;
    // 记录所有候选接口，用于回退
    HANDLE fallback_handle = INVALID_HANDLE_VALUE;
    ULONG  fallback_report_size = 0;

    for (DWORD idx = 0; SetupDiEnumDeviceInterfaces(dev_info, nullptr,
         &hid_guid, idx, &iface_data); ++idx) {
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailA(dev_info, &iface_data,
            nullptr, 0, &required, nullptr);
        if (required == 0) continue;

        std::vector<char> buf(required);
        auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_A>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (!SetupDiGetDeviceInterfaceDetailA(dev_info, &iface_data,
            detail, required, nullptr, nullptr)) {
            continue;
        }

        HANDLE handle = CreateFileA(detail->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            handle = CreateFileA(detail->DevicePath,
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, 0, nullptr);
            if (handle == INVALID_HANDLE_VALUE) continue;
        }

        HIDD_ATTRIBUTES attrs = {};
        attrs.Size = sizeof(attrs);
        if (!HidD_GetAttributes(handle, &attrs) || attrs.VendorID != kAsusVid) {
            CloseHandle(handle);
            continue;
        }

        bool pid_match = false;
        for (int j = 0; j < kNumPids; ++j) {
            if (attrs.ProductID == kKnownPids[j]) {
                pid_match = true;
                break;
            }
        }
        if (!pid_match) {
            CloseHandle(handle);
            continue;
        }

        ally_iface_count++;
        char dbg[512];

        // 获取 preparsed data 和 caps
        PHIDP_PREPARSED_DATA preparsed = nullptr;
        if (!HidD_GetPreparsedData(handle, &preparsed)) {
            CloseHandle(handle);
            continue;
        }

        HIDP_CAPS caps = {};
        NTSTATUS status = HidP_GetCaps(preparsed, &caps);
        HidD_FreePreparsedData(preparsed);

        if (status != HIDP_STATUS_SUCCESS) {
            CloseHandle(handle);
            continue;
        }

        // 记录接口详细信息
        std::snprintf(dbg, sizeof(dbg),
            "[AllyHID] 接口 #%d: PID=0x%04X, UsagePage=0x%04hX, Usage=0x%04hX, "
            "FeatureReportByteLength=%hu, InputReportByteLength=%hu, "
            "OutputReportByteLength=%hu, NumberFeatureValueCaps=%hu\n",
            ally_iface_count, attrs.ProductID,
            caps.UsagePage, caps.Usage,
            caps.FeatureReportByteLength,
            caps.InputReportByteLength,
            caps.OutputReportByteLength,
            caps.NumberFeatureValueCaps);
        WriteHidLog(dbg);

        // ROG Ally 配置接口的特征：有 Feature Report 且大小 >= 64
        if (caps.FeatureReportByteLength < kMinFeatureReportLen) {
            std::snprintf(dbg, sizeof(dbg),
                "[AllyHID] 接口 #%d: FeatureReport 太小, 跳过\n",
                ally_iface_count);
            WriteHidLog(dbg);
            CloseHandle(handle);
            continue;
        }

        ULONG feat_size = caps.FeatureReportByteLength;
        std::vector<unsigned char> probe(feat_size, 0);
        probe[0] = kReportId;  // Report ID 0x5A

        if (HidD_GetFeature(handle, probe.data(), feat_size)) {
            // GetFeature 成功，确认是配置接口
            hid_device_ = handle;
            report_size_ = feat_size;

            // 记录返回的数据前几个字节
            std::snprintf(dbg, sizeof(dbg),
                "[AllyHID] 接口 #%d: GetFeature(0x5A) 成功, "
                "返回数据: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                ally_iface_count,
                probe[0], probe[1], probe[2], probe[3],
                probe[4], probe[5], probe[6], probe[7]);
            WriteHidLog(dbg);

            // 如果之前有回退候选，关闭它
            if (fallback_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(fallback_handle);
                fallback_handle = INVALID_HANDLE_VALUE;
            }
            SetupDiDestroyDeviceInfoList(dev_info);
            WriteHidLog("[AllyHID] 已连接 ROG Ally 配置接口 (GetFeature 验证通过)\n");
            return true;
        } else {
            DWORD err = GetLastError();
            std::snprintf(dbg, sizeof(dbg),
                "[AllyHID] 接口 #%d: GetFeature(0x5A) 失败, GLE=%lu\n",
                ally_iface_count, err);
            WriteHidLog(dbg);

            // 作为回退候选（仅当没有更好的候选时使用）
            if (fallback_handle == INVALID_HANDLE_VALUE &&
                caps.NumberFeatureValueCaps > 0) {
                fallback_handle = handle;
                fallback_report_size = feat_size;
                WriteHidLog("[AllyHID] 此接口作为回退候选\n");
            } else {
                CloseHandle(handle);
            }
        }
    }

    // 如果没有通过 GetFeature 验证的接口，使用回退候选
    if (fallback_handle != INVALID_HANDLE_VALUE) {
        hid_device_ = fallback_handle;
        report_size_ = fallback_report_size;
        SetupDiDestroyDeviceInfoList(dev_info);
        WriteHidLog("[AllyHID] 使用回退候选接口 (NumberFeatureValueCaps > 0)\n");
        return true;
    }

    SetupDiDestroyDeviceInfoList(dev_info);

    if (ally_iface_count == 0) {
        std::snprintf(last_error_, sizeof(last_error_),
            "未找到 ROG Ally HID 设备");
    } else {
        std::snprintf(last_error_, sizeof(last_error_),
            "找到 %d 个 ASUS 接口但无配置接口", ally_iface_count);
    }
    WriteHidLog(last_error_);
    WriteHidLog("\n");
    return false;
}

bool AllyHidController::SendFeatureReport(const unsigned char* report, size_t size) {
    if (hid_device_ == INVALID_HANDLE_VALUE || report_size_ == 0) return false;

    std::vector<unsigned char> buf(report_size_, 0);
    size_t copy_len = (size < report_size_) ? size : report_size_;
    memcpy(buf.data(), report, copy_len);

    return HidD_SetFeature(hid_device_, buf.data(), report_size_);
}

}  // namespace app
