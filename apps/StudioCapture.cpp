#include "StudioCapture.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <GL/gl.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include "Framebuffer.h"
#include "GifWriter.h"
#include "Handshapes.h"
#include "Signs3D.h"

#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MAKE_DIR(p) mkdir(p, 0755)
#endif

namespace {

int gW = 960, gH = 720;

// The finished picture, read back from OpenGL into our framebuffer. Both
// store the bottom row first, so rows copy across unchanged.
Framebuffer grab(int w = 0, int h = 0) {
    if (w == 0) w = gW, h = gH;
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    Framebuffer fb(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const unsigned char* p = &px[(static_cast<size_t>(y) * w + x) * 3];
            fb.setPixel(x, y, {p[0], p[1], p[2]});
        }
    return fb;
}

// Copies a whole image into 'dst' with its bottom-left corner at (dx, dy).
void paste(Framebuffer& dst, const Framebuffer& src, int dx, int dy) {
    for (int y = 0; y < src.height(); ++y)
        for (int x = 0; x < src.width(); ++x)
            if (dst.inside(dx + x, dy + y)) dst.setPixel(dx + x, dy + y, src.getPixel(x, y));
}

// Renders one square tile: the drawing area is shrunk to tile x tile (the
// bottom-left corner of the window), so the projection's aspect ratio and
// the label size are right for the tile.
template <typename Setup>
Framebuffer renderTile(StudioCore& core, int tile, const std::string& label, int labelSize, Setup setup) {
    core.resize(tile, tile);
    setup();
    core.renderScene();
    if (!label.empty()) core.drawLabel(14, labelSize ? 34 : 26, label, labelSize);
    Framebuffer fb = grab(tile, tile);
    core.resize(gW, gH);
    return fb;
}

std::string gFolder;
int gCount = 0;

void save(const Framebuffer& fb, const std::string& name) {
    if (fb.saveBMP(gFolder + "/" + name)) {
        ++gCount;
        std::printf("  %s\n", name.c_str());
    }
}

// One frame of the scene, optionally with the user-interface overlay.
Framebuffer frame(StudioCore& core, bool overlay) {
    if (overlay) core.render();
    else core.renderScene();
    return grab();
}

// A chart of signs: each one rendered as a labelled tile.
void chart(StudioCore& core, Category cat, const std::vector<Sign3D>& signs, int cols, const std::string& name,
           double at = 0.5) {
    const int tile = 240;
    const int rows = (static_cast<int>(signs.size()) + cols - 1) / cols;
    Framebuffer sheet(cols * tile, rows * tile);
    sheet.clear({12, 16, 30});
    const bool word = cat == Category::Words;
    for (size_t i = 0; i < signs.size(); ++i) {
        const Framebuffer fb = renderTile(core, tile, signs[i].gloss, 1, [&] {
            core.showSign(cat, static_cast<int>(i));
            core.setPlaybackTime(signs[i].duration() * at);
            core.camera().setPreset(OrbitCamera::Front);
            core.camera().orbit(word ? 0.0 : 18.0, 4.0);
            core.camera().focus(word ? Vec3{0.0, 2.0, 0.6}
                                     : core.rightHand().worldSite(HandSite::PalmFront) + Vec3{0.0, 0.25, 0.0},
                                word ? 15.5 : 7.0);
        });
        const int col = static_cast<int>(i) % cols, row = static_cast<int>(i) / cols;
        paste(sheet, fb, col * tile, (rows - 1 - row) * tile);
    }
    save(sheet, name);
}

// An animated GIF of the current timeline, 15 frames per second, from the
// middle 640 x 480 of the window shrunk to 320 x 240... kept full size here
// for clarity: the whole window at half size.
void gif(StudioCore& core, const std::string& name, bool overlay, double start, double end) {
    GifWriter w;
    const int outW = gW / 2, outH = gH / 2;
    if (!w.open(gFolder + "/" + name, outW, outH, 1)) return;
    const double fps = 15.0;
    for (double t = start; t <= end + 1e-9; t += 1.0 / fps) {
        core.setPlaybackTime(t);
        const Framebuffer fb = frame(core, overlay);
        Framebuffer shrunk(outW, outH);
        for (int y = 0; y < outH; ++y)
            for (int x = 0; x < outW; ++x) {
                int r = 0, g = 0, b = 0;
                for (int j = 0; j < 2; ++j)
                    for (int i = 0; i < 2; ++i) {
                        const Color c = fb.getPixel(2 * x + i, 2 * y + j);
                        r += c.r, g += c.g, b += c.b;
                    }
                shrunk.setPixel(x, y, {static_cast<std::uint8_t>(r / 4), static_cast<std::uint8_t>(g / 4),
                                      static_cast<std::uint8_t>(b / 4)});
            }
        w.addFrame(shrunk, static_cast<int>(100.0 / fps + 0.5));
    }
    if (w.close()) {
        ++gCount;
        std::printf("  %s (%d frames)\n", name.c_str(), w.frameCount());
    }
}

int wordIndex(const std::string& gloss) {
    const auto& w = wordSigns();
    for (size_t i = 0; i < w.size(); ++i)
        if (w[i].gloss == gloss) return static_cast<int>(i);
    return 0;
}

}  // namespace

