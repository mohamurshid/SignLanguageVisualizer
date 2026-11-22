#include "Handshapes.h"

#include <cmath>

#include "Hand3D.h"

namespace {

FingerPose straight(double spread = 0.0) {
    FingerPose f;
    f.bend = {spread, 0.0, 0.0};
    return f;
}

FingerPose flexed(double knuckle, double middle, double end, double spread = 0.0) {
    FingerPose f;
    f.bend = {spread, 0.0, 0.0};
    f.flex = {knuckle, middle, end};
    return f;
}

FingerPose curled() { return flexed(88.0, 98.0, 58.0); }  // into the palm (a fist)

FingerPose thumb(double bendCMC, double bendMCP, double bendIP, double flexCMC, double flexMCP, double flexIP) {
    FingerPose f;
    f.bend = {bendCMC, bendMCP, bendIP};
    f.flex = {flexCMC, flexMCP, flexIP};
    return f;
}

// Common thumb positions (3D-tuned).
FingerPose thumbOut(double spread) { return thumb(spread, 4.0, 0.0, 0.0, 0.0, 0.0); }  // out to the side
FingerPose thumbBeside() { return thumb(30.0, 2.0, 0.0, 6.0, 4.0, 4.0); }               // along the index (A)
FingerPose thumbAlong() { return thumb(34.0, 0.0, 0.0, 14.0, 6.0, 4.0); }               // flat hand, thumb in
FingerPose thumbTucked() { return thumb(58.0, 32.0, 20.0, 48.0, 34.0, 26.0); }          // across the palm (B, 4)
FingerPose thumbOverFingers() { return thumb(66.0, 34.0, 24.0, 52.0, 30.0, 18.0); }     // holds curled fingers
FingerPose thumbAcrossFront() { return thumb(70.0, 40.0, 26.0, 62.0, 28.0, 16.0); }     // in front of a fist (S)

HandPose pose(FingerPose t, FingerPose i, FingerPose m, FingerPose r, FingerPose p) {
    HandPose h;
    h.fingers = {t, i, m, r, p};
    return h;
}

Handshape shape(const char* name, const char* description, HandPose p, ShapeOrientation o = {}) {
    p.name = name;
    p.description = description;
    return {name, description, p, o};
}

// Thumb tip meets the tip of 'finger'.
HandPose touching(HandPose p, int finger, double reach = 1.0) { return touchThumbTo(p, finger, reach); }

const ShapeOrientation kSideView{0.0, 0.0, 58.0};    // C, O: turned so the curve shows from the side
const ShapeOrientation kSideways{-90.0, 0.0, 180.0};  // G, H: pointing across the body
const ShapeOrientation kPalmIn{0.0, 0.0, 180.0};     // numbers 1-5 face the signer

}  // namespace

