#include "StudioCore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Handshapes.h"
#include "Signs3D.h"

namespace {

std::string format(const char* fmt, double a = 0, double b = 0, double c = 0) {
    char buf[256];
    std::snprintf(buf, sizeof buf, fmt, a, b, c);
    return buf;
}

const char* categoryName(Category c) {
    switch (c) {
        case Category::Letters: return "Letters";
        case Category::Numbers: return "Numbers";
        case Category::Words: return "Words";
        default: return "";
    }
}

const char* kFingerNames[kFingerCount] = {"thumb", "index", "middle", "ring", "little finger"};

// Colours of the overlay.
struct Rgb {
    float r, g, b;
};
const Rgb kText{0.93f, 0.91f, 0.87f};
const Rgb kMuted{0.70f, 0.75f, 0.82f};
const Rgb kAccent{0.94f, 0.64f, 0.37f};

}  // namespace

StudioCore::StudioCore() {
    buildPose_ = shapePose("5open");
    showSign(Category::Words, 0);
}

void StudioCore::setText(std::function<void(const OverlayText&, int)> draw, std::function<int(const std::string&, int)> measure) {
    drawText_ = std::move(draw);
    measureText_ = std::move(measure);
}

void StudioCore::resize(int width, int height) { renderer_.resize(width, height); }

const std::vector<Sign3D>& StudioCore::categoryList(Category c) const {
    switch (c) {
        case Category::Letters: return letterSigns();
        case Category::Numbers: return numberSigns();
        default: return wordSigns();
    }
}

void StudioCore::showSign(Category c, int index) {
    category_ = c;
    const auto& list = categoryList(c);
    const int n = static_cast<int>(list.size());
    index_[static_cast<int>(c)] = ((index % n) + n) % n;
    mode_ = StudioMode::Learn;
    performing_ = false;
    playCurrent();
}

void StudioCore::playCurrent() {
    const Sign3D& s = categoryList(category_)[index_[static_cast<int>(category_)]];
    // Raise the hands from rest, then loop the sign.
    keys_.clear();
    keys_.push_back({0.0, restRight(), restLeft()});
    appendKeys(keys_, s.keys, 0.45);
    // Loop from the sign's own first key, not from rest: shift so the sign
    // starts at 0 and keep the rest key only for the very first play.
    std::vector<Key3D> loop(keys_.begin() + 1, keys_.end());
    const double t0 = loop.front().time;
    for (Key3D& k : loop) k.time -= t0;
    keys_ = loop;
    player_.start(&keys_, true);
    applyFrame(player_.frame());
}

void StudioCore::performText(const std::string& text) {
    performance_ = perform(text);
    keys_ = performance_.keys;
    performing_ = true;
    mode_ = StudioMode::Type;
    player_.start(&keys_, false);
    applyFrame(player_.frame());
}

void StudioCore::setPlaybackTime(double t) {
    player_.seek(t);
    applyFrame(player_.frame());
}

double StudioCore::playbackDuration() const { return keys_.empty() ? 0.0 : keys_.back().time; }

void StudioCore::enterBuildMode() {
    mode_ = StudioMode::Build;
    performing_ = false;
    typing_ = false;
    player_.stop();
    // Start from the hand as it is now, held up in front of the body.
    buildPose_ = right_.pose();
    buildPlace_ = Placement3D{};
    buildPlace_.scale = kHandScale;
    buildPlace_ = placeSiteAt(buildPose_, buildPlace_, HandSite::PalmFront, spellingCentre() + Vec3{0.25, -0.2, 0.2});
    right_.set(buildPose_, buildPlace_);
    left_.set(restLeft().pose, restLeft().place);
    selected_ = JointRef{};
}

void StudioCore::setBuildPose(const HandPose& pose) {
    buildPose_ = pose;
    right_.set(buildPose_, buildPlace_);
}

void StudioCore::applyFrame(const Frame3D& f) {
    right_.set(f.right.pose, f.right.place);
    left_.set(f.left.pose, f.left.place);
}

bool StudioCore::update(double nowSeconds) {
    now_ = nowSeconds;
    const double dt = lastTime_ < 0.0 ? 0.0 : std::min(0.1, nowSeconds - lastTime_);
    lastTime_ = nowSeconds;
    if (mode_ != StudioMode::Build && player_.active()) {
        player_.advance(dt);
        applyFrame(player_.frame());
    }
    if (follow_) {
        // The close-up camera glides after the palm.
        const Vec3 goal = right_.worldSite(HandSite::PalmFront) + Vec3{0.0, 0.35, 0.0};
        const Vec3 cur = camera_.target();
        const double k = std::min(1.0, dt * 6.0);
        camera_.setTarget(dt == 0.0 ? goal : cur + (goal - cur) * k);
    }
    return true;
}

