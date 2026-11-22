#pragma once
// Handshapes: every handshape the 3D visualizer uses (Checkpoint 3).
//
//   - the 26 letters of the ASL manual alphabet (J and Z are a handshape
//     plus a movement, see Signs3D)
//   - the numbers 0-10
//   - extra shapes used inside word signs: flat hand, bent hand, claw,
//     flat-O, ...
//
// A handshape is still just a list of joint angles (HandPose), exactly as in
// Checkpoints 1 and 2. Some letters are also turned: C and O are seen from
// the side, G and H point sideways, P and Q point down, and the numbers 1-5
// face the signer. That turn is stored with the shape (ShapeOrientation) and
// applied when the letter is placed in front of the body.
//
// Where the thumb has to touch a fingertip (O, D, F, 6, 7, 8, 9, flat-O) the
// angles are not typed in by hand: touchThumbTo (Hand3D) searches for thumb
// angles that make the two tips meet.

#include <string>
#include <vector>

#include "Hand.h"

struct ShapeOrientation {
    double turn = 0.0;  // see Placement3D
    double tip = 0.0;
    double roll = 0.0;
    double lower = 0.0;  // held this much lower than usual (P and Q point down)
};

struct Handshape {
    std::string name;
    std::string description;
    HandPose pose;
    ShapeOrientation orientation;  // how it is held when shown on its own
};

// All letters A-Z (in order), the numbers 0-10 (in order) and the extra shapes.
const std::vector<Handshape>& letterShapes();
const std::vector<Handshape>& numberShapes();
const std::vector<Handshape>& extraShapes();

// Any handshape by name ("A", "7", "flat", "claw", ...). Unknown -> the open "5".
const Handshape& handshape(const std::string& name);
inline const HandPose& shapePose(const std::string& name) { return handshape(name).pose; }

// The handshape in the library closest to a pose (Build mode's "looks
// like" hint): the smallest average difference of all joint angles.
const Handshape& closestHandshape(const HandPose& pose, double* averageDifference = nullptr);
