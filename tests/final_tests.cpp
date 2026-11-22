// final_tests: automated checks for the 3D version (Checkpoint 3 and the
// final project). Called from render_test, so one program checks everything.
// None of these need a window or OpenGL: they check the maths, the hand, the
// meshes, the sign dictionary and the speller directly.

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

#include "Body.h"
#include "Camera.h"
#include "GifWriter.h"
#include "Hand3D.h"
#include "HandMesh.h"
#include "Handshapes.h"
#include "Math3D.h"
#include "Mesh.h"
#include "Poses.h"
#include "Signs3D.h"
#include "Speller.h"
#include "Timeline.h"

namespace {

using Check = std::function<void(bool, const std::string&)>;

bool close(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
bool close(Vec3 a, Vec3 b, double eps = 1e-9) { return length(a - b) <= eps; }

bool isIdentity(const Mat4& m, double eps = 1e-9) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (!close(m.m[i][j], i == j ? 1.0 : 0.0, eps)) return false;
    return true;
}

void mathTests(const Check& check) {
    std::printf("3D maths (Math3D)\n");
    check(close(xf3::rotateZ(90).apply({1, 0, 0}), {0, 1, 0}), "rotateZ(90): (1,0,0) -> (0,1,0)");
    check(close(xf3::rotateX(90).apply({0, 1, 0}), {0, 0, 1}), "rotateX(90): (0,1,0) -> (0,0,1)");
    check(close(xf3::rotateY(90).apply({0, 0, 1}), {1, 0, 0}), "rotateY(90): (0,0,1) -> (1,0,0)");
    const Mat4 rod = xf3::rotateAxis({0, 0, 1}, 37.0);
    const Mat4 rz = xf3::rotateZ(37.0);
    bool same = true;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) same = same && close(rod.m[i][j], rz.m[i][j], 1e-12);
    check(same, "Rodrigues' rotation about z equals rotateZ");
    const Mat4 m = xf3::translate(1.5, -2.0, 0.7) * xf3::rotateY(33) * xf3::rotateX(-71) * xf3::scale(1.3, 0.8, 2.1);
    check(isIdentity(m * m.inverse(), 1e-9) && isIdentity(m.inverse() * m, 1e-9), "4x4 inverse: M * M^-1 = I");
    check(xf3::reflectX().determinant() < 0 && m.determinant() > 0, "a reflection has a negative determinant");
    check(close(xf3::translate(1, 2, 3).applyVector({1, 0, 0}), {1, 0, 0}), "directions ignore translation (w = 0)");
    const Mat4 view = xf3::lookAt({0, 0, 10}, {0, 0, 0}, {0, 1, 0});
    check(close(view.apply({0, 0, 10}), {0, 0, 0}) && close(view.apply({0, 0, 0}), {0, 0, -10}),
          "lookAt: the eye goes to the origin, the target to (0, 0, -distance)");
    const Mat4 proj = xf3::perspective(60, 1.5, 1.0, 100.0);
    const Vec4 nearP = proj.apply4({0, 0, -1, 1}), farP = proj.apply4({0, 0, -100, 1});
    check(close(nearP.z / nearP.w, -1.0, 1e-9) && close(farP.z / farP.w, 1.0, 1e-9), "perspective: near -> -1, far -> +1");
    double px, py, depth;
    xf3::project(proj * view, {0, 0, 0}, 800, 600, px, py, depth);
    check(close(px, 400, 1e-6) && close(py, 300, 1e-6), "project: the target lands in the centre of the window");
    MatrixStack4 stack;
    stack.push();
    stack.translate(1, 2, 3);
    stack.rotateZ(45);
    stack.pop();
    stack.pop();  // popping the last matrix does nothing
    check(stack.depth() == 1 && isIdentity(stack.top()), "MatrixStack4: push / pop restore the matrix");
    const auto cm = xf3::translate(4, 5, 6).toColumnMajor();
    check(cm[12] == 4 && cm[13] == 5 && cm[14] == 6, "toColumnMajor: translation in elements 12-14 (OpenGL order)");
}

