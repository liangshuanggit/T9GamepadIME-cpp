// Windows 兼容 shim：替代 POSIX <sys/time.h>（提供 struct timeval 与 gettimeofday）。
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN  // 避免拉入 winsock.h 的不完整 struct timeval
#endif
#include <windows.h>
#include <time.h>

#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
struct timeval {
    long tv_sec;
    long tv_usec;
};
#endif

// 基于 GetSystemTimeAsFileTime 实现，精度足够 userdict 的时间戳用途
static inline int gettimeofday(struct timeval* tv, void* /*tz*/) {
    if (!tv) return -1;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    // FILETIME 以 100ns 为单位，起点 1601-01-01；转为自 1970 的微秒
    t -= 116444736000000000ULL;  // 1601->1970 的 100ns 数
    t /= 10ULL;                  // 转微秒
    tv->tv_sec  = (long)(t / 1000000ULL);
    tv->tv_usec = (long)(t % 1000000ULL);
    return 0;
}
