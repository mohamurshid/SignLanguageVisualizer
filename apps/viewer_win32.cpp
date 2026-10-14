// Sign Language Gesture Visualizer - interactive viewer for Windows.
//
// No graphics library: only the window functions built into Windows
// (user32 + gdi32). Windows is used for three things only:
//   1. opening a window and receiving key presses,
//   2. copying our finished framebuffer to the window (StretchDIBits),
//   3. drawing the text labels (TextOut).
// Every shape is drawn by our own algorithms in src/, exactly as in
// cp1_render.
//
// Controls
//   1-8      choose a sign (the hand moves smoothly to it)
//   P        play through all signs automatically
//   D        switch line algorithm: DDA <-> Bresenham
//   F        switch filled (scan-line fill) <-> wireframe (outlines only)
//   S        switch view: skin + skeleton / skin only / skeleton only
//   X        palm details on/off (Bezier creases, ellipse pads and nails)
//   C        detail view on/off (clipping + window-to-viewport)
//   M        motion trails on/off (spline paths of the last movement)
//   T        algorithm pages: lines, circles/ellipses, polygons, clipping, curves
//   + / -    make pixels bigger / smaller (to see the algorithms' steps)
//   Esc      quit

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Framebuffer.h"
#include "Hand.h"
#include "HandRenderer.h"
#include "Poses.h"
#include "TestScene.h"

// Windows headers last, so their macros cannot affect our own headers.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

// ---- Window and framebuffer --------------------------------------------------
int gClientW = 960;
int gClientH = 720;
const int kPixelSizes[] = {1, 2, 3, 4, 6, 8, 12};
const int kPixelSizeCount = sizeof(kPixelSizes) / sizeof(kPixelSizes[0]);
int gPixelSizeIndex = 1;  // start at 2x
Framebuffer gFramebuffer;
std::vector<std::uint32_t> gBgra;  // the framebuffer converted for Windows

int pixelSize() { return kPixelSizes[gPixelSizeIndex]; }

// ---- Scene state ---------------------------------------------------------------
Hand gHand;
RenderSettings gSettings;
int gPage = -1;     // -1 = the hand, 0..4 = algorithm pages
int gViewMode = 0;  // 0 = skin + skeleton, 1 = skin only, 2 = skeleton only
bool gDetailView = false;
bool gMotionTrails = false;

// ---- Pose animation ------------------------------------------------------------
HandPose gFromPose;
HandPose gToPose;
int gTargetIndex = 0;
double gAnimStartMs = -1.0;  // -1 = not animating
const double kAnimDurationMs = 500.0;
bool gAutoPlay = false;
double gLastAutoMs = 0.0;
const double kAutoPlayIntervalMs = 1800.0;

double nowMs() { return static_cast<double>(GetTickCount64()); }
bool curvesPageShown() { return gPage == 4; }
double curveT() { return 0.5 + 0.45 * std::sin(nowMs() * 0.0012); }

void startTransition(int index) {
    const auto& library = signLibrary();
    gTargetIndex = index % static_cast<int>(library.size());
    gFromPose = gHand.pose();
    gToPose = library[gTargetIndex];
    gAnimStartMs = nowMs();
}

// Called about 60 times a second. Returns true if the picture changed.
bool update() {
    bool changed = false;
    const double now = nowMs();
    if (gAutoPlay && now - gLastAutoMs >= kAutoPlayIntervalMs) {
        gLastAutoMs = now;
        startTransition(gTargetIndex + 1);
    }
    if (gAnimStartMs >= 0.0) {
        const double t = (now - gAnimStartMs) / kAnimDurationMs;
        if (t >= 1.0) {
            gHand.setPose(gToPose);
            gAnimStartMs = -1.0;
        } else {
            gHand.setPose(lerpPose(gFromPose, gToPose, easeInOut(t)));
        }
        changed = true;
    }
    if (curvesPageShown()) changed = true;
    return changed;
}