void hand3dTests(const Check& check) {
    std::printf("3D hand (Hand3D)\n");
    // With no sideways bend after the knuckle, the 3D hand seen from the front
    // (x and y only) is exactly the Checkpoint 2 hand.
    bool matches = true;
    int compared = 0;
    for (const HandPose& p : signLibrary()) {
        Hand h2;
        h2.setPose(p);
        Hand3D h3;
        h3.set(p, Placement3D{});
        for (int f = Index; f <= Pinky; ++f) {
            if (p.fingers[f].bend[1] != 0.0 || p.fingers[f].bend[2] != 0.0) continue;
            for (int k = 0; k < kPoints; ++k) {
                const Vec2 a = h2.chain(f).point[k];
                const Vec3 b = h3.chain(f).point[k];
                matches = matches && close(a.x, b.x, 1e-9) && close(a.y, b.y, 1e-9);
                ++compared;
            }
        }
    }
    check(matches && compared > 200, "3D fingers seen from the front = the 2D fingers (" + std::to_string(compared) + " joints)");

    Hand3D fist;
    fist.set(shapePose("A"), Placement3D{});
    check(fist.chain(Index).point[2].z > 0.1, "a curled finger curls towards the palm side (+z)");

    Placement3D pl;
    pl.turn = 20;
    pl.tip = -30;
    pl.roll = 140;
    pl.scale = 1.15;
    const Vec3 target{-0.3, 3.0, 0.5};
    pl = placeSiteAt(shapePose("flat"), pl, HandSite::MiddleTip, target);
    Hand3D placed;
    placed.set(shapePose("flat"), pl);
    check(close(placed.worldSite(HandSite::MiddleTip), target, 1e-9), "placeSiteAt puts the fingertip exactly on the target");

    Placement3D l = pl;
    l.left = true;
    check(l.handMatrix().determinant() < 0, "a left hand is a mirror image (negative determinant)");

    const Handshape o = handshape("O");
    Hand3D oh;
    oh.set(o.pose, Placement3D{});
    const double contact = 0.5 * (oh.shape().spec(Thumb).width[3] + oh.shape().spec(Index).width[3]);
    const double gap = length(oh.chain(Thumb).point[3] - oh.chain(Index).point[3]);
    check(std::fabs(gap - contact) < 0.02, "O: the thumb search makes the thumb and index tips touch");

    // Two-bone arm: the elbow keeps both arm lengths.
    bool lengthsKept = true;
    const Vec3 wrists[3] = {{-1.15, 1.25, 1.85}, {-0.5, 3.0, 0.6}, {-1.5, -2.6, 1.2}};
    for (const Vec3& w : wrists) {
        const Vec3 e = solveElbow(shoulderRight(), w, false);
        lengthsKept = lengthsKept && close(length(e - shoulderRight()), kUpperArmLength, 1e-6) &&
                      close(length(w - e), kForearmLength, 1e-6);
    }
    check(lengthsKept, "arm IK: the elbow keeps the upper arm and forearm lengths");
    check(solveElbow(shoulderRight(), wrists[0], false).y < shoulderRight().y, "arm IK: the elbow hangs below the shoulder");
}

void meshTests(const Check& check) {
    std::printf("Meshes (Mesh, HandMesh)\n");
    const Mesh s = meshes::sphere(16, 10);
    bool outward = true;
    for (size_t i = 0; i < s.vertexCount(); ++i) outward = outward && close(length(s.normal(i)), 1.0, 1e-6);
    bool ccw = true;  // each triangle's winding normal points away from the centre
    for (size_t t = 0; t < s.triangleCount(); ++t) {
        const Vec3 a = s.position(s.indices[3 * t]), b = s.position(s.indices[3 * t + 1]), c = s.position(s.indices[3 * t + 2]);
        ccw = ccw && dot(cross(b - a, c - a), a + b + c) > 0;
    }
    check(outward && ccw, "sphere: unit normals, triangles wound anticlockwise from outside");

    const Mesh f = meshes::frustum(0.1, 0.08, 0.5, 12);
    bool fOut = true;
    for (size_t t = 0; t < f.triangleCount(); ++t) {
        const Vec3 a = f.position(f.indices[3 * t]), b = f.position(f.indices[3 * t + 1]), c = f.position(f.indices[3 * t + 2]);
        const Vec3 centre = (a + b + c) * (1.0 / 3.0);
        fOut = fOut && dot(cross(b - a, c - a), Vec3{0, centre.y, centre.z}) > 0;
    }
    check(fOut, "bone tube: triangles face outwards");

    // An L-shaped (concave) polygon: ear clipping gives n - 2 triangles
    // whose areas add up to the polygon's area.
    const std::vector<Vec2> ell{{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}};
    const std::vector<int> tris = meshes::triangulate(ell);
    double area = 0.0;
    for (size_t t = 0; t + 2 < tris.size(); t += 3) {
        const Vec2 a = ell[tris[t]], b = ell[tris[t + 1]], c = ell[tris[t + 2]];
        area += 0.5 * ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
    }
    check(tris.size() == 3 * (ell.size() - 2) && close(area, 3.0, 1e-9), "ear clipping: concave L -> 4 triangles, area 3");

    const HandMesh& hm = HandMesh::get();
    check(hm.palm.triangleCount() > 200 && hm.forearm.triangleCount() > 100, "hand meshes built (palm, forearm)");
    bool frontFlat = false;
    for (size_t i = 0; i < hm.palm.vertexCount(); ++i)
        if (close(hm.palm.normal(i).z, 1.0, 1e-6)) frontFlat = true;
    check(frontFlat, "palm: the front face has normals (0, 0, 1)");

    Mesh m;
    m.append(meshes::sphere(8, 6), xf3::reflectX());
    bool flipped = true;
    for (size_t t = 0; t < m.triangleCount(); ++t) {
        const Vec3 a = m.position(m.indices[3 * t]), b = m.position(m.indices[3 * t + 1]), c = m.position(m.indices[3 * t + 2]);
        flipped = flipped && dot(cross(b - a, c - a), a + b + c) > 0;
    }
    check(flipped, "a mirrored mesh has its winding reversed, so it still faces outwards");
}

