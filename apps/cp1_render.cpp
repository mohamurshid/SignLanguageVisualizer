// cp1_render - Checkpoint 1 renderer, written in PURE C++.
//
// Uses only the C++ standard library: no OpenGL, no window, no image
// library. Every image is drawn pixel by pixel by our own algorithms in
// src/ and saved with our own .bmp writer (Framebuffer::saveBMP).
//
// Usage:   cp1_render [output_folder]        (default: output)
//
// Writes:
//   hand_1_5.bmp ... hand_9_I.bmp     the 9 ASL signs
//   hand_wireframe.bmp                outlines only: polygons, circles,
//                                     ellipses, spline control points
//   hand_skeleton.bmp                 joint hierarchy (lines + circles)
//   hand_pixels_8x.bmp                low resolution, each pixel enlarged 8x
//   hand_detail_view.bmp              clipping + window-to-viewport inset
//   hand_motion_trail.bmp             spline motion paths of the fingertips
//   page_1_lines.bmp ... page_5_curves.bmp      one page per algorithm group
//   page_*_zoom.bmp                   the same pages at 6x pixel size

#include <cstdio>
#include <filesystem>
#include <string>

#include "Framebuffer.h"
#include "Hand.h"
#include "HandRenderer.h"
#include "Poses.h"
#include "TestScene.h"

namespace {

int gWritten = 0;
int gFailed = 0;

// Every image is rendered at 480x360 and saved at 2x (960x720), which is
// exactly what the optional viewer shows at its default pixel size.
constexpr int kWidth = 480;
constexpr int kHeight = 360;
constexpr int kScale = 2;

void save(const Framebuffer& fb, const std::string& dir, const std::string& name, int scale, const char* what) {
    const std::string path = dir + "/" + name;
    if (fb.saveBMP(path, scale)) {
        ++gWritten;
        std::printf("  %-28s %s\n", name.c_str(), what);
    } else {
        ++gFailed;
        std::printf("  FAILED to write %s\n", path.c_str());
    }
}

const HandPose& sign(const char* name) {
    for (const HandPose& p : signLibrary()) {
        if (p.name == name) return p;
    }
    return signLibrary()[0];
}

}  // namespace

int main(int argc, char** argv) {
    const std::string outDir = argc > 1 ? argv[1] : "output";
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);

    std::printf("Sign Language Gesture Visualizer - Checkpoint 1 (pure C++)\n");
    std::printf("Writing images to: %s\n\n", outDir.c_str());

    Framebuffer fb(kWidth, kHeight);
    Hand hand;
    RenderSettings settings;

    // ---- The 9 signs --------------------------------------------------------
    std::printf("Hand signs (scan-line fill, Catmull-Rom palm, Bezier creases, ellipses):\n");
    int index = 1;
    for (const HandPose& pose : signLibrary()) {
        hand.setPose(pose);
        drawHandScene(fb, hand, settings);
        save(fb, outDir, "hand_" + std::to_string(index++) + "_" + pose.name + ".bmp", kScale,
             pose.description.c_str());
    }

    // ---- Views of the hand ----------------------------------------------------
    std::printf("\nViews of the hand:\n");
    hand.setPose(sign("5"));

    settings.filled = false;
    drawHandScene(fb, hand, settings);
    save(fb, outDir, "hand_wireframe.bmp", kScale, "outlines: polygons, circles, ellipses, spline control points");
    settings.filled = true;

    settings.showSkin = false;
    drawHandScene(fb, hand, settings);
    save(fb, outDir, "hand_skeleton.bmp", kScale, "joint hierarchy: Bresenham lines + midpoint circles");
    settings.showSkin = true;

    {
        Framebuffer coarse(kWidth / 4, kHeight / 4);
        hand.setPose(sign("ILY"));
        drawHandScene(coarse, hand, settings);
        save(coarse, outDir, "hand_pixels_8x.bmp", 8, "120x90 pixels shown 8x: the steps of each algorithm");
    }

    hand.setPose(sign("A"));
    drawHandScene(fb, hand, settings);
    drawDetailView(fb, hand, settings);
    save(fb, outDir, "hand_detail_view.bmp", kScale, "window-to-viewport inset, clipped (Cohen-Sutherland, Sutherland-Hodgman)");

    hand.setPose(sign("A"));
    drawHandScene(fb, hand, settings, &sign("5"));
    drawMotionTrails(fb, sign("5"), sign("A"), settings);
    save(fb, outDir, "hand_motion_trail.bmp", kScale, "sign 5 -> A: Catmull-Rom splines through 5 keyframes");

    // ---- Algorithm pages ------------------------------------------------------
    std::printf("\nAlgorithm pages:\n");
    for (int page = 0; page < algorithmPageCount(); ++page) {
        drawAlgorithmPage(fb, page, settings, 0.5);
        const std::string base = "page_" + std::to_string(page + 1) + "_" + algorithmPageFileName(page);
        save(fb, outDir, base + ".bmp", kScale, algorithmPageTitle(page));
    }
    std::printf("    DDA vs Bresenham: %d pixels differ on the starburst\n", lastLineDifferenceCount());
    drawAlgorithmPage(fb, 3, settings);
    const ClipStats cs = lastClipStats();
    std::printf("    Cohen-Sutherland: %d whole, %d clipped, %d rejected\n", cs.whole, cs.clipped, cs.rejected);
    std::printf("    Sutherland-Hodgman: %zu vertices in, %zu out\n", cs.polygonIn, cs.polygonOut);

    std::printf("\nZoomed pages (each pixel shown 6x):\n");
    {
        Framebuffer lowRes(kWidth / 3, kHeight / 3);
        for (int page : {0, 1, 4}) {
            drawAlgorithmPage(lowRes, page, settings, 0.5);
            const std::string base = "page_" + std::to_string(page + 1) + "_" + algorithmPageFileName(page);
            save(lowRes, outDir, base + "_zoom.bmp", 6, algorithmPageTitle(page));
        }
    }

    std::printf("\n%d images written", gWritten);
    if (gFailed) std::printf(", %d FAILED", gFailed);
    std::printf(". Open the .bmp files in any image viewer.\n");
    return gFailed == 0 ? 0 : 1;
}
