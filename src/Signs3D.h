#pragma once
// Signs3D: the sign dictionary of the final project.
//
//   letters  A-Z   fingerspelling; J and Z trace their shape in the air
//   numbers  0-10  10 is a thumbs-up shaken with a twist
//   words    32    a basic conversation set: greetings, manners, people,
//                  questions, understanding, needs (see docs/roadmap.md)
//
// Every sign is a list of keyframes for both hands (Timeline.h). Word signs
// are written against the BODY: "fingertips on the chin", "palm on the
// chest", "the right H taps the left H". placeSiteAt (Hand3D) turns that into
// wrist positions, so a sign stays correct if the hand's size changes.
//
// The signs follow the common citation forms taught in beginner courses
// (Lifeprint ASL University, Signing Savvy, Start ASL). Many signs have
// regional variants; the one used is the one described in each sign.

#include <string>
#include <vector>

#include "Timeline.h"

constexpr double kHandScale = 1.15;  // hands drawn slightly large so handshapes read clearly

const std::vector<Sign3D>& letterSigns();
const std::vector<Sign3D>& numberSigns();
const std::vector<Sign3D>& wordSigns();

// A sign by its gloss or one of its aliases (any case): "thank you",
// "thanks", "A", "7", "bathroom", "toilet" ... nullptr if none.
const Sign3D* findSign(const std::string& word);

// The hands at rest: lowered in front of the body, outside the picture.
HandKey restRight();
HandKey restLeft();

// Where fingerspelling happens: the palm centre, beside the right shoulder.
Vec3 spellingCentre();
