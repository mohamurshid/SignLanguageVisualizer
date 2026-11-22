// Sign Language Gesture Visualizer - 3D application for Windows.
//
// Needs nothing installed: only Windows' own window functions (user32,
// gdi32) and the OpenGL that comes with Windows (opengl32). This file:
//   1. opens a window and creates an OpenGL context for it (WGL),
//   2. turns two Windows fonts into OpenGL bitmap text (wglUseFontBitmaps),
//   3. passes keys, mouse and time to StudioCore, which does everything else.
//
//   bin\sign_studio.exe              the interactive program
//   bin\sign_studio.exe --capture    renders the documentation images into the output_final folder

#include <cstdio>
#include <cstring>
#include <string>

#include "StudioCapture.h"
#include "StudioCore.h"

// Windows headers last, so their macros cannot affect our own headers.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>

namespace {

StudioCore* gCore = nullptr;
HDC gDC = nullptr;
int gW = 1100, gH = 760;

// Two fonts as OpenGL display lists: list base + character code draws it.
struct GLFont {
    GLuint base = 0;
    int widths[256] = {};
};
GLFont gFonts[2];

void makeFont(GLFont& f, int height, int weight, const char* face) {
    HFONT font = CreateFontA(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face);
    HGDIOBJ old = SelectObject(gDC, font);
    f.base = glGenLists(256);
    wglUseFontBitmapsA(gDC, 0, 256, f.base);
    for (int c = 0; c < 256; ++c) {
        INT w = 0;
        GetCharWidth32A(gDC, static_cast<UINT>(c), static_cast<UINT>(c), &w);
        f.widths[c] = w;
    }
    SelectObject(gDC, old);
    DeleteObject(font);
}

void drawText(const OverlayText& t, int size) {
    const GLFont& f = gFonts[size ? 1 : 0];
    glColor3f(t.r, t.g, t.b);
    glRasterPos2i(t.x, t.y);
    glListBase(f.base);
    glCallLists(static_cast<GLsizei>(t.text.size()), GL_UNSIGNED_BYTE, t.text.data());
}

int measureText(const std::string& s, int size) {
    int w = 0;
    for (unsigned char c : s) w += gFonts[size ? 1 : 0].widths[c];
    return w;
}

double nowSeconds() { return static_cast<double>(GetTickCount64()) / 1000.0; }

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            gW = LOWORD(lParam) > 0 ? LOWORD(lParam) : 1;
            gH = HIWORD(lParam) > 0 ? HIWORD(lParam) : 1;
            if (gCore) gCore->resize(gW, gH);
            return 0;
        case WM_ERASEBKGND: return 1;  // OpenGL paints every pixel
        case WM_CHAR: gCore->onChar(static_cast<char>(wParam)); return 0;
        case WM_KEYDOWN:
            if (wParam == VK_LEFT) gCore->onKey(StudioKey::Left);
            if (wParam == VK_RIGHT) gCore->onKey(StudioKey::Right);
            if (wParam == VK_UP) gCore->onKey(StudioKey::Up);
            if (wParam == VK_DOWN) gCore->onKey(StudioKey::Down);
            return 0;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            SetCapture(hwnd);  // keep receiving the mouse while dragging
            gCore->onMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), msg == WM_RBUTTONDOWN);
            return 0;
        case WM_MOUSEMOVE: gCore->onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            ReleaseCapture();
            gCore->onMouseUp();
            return 0;
        case WM_MOUSEWHEEL: gCore->onWheel(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA); return 0;
        case WM_CLOSE: DestroyWindow(hwnd); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const bool capture = argc > 1 && std::strcmp(argv[1], "--capture") == 0;
    if (capture) gW = 960, gH = 720;

    StudioCore core;
    gCore = &core;

    HINSTANCE instance = GetModuleHandleA(nullptr);
    WNDCLASSA wc{};
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;  // OpenGL needs its own device context
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "SignStudioWindow";
    if (!RegisterClassA(&wc)) return 1;

    RECT r{0, 0, gW, gH};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, StudioCore::windowTitle(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 1;
    gDC = GetDC(hwnd);

    // An OpenGL pixel format: 24-bit colour, 24-bit depth buffer, double buffering.
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    const int format = ChoosePixelFormat(gDC, &pfd);
    if (!format || !SetPixelFormat(gDC, format, &pfd)) {
        std::printf("Could not set an OpenGL pixel format.\n");
        return 1;
    }
    HGLRC context = wglCreateContext(gDC);
    if (!context || !wglMakeCurrent(gDC, context)) {
        std::printf("Could not create an OpenGL context.\n");
        return 1;
    }
    std::printf("Sign Language Gesture Visualizer - 3D (OpenGL %s, %s)\n",
                reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    makeFont(gFonts[0], 16, FW_NORMAL, "Consolas");
    makeFont(gFonts[1], 26, FW_SEMIBOLD, "Segoe UI");
    core.setText(drawText, measureText);
    core.resize(gW, gH);

    if (capture) {
        ShowWindow(hwnd, SW_SHOW);
        const int n = runCapture(core, "output_final", gW, gH);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(context);
        return n > 0 ? 0 : 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    std::printf("Click the window, then press H for help. Enter = type a sentence, B = build mode.\n");

    MSG msg;
    bool running = true;
    while (running) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!running) break;
        if (core.wantsQuit()) {
            DestroyWindow(hwnd);
            continue;
        }
        core.update(nowSeconds());
        core.render();
        SwapBuffers(gDC);
        Sleep(10);  // about 60-90 frames a second without using a whole CPU core
    }
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(context);
    return 0;
}
