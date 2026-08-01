// 临时 UI 预览工具：创建 Overlay，填充示例状态，截图保存为 BMP。
// 仅用于验证新布局渲染效果，用完即删。
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#include <vector>

#include "ui/overlay.h"
#include "gamepad/stick.h"

static bool SaveBmp(const wchar_t* path, HDC src, int w, int h) {
    HDC mdc = CreateCompatibleDC(src);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(mdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) { if (bmp) DeleteObject(bmp); DeleteDC(mdc); return false; }
    HGDIOBJ old = SelectObject(mdc, bmp);
    BitBlt(mdc, 0, 0, w, h, src, 0, 0, SRCCOPY);

    BITMAPFILEHEADER bfh = {};
    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + w * h * 4;

    FILE* f = nullptr;
    _wfopen_s(&f, path, L"wb");
    if (!f) { SelectObject(mdc, old); DeleteObject(bmp); DeleteDC(mdc); return false; }
    fwrite(&bfh, 1, sizeof(bfh), f);
    fwrite(&bi.bmiHeader, 1, sizeof(BITMAPINFOHEADER), f);
    fwrite(bits, 1, w * h * 4, f);
    fclose(f);

    SelectObject(mdc, old);
    DeleteObject(bmp);
    DeleteDC(mdc);
    return true;
}

int main() {
    ui::Overlay ov;
    if (!ov.Create(1.0f)) { printf("overlay create failed\n"); return 1; }

    ui::OverlayState s;
    s.enabled = true;
    s.pad_connected = true;
    s.pointing = gamepad::Direction::kRight;  // 模拟指向右（6 高亮）
    s.letter_mode = false;
    s.digits = "64";
    s.pinyin = {"ni", "mi"};
    s.candidates = {"你", "米", "迷", "密", "蜜", "秘", "谜", "弥"};
    s.selected = 2;
    s.page_start = 0;
    s.page_size = 8;
    s.last_committed = "你好";
    s.hotkey_desc = "Start";
    s.edit_highlight = 'X';  // 模拟 LB+X 高亮
    ov.SetVisible(true);
    ov.Refresh(s);
    Sleep(400);

    HWND h = FindWindowW(L"T9GamepadOverlay", nullptr);
    if (!h) { printf("window not found\n"); return 2; }
    RECT rc;
    GetWindowRect(h, &rc);
    int w = rc.right - rc.left;
    int hh = rc.bottom - rc.top;
    HDC wdc = GetWindowDC(h);
    bool ok = SaveBmp(L"build_preview/ui_preview.bmp", wdc, w, hh);
    ReleaseDC(h, wdc);
    printf("saved ui_preview.bmp %dx%d %s\n", w, hh, ok ? "OK" : "FAIL");

    // 第二张：关闭态（灰显）
    s.enabled = false;
    s.pad_connected = false;
    s.digits.clear();
    s.pinyin.clear();
    s.candidates = {};
    s.selected = 0;
    s.last_committed.clear();
    s.edit_highlight = 0;
    ov.Refresh(s);
    Sleep(400);
    h = FindWindowW(L"T9GamepadOverlay", nullptr);
    if (h) {
        wdc = GetWindowDC(h);
        ok = SaveBmp(L"build_preview/ui_preview_off.bmp", wdc, w, hh);
        ReleaseDC(h, wdc);
        printf("saved ui_preview_off.bmp %s\n", ok ? "OK" : "FAIL");
    }

    ov.SetVisible(false);
    ov.Destroy();
    return 0;
}
