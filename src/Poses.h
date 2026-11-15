#pragma once
// Poses: the ASL handshapes the visualizer can show.
// Each sign is just a set of joint angles. The Hand solves where the joints
// end up, so adding a new sign never requires new drawing code.

#include <string>
#include <vector>

#include "Hand.h"

const std::vector<HandPose>& signLibrary();

// The sign with this name (the first sign if there is none).
const HandPose& signByName(const std::string& name);
