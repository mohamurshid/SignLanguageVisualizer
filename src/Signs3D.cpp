#include "Signs3D.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "Body.h"
#include "Handshapes.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

// Which way the hand faces (see Placement3D). Useful combinations:
//   {}                          palm to the viewer, fingers up
//   {.roll = 180}               palm to the signer, fingers up
//   {.roll = 90}                palm towards the body's midline, fingers up
//   {.tip = 90}                 palm down, fingers forward
//   {.tip = 90, .roll = 180}    palm up, fingers forward
//   {.turn = -90, .roll = 180}  fingers across the body, palm to the signer
//   {.tip = -90, .roll = 90}    index pointing back at the chest
// The left hand uses the same numbers and is mirrored, so "towards the
// midline" means the same thing for both hands.
struct Orient {
    double turn = 0.0, tip = 0.0, roll = 0.0, wristBend = 0.0, wristFlex = 0.0;
};

Placement3D placement(Orient o, bool left) {
    Placement3D p;
    p.turn = o.turn;
    p.tip = o.tip;
    p.roll = o.roll;
    p.wristBend = o.wristBend;
    p.wristFlex = o.wristFlex;
    p.scale = kHandScale;
    p.left = left;
    return p;
}

// A hand with 'shape', facing 'o', with one of its sites exactly on 'target'.
HandKey at(const HandPose& pose, Orient o, HandSite site, Vec3 target, bool left = false) {
    return {pose, placeSiteAt(pose, placement(o, left), site, target)};
}
HandKey at(const std::string& shape, Orient o, HandSite site, Vec3 target, bool left = false) {
    return at(shapePose(shape), o, site, target, left);
}

// A hand with its wrist at 'wrist'.
HandKey wristAt(const HandPose& pose, Orient o, Vec3 wrist, bool left = false) {
    Placement3D p = placement(o, left);
    p.position = wrist;
    return {pose, p};
}
HandKey wristAt(const std::string& shape, Orient o, Vec3 wrist, bool left = false) {
    return wristAt(shapePose(shape), o, wrist, left);
}

HandKey shifted(HandKey k, Vec3 d) {
    k.place.position = k.place.position + d;
    return k;
}
HandKey reshaped(HandKey k, const HandPose& pose) {
    k.pose = pose;
    return k;
}
HandKey reshaped(HandKey k, const std::string& shape) { return reshaped(k, shapePose(shape)); }
HandKey turned(HandKey k, Orient o) {  // same wrist, new orientation
    const Vec3 w = k.place.position;
    const bool left = k.place.left;
    k.place = placement(o, left);
    k.place.position = w;
    return k;
}

// World position of a site of a hand key (to put one hand against the other).
Vec3 siteOf(const HandKey& k, HandSite s) {
    Hand3D h;
    h.set(k.pose, k.place);
    return h.worldSite(s);
}

// Builds a sign key by key. key(dt) records both hands 'dt' seconds after
// the previous key; a key that repeats the previous one is a hold.
class Builder {
public:
    Builder(std::string gloss, SignKind kind, std::string description, std::string location, bool twoHanded = false) {
        s_.gloss = std::move(gloss);
        s_.kind = kind;
        s_.description = std::move(description);
        s_.location = std::move(location);
        s_.twoHanded = twoHanded;
        right_ = restRight();
        left_ = restLeft();
    }
    Builder& R(const HandKey& k) {
        right_ = k;
        return *this;
    }
    Builder& L(const HandKey& k) {
        left_ = k;
        return *this;
    }
    Builder& key(double dt) {
        t_ += s_.keys.empty() ? 0.0 : dt;
        s_.keys.push_back({t_, right_, left_});
        return *this;
    }
    Builder& aliases(std::vector<std::string> a) {
        s_.aliases = std::move(a);
        return *this;
    }
    const HandKey& right() const { return right_; }
    const HandKey& left() const { return left_; }
    Sign3D done() { return s_; }

private:
    Sign3D s_;
    HandKey right_, left_;
    double t_ = 0.0;
};