void StudioCore::render() {
    renderer_.drawScene(camera_, right_, left_, options_, mode_ == StudioMode::Build ? (selected_.valid() ? selected_ : hover_) : JointRef{});
    Overlay o;
    textSizes_.clear();
    buildOverlay(o);
    size_t i = 0;
    renderer_.drawOverlay(o, [&](const OverlayText& t) {
        if (drawText_) drawText_(t, i < textSizes_.size() ? textSizes_[i] : 0);
        ++i;
    });
}

void StudioCore::renderScene() {
    renderer_.drawScene(camera_, right_, left_, options_, mode_ == StudioMode::Build ? selected_ : JointRef{});
}

void StudioCore::drawLabel(int x, int y, const std::string& label, int size) {
    Overlay o;
    const int w = measureText_ ? measureText_(label, size) : static_cast<int>(label.size()) * 9;
    o.rects.push_back({x - 8, y - (size ? 26 : 17), w + 16, size ? 34 : 24, 0.04f, 0.06f, 0.11f, 0.7f});
    o.texts.push_back({x, y, label, kText.r, kText.g, kText.b});
    renderer_.drawOverlay(o, [&](const OverlayText& t) {
        if (drawText_) drawText_(t, size);
    });
}

// ---- Input ----------------------------------------------------------------

void StudioCore::startTyping() {
    typing_ = true;
    typed_.clear();
    mode_ = StudioMode::Type;
}

void StudioCore::onChar(char c) {
    if (typing_) {
        if (c == '\r' || c == '\n') {
            typing_ = false;
            if (!typed_.empty()) {
                lastTyped_ = typed_;
                performText(typed_);
            } else if (!performing_) {
                showSign(category_, index_[static_cast<int>(category_)]);
            }
        } else if (c == 27) {
            typing_ = false;
            if (!performing_) showSign(category_, index_[static_cast<int>(category_)]);
        } else if (c == '\b') {
            if (!typed_.empty()) typed_.pop_back();
        } else if (static_cast<unsigned char>(c) >= 32 && typed_.size() < 60) {
            typed_ += c;
        }
        return;
    }
    switch (c) {
        case 27: quit_ = true; break;
        case '\r':
        case '\n': startTyping(); break;
        case '\t': onKey(StudioKey::Tab); break;
        case ' ':
            if (player_.active()) player_.seek(0.0);
            break;
        case 'p': case 'P': player_.togglePause(); break;
        case '[': player_.setSpeed(player_.speed() / 2.0); break;
        case ']': player_.setSpeed(player_.speed() * 2.0); break;
        case 'b': case 'B':
            if (mode_ == StudioMode::Build) showSign(category_, index_[static_cast<int>(category_)]);
            else enterBuildMode();
            break;
        case 'c': case 'C':
            camera_.setPreset(static_cast<OrbitCamera::Preset>((camera_.preset() + 1) % OrbitCamera::PresetCount));
            follow_ = false;
            break;
        case 'z': case 'Z':
            follow_ = !follow_;
            if (follow_) camera_.focus(right_.worldSite(HandSite::PalmFront) + Vec3{0.0, 0.35, 0.0}, 5.2);
            else camera_.setPreset(camera_.preset());
            break;
        case 's': case 'S':
            options_.shade = static_cast<ShadeMode>((static_cast<int>(options_.shade) + 1) % static_cast<int>(ShadeMode::Count));
            break;
        case 'k': case 'K': options_.skeleton = !options_.skeleton; break;
        case 'g': case 'G': options_.frames = !options_.frames; break;
        case 'o': case 'O': options_.body = !options_.body; break;
        case 'l': case 'L': options_.lighting = !options_.lighting; break;
        case 'm': case 'M': options_.leftHanded = !options_.leftHanded; break;
        case 'h': case 'H': case '?': help_ = !help_; break;
        case 'r': case 'R':
            camera_.setPreset(OrbitCamera::Front);
            follow_ = false;
            player_.setSpeed(1.0);
            if (mode_ == StudioMode::Build) {
                buildPose_ = shapePose("5open");
                right_.set(buildPose_, buildPlace_);
            }
            break;
        default: break;
    }
}

