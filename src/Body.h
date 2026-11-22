#pragma once
// Body: the signer's head and torso, used as a REFERENCE for where signs are
// made (Checkpoint 3). It is not an avatar: no face expressions, no moving
// body - just enough shape to show "at the chin", "on the chest", "beside
// the forehead".
//
// Body space (hand units, 1 unit is roughly 9 cm):
//   the signer faces the viewer (+z); the signer's RIGHT side is at -x,
//   so the right hand appears on the viewer's left;
//   y = 0 is the middle of the chest.
//
// Named anchors give every sign a place to start from, so a sign is written
// as "fingertips on the chin, then forward" instead of raw coordinates.

#include <string>
#include <vector>

#include "Math3D.h"
#include "Mesh.h"

enum class Anchor {
    Forehead,
    Temple,     // the signer's right temple (HELLO)
    Eye,
    Nose,
    Mouth,
    Chin,
    Cheek,
    Ear,
    Chest,
    ChestRight,
    Shoulder,   // the signer's right shoulder
    Neutral,    // in front of the body, where most signs happen
    NeutralLow,
    Spelling,   // where fingerspelling is done: beside the shoulder
};

// Position of an anchor in body space (for the signer's right side).
Vec3 anchorPoint(Anchor a);
// The same anchor mirrored to the signer's left side.
Vec3 anchorPointLeft(Anchor a);
const char* anchorName(Anchor a);

// The parts of the body, each a mesh with its own colour.
struct BodyPart {
    std::string name;
    Mesh mesh;
    float r, g, b;  // diffuse colour
    bool isHead;    // part of the head (as opposed to the torso)
};

const std::vector<BodyPart>& bodyParts();

// The shoulder joints (for drawing the upper arms to the elbows).
Vec3 shoulderRight();
Vec3 shoulderLeft();
constexpr double kForearmLength = 2.6;  // wrist to elbow, hand units
