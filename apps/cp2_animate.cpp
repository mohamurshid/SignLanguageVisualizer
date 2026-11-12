// cp2_animate - Checkpoint 2 renderer: transformations and animation, in
// PURE C++.
//
// Uses only the C++ standard library. Every frame is drawn by our own
// algorithms, moved by our own matrices, in-betweened by our own keyframe
// code, and saved with our own BMP and GIF writers.
//
// Usage:   cp2_animate [output_folder]        (default: output_cp2)
//
// Writes:
//   transform_1_basics.bmp        translate, rotate (origin / pivot), scale, reflect, shear
//   transform_2_composite.bmp     matrix order, rotation about a pivot step by step, matrix stack
//   hand_frames.bmp               every joint's local coordinate frame (the matrix stack)
//   hand_placements.bmp           one sign moved, rotated at the wrist, scaled and mirrored
//   hand_left_right.bmp           a right hand and its reflection: a left hand
//   anim_N_<sign>.gif             each animated sign, 20 frames per second
//   anim_N_<sign>_frames.bmp      six moments of the animation side by side
//   anim_N_<sign>_path.bmp        start (dim), end, and the wrist's motion path

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>

#include "Animation.h"
#include "Canvas.h"
#include "Framebuffer.h"
#include "GifWriter.h"
#include "Hand.h"
#include "HandRenderer.h"
#include "Poses.h"
#include "TransformScene.h"

namespace {

constexpr int kWidth = 480;
constexpr int kHeight = 360;
constexpr int kScale = 2;   // BMPs are saved at 960 x 720
constexpr int kFps = 20;    // GIF frame rate

int gWritten = 0;
int gFailed = 0;

void report(bool ok, const std::string& name, const std::string& what) {
    if (ok) {
        ++gWritten;
        std::printf("  %-32s %s\n", name.c_str(), what.c_str());
    } else {
        ++gFailed;
        std::printf("  FAILED to write %s\n", name.c_str());
    }
}

void save(const Framebuffer& fb, const std::string& dir, const std::string& name, const std::string& what) {
    report(fb.saveBMP(dir + "/" + name, kScale), name, what);
}

std::string fileName(const std::string& sign) {
    std::string s;
    for (char c : sign) s += (c == ' ') ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// The view for one cell of a grid: window-to-viewport. The window is the
// area of hand units where the hand moves (x -1.4..1.4, y -0.6..2.4);
// it is scaled by the same k in x and y (so the hand keeps its shape) and
// centred in the cell:
//      cell view = T(cell centre) * S(k, k) * T(-window centre)
HandView cellView(const ClipRect& cell) {
    const double winW = 2.8, winH = 3.05;
    const Vec2 winCentre{0.0, 0.9};
    const double k = std::min(cell.width() / winW, cell.height() / winH);
    const Vec2 cellCentre{(cell.xmin + cell.xmax) / 2.0, (cell.ymin + cell.ymax) / 2.0};
    return {xf::translate(cellCentre.x, cellCentre.y) * xf::scale(k, k) * xf::translate(-winCentre.x, -winCentre.y), k};
}

ClipRect gridCell(int index, int cols, int rows) {
    const int w = kWidth / cols;
    const int h = kHeight / rows;
    const int col = index % cols;
    const int row = index / cols;
    const int x0 = col * w;
    const int y0 = (rows - 1 - row) * h;  // row 0 at the top
    return {x0 + 1, y0 + 1, x0 + w - 2, y0 + h - 2};
}

void drawInCell(Framebuffer& fb, const ClipRect& cell, const Hand& hand, const RenderSettings& s) {
    Canvas canvas(fb, cell, s.lineAlgo);
    canvas.fillClipRect(palette::background);
    drawHand(canvas, hand, cellView(cell), s);
    canvas.rectOutline(cell, Color{40, 52, 84});
}

}  // namespace

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : "output_cp2";
    std::error_code ec;
    std::filesystem::create_directories(out, ec);

    std::printf("Sign Language Gesture Visualizer - Checkpoint 2 (pure C++)\n");
    std::printf("Writing images and animations to: %s\n\n", out.c_str());

    Framebuffer fb(kWidth, kHeight);
    RenderSettings settings;

    // ---- Transformation pages ---------------------------------------------------
    std::printf("Transformations:\n");
    for (int page = 0; page < transformPageCount(); ++page) {
        drawTransformPage(fb, page, settings);
        save(fb, out, "transform_" + std::to_string(page + 1) + "_" + (page == 0 ? "basics" : "composite") + ".bmp",
             transformPageTitle(page));
    }

