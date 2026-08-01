#include "app/log.h"

#include <cstdio>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace app {

void Log::Write(const char* msg) {
    if (!msg) return;
#if defined(_WIN32)
    OutputDebugStringA(msg);
    char buf[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    std::string d(buf, n);
    size_t slash = d.find_last_of("\\/");
    if (slash == std::string::npos) return;
    d = d.substr(0, slash + 1);
    FILE* f = std::fopen((d + "t9ime.log").c_str(), "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        std::fprintf(f, "[%02d:%02d:%02d.%03d] %s",
                     st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
        std::fclose(f);
    }
#else
    (void)msg;
#endif
}

}  // namespace app