// ---- Drawing ---------------------------------------------------------------------
void renderScene() {
    const int ps = pixelSize();
    const int fbW = (gClientW + ps - 1) / ps;
    const int fbH = (gClientH + ps - 1) / ps;
    if (gFramebuffer.width() != fbW || gFramebuffer.height() != fbH) gFramebuffer.resize(fbW, fbH);

    // Draw with OUR algorithms into OUR framebuffer.
    if (gPage >= 0) {
        drawAlgorithmPage(gFramebuffer, gPage, gSettings, curvesPageShown() ? curveT() : 0.5);
    } else {
        drawHandScene(gFramebuffer, gHand, gSettings, gMotionTrails ? &gFromPose : nullptr);
        if (gMotionTrails) drawMotionTrails(gFramebuffer, gFromPose, gToPose, gSettings);
        if (gDetailView) drawDetailView(gFramebuffer, gHand, gSettings);
    }

    // Convert RGB bytes to the 32-bit blue-green-red format Windows expects.
    // Both store the bottom row first, so the rows stay in the same order.
    gBgra.resize(static_cast<size_t>(fbW) * fbH);
    const std::uint8_t* src = gFramebuffer.data();
    for (size_t i = 0; i < gBgra.size(); ++i) {
        gBgra[i] = (static_cast<std::uint32_t>(src[i * 3]) << 16) | (static_cast<std::uint32_t>(src[i * 3 + 1]) << 8) |
                   static_cast<std::uint32_t>(src[i * 3 + 2]);
    }
}

// Text positions below are measured from the BOTTOM of the window, like our
// framebuffer; Windows measures from the top, so we convert here.
void text(HDC dc, int x, int yFromBottom, const std::string& s, Color c) {
    const int y = gClientH - yFromBottom - 15;
    SetTextColor(dc, RGB(0, 0, 0));
    TextOutA(dc, x + 1, y + 1, s.c_str(), static_cast<int>(s.size()));
    SetTextColor(dc, RGB(c.r, c.g, c.b));
    TextOutA(dc, x, y, s.c_str(), static_cast<int>(s.size()));
}

void centredText(HDC dc, int cx, int yFromBottom, const std::string& s, Color c) {
    SIZE size{};
    GetTextExtentPoint32A(dc, s.c_str(), static_cast<int>(s.size()), &size);
    int x = cx - size.cx / 2;
    if (x + size.cx > gClientW - 4) x = gClientW - 4 - size.cx;
    if (x < 4) x = 4;
    text(dc, x, yFromBottom, s, c);
}

const char* viewModeName() {
    switch (gViewMode) {
        case 1: return "skin";
        case 2: return "skeleton";
        default: return "skin + skeleton";
    }
}

void drawHud(HDC dc) {
    const Color dim{150, 165, 200};
    std::string status;
    if (gPage >= 0) {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "Page %d/%d: %s", gPage + 1, algorithmPageCount(), algorithmPageTitle(gPage));
        status = buf;
    } else {
        status = "Sign: " + gToPose.name + " - " + gToPose.description;
    }
    text(dc, 12, gClientH - 22, "Sign Language Gesture Visualizer  |  Checkpoint 1: technical foundation", palette::text);
    text(dc, 12, gClientH - 42, status, palette::root);

    char settings[220];
    std::snprintf(settings, sizeof(settings), "Lines: %s   Fill: %s   View: %s   Details: %s   Pixel size: %dx%s%s%s",
                  lineAlgoName(gSettings.lineAlgo), gSettings.filled ? "scan-line" : "wireframe", viewModeName(),
                  gSettings.showDetails ? "on" : "off", pixelSize(), gDetailView ? "   [detail view]" : "",
                  gMotionTrails ? "   [motion trails]" : "", gAutoPlay ? "   [autoplay]" : "");
    text(dc, 12, gClientH - 62, settings, dim);

    if (gPage >= 0) {
        for (const PageLabel& label : algorithmPageLabels(gPage)) {
            centredText(dc, static_cast<int>(label.fx * gClientW), static_cast<int>(label.fy * gClientH), label.text,
                        palette::text);
        }
    } else if (gDetailView) {
        const DetailView dv = detailViewFor(gFramebuffer, gHand);
        const int ps = pixelSize();
        text(dc, dv.viewport.xmin * ps, (dv.viewport.ymax + 1) * ps + 6, "Detail view (clipped)", palette::root);
    }
    text(dc, 12, 10, "1-8 sign  P play  D lines  F fill  S view  X details  C detail  M trails  T pages  +/- pixels  Esc",
         dim);
}

void paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC windowDc = BeginPaint(hwnd, &ps);

    // Draw into an off-screen bitmap first, then copy it in one go (no flicker).
    HDC dc = CreateCompatibleDC(windowDc);
    HBITMAP bmp = CreateCompatibleBitmap(windowDc, gClientW, gClientH);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    renderScene();
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = gFramebuffer.width();
    info.bmiHeader.biHeight = gFramebuffer.height();  // positive = bottom row first
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    const int scale = pixelSize();
    // Each of our pixels becomes a scale x scale block (nearest neighbour).
    // The framebuffer is rounded up to whole pixels, so it can be a little
    // taller than the window: keep its bottom row on the window's bottom edge,
    // like the framebuffer's (0, 0) in the bottom-left corner.
    const int destY = gClientH - gFramebuffer.height() * scale;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, 0, destY, gFramebuffer.width() * scale, gFramebuffer.height() * scale, 0, 0,
                  gFramebuffer.width(), gFramebuffer.height(), gBgra.data(), &info, DIB_RGB_COLORS, SRCCOPY);

    SetBkMode(dc, TRANSPARENT);
    HFONT font = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    HGDIOBJ oldFont = SelectObject(dc, font);
    drawHud(dc);
    SelectObject(dc, oldFont);
    DeleteObject(font);

    BitBlt(windowDc, 0, 0, gClientW, gClientH, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
    EndPaint(hwnd, &ps);
}

void applyViewMode() {
    gSettings.showSkin = gViewMode != 2;
    gSettings.showSkeleton = gViewMode != 1;
}

void onChar(HWND hwnd, char key) {
    const int signCount = static_cast<int>(signLibrary().size());
    if (key >= '1' && key <= '9' && key - '1' < signCount) {
        gPage = -1;
        gAutoPlay = false;
        startTransition(key - '1');
    }
    switch (key) {
        case 'p': case 'P':
            gAutoPlay = !gAutoPlay;
            gPage = -1;
            gLastAutoMs = nowMs() - kAutoPlayIntervalMs;
            break;
        case 'd': case 'D':
            gSettings.lineAlgo = gSettings.lineAlgo == LineAlgo::DDA ? LineAlgo::Bresenham : LineAlgo::DDA;
            break;
        case 'f': case 'F': gSettings.filled = !gSettings.filled; break;
        case 's': case 'S': gViewMode = (gViewMode + 1) % 3; applyViewMode(); break;
        case 'x': case 'X': gSettings.showDetails = !gSettings.showDetails; break;
        case 'c': case 'C': gDetailView = !gDetailView; gPage = -1; break;
        case 'm': case 'M': gMotionTrails = !gMotionTrails; gPage = -1; break;
        case '+': case '=': if (gPixelSizeIndex < kPixelSizeCount - 1) ++gPixelSizeIndex; break;
        case '-': case '_': if (gPixelSizeIndex > 0) --gPixelSizeIndex; break;
        case 't': case 'T':
            gPage = (gPage + 1 >= algorithmPageCount()) ? -1 : gPage + 1;
            gAutoPlay = false;
            break;
        default: break;
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: paint(hwnd); return 0;
        case WM_ERASEBKGND: return 1;  // we paint every pixel ourselves
        case WM_SIZE:
            gClientW = LOWORD(lParam) > 0 ? LOWORD(lParam) : 1;
            gClientH = HIWORD(lParam) > 0 ? HIWORD(lParam) : 1;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_TIMER:
            if (update()) InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_CHAR: onChar(hwnd, static_cast<char>(wParam)); return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(hwnd);
            if (wParam == VK_ADD) onChar(hwnd, '+');
            if (wParam == VK_SUBTRACT) onChar(hwnd, '-');
            return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

}  // namespace

int main() {
    gToPose = signLibrary()[0];
    gFromPose = gToPose;
    gHand.setPose(gToPose);

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
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Sign Language Gesture Visualizer - Checkpoint 1",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                                nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOW);
    SetTimer(hwnd, 1, 16, nullptr);  // about 60 updates per second

    std::printf("Sign Language Gesture Visualizer - Checkpoint 1 (interactive viewer)\n");
    std::printf("Click the window, then: 1-8 sign | P play | D lines | F fill | S view | X details | C detail view | "
                "M trails | T pages | +/- pixels | Esc\n");

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}
