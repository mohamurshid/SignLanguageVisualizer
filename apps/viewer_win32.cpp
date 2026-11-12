// Sign Language Gesture Visualizer - interactive viewer for Windows.
//
// No graphics library: only the window functions built into Windows
// (user32 + gdi32). Windows is used for three things only:
//   1. opening a window and receiving keys, mouse and a timer,
//   2. copying our finished framebuffer to the window (StretchDIBits),
//   3. drawing the text labels (TextOut).
// Everything else - drawing, transformations, animation, Build mode - is in
// ViewerCore and src/, shared with the OpenGL viewer.
//
// Controls: see the two help lines at the bottom of the window.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "ViewerCore.h"

// Windows headers last, so their macros cannot affect our own headers.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

ViewerCore* gCore = nullptr;
int gClientW = 960;
int gClientH = 720;
std::vector<std::uint32_t> gBgra;  // the framebuffer converted for Windows

double nowSeconds() { return static_cast<double>(GetTickCount64()) / 1000.0; }

void drawText(HDC dc, const HudText& t) {
    SIZE size{};
    GetTextExtentPoint32A(dc, t.text.c_str(), static_cast<int>(t.text.size()), &size);
    int x = t.x;
    if (t.centred) {
        x = t.x - size.cx / 2;
        if (x + size.cx > gClientW - 4) x = gClientW - 4 - size.cx;
        if (x < 4) x = 4;
    }
    // Our positions are measured from the bottom; Windows measures from the top.
    const int y = gClientH - t.yFromBottom - 15;
    SetTextColor(dc, RGB(0, 0, 0));
    TextOutA(dc, x + 1, y + 1, t.text.c_str(), static_cast<int>(t.text.size()));
    SetTextColor(dc, RGB(t.color.r, t.color.g, t.color.b));
    TextOutA(dc, x, y, t.text.c_str(), static_cast<int>(t.text.size()));
}

void paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC windowDc = BeginPaint(hwnd, &ps);

    // Draw into an off-screen bitmap first, then copy it in one go (no flicker).
    HDC dc = CreateCompatibleDC(windowDc);
    HBITMAP bmp = CreateCompatibleBitmap(windowDc, gClientW, gClientH);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    gCore->render();
    const Framebuffer& fb = gCore->framebuffer();

    // Convert RGB bytes to the 32-bit blue-green-red format Windows expects.
    // Both store the bottom row first, so the rows stay in the same order.
    gBgra.resize(static_cast<size_t>(fb.width()) * fb.height());
    const std::uint8_t* src = fb.data();
    for (size_t i = 0; i < gBgra.size(); ++i) {
        gBgra[i] = (static_cast<std::uint32_t>(src[i * 3]) << 16) | (static_cast<std::uint32_t>(src[i * 3 + 1]) << 8) |
                   static_cast<std::uint32_t>(src[i * 3 + 2]);
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = fb.width();
    info.bmiHeader.biHeight = fb.height();  // positive = bottom row first
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    // Each of our pixels becomes a scale x scale block, and the bottom row
    // sits on the window's bottom edge (like the framebuffer's (0, 0)).
    const int scale = gCore->pixelSize();
    const int destY = gClientH - fb.height() * scale;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, 0, destY, fb.width() * scale, fb.height() * scale, 0, 0, fb.width(), fb.height(), gBgra.data(),
                  &info, DIB_RGB_COLORS, SRCCOPY);

    SetBkMode(dc, TRANSPARENT);
    HFONT font = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    HGDIOBJ oldFont = SelectObject(dc, font);
    for (const HudText& t : gCore->hud()) drawText(dc, t);
    SelectObject(dc, oldFont);
    DeleteObject(font);

    BitBlt(windowDc, 0, 0, gClientW, gClientH, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
    EndPaint(hwnd, &ps);
}

void redraw(HWND hwnd) {
    if (gCore->wantsQuit()) {
        DestroyWindow(hwnd);
        return;
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    const int mouseX = static_cast<short>(LOWORD(lParam));
    const int mouseY = static_cast<short>(HIWORD(lParam));
    switch (msg) {
        case WM_PAINT: paint(hwnd); return 0;
        case WM_ERASEBKGND: return 1;  // we paint every pixel ourselves
        case WM_SIZE:
            gClientW = LOWORD(lParam) > 0 ? LOWORD(lParam) : 1;
            gClientH = HIWORD(lParam) > 0 ? HIWORD(lParam) : 1;
            gCore->resize(gClientW, gClientH);
            redraw(hwnd);
            return 0;
        case WM_TIMER:
            if (gCore->update(nowSeconds())) redraw(hwnd);
            return 0;
        case WM_CHAR: gCore->onChar(static_cast<char>(wParam)); redraw(hwnd); return 0;
        case WM_KEYDOWN:
            if (wParam == VK_LEFT) gCore->onSpecialKey(SpecialKey::Left);
            if (wParam == VK_RIGHT) gCore->onSpecialKey(SpecialKey::Right);
            if (wParam == VK_UP) gCore->onSpecialKey(SpecialKey::Up);
            if (wParam == VK_DOWN) gCore->onSpecialKey(SpecialKey::Down);
            if (wParam == VK_ADD) gCore->onChar('+');
            if (wParam == VK_SUBTRACT) gCore->onChar('-');
            redraw(hwnd);
            return 0;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);  // keep receiving the mouse while dragging
            gCore->onMouseDown(mouseX, mouseY);
            redraw(hwnd);
            return 0;
        case WM_MOUSEMOVE:
            if (wParam & MK_LBUTTON) {
                gCore->onMouseMove(mouseX, mouseY);
                redraw(hwnd);
            }
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            gCore->onMouseUp();
            return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

}  // namespace

int main() {
    ViewerCore core;
    gCore = &core;

    HINSTANCE instance = GetModuleHandleA(nullptr);
    WNDCLASSA wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "SignVisualizerWindow";
    if (!RegisterClassA(&wc)) return 1;

    // Ask for a 960 x 720 drawing area; the border and title bar come on top.
    RECT r{0, 0, gClientW, gClientH};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, ViewerCore::windowTitle(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOW);
    SetTimer(hwnd, 1, 16, nullptr);  // about 60 updates per second

    std::printf("Sign Language Gesture Visualizer - Checkpoint 2 (interactive viewer)\n");
    std::printf("Click the window first. W = watch an animated sign, B = build mode (drag joints with the mouse).\n");

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}