const std::vector<Handshape>& letterShapes() {
    static const std::vector<Handshape> letters = {
        shape("A", "fist, thumb straight up against the side of the index finger",
              pose(thumbBeside(), curled(), curled(), curled(), curled())),
        shape("B", "flat hand, fingers together and straight, thumb folded across the palm",
              pose(thumbTucked(), straight(-2), straight(0), straight(2), straight(4))),
        shape("C", "fingers and thumb curved into the shape of a C",
              pose(thumb(-18, 10, 10, 58, 16, 12), flexed(32, 46, 30, -2), flexed(32, 46, 30), flexed(32, 46, 30, 2),
                   flexed(32, 46, 30, 4)),
              kSideView),
        shape("D", "index up; the other fingers curve round to touch the thumb",
              touching(pose(thumb(20, 10, 10, 40, 20, 10), straight(0), flexed(55, 70, 40), flexed(55, 70, 40, 3),
                            flexed(55, 70, 40, 6)),
                       Middle)),
        shape("E", "fingertips bent down over the thumb, thumb tucked under",
              pose(thumb(64, 34, 24, 48, 34, 26), flexed(55, 112, 68), flexed(55, 112, 68), flexed(55, 112, 68),
                   flexed(55, 112, 68))),
        shape("F", "index and thumb touch in a circle, the other three fingers up and spread",
              touching(pose(thumb(10, 10, 10, 30, 20, 10), flexed(40, 62, 40), straight(2), straight(10), straight(18)),
                       Index)),
        shape("G", "index and thumb point sideways, parallel, the other fingers curled",
              pose(thumb(42, 0, 0, 8, 0, 0), straight(0), curled(), curled(), curled()), kSideways),
        shape("H", "index and middle point sideways together, thumb tucked",
              pose(thumbOverFingers(), straight(-1), straight(1), curled(), curled()), kSideways),
        shape("I", "little finger up, thumb across the curled fingers",
              pose(thumbOverFingers(), curled(), curled(), curled(), straight(8))),
        shape("J", "the I handshape, tracing a J with the little finger",
              pose(thumbOverFingers(), curled(), curled(), curled(), straight(8))),
        shape("K", "index up, middle finger angled forward, thumb between them",
              pose(thumb(36, 14, 0, 42, 10, 0), straight(-4), flexed(38, 6, 0, 8), curled(), curled())),
        shape("L", "index up and thumb out: an L", pose(thumbOut(-42), straight(0), curled(), curled(), curled())),
        shape("M", "three fingers folded over the tucked thumb",
              pose(thumb(78, 26, 18, 54, 18, 10), flexed(76, 72, 30), flexed(76, 72, 30), flexed(76, 72, 30), curled())),
        shape("N", "two fingers folded over the tucked thumb",
              pose(thumb(62, 26, 18, 54, 18, 10), flexed(76, 72, 30), flexed(76, 72, 30), curled(), curled())),
        shape("O", "all fingertips curve round to meet the thumb: an O",
              touching(pose(thumb(-2, 10, 10, 40, 22, 14), flexed(42, 62, 44, -4), flexed(44, 62, 44, -1),
                            flexed(44, 62, 44, 3), flexed(46, 62, 44, 8)),
                       Index),
              kSideView),
        shape("P", "the K handshape pointing down",
              pose(thumb(36, 14, 0, 42, 10, 0), straight(-4), flexed(38, 6, 0, 8), curled(), curled()),
              {0.0, 100.0, 60.0, 0.6}),
        shape("Q", "the G handshape pointing down",
              pose(thumb(42, 0, 0, 8, 0, 0), straight(0), curled(), curled(), curled()), {0.0, 100.0, 60.0, 0.6}),
        shape("R", "index and middle fingers crossed",
              pose(thumbOverFingers(), flexed(-6, 0, 0, 10), flexed(10, 0, 0, -12), curled(), curled())),
        shape("S", "fist with the thumb across the front of the fingers",
              pose(thumbAcrossFront(), curled(), curled(), curled(), curled())),
        shape("T", "fist with the thumb tucked between the index and middle fingers",
              pose(thumb(40, 12, 6, 50, 14, 6), flexed(78, 84, 46), curled(), curled(), curled())),
        shape("U", "index and middle fingers up, together",
              pose(thumbOverFingers(), straight(-1), straight(1), curled(), curled())),
        shape("V", "index and middle fingers up, spread in a V",
              pose(thumbOverFingers(), straight(-11), straight(9), curled(), curled())),
        shape("W", "index, middle and ring fingers up and spread; thumb holds the little finger",
              touching(pose(thumb(60, 30, 20, 50, 30, 20), straight(-13), straight(0), straight(12),
                            flexed(70, 85, 50)),
                       Pinky, 0.95)),
        shape("X", "index finger hooked, the other fingers curled",
              pose(thumbOverFingers(), flexed(14, 92, 62), curled(), curled(), curled())),
        shape("Y", "thumb and little finger out, the others curled",
              pose(thumbOut(-30), curled(), curled(), curled(), straight(26))),
        shape("Z", "index finger up, tracing a Z", pose(thumbOverFingers(), straight(0), curled(), curled(), curled())),
    };
    return letters;
}

const std::vector<Handshape>& numberShapes() {
    static const std::vector<Handshape> numbers = {
        shape("0", "all fingertips meet the thumb in an O",
              touching(pose(thumb(-2, 10, 10, 40, 22, 14), flexed(42, 62, 44, -4), flexed(44, 62, 44, -1),
                            flexed(44, 62, 44, 3), flexed(46, 62, 44, 8)),
                       Index),
              kSideView),
        shape("1", "index finger up, palm facing the signer",
              pose(thumbOverFingers(), straight(0), curled(), curled(), curled()), kPalmIn),
        shape("2", "index and middle fingers up, palm facing the signer",
              pose(thumbOverFingers(), straight(-11), straight(9), curled(), curled()), kPalmIn),
        shape("3", "thumb, index and middle fingers out, palm facing the signer",
              pose(thumbOut(-34), straight(-9), straight(7), curled(), curled()), kPalmIn),
        shape("4", "four fingers up and spread, thumb across the palm, palm facing the signer",
              pose(thumbTucked(), straight(-9), straight(-2), straight(5), straight(12)), kPalmIn),
        shape("5", "all five fingers spread, palm facing the signer",
              pose(thumbOut(-16), straight(-10), straight(0), straight(9), straight(18)), kPalmIn),
        shape("6", "thumb touches the little finger; three fingers up",
              touching(pose(thumb(60, 30, 20, 50, 30, 20), straight(-13), straight(0), straight(12),
                            flexed(70, 85, 50)),
                       Pinky, 0.95)),
        shape("7", "thumb touches the ring finger; the others up",
              touching(pose(thumb(50, 30, 20, 50, 30, 20), straight(-12), straight(-2), flexed(70, 85, 50, 4),
                            straight(20)),
                       Ring, 0.95)),
        shape("8", "thumb touches the middle finger; the others up",
              touching(pose(thumb(40, 30, 20, 45, 30, 20), straight(-14), flexed(66, 85, 50), straight(8),
                            straight(18)),
                       Middle, 0.95)),
        shape("9", "thumb touches the index finger; the others up",
              touching(pose(thumb(10, 10, 10, 30, 20, 10), flexed(40, 62, 40), straight(2), straight(10), straight(18)),
                       Index)),
        shape("10", "thumbs-up fist, shaken with a twist of the wrist",
              pose(thumb(32, 0, 0, 0, 0, 0), curled(), curled(), curled(), curled()), {0.0, 0.0, 90.0}),
    };
    return numbers;
}

