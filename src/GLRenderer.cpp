#include "GLRenderer.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <GL/gl.h>

#include "Body.h"
#include "HandMesh.h"

namespace {

// Colours (diffuse) and shininess of the materials.
struct Material {
    float r, g, b;
    float specular;
    float shininess;
};
const Material kSkin{0.86f, 0.64f, 0.50f, 0.22f, 22.0f};
const Material kSkinShade{0.80f, 0.58f, 0.46f, 0.18f, 18.0f};  // palm slightly darker than fingers
const Material kNail{0.96f, 0.82f, 0.78f, 0.65f, 70.0f};
const Material kShirt{0.30f, 0.38f, 0.50f, 0.06f, 8.0f};
const Material kHighlight{1.00f, 0.60f, 0.22f, 0.50f, 40.0f};

ShadeMode gShade = ShadeMode::Smooth;

void setMaterial(const Material& m) {
    if (gShade == ShadeMode::Wireframe) {
        glColor3f(m.r, m.g, m.b);
        return;
    }
    glColor3f(m.r, m.g, m.b);  // GL_COLOR_MATERIAL: ambient + diffuse follow glColor
    const GLfloat spec[4] = {m.specular, m.specular, m.specular, 1.0f};
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT, GL_SHININESS, m.shininess);
}

void loadMatrix(const Mat4& m) {
    const auto a = m.toColumnMajor();
    glLoadMatrixd(a.data());
}

// Loads view * model and tells OpenGL which winding is the front: a mirror
// (negative determinant) turns anticlockwise triangles clockwise.
void setModel(const Mat4& viewScene, const Mat4& model) {
    const Mat4 mv = viewScene * model;
    loadMatrix(mv);
    glFrontFace(mv.determinant() < 0.0 ? GL_CW : GL_CCW);
}

void drawMesh(const Mesh& m) {
    glVertexPointer(3, GL_FLOAT, 0, m.positions.data());
    glNormalPointer(GL_FLOAT, 0, m.normals.data());
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m.indices.size()), GL_UNSIGNED_INT, m.indices.data());
}

void vertex(Vec3 p) { glVertex3d(p.x, p.y, p.z); }

}  // namespace

const char* shadeModeName(ShadeMode m) {
    switch (m) {
        case ShadeMode::Smooth: return "smooth (Gouraud)";
        case ShadeMode::Flat: return "flat";
        case ShadeMode::Wireframe: return "wireframe";
        default: return "";
    }
}

void GLRenderer::resize(int width, int height) {
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;
}

Mat4 GLRenderer::projViewScene(const OrbitCamera& camera, const RenderOptions& options) const {
    const Mat4 scene = options.leftHanded ? xf3::reflectX() : Mat4::identity();
    return camera.projection(static_cast<double>(width_) / height_) * camera.view() * scene;
}

void GLRenderer::drawBackground() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glBegin(GL_QUADS);
    glColor3f(0.043f, 0.063f, 0.125f);  // bottom
    glVertex2f(-1.0f, -1.0f);
    glVertex2f(1.0f, -1.0f);
    glColor3f(0.13f, 0.18f, 0.28f);  // top
    glVertex2f(1.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();
}