    // ---- The hand under transformations ------------------------------------------
    std::printf("\nThe hand and its matrices:\n");
    {
        Hand hand;
        hand.setPose(signByName("5"));
        RenderSettings s = settings;
        s.filled = false;
        s.showFrames = true;
        drawHandScene(fb, hand, s);
        save(fb, out, "hand_frames.bmp", "local frame of every joint (red = x, green = y)");
    }
    {
        fb.clear(palette::background);
        Hand hand;
        hand.setPose(signByName("L"));
        Placement place[4];
        place[1].wristAngle = 30;                  // rotate about the wrist
        place[2].position = {0.6, -0.15};          // translate ...
        place[2].scale = 0.7;                      // ... and scale
        place[3].mirror = true;                    // reflect: a left hand
        place[3].position = {0.2, 0.0};
        for (int i = 0; i < 4; ++i) {
            hand.setPlacement(place[i]);
            drawInCell(fb, gridCell(i, 2, 2), hand, settings);
        }
        save(fb, out, "hand_placements.bmp", "original / wrist rotated 30 / scaled 0.7 and moved / mirrored");
    }
    {
        fb.clear(palette::background);
        Canvas canvas(fb, fb.bounds(), settings.lineAlgo);
        const HandView view = HandView::fit(kWidth, kHeight);
        const double mirrorX = view.toPixels({0.0, 0.0}).x;
        canvas.dashedLine({mirrorX, 0.0}, {mirrorX, kHeight - 1.0}, palette::dim, 0x3333);
        Hand right;
        right.setPose(signByName("ILY"));
        Placement p;
        p.position = {0.72, 0.0};
        p.scale = 0.85;
        right.setPlacement(p);
        drawHand(canvas, right, view, settings);
        Hand left = right;
        // The left hand is the right hand reflected in the line x = 0:
        // reflecting T(0.72) * S gives T(-0.72) * S * reflectY.
        p.position = {-0.72, 0.0};
        p.mirror = true;
        left.setPlacement(p);
        drawHand(canvas, left, view, settings);
        save(fb, out, "hand_left_right.bmp", "right hand and its reflection (a left hand)");
    }

    // ---- Animations ---------------------------------------------------------------
    std::printf("\nAnimations (keyframes, eased interpolation, Catmull-Rom motion paths):\n");
    const auto& anims = animationLibrary();
    for (size_t a = 0; a < anims.size(); ++a) {
        const SignAnimation& anim = anims[a];
        const std::string base = "anim_" + std::to_string(a + 1) + "_" + fileName(anim.name);
        Hand hand;

        // 1. The animated GIF: one frame every 1/20 s of animation time.
        GifWriter gif;
        bool ok = gif.open(out + "/" + base + ".gif", kWidth, kHeight, 1);
        const int frames = static_cast<int>(anim.duration() * kFps) + 1;
        for (int i = 0; i < frames; ++i) {
            const AnimationFrame f = sampleAnimation(anim, static_cast<double>(i) / kFps);
            hand.setPose(f.pose);
            hand.setPlacement(f.placement);
            drawHandScene(fb, hand, settings);
            ok = gif.addFrame(fb, 100 / kFps) && ok;
        }
        // Hold the last frame briefly before the GIF loops.
        ok = gif.addFrame(fb, 60) && ok;
        ok = gif.close() && ok;
        report(ok, base + ".gif",
               anim.name + ": " + anim.description + " (" + std::to_string(frames) + " frames)");

        // 2. Six moments of the animation in a 3 x 2 grid.
        fb.clear(palette::background);
        for (int i = 0; i < 6; ++i) {
            const AnimationFrame f = sampleAnimation(anim, anim.duration() * i / 5.0);
            hand.setPose(f.pose);
            hand.setPlacement(f.placement);
            drawInCell(fb, gridCell(i, 3, 2), hand, settings);
        }
        save(fb, out, base + "_frames.bmp", "6 moments, from t = 0 to t = " + std::to_string(anim.duration()).substr(0, 4) + " s");

        // 3. Start and end of the movement with the wrist's motion path.
        const std::vector<Vec2> keys = keyframePositions(anim);
        if (keys.size() > 1) {
            Hand start;
            const AnimationFrame f0 = sampleAnimation(anim, 0.0);
            start.setPose(f0.pose);
            start.setPlacement(f0.placement);
            const AnimationFrame f1 = sampleAnimation(anim, anim.duration());
            hand.setPose(f1.pose);
            hand.setPlacement(f1.placement);
            drawHandSceneWithGhost(fb, hand, settings, start);
            Canvas canvas(fb, fb.bounds(), settings.lineAlgo);
            drawWristPath(canvas, HandView::fit(kWidth, kHeight), motionPath(anim), keys);
            save(fb, out, base + "_path.bmp", "motion path of the wrist through " + std::to_string(keys.size()) + " keyframes");
        }
    }

    std::printf("\n%d files written", gWritten);
    if (gFailed) std::printf(", %d FAILED", gFailed);
    std::printf(". Open the .gif files to watch the animations.\n");
    return gFailed == 0 ? 0 : 1;
}