const std::vector<Handshape>& extraShapes() {
    static const std::vector<Handshape> extras = {
        shape("5open", "open hand, fingers spread",
              pose(thumbOut(-16), straight(-10), straight(0), straight(9), straight(18))),
        shape("flat", "flat hand, fingers together, thumb alongside",
              pose(thumbAlong(), straight(-2), straight(0), straight(2), straight(4))),
        shape("bent", "flat hand bent at the knuckles",
              pose(thumbAlong(), flexed(72, 4, 2, -2), flexed(72, 4, 2), flexed(72, 4, 2, 2), flexed(72, 4, 2, 4))),
        shape("claw", "fingers spread and bent like claws",
              pose(thumb(-10, 20, 20, 25, 25, 20), flexed(25, 62, 40, -10), flexed(25, 62, 40),
                   flexed(25, 62, 40, 9), flexed(25, 62, 40, 18))),
        shape("clawTight", "claw closing towards a fist",
              pose(thumb(5, 25, 25, 35, 30, 25), flexed(45, 80, 50, -8), flexed(45, 80, 50), flexed(45, 80, 50, 7),
                   flexed(45, 80, 50, 14))),
        shape("flatO", "fingertips together on the thumb tip",
              touching(pose(thumb(6, 10, 10, 34, 16, 10), flexed(52, 22, 18, 4), flexed(54, 22, 18), flexed(54, 22, 18, -3),
                            flexed(56, 22, 18, -6)),
                       Middle)),
        shape("relaxed", "relaxed hand", pose(thumb(10, 8, 6, 10, 10, 6), flexed(14, 22, 12), flexed(16, 24, 12),
                                           flexed(18, 26, 14), flexed(20, 28, 14))),
        shape("noOpen", "index and middle together and straight, thumb out",
              pose(thumbOut(-30), straight(-1), straight(1), curled(), curled())),
        shape("noClosed", "index and middle closed down onto the thumb",
              touching(pose(thumb(10, 10, 10, 30, 20, 10), flexed(48, 18, 12, -1), flexed(48, 18, 12, 1), curled(),
                            curled()),
                       Index)),
        shape("ILY", "\"I love you\": thumb, index and little finger out",
              pose(thumbOut(-22), straight(-4), curled(), curled(), straight(20))),
    };
    return extras;
}

const Handshape& handshape(const std::string& name) {
    for (const auto* list : {&letterShapes(), &numberShapes(), &extraShapes()})
        for (const Handshape& h : *list)
            if (h.name == name) return h;
    return extraShapes().front();
}

const Handshape& closestHandshape(const HandPose& p, double* averageDifference) {
    const Handshape* best = &letterShapes().front();
    double bestScore = 1e18;
    for (const auto* list : {&letterShapes(), &numberShapes()})
        for (const Handshape& h : *list) {
            double sum = 0.0;
            for (int f = 0; f < kFingerCount; ++f)
                for (int b = 0; b < kBones; ++b)
                    sum += std::fabs(p.fingers[f].flex[b] - h.pose.fingers[f].flex[b]) +
                           std::fabs(p.fingers[f].bend[b] - h.pose.fingers[f].bend[b]);
            const double score = sum / (2.0 * kFingerCount * kBones);
            if (score < bestScore) {
                bestScore = score;
                best = &h;
            }
        }
    if (averageDifference) *averageDifference = bestScore;
    return *best;
}