void GLRenderer::drawScene(const OrbitCamera& camera, const Hand3D& right, const Hand3D& left,
                           const RenderOptions& options, const JointRef& highlight) {
    gShade = options.shade;
    glViewport(0, 0, width_, height_);
    glClearColor(0.05f, 0.07f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawBackground();

    // Projection and view: our own matrices.
    glMatrixMode(GL_PROJECTION);
    loadMatrix(camera.projection(static_cast<double>(width_) / height_));
    glMatrixMode(GL_MODELVIEW);
    const Mat4 scene = options.leftHanded ? xf3::reflectX() : Mat4::identity();
    const Mat4 viewScene = camera.view() * scene;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_NORMALIZE);  // scaled meshes (spheres -> ellipsoids) need re-normalised normals
    glShadeModel(options.shade == ShadeMode::Flat ? GL_FLAT : GL_SMOOTH);
    glPolygonMode(GL_FRONT_AND_BACK, options.shade == ShadeMode::Wireframe ? GL_LINE : GL_FILL);

    const bool lit = options.lighting && options.shade != ShadeMode::Wireframe;
    if (lit) {
        glEnable(GL_LIGHTING);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
        glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);  // true Phong highlights
        const GLfloat ambient[4] = {0.20f, 0.21f, 0.25f, 1.0f};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
        // Light positions are given in world space: load the view first.
        loadMatrix(viewScene);
        struct Light {
            GLenum id;
            GLfloat pos[4], diffuse[4], specular[4];
        };
        const Light lights[3] = {
            {GL_LIGHT0, {-5.0f, 8.0f, 10.0f, 0.0f}, {0.95f, 0.90f, 0.84f, 1.0f}, {0.9f, 0.88f, 0.85f, 1.0f}},  // key
            {GL_LIGHT1, {8.0f, 1.0f, 6.0f, 0.0f}, {0.30f, 0.36f, 0.48f, 1.0f}, {0.1f, 0.1f, 0.1f, 1.0f}},      // fill
            {GL_LIGHT2, {2.0f, 6.0f, -10.0f, 0.0f}, {0.40f, 0.40f, 0.48f, 1.0f}, {0.3f, 0.3f, 0.35f, 1.0f}},   // rim
        };
        for (const Light& l : lights) {
            glEnable(l.id);
            glLightfv(l.id, GL_POSITION, l.pos);
            glLightfv(l.id, GL_DIFFUSE, l.diffuse);
            glLightfv(l.id, GL_SPECULAR, l.specular);
            const GLfloat zero[4] = {0, 0, 0, 1};
            glLightfv(l.id, GL_AMBIENT, zero);
        }
    } else {
        glDisable(GL_LIGHTING);
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    if (options.body) {
        for (const BodyPart& part : bodyParts()) {
            if (camera.hidesBody()) continue;
            setMaterial({part.r, part.g, part.b, part.name == "right eye" || part.name == "left eye" ? 0.8f : 0.08f,
                         part.name == "right eye" || part.name == "left eye" ? 80.0f : 10.0f});
            setModel(viewScene, Mat4::identity());
            drawMesh(part.mesh);
        }
        drawArm(viewScene, right, false, options);
        drawArm(viewScene, left, true, options);
    }
    drawHand(viewScene, right, 0, options, highlight);
    drawHand(viewScene, left, 1, options, highlight);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    if (options.skeleton) {
        drawSkeleton(viewScene, right);
        drawSkeleton(viewScene, left);
    }
    if (options.frames) {
        drawFrames(viewScene, right);
        drawFrames(viewScene, left);
    }
}

void GLRenderer::drawArm(const Mat4& viewScene, const Hand3D& hand, bool leftArm, const RenderOptions& options) {
    (void)options;
    const HandMesh& hm = HandMesh::get();
    const Vec3 shoulder = leftArm ? shoulderLeft() : shoulderRight();
    const Vec3 elbow = solveElbow(shoulder, hand.handMatrix().origin(), leftArm);

    // Upper arm (skin) with a short sleeve over its top part.
    setMaterial(kSkin);
    setModel(viewScene, alignX(shoulder, elbow));
    static const Mesh upper = meshes::frustum(0.40, 0.33, kUpperArmLength, 22);
    drawMesh(upper);
    setModel(viewScene, xf3::translate(elbow) * xf3::scale(0.33, 0.31, 0.26));
    drawMesh(hm.unitSphere);
    setMaterial(kShirt);
    setModel(viewScene, alignX(shoulder, elbow));
    static const Mesh sleeve = meshes::frustum(0.47, 0.42, 1.45, 24);
    drawMesh(sleeve);
    setModel(viewScene, xf3::translate(shoulder) * xf3::scale(0.47));
    drawMesh(hm.unitSphere);

    // Forearm from the elbow to the wrist, twisting with the hand.
    setMaterial(kSkin);
    setModel(viewScene, forearmMatrix(hand, elbow));
    drawMesh(hm.forearm);
}