void dictionaryTests(const Check& check) {
    std::printf("Sign dictionary (Handshapes, Signs3D)\n");
    bool alphabet = letterSigns().size() == 26;
    for (int i = 0; i < 26 && alphabet; ++i) alphabet = letterSigns()[i].gloss == std::string(1, static_cast<char>('A' + i));
    check(alphabet, "26 letters, A to Z in order");
    bool numbers = numberSigns().size() == 11;
    for (int i = 0; i <= 10 && numbers; ++i) numbers = numberSigns()[i].gloss == std::to_string(i);
    check(numbers, "11 numbers, 0 to 10");
    check(wordSigns().size() >= 30, "conversation set: " + std::to_string(wordSigns().size()) + " word signs");

    bool sorted = true, positive = true;
    for (const auto* list : {&letterSigns(), &numberSigns(), &wordSigns()})
        for (const Sign3D& s : *list) {
            positive = positive && s.duration() > 0.0;
            for (size_t k = 1; k < s.keys.size(); ++k) sorted = sorted && s.keys[k].time >= s.keys[k - 1].time;
        }
    check(sorted && positive, "every sign: keyframes in time order, length > 0");

    check(findSign("thanks") && findSign("thanks")->gloss == "THANK YOU", "findSign: 'thanks' -> THANK YOU");
    check(findSign("Toilet") && findSign("Toilet")->gloss == "BATHROOM", "findSign: 'Toilet' -> BATHROOM (any case)");
    check(findSign("seven") && findSign("seven")->gloss == "7", "findSign: 'seven' -> 7");
    check(findSign("xylophone") == nullptr, "findSign: unknown word -> nothing");

    // THANK YOU starts with the middle fingertip on the chin.
    const Sign3D* ty = findSign("thank you");
    Hand3D h;
    h.set(ty->keys.front().right.pose, ty->keys.front().right.place);
    check(close(h.worldSite(HandSite::MiddleTip), anchorPoint(Anchor::Chin) + Vec3{0.0, -0.05, 0.12}, 1e-9),
          "THANK YOU: the fingertip starts on the chin");

    int twoHanded = 0;
    for (const Sign3D& s : wordSigns()) twoHanded += s.twoHanded ? 1 : 0;
    check(twoHanded >= 8, std::to_string(twoHanded) + " two-handed signs");

    check(closestHandshape(shapePose("B")).name == "B" && closestHandshape(shapePose("L")).name == "L",
          "Build mode hint: a letter's own pose is closest to itself");
}

void timelineTests(const Check& check) {
    std::printf("Two-hand timeline (Timeline)\n");
    const Sign3D* yes = findSign("yes");
    const Frame3D first = sampleSign(*yes, -1.0), last = sampleSign(*yes, 99.0);
    check(close(first.right.place.wristFlex, yes->keys.front().right.place.wristFlex) &&
              close(last.right.place.wristFlex, yes->keys.back().right.place.wristFlex),
          "sampling before / after the sign gives the first / last key");
    const Frame3D mid = sampleSign(*yes, 0.11);  // half way to the first nod
    check(mid.right.place.wristFlex > 5.0 && mid.right.place.wristFlex < 37.0, "between keys the wrist is part way");
    // A hold (two identical keys) stays perfectly still.
    std::vector<Key3D> keys = {{0.0, restRight(), restLeft()}, {1.0, restRight(), restLeft()}};
    keys[0].right.place.position = keys[1].right.place.position = {1, 2, 3};
    check(close(sampleTimeline(keys, 0.37).right.place.position, {1, 2, 3}), "a hold stays exactly still");
    std::vector<Key3D> joined = yes->keys;
    appendKeys(joined, findSign("no")->keys, 0.3);
    check(close(joined[yes->keys.size()].time, yes->duration() + 0.3, 1e-9), "appendKeys: the next sign starts after the blend");
}

