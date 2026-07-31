// Windows 兼容 shim：替代 Android 的 <cutils/log.h>
// libgooglepinyin 仅使用 LOGD 宏做调试打印，这里统一实现为可开关的空操作。
#pragma once

#include <stdio.h>

#ifndef T9IME_ENABLE_GP_LOG
#define T9IME_ENABLE_GP_LOG 0
#endif

#if T9IME_ENABLE_GP_LOG
#define LOGD(...) do { fprintf(stderr, "[gp] " __VA_ARGS__); fprintf(stderr, "\n"); } while (0)
#else
#define LOGD(...) do { } while (0)
#endif

// 兼容其它可能出现的 Android 日志宏
#ifndef LOGE
#define LOGE(...) LOGD(__VA_ARGS__)
#endif
#ifndef LOGI
#define LOGI(...) LOGD(__VA_ARGS__)
#endif
#ifndef LOGW
#define LOGW(...) LOGD(__VA_ARGS__)
#endif
#ifndef LOGV
#define LOGV(...) LOGD(__VA_ARGS__)
#endif
