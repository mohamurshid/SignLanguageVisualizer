#include "Poses.h"

namespace {

// A straight finger, rotated sideways by 'spread' degrees
// (+ve spreads towards the pinky side, -ve towards the thumb side).
FingerPose straight(double spread) {
    FingerPose f;
    f.bend = {spread, 0.0, 0.0};
    return f;
}

// A finger curled into the palm (knuckle, middle and end joints flexed).
FingerPose curled() {
    FingerPose f;
    f.flex = {88.0, 95.0, 55.0};
    return f;
}

// The thumb needs individual control of every angle.
FingerPose thumb(double bendCMC, double bendMCP, double bendIP,
                 double flexCMC, double flexMCP, double flexIP) {
    FingerPose f;
    f.bend = {bendCMC, bendMCP, bendIP};
    f.flex = {flexCMC, flexMCP, flexIP};
    return f;
}

// Common thumb positions.
FingerPose thumbOut(double bend) { return thumb(bend, 5, 0, 0, 0, 0); }   // sticking out sideways
FingerPose thumbAcrossPalm() { return thumb(62, 30, 20, 35, 30, 20); }    // folded across the palm
FingerPose thumbBeside() { return thumb(35, 5, 0, 15, 10, 10); }          // up against the index finger
FingerPose thumbOverFingers(double reach) {                               // holding curled fingers down
    return thumb(reach, 35, 25, 30, 25, 15);
}

HandPose make(const char* name, const char* description, FingerPose thumbPose,
              FingerPose index, FingerPose middle, FingerPose ring, FingerPose pinky) {
    HandPose p;
    p.name = name;
    p.description = description;
    p.fingers = {thumbPose, index, middle, ring, pinky};
    return p;
}

}  // namespace

const std::vector<HandPose>& signLibrary() {
    static const std::vector<HandPose> library = {
        make("5", "open hand, all fingers spread",
             thumb(-12, 6, 6, 0, 0, 0), straight(-10), straight(0), straight(9), straight(18)),
        make("A", "fist, thumb against the side of the index finger",
             thumbBeside(), curled(), curled(), curled(), curled()),
        make("B", "flat hand, fingers together, thumb across the palm",
             thumbAcrossPalm(), straight(-1), straight(0), straight(1), straight(2)),
        make("L", "index up, thumb out: an L shape",
             thumbOut(-38), straight(0), curled(), curled(), curled()),
        make("V", "index and middle fingers spread in a V",
             thumbOverFingers(70), straight(-10), straight(8), curled(), curled()),
        make("W", "index, middle and ring fingers spread",
             thumbOverFingers(85), straight(-12), straight(0), straight(12), curled()),
        make("Y", "thumb and pinky out, other fingers curled",
             thumbOut(-22), curled(), curled(), curled(), straight(25)),
        make("ILY", "\"I love you\": thumb, index and pinky out",
             thumbOut(-18), straight(-4), curled(), curled(), straight(20)),
        make("I", "fist with only the pinky up, thumb across the fingers",
             thumbOverFingers(80), curled(), curled(), curled(), straight(8)),
    };
    return library;
}