int runCapture(StudioCore& core, const std::string& folder, int width, int height) {
    gW = width;
    gH = height;
    gFolder = folder;
    gCount = 0;
    MAKE_DIR(folder.c_str());
    std::printf("Rendering the final-project images into %s/ ...\n", folder.c_str());

    // 1. The main view: THANK YOU from three-quarter, with the interface.
    core.showSign(Category::Words, wordIndex("THANK YOU"));
    core.camera().setPreset(OrbitCamera::ThreeQuarter);
    core.setPlaybackTime(0.15);
    save(frame(core, true), "final_01_studio.bmp");

    // 2. Charts: the alphabet, the numbers and the conversation signs.
    chart(core, Category::Letters, letterSigns(), 7, "final_02_alphabet.bmp", 0.6);
    chart(core, Category::Numbers, numberSigns(), 6, "final_03_numbers.bmp", 0.6);
    chart(core, Category::Words, wordSigns(), 8, "final_04_words.bmp", 0.55);

    // 3. Shading: smooth, flat and wireframe of the same close-up.
    {
        core.showSign(Category::Letters, 1);  // B
        core.setPlaybackTime(0.2);
        const int tile = 320;
        Framebuffer sheet(3 * tile, tile);
        const ShadeMode modes[3] = {ShadeMode::Smooth, ShadeMode::Flat, ShadeMode::Wireframe};
        for (int i = 0; i < 3; ++i) {
            paste(sheet, renderTile(core, tile, shadeModeName(modes[i]), 0, [&] {
                      core.options().shade = modes[i];
                      core.camera().setPreset(OrbitCamera::Front);
                      core.camera().orbit(30.0, 8.0);
                      core.camera().focus(core.rightHand().worldSite(HandSite::PalmFront) + Vec3{0, 0.3, 0}, 8.0);
                  }),
                  i * tile, 0);
        }
        core.options().shade = ShadeMode::Smooth;
        save(sheet, "final_05_shading.bmp");
    }

    // 4. Lighting off / on, skeleton, joint axes.
    {
        core.showSign(Category::Letters, 21);  // V
        core.setPlaybackTime(0.2);
        const int tile = 320;
        Framebuffer sheet(4 * tile, tile);
        const char* labels[4] = {"lighting off", "lit: key + fill + rim", "skeleton", "joint axes"};
        for (int i = 0; i < 4; ++i) {
            paste(sheet, renderTile(core, tile, labels[i], 0, [&] {
                      core.options().lighting = i != 0;
                      core.options().skeleton = i == 2;
                      core.options().frames = i == 3;
                      core.camera().setPreset(OrbitCamera::Front);
                      core.camera().orbit(25.0, 6.0);
                      core.camera().focus(core.rightHand().worldSite(HandSite::PalmFront) + Vec3{0, 0.3, 0}, 8.0);
                  }),
                  i * tile, 0);
        }
        core.options() = RenderOptions{};
        save(sheet, "final_06_lighting.bmp");
    }

    // 5. The four camera presets on PLEASE.
    {
        core.showSign(Category::Words, wordIndex("PLEASE"));
        core.setPlaybackTime(0.3);
        const int tw = 480, th = 360;
        Framebuffer sheet(2 * tw, 2 * th);
        for (int i = 0; i < 4; ++i) {
            core.camera().setPreset(static_cast<OrbitCamera::Preset>(i));
            core.renderScene();
            core.drawLabel(24, 44, std::string("camera: ") + OrbitCamera::presetName(static_cast<OrbitCamera::Preset>(i)), 1);
            Framebuffer fb = grab();
            for (int y = 0; y < th; ++y)
                for (int x = 0; x < tw; ++x) {
                    int r = 0, g = 0, b = 0;
                    for (int j = 0; j < 2; ++j)
                        for (int k = 0; k < 2; ++k) {
                            const Color c = fb.getPixel(2 * x + k, 2 * y + j);
                            r += c.r, g += c.g, b += c.b;
                        }
                    sheet.setPixel((i % 2) * tw + x, (1 - i / 2) * th + y,
                                   {static_cast<std::uint8_t>(r / 4), static_cast<std::uint8_t>(g / 4),
                                    static_cast<std::uint8_t>(b / 4)});
                }
        }
        core.camera().setPreset(OrbitCamera::Front);
        save(sheet, "final_07_cameras.bmp");
    }

    // 6. Two-handed signs and the left-handed mirror.
    core.showSign(Category::Words, wordIndex("NAME"));
    core.setPlaybackTime(0.25);
    core.camera().setPreset(OrbitCamera::ThreeQuarter);
    save(frame(core, true), "final_08_two_hands.bmp");
    core.options().leftHanded = true;
    save(frame(core, true), "final_09_left_handed.bmp");
    core.options().leftHanded = false;

    // 7. Type a sentence: subtitles follow the signing.
    core.camera().setPreset(OrbitCamera::Front);
    core.performText("hello my name abdalla");
    {
        const double d = core.playbackDuration();
        const double moments[3] = {0.18, 0.5, 0.72};
        for (int i = 0; i < 3; ++i) {
            core.setPlaybackTime(d * moments[i]);
            char n[64];
            std::snprintf(n, sizeof n, "final_1%d_sentence_%d.bmp", i, i + 1);
            save(frame(core, true), n);
        }
    }

    // 8. Build mode with a selected joint.
    core.camera().setPreset(OrbitCamera::Front);
    core.enterBuildMode();
    {
        HandPose p = shapePose("L");
        p.fingers[Index].flex = {20.0, 35.0, 20.0};
        core.setBuildPose(p);
        core.selectJoint({0, Index, 1});
        core.setFollowHand(true);
        core.update(0.0);
        core.update(0.0);
        save(frame(core, true), "final_13_build.bmp");
        core.setFollowHand(false);
    }

    // 9. Animations (GIF): a word, a two-handed sign, fingerspelling and a sentence.
    core.camera().setPreset(OrbitCamera::Front);
    core.showSign(Category::Words, wordIndex("HELLO"));
    gif(core, "final_anim_hello.gif", false, 0.0, core.playbackDuration());
    core.camera().setPreset(OrbitCamera::ThreeQuarter);
    core.showSign(Category::Words, wordIndex("THANK YOU"));
    gif(core, "final_anim_thank_you.gif", false, 0.0, core.playbackDuration());
    core.showSign(Category::Words, wordIndex("NAME"));
    gif(core, "final_anim_name.gif", false, 0.0, core.playbackDuration());
    core.camera().setPreset(OrbitCamera::Front);
    core.performText("nice to meet you");
    gif(core, "final_anim_nice_to_meet_you.gif", true, 0.0, core.playbackDuration());
    core.performText("abdalla");
    gif(core, "final_anim_spelling.gif", true, 0.0, core.playbackDuration());

    core.showSign(Category::Words, 0);
    std::printf("%d files written.\n", gCount);
    return gCount;
}