void StudioCore::onKey(StudioKey k) {
    if (typing_) {
        if (k == StudioKey::Enter) onChar('\r');
        if (k == StudioKey::Escape) onChar(27);
        if (k == StudioKey::Backspace) onChar('\b');
        return;
    }
    if (mode_ == StudioMode::Build) {
        if (selected_.valid() && selected_.hand == 0) {
            FingerPose& fp = buildPose_.fingers[selected_.finger];
            const int b = std::min(selected_.point, 2);
            if (k == StudioKey::Up) fp.flex[b] = std::clamp(fp.flex[b] - 10.0, -20.0, 120.0);
            if (k == StudioKey::Down) fp.flex[b] = std::clamp(fp.flex[b] + 10.0, -20.0, 120.0);
            if (k == StudioKey::Left) fp.bend[b] = std::clamp(fp.bend[b] + 5.0, -60.0, 60.0);
            if (k == StudioKey::Right) fp.bend[b] = std::clamp(fp.bend[b] - 5.0, -60.0, 60.0);
        } else {
            // No joint selected: turn the whole hand to see it from all sides.
            if (k == StudioKey::Left) buildPlace_.roll -= 15.0;
            if (k == StudioKey::Right) buildPlace_.roll += 15.0;
            if (k == StudioKey::Up) buildPlace_.tip -= 10.0;
            if (k == StudioKey::Down) buildPlace_.tip += 10.0;
        }
        if (k == StudioKey::Escape) selected_ = JointRef{};
        right_.set(buildPose_, buildPlace_);
        return;
    }
    const int n = static_cast<int>(categoryList(category_).size());
    int& idx = index_[static_cast<int>(category_)];
    switch (k) {
        case StudioKey::Left: showSign(category_, (idx + n - 1) % n); break;
        case StudioKey::Right: showSign(category_, (idx + 1) % n); break;
        case StudioKey::Tab:
            showSign(static_cast<Category>((static_cast<int>(category_) + 1) % static_cast<int>(Category::Count)),
                     index_[(static_cast<int>(category_) + 1) % static_cast<int>(Category::Count)]);
            break;
        case StudioKey::Up: player_.setSpeed(player_.speed() * 2.0); break;
        case StudioKey::Down: player_.setSpeed(player_.speed() / 2.0); break;
        case StudioKey::Enter: startTyping(); break;
        case StudioKey::Escape: quit_ = true; break;
        default: break;
    }
}

// Picking: project every joint to the screen with the same matrices the
// renderer uses (projection x view x hand) and take the nearest one within
// 24 pixels of the mouse.
JointRef StudioCore::pick(int x, int y) const {
    const Mat4 pv = renderer_.projViewScene(camera_, options_);
    JointRef best;
    double bestD = 24.0 * 24.0;
    double bestDepth = 2.0;
    const Hand3D* hands[1] = {&right_};
    for (int h = 0; h < 1; ++h)
        for (int f = 0; f < kFingerCount; ++f)
            for (int p = 0; p < kPoints; ++p) {
                const Vec3 world = hands[h]->handMatrix().apply(hands[h]->chain(f).point[p]);
                double px, py, depth;
                if (!xf3::project(pv, world, renderer_.width(), renderer_.height(), px, py, depth)) continue;
                const double d = (px - x) * (px - x) + (py - y) * (py - y);
                if (d < bestD - 1e-9 || (std::fabs(d - bestD) < 4.0 && depth < bestDepth)) {
                    bestD = d;
                    bestDepth = depth;
                    best = {h, f, p};
                }
            }
    return best;
}

void StudioCore::onMouseDown(int x, int y, bool rightButton) {
    lastX_ = x;
    lastY_ = y;
    dragging_ = true;
    draggingJoint_ = false;
    if (mode_ == StudioMode::Build && !rightButton) {
        const JointRef j = pick(x, y);
        if (j.valid()) {
            selected_ = j;
            draggingJoint_ = true;
        }
    }
}