constexpr double kHold = 0.42;  // how long a letter or number is held

// A letter or number shown on its own at the fingerspelling position.
HandKey spelled(const Handshape& h, Vec3 offset = {}) {
    return at(h.pose, {h.orientation.turn, h.orientation.tip, h.orientation.roll}, HandSite::PalmFront,
              spellingCentre() + offset + Vec3{0.0, -h.orientation.lower, 0.0});
}

Sign3D staticShape(const Handshape& h, SignKind kind) {
    Builder b(h.name, kind, h.description, "fingerspelling position");
    b.R(spelled(h)).key(0).key(kHold);
    return b.done();
}

// Points round a circle in the picture plane (x-y) or the side plane (y-z).
Vec3 onCircleXY(Vec3 c, double r, double degrees) {
    return c + Vec3{r * std::cos(degrees * kPi / 180.0), r * std::sin(degrees * kPi / 180.0), 0.0};
}
Vec3 onCircleYZ(Vec3 c, double r, double degrees) {
    return c + Vec3{0.0, r * std::sin(degrees * kPi / 180.0), r * std::cos(degrees * kPi / 180.0)};
}

HandPose withIndexFlex(HandPose p, double knuckle, double middle) {
    p.fingers[Index].flex[0] = knuckle;
    p.fingers[Index].flex[1] = middle;
    return p;
}

std::vector<Sign3D> buildLetters() {
    std::vector<Sign3D> out;
    for (const Handshape& h : letterShapes()) {
        if (h.name == "J") {
            // The little finger draws a J: down, then curving across while the
            // palm turns towards the signer.
            Builder b("J", SignKind::Letter, h.description, "fingerspelling position");
            const Handshape i = handshape("I");
            b.R(spelled(i)).key(0).key(0.12);
            b.R(at(i.pose, {0, 0, 25}, HandSite::PalmFront, spellingCentre() + Vec3{0.0, -0.22, 0.0})).key(0.18);
            b.R(at(i.pose, {-10, 0, 70}, HandSite::PalmFront, spellingCentre() + Vec3{0.18, -0.36, 0.05})).key(0.18);
            b.R(at(i.pose, {-20, 0, 110}, HandSite::PalmFront, spellingCentre() + Vec3{0.34, -0.22, 0.1})).key(0.18);
            b.key(0.2);
            out.push_back(b.done());
        } else if (h.name == "Z") {
            // The index finger draws a Z as the SIGNER writes it, so the
            // viewer sees it mirrored. Repeated keys make sharp corners.
            Builder b("Z", SignKind::Letter, h.description, "fingerspelling position");
            const Vec3 corners[4] = {{0.28, 0.2, 0.0}, {-0.28, 0.2, 0.0}, {0.28, -0.2, 0.0}, {-0.28, -0.2, 0.0}};
            b.R(spelled(h, corners[0])).key(0).key(0.1);
            for (int c = 1; c < 4; ++c) b.R(spelled(h, corners[c])).key(0.2).key(0.05);
            b.key(0.15);
            out.push_back(b.done());
        } else {
            out.push_back(staticShape(h, SignKind::Letter));
        }
    }
    return out;
}