void spellerTests(const Check& check) {
    std::printf("Type a sentence (Speller)\n");
    const std::vector<std::string> t = tokenize("Nice to meet YOU!");
    check(t.size() == 4 && t[0] == "nice" && t[3] == "you", "tokenize: 'Nice to meet YOU!' -> nice to meet you");
    const Performance p = perform("nice to meet you");
    check(p.gloss.size() == 3 && p.gloss[0] == "NICE" && p.gloss[1] == "MEET" && p.gloss[2] == "YOU",
          "'nice to meet you' -> NICE MEET YOU ('to' is left out)");
    const Performance n = perform("hello my name abdalla");
    check(n.gloss.size() == 4 && n.gloss[3] == "A-B-D-A-L-L-A" && n.parts.size() == 10,
          "'hello my name abdalla' -> HELLO MY NAME A-B-D-A-L-L-A (10 parts)");
    bool ordered = true;
    for (size_t i = 0; i < n.parts.size(); ++i) {
        ordered = ordered && n.parts[i].start < n.parts[i].end && n.parts[i].end <= n.duration();
        if (i) ordered = ordered && n.parts[i].start > n.parts[i - 1].end;
    }
    check(ordered, "parts are in order and inside the performance");
    // The second L of ABDALLA slides sideways.
    const Frame3D l1 = sampleTimeline(n.keys, n.parts[7].start), l2 = sampleTimeline(n.keys, n.parts[8].start);
    check(close(l2.right.place.position.x - l1.right.place.position.x, -SpellerTiming{}.doubleLetterSlide, 1e-9),
          "a double letter slides sideways (LL)");
    const Frame3D start = sampleTimeline(n.keys, 0.0), end = sampleTimeline(n.keys, n.duration());
    check(close(start.right.place.position, restRight().place.position) && close(end.left.place.position, restLeft().place.position),
          "the performance starts and ends with the hands at rest");
    check(perform("I'm deaf").gloss == std::vector<std::string>({"I", "DEAF"}), "\"I'm deaf\" -> I DEAF");
    check(perform("a").gloss == std::vector<std::string>({"A"}), "'a' on its own is the letter A");
    check(perform("25").gloss == std::vector<std::string>({"2-5"}), "'25' -> 2-5 (digit by digit); 0-10 have signs");
    check(perform("thank you seven").gloss == std::vector<std::string>({"THANK YOU", "7"}), "phrases: 'thank you' is one sign");
    check(n.partAt(n.parts[3].start + 0.01) == 3 && n.partAt(-1.0) == -1, "partAt finds the part being signed");
}

void quantizeTests(const Check& check) {
    std::printf("GIF colours (median cut)\n");
    Framebuffer fb(64, 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            fb.setPixel(x, y, {static_cast<std::uint8_t>(x * 4), static_cast<std::uint8_t>(y * 4), static_cast<std::uint8_t>(128)});
    const std::vector<Color> pal = medianCutPalette(fb, 256);
    double worst = 0.0;
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const Color c = fb.getPixel(x, y);
            double best = 1e9;
            for (const Color& p : pal)
                best = std::min(best, std::sqrt(std::pow(c.r - p.r, 2.0) + std::pow(c.g - p.g, 2.0) + std::pow(c.b - p.b, 2.0)));
            worst = std::max(worst, best);
        }
    check(pal.size() <= 256 && pal.size() > 200 && worst < 20.0,
          "median cut: 4096 colours -> " + std::to_string(pal.size()) + " palette colours, worst error " +
              std::to_string(static_cast<int>(worst)));
}

}  // namespace

void finalTests(const std::function<void(bool, const std::string&)>& check) {
    mathTests(check);
    hand3dTests(check);
    meshTests(check);
    dictionaryTests(check);
    timelineTests(check);
    spellerTests(check);
    quantizeTests(check);
}