void StudioCore::onMouseMove(int x, int y) {
    const int dx = x - lastX_, dy = y - lastY_;
    if (!dragging_) {
        if (mode_ == StudioMode::Build) hover_ = pick(x, y);
        return;
    }
    lastX_ = x;
    lastY_ = y;
    if (draggingJoint_ && selected_.valid()) {
        // Dragging a joint: down curls it (flex), sideways spreads it (bend).
        FingerPose& fp = buildPose_.fingers[selected_.finger];
        const int b = std::min(selected_.point, 2);
        const double sign = options_.leftHanded ? -1.0 : 1.0;
        fp.flex[b] = std::clamp(fp.flex[b] + dy * 0.6, -20.0, 120.0);
        fp.bend[b] = std::clamp(fp.bend[b] - sign * dx * 0.35, -60.0, 60.0);
        right_.set(buildPose_, buildPlace_);
    } else {
        camera_.orbit(-dx * 0.4, dy * 0.3);
        if (follow_) camera_.setTarget(camera_.target());
    }
}

void StudioCore::onMouseUp() {
    dragging_ = false;
    draggingJoint_ = false;
}

void StudioCore::onWheel(int steps) { camera_.zoom(std::pow(0.88, steps)); }

// ---- Overlay -----------------------------------------------------------------