std::vector<Sign3D> buildNumbers() {
    std::vector<Sign3D> out;
    for (const Handshape& h : numberShapes()) {
        if (h.name == "10") {
            // Thumbs-up, shaken by twisting the forearm.
            Builder b("10", SignKind::Number, h.description, "fingerspelling position");
            for (int k = 0; k < 5; ++k) {
                const double roll = k % 2 == 0 ? 70.0 : 120.0;
                b.R(at(h.pose, {0, 0, roll}, HandSite::PalmFront, spellingCentre())).key(k == 0 ? 0.0 : 0.16);
            }
            b.key(0.2);
            out.push_back(b.done());
        } else {
            out.push_back(staticShape(h, SignKind::Number));
        }
    }
    out.front().aliases = {"zero"};
    const char* names[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten"};
    for (size_t i = 0; i < out.size(); ++i) out[i].aliases = {names[i]};
    return out;
}

std::vector<Sign3D> buildWords() {
    std::vector<Sign3D> w;
    const Vec3 chin = anchorPoint(Anchor::Chin);
    const Vec3 chest = anchorPoint(Anchor::Chest);
    const Vec3 temple = anchorPoint(Anchor::Temple);
    const Vec3 mouth = anchorPoint(Anchor::Mouth);

    // Left hand, flat and palm up, in front of the body (the base hand for
    // GOOD, NICE, AGAIN, HELP).
    const HandKey leftPalmUp = at("flat", {0, 90, 180}, HandSite::PalmFront, {0.5, -0.35, 2.05}, true);
    const Vec3 leftPalm = siteOf(leftPalmUp, HandSite::PalmFront);

    {  // ---- Greetings and manners ----
        Builder b("HELLO", SignKind::Word, "flat hand salutes from the temple, then moves out", "temple");
        const HandKey start = at("flat", {-28, 0, -10}, HandSite::IndexTip, temple + Vec3{-0.25, 0.1, 0.12});
        b.R(start).key(0).key(0.25);
        b.R(turned(shifted(start, {-0.75, -0.15, 0.75}), {-6, 0, 0})).key(0.6).key(0.3);
        w.push_back(b.aliases({"hi", "hey"}).done());
    }
    {
        Builder b("GOODBYE", SignKind::Word, "open palm faces out; the fingers fold down and up (waving)",
                  "beside the shoulder");
        const HandKey open = wristAt("flat", {}, {-1.2, 1.3, 2.0});
        b.R(open).key(0);
        for (int k = 0; k < 4; ++k) b.R(reshaped(open, k % 2 == 0 ? "bent" : "flat")).key(0.22);
        b.key(0.25);
        w.push_back(b.aliases({"bye"}).done());
    }
    {
        Builder b("THANK YOU", SignKind::Word, "fingertips of the flat hand on the chin, then forward and down",
                  "chin");
        const HandKey start = at("flat", {0, 0, 180}, HandSite::MiddleTip, chin + Vec3{0.0, -0.05, 0.12});
        b.R(start).key(0).key(0.25);
        b.R(turned(shifted(start, {-0.25, -0.65, 1.55}), {0, 70, 180})).key(0.55).key(0.3);
        w.push_back(b.aliases({"thanks", "thank"}).done());
    }
    {
        Builder b("PLEASE", SignKind::Word, "flat hand on the chest, circling", "chest");
        const Vec3 c = chest + Vec3{-0.1, 0.05, 0.04};
        for (int k = 0; k <= 8; ++k) {
            b.R(at("flat", {-55, 0, 180}, HandSite::PalmFront, onCircleXY(c, 0.38, 90.0 + 90.0 * k))).key(k == 0 ? 0 : 0.17);
        }
        b.key(0.25);
        w.push_back(b.done());
    }
    {
        Builder b("SORRY", SignKind::Word, "fist (A handshape) on the chest, circling", "chest");
        const Vec3 c = chest + Vec3{-0.1, 0.05, 0.16};
        for (int k = 0; k <= 8; ++k) {
            b.R(at("A", {-20, 0, 180}, HandSite::PalmFront, onCircleXY(c, 0.36, 90.0 + 90.0 * k))).key(k == 0 ? 0 : 0.17);
        }
        b.key(0.25);
        w.push_back(b.aliases({"apologize", "apologise"}).done());
    }
    {
        Builder b("NICE", SignKind::Word, "flat right hand slides across the left palm", "neutral space", true);
        b.L(leftPalmUp);
        const HandKey start = at("flat", {0, 90, 0}, HandSite::PalmFront, leftPalm + Vec3{0.0, 0.07, -0.35});
        b.R(start).key(0).key(0.15);
        b.R(at("flat", {0, 90, 0}, HandSite::PalmFront, leftPalm + Vec3{0.0, 0.07, 0.6})).key(0.5).key(0.25);
        w.push_back(b.done());
    }
    {
        Builder b("MEET", SignKind::Word, "two index fingers, palms facing, come together", "neutral space", true);
        b.R(wristAt(shapePose("Z"), {0, 0, 90}, {-1.45, 0.35, 2.1}))
            .L(wristAt(shapePose("Z"), {0, 0, 90}, {1.45, 0.35, 2.1}, true))
            .key(0);
        b.R(wristAt(shapePose("Z"), {0, 0, 90}, {-0.34, 0.35, 2.1}))
            .L(wristAt(shapePose("Z"), {0, 0, 90}, {0.34, 0.35, 2.1}, true))
            .key(0.5)
            .key(0.3);
        w.push_back(b.done());
    }
    {  // ---- Answers ----
        Builder b("YES", SignKind::Word, "fist nods at the wrist", "neutral space");
        for (int k = 0; k < 5; ++k)
            b.R(wristAt("S", {0, 0, 0, 0, k % 2 == 1 ? 42.0 : 0.0}, {-1.0, 0.95, 2.0})).key(k == 0 ? 0 : 0.22);
        b.key(0.2);
        w.push_back(b.done());
    }
    {
        Builder b("NO", SignKind::Word, "index and middle fingers snap shut onto the thumb, twice", "neutral space");
        const HandKey open = wristAt("noOpen", {0, 15, 60}, {-1.05, 1.05, 2.0});
        b.R(open).key(0);
        for (int k = 0; k < 4; ++k) b.R(reshaped(open, k % 2 == 0 ? "noClosed" : "noOpen")).key(0.2);
        b.key(0.25);
        w.push_back(b.done());
    }
    {
        Builder b("GOOD", SignKind::Word, "flat hand from the chin down into the left palm", "chin", true);
        b.L(leftPalmUp);
        b.R(at("flat", {0, 0, 180}, HandSite::MiddleTip, chin + Vec3{0.0, -0.05, 0.12})).key(0).key(0.2);
        b.R(at("flat", {0, 90, 180}, HandSite::PalmBack, leftPalm + Vec3{-0.05, 0.08, 0.15})).key(0.6).key(0.3);
        w.push_back(b.done());
    }
    {
        Builder b("BAD", SignKind::Word, "flat hand from the chin turns down and away", "chin");
        const HandKey start = at("flat", {0, 0, 180}, HandSite::MiddleTip, chin + Vec3{0.0, -0.05, 0.12});
        b.R(start).key(0).key(0.2);
        b.R(turned(shifted(start, {-0.25, -0.85, 1.0}), {0, 80, 0})).key(0.5).key(0.3);
        w.push_back(b.done());
    }
    {
        Builder b("FINE", SignKind::Word, "thumb of the open 5 hand taps the chest", "chest");
        const HandKey touch = at("5open", {0, 0, 90}, HandSite::ThumbTip, chest + Vec3{0.0, 0.1, 0.07});
        const HandKey away = shifted(touch, {0.0, 0.0, 0.3});
        b.R(away).key(0).R(touch).key(0.2).R(away).key(0.18).R(touch).key(0.18).key(0.3);
        w.push_back(b.done());
    }
    {  // ---- People ----
        Builder b("I", SignKind::Word, "index finger points to your own chest", "chest");
        const HandKey touch = at(shapePose("Z"), {0, -90, 90}, HandSite::IndexTip, chest + Vec3{0.0, 0.1, 0.07});
        b.R(shifted(touch, {0.0, 0.0, 0.45})).key(0).R(touch).key(0.25).key(0.3);
        w.push_back(b.aliases({"me", "myself"}).done());
    }
    {
        Builder b("YOU", SignKind::Word, "index finger points at the other person", "neutral space");
        const HandKey start = wristAt(shapePose("Z"), {0, 90, 90}, {-0.8, 0.6, 1.5});
        b.R(start).key(0).R(shifted(start, {0.15, 0.05, 0.6})).key(0.35).key(0.3);
        w.push_back(b.aliases({"u"}).done());
    }
    {
        Builder b("MY", SignKind::Word, "flat palm placed on the chest", "chest");
        const HandKey touch = at("flat", {-40, 0, 180}, HandSite::PalmFront, chest + Vec3{-0.1, 0.05, 0.04});
        b.R(shifted(touch, {0.0, 0.0, 0.4})).key(0).R(touch).key(0.3).key(0.3);
        w.push_back(b.aliases({"mine"}).done());
    }
    {
        Builder b("YOUR", SignKind::Word, "flat palm faces the other person and pushes forward", "neutral space");
        const HandKey start = wristAt("flat", {}, {-0.95, 0.55, 1.6});
        b.R(start).key(0).R(shifted(start, {0.1, 0.0, 0.55})).key(0.35).key(0.3);
        w.push_back(b.aliases({"yours"}).done());
    }
    {
        Builder b("NAME", SignKind::Word, "the right H taps across the left H, twice", "neutral space", true);
        const HandKey left = wristAt("H", {-90, 0, 110}, {1.55, 0.45, 1.95}, true);
        Hand3D lh;
        lh.set(left.pose, left.place);
        const Vec3 leftIndex = lh.handMatrix().apply(lerp(lh.chain(Index).point[1], lh.chain(Index).point[2], 0.5));
        const HandKey down = at("H", {-55, 0, 110}, HandSite::FingersFront, leftIndex + Vec3{0.0, 0.22, 0.0});
        const HandKey up = shifted(down, {0.0, 0.22, 0.0});
        b.L(left).R(up).key(0).R(down).key(0.2).R(up).key(0.16).R(down).key(0.16).key(0.3);
        w.push_back(b.done());
    }
    {  // ---- Questions ----
        Builder b("WHAT", SignKind::Word, "both open hands, palms up, shake side to side", "neutral space", true);
        for (int k = 0; k < 5; ++k) {
            const double dx = k % 2 == 0 ? 0.0 : 0.14;
            b.R(wristAt("5open", {0, 90, 180}, {-1.15 - dx, -0.35, 1.9}))
                .L(wristAt("5open", {0, 90, 180}, {1.15 + dx, -0.35, 1.9}, true))
                .key(k == 0 ? 0 : 0.16);
        }
        b.key(0.25);
        w.push_back(b.done());
    }
    {
        Builder b("WHERE", SignKind::Word, "index finger up, wagging side to side", "neutral space");
        for (int k = 0; k < 5; ++k) {
            const double wb = k == 0 || k == 4 ? 0.0 : (k % 2 == 1 ? 18.0 : -18.0);
            b.R(wristAt(shapePose("Z"), {0, 0, 0, wb, 0}, {-1.05, 1.0, 2.0})).key(k == 0 ? 0 : 0.18);
        }
        b.key(0.2);
        w.push_back(b.done());
    }
    {
        Builder b("WHO", SignKind::Word, "thumb of the L hand on the chin; the index finger bends twice", "chin");
        const HandPose l = shapePose("L");
        const HandKey k0 = at(l, {0, 0, 90}, HandSite::ThumbTip, chin + Vec3{0.0, 0.0, 0.1});
        b.R(k0).key(0);
        for (int k = 0; k < 4; ++k) b.R(reshaped(k0, k % 2 == 0 ? withIndexFlex(l, 55, 20) : l)).key(0.18);
        b.key(0.25);
        w.push_back(b.done());
    }
    {
        Builder b("HOW", SignKind::Word, "bent hands, backs together, roll forward until the palms face up",
                  "neutral space", true);
        b.R(at("bent", {0, 90, 0}, HandSite::Knuckles, {-0.12, 0.35, 2.0}))
            .L(at("bent", {0, 90, 0}, HandSite::Knuckles, {0.12, 0.35, 2.0}, true))
            .key(0)
            .key(0.15);
        b.R(at("bent", {0, 90, 180}, HandSite::Knuckles, {-0.2, 0.25, 2.25}))
            .L(at("bent", {0, 90, 180}, HandSite::Knuckles, {0.2, 0.25, 2.25}, true))
            .key(0.5)
            .key(0.3);
        w.push_back(b.done());
    }
    {  // ---- Understanding ----
        Builder b("UNDERSTAND", SignKind::Word, "fist beside the forehead; the index finger flicks up", "forehead");
        const HandKey fist = at("S", {15, 0, 180}, HandSite::Knuckles, temple + Vec3{-0.5, 0.05, 0.3});
        b.R(fist).key(0).key(0.2).R(reshaped(fist, shapePose("Z"))).key(0.2).key(0.35);
        w.push_back(b.aliases({"understood"}).done());
    }
    {
        Builder b("AGAIN", SignKind::Word, "bent right hand flips over into the left palm", "neutral space", true);
        b.L(leftPalmUp);
        b.R(wristAt("bent", {0, 90, 180}, {-1.3, 0.15, 2.0})).key(0);
        b.R(wristAt("bent", {0, 60, 90}, {-0.65, 0.85, 2.0})).key(0.25);
        b.R(at("bent", {0, 90, 0}, HandSite::MiddleTip, leftPalm + Vec3{0.0, 0.1, 0.05})).key(0.25).key(0.3);
        w.push_back(b.aliases({"repeat"}).done());
    }
    {
        Builder b("DEAF", SignKind::Word, "index finger touches near the ear, then near the mouth", "cheek and mouth");
        const HandPose one = shapePose("Z");
        b.R(at(one, {0, -25, 180}, HandSite::IndexTip, {-0.72, 3.95, 0.06})).key(0).key(0.15);
        b.R(at(one, {0, -25, 180}, HandSite::IndexTip, {-0.28, 3.42, 0.44})).key(0.4).key(0.3);
        w.push_back(b.done());
    }
    {
        Builder b("HEARING", SignKind::Word, "index finger across the mouth, circling forward", "mouth");
        const HandPose one = shapePose("Z");
        const Vec3 c = mouth + Vec3{0.05, 0.0, 0.28};
        for (int k = 0; k <= 8; ++k)
            b.R(at(one, {-62, 0, 180}, HandSite::IndexTip, onCircleYZ(c, 0.14, 90.0 - 90.0 * k))).key(k == 0 ? 0 : 0.14);
        b.key(0.25);
        w.push_back(b.done());
    }
    {  // ---- Needs ----
        Builder b("NEED", SignKind::Word, "hooked index finger bends down, twice", "neutral space");
        const HandKey up = wristAt("X", {0, 55, 0}, {-0.95, 0.45, 2.0});
        const HandKey down = shifted(wristAt("X", {0, 55, 0, 0, 35}, {-0.95, 0.45, 2.0}), {0.0, -0.14, 0.0});
        b.R(up).key(0).R(down).key(0.2).R(up).key(0.18).R(down).key(0.18).key(0.3);
        w.push_back(b.aliases({"must", "should"}).done());
    }
    {
        Builder b("WANT", SignKind::Word, "both claw hands, palms up, pull towards the body", "neutral space", true);
        b.R(wristAt("claw", {0, 90, 180}, {-1.15, -0.1, 2.6})).L(wristAt("claw", {0, 90, 180}, {1.15, -0.1, 2.6}, true)).key(0);
        b.R(wristAt("clawTight", {0, 90, 180}, {-1.05, -0.05, 1.75}))
            .L(wristAt("clawTight", {0, 90, 180}, {1.05, -0.05, 1.75}, true))
            .key(0.5)
            .key(0.25);
        w.push_back(b.done());
    }
    {
        Builder b("HELP", SignKind::Word, "thumbs-up fist resting on the left palm; both rise", "neutral space", true);
        const HandKey left = at("flat", {0, 90, 180}, HandSite::PalmFront, {0.35, -0.75, 2.0}, true);
        const Vec3 palm = siteOf(left, HandSite::PalmFront);
        const HandKey right = wristAt("A", {0, 0, 90}, palm + Vec3{-0.05, 0.2, 0.05});
        b.L(left).R(right).key(0).key(0.15);
        b.L(shifted(left, {0, 0.6, 0})).R(shifted(right, {0, 0.6, 0})).key(0.5).key(0.25);
        w.push_back(b.done());
    }
    {
        Builder b("MORE", SignKind::Word, "fingertips of both flat-O hands tap together", "neutral space", true);
        auto pair = [&](double gap) {
            b.R(at("flatO", {-45, 40, 90}, HandSite::MiddleTip, {-gap, 0.55, 2.2}))
                .L(at("flatO", {-45, 40, 90}, HandSite::MiddleTip, {gap, 0.55, 2.2}, true));
        };
        pair(0.45);
        b.key(0);
        pair(0.07);
        b.key(0.22);
        pair(0.4);
        b.key(0.16);
        pair(0.07);
        b.key(0.16).key(0.3);
        w.push_back(b.done());
    }
    {
        Builder b("WATER", SignKind::Word, "index finger of the W hand taps the chin", "chin");
        const HandKey touch = at("W", {0, 0, 90}, HandSite::IndexTip, chin + Vec3{0.0, 0.0, 0.1});
        const HandKey away = shifted(touch, {0.0, 0.0, 0.22});
        b.R(away).key(0).R(touch).key(0.16).R(away).key(0.14).R(touch).key(0.14).key(0.3);
        w.push_back(b.aliases({"drink water"}).done());
    }
    {
        Builder b("BATHROOM", SignKind::Word, "T handshape shakes with a twist", "neutral space");
        for (int k = 0; k < 5; ++k) {
            const double roll = k == 0 || k == 4 ? 0.0 : (k % 2 == 1 ? 28.0 : -22.0);
            b.R(wristAt("T", {0, 0, roll}, {-1.0, 0.95, 2.0})).key(k == 0 ? 0 : 0.16);
        }
        b.key(0.2);
        w.push_back(b.aliases({"toilet", "restroom", "washroom"}).done());
    }
    {  // ---- Feelings ----
        Builder b("I LOVE YOU", SignKind::Word, "thumb, index and little finger out, palm to the viewer",
                  "neutral space");
        const HandKey k0 = wristAt("ILY", {}, {-1.1, 1.15, 2.0});
        b.R(k0).key(0).R(turned(k0, {0, 0, 0, 7, 0})).key(0.3).R(turned(k0, {0, 0, 0, -6, 0})).key(0.3).R(k0).key(0.3);
        w.push_back(b.aliases({"ily", "love you"}).done());
    }
    return w;
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

Vec3 spellingCentre() { return anchorPoint(Anchor::Spelling) + Vec3{0.0, 0.75, 0.15}; }

HandKey restRight() { return wristAt("relaxed", {0, 100, 90}, {-1.55, -2.9, 1.2}); }
HandKey restLeft() { return wristAt("relaxed", {0, 100, 90}, {1.55, -2.9, 1.2}, true); }

const std::vector<Sign3D>& letterSigns() {
    static const std::vector<Sign3D> s = buildLetters();
    return s;
}
const std::vector<Sign3D>& numberSigns() {
    static const std::vector<Sign3D> s = buildNumbers();
    return s;
}
const std::vector<Sign3D>& wordSigns() {
    static const std::vector<Sign3D> s = buildWords();
    return s;
}

const Sign3D* findSign(const std::string& word) {
    const std::string w = lower(word);
    for (const auto* list : {&wordSigns(), &numberSigns(), &letterSigns()}) {
        for (const Sign3D& s : *list) {
            if (lower(s.gloss) == w) return &s;
            for (const std::string& a : s.aliases)
                if (lower(a) == w) return &s;
        }
    }
    return nullptr;
}
