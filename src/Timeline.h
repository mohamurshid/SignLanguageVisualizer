#pragma once
// Timeline: keyframe animation for TWO hands in 3D (Checkpoint 3).
//
// The Checkpoint 2 animation system, extended:
//   - a keyframe now holds BOTH hands: the dominant (right) hand and the base
//     (left) hand, each with its handshape and its 3D placement;
//   - placements are 3D: position in body space plus arm/wrist angles.
// In-betweening is the same as Checkpoint 2:
//   joint angles and arm angles  a + (b - a) * e,  e = u^2 (3 - 2u)  (eased)
//   wrist positions              Catmull-Rom spline through the keyframes,
//                                held still where two keys are the same
// Several signs are joined into one timeline (a sentence) by appending their
// keyframes one after another with a short blend between them.

#include <string>
#include <vector>

#include "Hand3D.h"

struct HandKey {
    HandPose pose;
    Placement3D place;
};

struct Key3D {
    double time = 0.0;  // seconds from the start
    HandKey right;
    HandKey left;
};

enum class SignKind { Letter, Number, Word };

struct Sign3D {
    std::string gloss;        // the sign's name, e.g. "THANK YOU"
    SignKind kind = SignKind::Word;
    std::string description;  // how it is made
    std::string location;     // where it is made (chin, chest, neutral space, ...)
    bool twoHanded = false;
    std::vector<std::string> aliases;  // other English words for the same sign
    std::vector<Key3D> keys;  // sorted by time, at least one

    double duration() const { return keys.empty() ? 0.0 : keys.back().time; }
};

struct Frame3D {
    HandKey right;
    HandKey left;
};

// The two hands at time t (clamped to the timeline).
Frame3D sampleTimeline(const std::vector<Key3D>& keys, double t);
inline Frame3D sampleSign(const Sign3D& s, double t) { return sampleTimeline(s.keys, t); }

// Appends 'next' after 'timeline', starting 'blend' seconds after its last key.
void appendKeys(std::vector<Key3D>& timeline, const std::vector<Key3D>& next, double blend);

// Time-based playback with a speed control (as in Checkpoint 2).
class TimelinePlayer {
public:
    void start(const std::vector<Key3D>* keys, bool loop);
    void stop() { keys_ = nullptr; }
    bool active() const { return keys_ != nullptr; }
    bool finished() const;
    void advance(double seconds);
    void togglePause() { paused_ = !paused_; }
    bool paused() const { return paused_; }
    void setSpeed(double s);
    double speed() const { return speed_; }
    double time() const { return time_; }
    void seek(double t) { time_ = t; }
    Frame3D frame() const;

private:
    const std::vector<Key3D>* keys_ = nullptr;
    double time_ = 0.0;
    double speed_ = 1.0;
    bool loop_ = false;
    bool paused_ = false;
};