void GLRenderer::drawHand(const Mat4& viewScene, const Hand3D& hand, int handIndex, const RenderOptions& options,
                          const JointRef& highlight) {
    const HandMesh& hm = HandMesh::get();
    const Mat4 handM = hand.handMatrix();
    if (!options.body) {  // without the body there is no arm: show a short forearm
        setMaterial(kSkin);
        setModel(viewScene, forearmMatrix(hand, handM.apply({0.0, -kForearmLength, 0.0})));
        drawMesh(hm.forearm);
    }
    setMaterial(kSkinShade);
    setModel(viewScene, handM);
    drawMesh(hm.palm);
    setMaterial(kSkin);
    drawMesh(hm.wrist);

    for (int f = 0; f < kFingerCount; ++f) {
        setMaterial(kSkin);
        for (int b = 0; b < kBones; ++b) {
            setModel(viewScene, HandMesh::boneMatrix(hand, f, b));
            drawMesh(hm.bones[f][b]);
        }
        for (int p = 0; p < kPoints; ++p) {
            const bool hi = highlight.valid() && highlight.hand == handIndex && highlight.finger == f && highlight.point == p;
            setMaterial(hi ? kHighlight : kSkin);
            setModel(viewScene, HandMesh::jointMatrix(hand, f, p) * (hi ? xf3::scale(1.45) : Mat4::identity()));
            drawMesh(hm.unitSphere);
        }
        setMaterial(kNail);
        setModel(viewScene, HandMesh::nailMatrix(hand, f));
        drawMesh(hm.unitSphere);
    }
}

void GLRenderer::drawSkeleton(const Mat4& viewScene, const Hand3D& hand) {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    loadMatrix(viewScene * hand.handMatrix());
    glLineWidth(2.5f);
    glColor3f(0.36f, 0.76f, 1.0f);
    glBegin(GL_LINES);
    for (int f = 0; f < kFingerCount; ++f) {
        const FingerChain3D& c = hand.chain(f);
        vertex({0.0, 0.0, 0.0});
        vertex(c.point[0]);
        for (int b = 0; b < kBones; ++b) {
            vertex(c.point[b]);
            vertex(c.point[b + 1]);
        }
    }
    glEnd();
    glPointSize(7.0f);
    glBegin(GL_POINTS);
    glColor3f(1.0f, 0.85f, 0.35f);
    vertex({0.0, 0.0, 0.0});
    glColor3f(0.95f, 0.97f, 1.0f);
    for (int f = 0; f < kFingerCount; ++f)
        for (int p = 0; p < kPoints; ++p) vertex(hand.chain(f).point[p]);
    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
}

void GLRenderer::drawFrames(const Mat4& viewScene, const Hand3D& hand) {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    loadMatrix(viewScene * hand.handMatrix());
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (int f = 0; f < kFingerCount; ++f)
        for (int b = 0; b < kBones; ++b) {
            const Mat4& m = hand.chain(f).boneFrame[b];
            const Vec3 o = m.origin();
            const float colours[3][3] = {{0.95f, 0.30f, 0.30f}, {0.35f, 0.90f, 0.40f}, {0.40f, 0.60f, 1.0f}};
            for (int a = 0; a < 3; ++a) {
                glColor3fv(colours[a]);
                vertex(o);
                vertex(o + normalize(m.axis(a)) * 0.16);
            }
        }
    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
}

void GLRenderer::drawOverlay(const Overlay& overlay, const std::function<void(const OverlayText&)>& drawText) {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, width_, height_, 0.0, -1.0, 1.0);  // pixels, origin top-left
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (const OverlayRect& r : overlay.rects) {
        glColor4f(r.r, r.g, r.b, r.a);
        glBegin(GL_QUADS);
        glVertex2i(r.x, r.y);
        glVertex2i(r.x + r.w, r.y);
        glVertex2i(r.x + r.w, r.y + r.h);
        glVertex2i(r.x, r.y + r.h);
        glEnd();
    }
    glDisable(GL_BLEND);
    for (const OverlayText& t : overlay.texts) drawText(t);
}
