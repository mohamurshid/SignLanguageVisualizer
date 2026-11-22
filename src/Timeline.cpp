#include "Timeline.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

Vec3 catmullRom3(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, double t) {
    const double t2 = t * t, t3 = t2 * t;
    return (p1 * 2.0 + (p2 - p0) * t + (p0 * 2.0 - p1 * 5.0 + p2 * 4.0 - p3) * t2 + (p1 * 3.0 - p0 - p2 * 3.0 + p3) * t3) *
           0.5;
}

bool samePlace(Vec3 a, Vec3 b) { return length(a - b) < 1e-9; }

// One hand between keys i and i + 1 at local fraction u.
HandKey blendHand(const std::vector<Key3D>& keys, size_t i, double u, bool right) {
    const auto& get = [&](size_t k) -> const HandKey& { return right ? keys[k].right : keys[k].left; };
    const HandKey& a = get(i);
    const HandKey& b = get(i + 1);
    const double e = easeInOut(u);
    HandKey out;
    out.pose = lerpPose(a.pose, b.pose, e);
    out.place = lerpPlacement(a.place, b.place, e);
    // Position: a Catmull-Rom curve through the neighbouring keys, so the
    // wrist travels along a smooth path. If the hand holds still between
    // these two keys, keep it exactly still (no spline overshoot).
    const Vec3 p1 = a.place.position, p2 = b.place.position;
    if (samePlace(p1, p2)) {
        out.place.position = p1;
    } else {
        const Vec3 p0 = i > 0 && !samePlace(get(i - 1).place.position, p1) ? get(i - 1).place.position : p1 * 2.0 - p2;
        const Vec3 p3 = i + 2 < keys.size() && !samePlace(get(i + 2).place.position, p2) ? get(i + 2).place.position
                                                                                         : p2 * 2.0 - p1;
        out.place.position = catmullRom3(p0, p1, p2, p3, e);
    }
    return out;
}

}  // namespace

Frame3D sampleTimeline(const std::vector<Key3D>& keys, double t) {
    if (keys.empty()) return {};
    if (t <= keys.front().time || keys.size() == 1) return {keys.front().right, keys.front().left};
    if (t >= keys.back().time) return {keys.back().right, keys.back().left};
    size_t i = 0;
    while (i + 1 < keys.size() && keys[i + 1].time < t) ++i;
    const double span = keys[i + 1].time - keys[i].time;
    const double u = span > 1e-9 ? (t - keys[i].time) / span : 1.0;
    return {blendHand(keys, i, u, true), blendHand(keys, i, u, false)};
}

void appendKeys(std::vector<Key3D>& timeline, const std::vector<Key3D>& next, double blend) {
    if (next.empty()) return;
    const double offset = timeline.empty() ? 0.0 : timeline.back().time + blend - next.front().time;
    for (Key3D k : next) {
        k.time += offset;
        timeline.push_back(k);
    }
}

void TimelinePlayer::start(const std::vector<Key3D>* keys, bool loop) {
    keys_ = keys;
    loop_ = loop;
    time_ = 0.0;
    paused_ = false;
}

bool TimelinePlayer::finished() const { return keys_ && !loop_ && !keys_->empty() && time_ >= keys_->back().time; }

void TimelinePlayer::advance(double seconds) {
    if (!keys_ || paused_ || keys_->empty()) return;
    time_ += seconds * speed_;
    const double end = keys_->back().time;
    if (loop_ && time_ > end + 0.8) time_ = 0.0;  // loop after a short pause
    if (!loop_) time_ = std::min(time_, end);
}

void TimelinePlayer::setSpeed(double s) { speed_ = std::clamp(s, 0.125, 4.0); }

Frame3D TimelinePlayer::frame() const { return keys_ ? sampleTimeline(*keys_, time_) : Frame3D{}; }
