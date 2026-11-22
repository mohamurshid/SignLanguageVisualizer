#include "Body.h"

Vec3 anchorPoint(Anchor a) {
    switch (a) {
        case Anchor::Forehead: return {0.0, 4.7, 0.30};
        case Anchor::Temple: return {-0.55, 4.55, 0.18};
        case Anchor::Eye: return {-0.30, 4.25, 0.46};
        case Anchor::Nose: return {0.0, 3.95, 0.68};
        case Anchor::Mouth: return {0.0, 3.50, 0.44};
        case Anchor::Chin: return {0.0, 3.00, 0.38};
        case Anchor::Cheek: return {-0.55, 3.75, 0.24};
        case Anchor::Ear: return {-0.88, 4.0, -0.38};
        case Anchor::Chest: return {0.0, 0.60, 0.44};
        case Anchor::ChestRight: return {-0.65, 0.70, 0.38};
        case Anchor::Shoulder: return {-1.95, 1.90, -0.30};
        case Anchor::Neutral: return {-0.90, 0.10, 2.10};
        case Anchor::NeutralLow: return {-0.80, -0.70, 2.00};
        case Anchor::Spelling: return {-1.15, 1.25, 1.85};
    }
    return {0.0, 0.0, 0.0};
}

Vec3 anchorPointLeft(Anchor a) {
    Vec3 p = anchorPoint(a);
    p.x = -p.x;
    return p;
}

const char* anchorName(Anchor a) {
    switch (a) {
        case Anchor::Forehead: return "forehead";
        case Anchor::Temple: return "temple";
        case Anchor::Eye: return "eye";
        case Anchor::Nose: return "nose";
        case Anchor::Mouth: return "mouth";
        case Anchor::Chin: return "chin";
        case Anchor::Cheek: return "cheek";
        case Anchor::Ear: return "ear";
        case Anchor::Chest: return "chest";
        case Anchor::ChestRight: return "chest (right)";
        case Anchor::Shoulder: return "shoulder";
        case Anchor::Neutral: return "neutral space";
        case Anchor::NeutralLow: return "neutral space (low)";
        case Anchor::Spelling: return "fingerspelling position";
    }
    return "";
}

Vec3 shoulderRight() { return {-1.82, 1.80, -0.58}; }
Vec3 shoulderLeft() { return {1.82, 1.80, -0.58}; }

namespace {

BodyPart ellipsoid(const char* name, Vec3 centre, Vec3 radii, float r, float g, float b, bool head) {
    Mesh m;
    m.append(meshes::sphere(32, 20), xf3::translate(centre) * xf3::scale(radii.x, radii.y, radii.z));
    return {name, m, r, g, b, head};
}

}  // namespace

const std::vector<BodyPart>& bodyParts() {
    static const std::vector<BodyPart> parts = [] {
        std::vector<BodyPart> p;
        const float shirt[3] = {0.30f, 0.38f, 0.50f};
        const float skin[3] = {0.80f, 0.62f, 0.50f};
        const float dark[3] = {0.12f, 0.10f, 0.10f};

        // Torso: elliptical rings from the waist up to the base of the neck.
        p.push_back({"torso",
                     meshes::profileTube({{-4.2, 1.50, 0.92, 0.0, -0.62},
                                          {-1.6, 1.60, 0.98, 0.0, -0.62},
                                          {0.0, 1.85, 1.04, 0.0, -0.60},
                                          {1.2, 2.05, 1.00, 0.0, -0.60},
                                          {1.85, 2.10, 0.86, 0.0, -0.64},
                                          {2.20, 1.70, 0.66, 0.0, -0.64},
                                          {2.42, 0.70, 0.48, 0.0, -0.58}},
                                         40),
                     shirt[0], shirt[1], shirt[2], false});
        p.push_back({"neck", meshes::profileTube({{2.2, 0.46, 0.44, 0.0, -0.52}, {3.1, 0.42, 0.40, 0.0, -0.48}}, 28),
                     skin[0], skin[1], skin[2], true});
        p.push_back(ellipsoid("head", {0.0, 4.0, -0.45}, {0.82, 1.15, 0.95}, skin[0], skin[1], skin[2], true));
        p.push_back(ellipsoid("jaw", {0.0, 3.25, -0.20}, {0.60, 0.45, 0.65}, skin[0], skin[1], skin[2], true));
        p.push_back(ellipsoid("nose", {0.0, 3.93, 0.48}, {0.10, 0.22, 0.16}, skin[0], skin[1], skin[2], true));
        p.push_back(ellipsoid("right ear", {-0.82, 4.0, -0.45}, {0.10, 0.28, 0.18}, skin[0], skin[1], skin[2], true));
        p.push_back(ellipsoid("left ear", {0.82, 4.0, -0.45}, {0.10, 0.28, 0.18}, skin[0], skin[1], skin[2], true));
        p.push_back(ellipsoid("right eye", {-0.30, 4.25, 0.38}, {0.075, 0.075, 0.075}, dark[0], dark[1], dark[2], true));
        p.push_back(ellipsoid("left eye", {0.30, 4.25, 0.38}, {0.075, 0.075, 0.075}, dark[0], dark[1], dark[2], true));
        p.push_back(ellipsoid("mouth", {0.0, 3.50, 0.39}, {0.20, 0.045, 0.06}, 0.45f, 0.20f, 0.20f, true));
        p.push_back(ellipsoid("hair", {0.0, 4.35, -0.62}, {0.86, 0.92, 0.88}, 0.16f, 0.12f, 0.10f, true));
        return p;
    }();
    return parts;
}