void StudioCore::buildOverlay(Overlay& o) {
    const int W = renderer_.width(), H = renderer_.height();
    auto measure = [&](const std::string& s, int size) {
        return measureText_ ? measureText_(s, size) : static_cast<int>(s.size()) * (size ? 16 : 9);
    };
    auto text = [&](int x, int y, const std::string& s, Rgb c, int size = 0) {
        o.texts.push_back({x, y, s, c.r, c.g, c.b});
        textSizes_.push_back(size);
    };
    auto panel = [&](int x, int y, int w, int h, float a = 0.62f) { o.rects.push_back({x, y, w, h, 0.04f, 0.06f, 0.11f, a}); };

    // Title and mode, top left.
    std::string modeLine;
    if (mode_ == StudioMode::Learn) {
        const int n = static_cast<int>(categoryList(category_).size());
        modeLine = std::string("Learn  \xB7  ") + categoryName(category_) +
                   format("  %.0f / %.0f", index_[static_cast<int>(category_)] + 1.0, n);
    } else if (mode_ == StudioMode::Type) {
        modeLine = "Type a word or sentence";
    } else {
        modeLine = "Build: drag a joint to pose the hand";
    }
    panel(12, 12, std::max(measure(modeLine, 0), measure("SIGN LANGUAGE GESTURE VISUALIZER", 0)) + 28, 58);
    text(26, 34, "SIGN LANGUAGE GESTURE VISUALIZER", kAccent);
    text(26, 58, modeLine, kText);

    // The sign being shown, top right.
    if (mode_ == StudioMode::Learn) {
        const Sign3D& s = categoryList(category_)[index_[static_cast<int>(category_)]];
        const std::string where = std::string("where: ") + s.location + (s.twoHanded ? "  \xB7  two hands" : "");
        const int w = std::max({measure(s.gloss, 1), measure(s.description, 0), measure(where, 0)}) + 32;
        panel(W - w - 12, 12, w, 96);
        text(W - w + 4, 46, s.gloss, kText, 1);
        text(W - w + 4, 72, s.description, kMuted);
        text(W - w + 4, 94, where, kMuted);
    } else if (mode_ == StudioMode::Build) {
        double diff = 0.0;
        const Handshape& near = closestHandshape(buildPose_, &diff);
        std::vector<std::string> lines;
        lines.push_back(std::string("Looks most like: ") + near.name + format("  (%.0f\xB0 average difference)", diff));
        if (selected_.valid()) {
            const FingerPose& fp = buildPose_.fingers[selected_.finger];
            const int b = std::min(selected_.point, 2);
            const char* joint = right_.shape().spec(selected_.finger).pointNames[selected_.point];
            lines.push_back(std::string(kFingerNames[selected_.finger]) + " " + joint +
                            format(":  curl %.0f\xB0   spread %.0f\xB0", fp.flex[b], fp.bend[b]));
        } else {
            lines.push_back("Click a joint; arrows turn the hand");
        }
        int w = 0;
        for (const auto& l : lines) w = std::max(w, measure(l, 0));
        w += 32;
        panel(W - w - 12, 12, w, 22 + 24 * static_cast<int>(lines.size()));
        for (size_t i = 0; i < lines.size(); ++i) text(W - w + 4, 36 + 24 * static_cast<int>(i), lines[i], i == 0 ? kText : kMuted);
    }

    // Subtitles while a sentence plays: the signs in order, the current one
    // in orange; for a spelled word, its letters with the current letter lit.
    if (performing_ && !performance_.parts.empty()) {
        const int cur = performance_.partAt(player_.time());
        std::vector<std::pair<std::string, bool>> tokens;  // text, highlighted
        std::string currentWord;
        for (size_t i = 0; i < performance_.parts.size(); ++i) {
            const PerformancePart& p = performance_.parts[i];
            const bool isCur = static_cast<int>(i) == cur;
            if (p.spelled) {
                const bool firstOfWord = i == 0 || performance_.parts[i - 1].word != p.word || !performance_.parts[i - 1].spelled;
                if (!firstOfWord) tokens.push_back({"-", false});
                else if (!tokens.empty()) tokens.push_back({"   ", false});
                tokens.push_back({p.label, isCur});
            } else {
                if (!tokens.empty()) tokens.push_back({"   ", false});
                tokens.push_back({p.label, isCur});
            }
        }
        int total = 0;
        for (const auto& t : tokens) total += measure(t.first, 1);
        const int barW = std::min(W - 24, total + 48);
        const int barX = (W - barW) / 2, barY = H - 128;
        panel(barX, barY, barW, 54, 0.7f);
        int x = (W - total) / 2;
        for (const auto& t : tokens) {
            text(x, barY + 37, t.first, t.second ? kAccent : kText, 1);
            x += measure(t.first, 1);
        }
        const std::string said = std::string("\"") + performance_.text + "\"";
        text((W - measure(said, 0)) / 2, barY - 10, said, kMuted);
    }

    // The text box.
    if (typing_) {
        const std::string line = std::string("Type: ") + typed_ + ((static_cast<long>(now_ * 2.0) % 2) ? "_" : " ");
        const std::string hint = "Enter = sign it    Esc = cancel    try: hello my name abdalla";
        const int w = std::max(measure(line, 1), measure(hint, 0)) + 40;
        panel((W - w) / 2, H - 140, w, 76, 0.8f);
        text((W - w) / 2 + 20, H - 104, line, kText, 1);
        text((W - w) / 2 + 20, H - 78, hint, kMuted);
    }

    // Status line and keys, bottom.
    const std::string status = format("speed %.2gx", player_.speed()) + "   camera: " +
                               OrbitCamera::presetName(camera_.preset()) + (follow_ ? " (close-up)" : "") +
                               "   shading: " + shadeModeName(options_.shade) + (options_.leftHanded ? "   left-handed" : "") +
                               (player_.paused() ? "   PAUSED" : "");
    const std::string keysLine = mode_ == StudioMode::Build
                                     ? "B back   drag joint = pose   arrows = joint / turn hand   R reset   H help"
                                     : "Enter type   Tab letters/numbers/words   \x3C \x3E sign   B build   C camera   Z close-up   H help";
    panel(0, H - 52, W, 52, 0.55f);
    text(16, H - 30, status, kMuted);
    text(16, H - 10, keysLine, kText);

    if (!toast_.empty() && now_ < toastUntil_) text(16, H - 64, toast_, kAccent);

    if (help_) {
        const std::vector<std::string> lines = {
            "KEYS",
            "Enter          type a word or sentence, Enter again to sign it",
            "Tab            letters / numbers / words",
            "Left, Right    previous / next sign",
            "Space  P       replay   pause",
            "[ ]  Up Down   slower / faster",
            "B              Build mode: drag joints to make a handshape",
            "C              camera: front, three-quarter, side, signer's view",
            "Z              close-up that follows the hand",
            "mouse drag     orbit the camera     wheel  zoom",
            "S              shading: smooth, flat, wireframe",
            "L              lighting on / off",
            "K  G           skeleton   joint axes",
            "O              body reference on / off",
            "M              left-handed signer (mirror)",
            "R              reset     H  close help     Esc  quit",
        };
        int w = 0;
        for (const auto& l : lines) w = std::max(w, measure(l, 0));
        w += 48;
        const int h = 26 * static_cast<int>(lines.size()) + 28;
        const int x0 = (W - w) / 2, y0 = std::max(80, (H - h) / 2 - 20);
        panel(x0, y0, w, h, 0.86f);
        for (size_t i = 0; i < lines.size(); ++i)
            text(x0 + 24, y0 + 34 + 26 * static_cast<int>(i), lines[i], i == 0 ? kAccent : kText);
    }
}
