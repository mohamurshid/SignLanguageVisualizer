// Sign Language Gesture Visualizer - OPTIONAL interactive viewer.
//
// The Checkpoint 1 deliverable is the pure C++ program apps/cp1_render.cpp.
// This viewer shows exactly the same drawings live in a window, so they can
// be explored with the keyboard during a demo.
//
// This is the ONLY file that uses a graphics library (OpenGL + freeglut), and
// only for three things: opening the window, copying our finished
// framebuffer to the screen (glDrawPixels), and drawing the text labels.
// Every shape is drawn by our own algorithms in src/.
//
// Controls
//   1-8      choose a sign (the hand moves smoothly to it)
//   P        play through all signs automatically
//   D        switch line algorithm: DDA <-> Bresenham
//   F        switch filled (scan-line fill) <-> wireframe (outlines only)
//   S        switch view: skin + skeleton / skin only / skeleton only
//   X        palm details on/off (Bezier creases, ellipse pads and nails)
//   C        detail view on/off (clipping + window-to-viewport)
//   M        motion trails on/off (spline paths of the last movement)
//   T        algorithm pages: lines, circles/ellipses, polygons, clipping, curves
//   + / -    make pixels bigger / smaller (to see the algorithms' steps)
//   Esc      quit

#include <GL/glut.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "Framebuffer.h"
#include "Hand.h"
#include "HandRenderer.h"
#include "Poses.h"
#include "TestScene.h"

namespace {

// ---- Window and framebuffer --------------------------------------------------
int gWindowW = 960;
int gWindowH = 720;
const int kPixelSizes[] = {1, 2, 3, 4, 6, 8, 12};
const int kPixelSizeCount = sizeof(kPixelSizes) / sizeof(kPixelSizes[0]);
int gPixelSizeIndex = 1;  // start at 2x
Framebuffer gFramebuffer;

int pixelSize() { return kPixelSizes[gPixelSizeIndex]; }

// ---- Scene state ---------------------------------------------------------------
Hand gHand;
RenderSettings gSettings;
int gPage = -1;       // -1 = the hand, 0..4 = algorithm pages
int gViewMode = 0;    // 0 = skin + skeleton, 1 = skin only, 2 = skeleton only
bool gDetailView = false;
bool gMotionTrails = false;

// ---- Pose animation ------------------------------------------------------------
// When a new sign is chosen we blend from the current angles to the target
// angles over a short time (linear interpolation with smooth easing).
// This is an early preview of the Checkpoint 2 animation work.
HandPose gFromPose;
HandPose gToPose;
int gTargetIndex = 0;
double gAnimStartMs = -1.0;  // -1 = not animating
const double kAnimDurationMs = 500.0;

bool gAutoPlay = false;
double gLastAutoMs = 0.0;
const double kAutoPlayIntervalMs = 1800.0;
bool gTimerRunning = false;

double nowMs() { return static_cast<double>(glutGet(GLUT_ELAPSED_TIME)); }

// The curves page animates the de Casteljau parameter t back and forth.
bool curvesPageShown() { return gPage == 4; }
double curveT() { return 0.5 + 0.45 * std::sin(nowMs() * 0.0012); }

void tick(int);

void ensureTimer() {
    if (!gTimerRunning) {
        gTimerRunning = true;
        glutTimerFunc(16, tick, 0);  // about 60 frames per second
    }
}

void startTransition(int index) {
    const auto& library = signLibrary();
    gTargetIndex = index % static_cast<int>(library.size());
    gFromPose = gHand.pose();
    gToPose = library[gTargetIndex];
    gAnimStartMs = nowMs();
    ensureTimer();
}

void tick(int) {
    bool keepGoing = false;
    const double now = nowMs();

    if (gAutoPlay) {
        if (now - gLastAutoMs >= kAutoPlayIntervalMs) {
            gLastAutoMs = now;
            startTransition(gTargetIndex + 1);
        }
        keepGoing = true;
    }

    if (gAnimStartMs >= 0.0) {
        double t = (now - gAnimStartMs) / kAnimDurationMs;
        if (t >= 1.0) {
            gHand.setPose(gToPose);
            gAnimStartMs = -1.0;
        } else {
            gHand.setPose(lerpPose(gFromPose, gToPose, easeInOut(t)));
            keepGoing = true;
        }
        glutPostRedisplay();
    }

    if (curvesPageShown()) {
        keepGoing = true;
        glutPostRedisplay();
    }

    if (keepGoing) {
        glutTimerFunc(16, tick, 0);
    } else {
        gTimerRunning = false;
    }
}

// ---- Text (help, status lines and page labels) ---------------------------------
void drawText(int x, int y, const std::string& text, Color c) {
    // Dark shadow first (offset by 1 pixel) so text stays readable over shapes.
    glColor3ub(0, 0, 0);
    glRasterPos2i(x + 1, y - 1);
    for (char ch : text) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, ch);
    glColor3ub(c.r, c.g, c.b);
    glRasterPos2i(x, y);
    for (char ch : text) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, ch);
}

void drawCentredText(int cx, int y, const std::string& text, Color c) {
    // Keep the whole text on screen: OpenGL skips text whose start position
    // falls outside the window.
    const int width = static_cast<int>(text.size()) * 9;
    int x = cx - width / 2;
    if (x + width > gWindowW - 4) x = gWindowW - 4 - width;
    if (x < 4) x = 4;
    drawText(x, y, text, c);
}

const char* viewModeName() {
    switch (gViewMode) {
        case 1: return "skin";
        case 2: return "skeleton";
        default: return "skin + skeleton";
    }
}

void drawHud() {
    const Color dim{150, 165, 200};
    std::string status;
    if (gPage >= 0) {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "Page %d/%d: %s", gPage + 1, algorithmPageCount(), algorithmPageTitle(gPage));
        status = buf;
    } else {
        status = "Sign: " + gToPose.name + " - " + gToPose.description;
    }
    drawText(12, gWindowH - 22, "Sign Language Gesture Visualizer  |  Checkpoint 1: technical foundation", palette::text);
    drawText(12, gWindowH - 42, status, palette::root);

    char settings[220];
    std::snprintf(settings, sizeof(settings), "Lines: %s   Fill: %s   View: %s   Details: %s   Pixel size: %dx%s%s%s",
                  lineAlgoName(gSettings.lineAlgo), gSettings.filled ? "scan-line" : "wireframe", viewModeName(),
                  gSettings.showDetails ? "on" : "off", pixelSize(), gDetailView ? "   [detail view]" : "",
                  gMotionTrails ? "   [motion trails]" : "", gAutoPlay ? "   [autoplay]" : "");
    drawText(12, gWindowH - 62, settings, dim);

    if (gPage >= 0) {
        for (const PageLabel& label : algorithmPageLabels(gPage)) {
            drawCentredText(static_cast<int>(label.fx * gWindowW), static_cast<int>(label.fy * gWindowH), label.text,
                            palette::text);
        }
    } else if (gDetailView) {
        const DetailView dv = detailViewFor(gFramebuffer, gHand);
        const int ps = pixelSize();
        drawText(dv.viewport.xmin * ps, (dv.viewport.ymax + 1) * ps + 8, "Detail view (clipped)", palette::root);
    }

    drawText(12, 12, "1-8 sign  P play  D lines  F fill  S view  X details  C detail  M trails  T pages  +/- pixels  Esc",
             dim);
}

// ---- GLUT callbacks ------------------------------------------------------------
void display() {
    // 1. Make sure the framebuffer matches the window at the chosen pixel size.
    const int ps = pixelSize();
    const int fbW = (gWindowW + ps - 1) / ps;
    const int fbH = (gWindowH + ps - 1) / ps;
    if (gFramebuffer.width() != fbW || gFramebuffer.height() != fbH) gFramebuffer.resize(fbW, fbH);

    // 2. Draw the scene with OUR algorithms into OUR framebuffer.
    if (gPage >= 0) {
        drawAlgorithmPage(gFramebuffer, gPage, gSettings, curvesPageShown() ? curveT() : 0.5);
    } else {
        drawHandScene(gFramebuffer, gHand, gSettings, gMotionTrails ? &gFromPose : nullptr);
        if (gMotionTrails) drawMotionTrails(gFramebuffer, gFromPose, gToPose, gSettings);
        if (gDetailView) drawDetailView(gFramebuffer, gHand, gSettings);
    }

    // 3. Copy the framebuffer to the window, each pixel scaled up ps times.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glRasterPos2i(0, 0);
    glPixelZoom(static_cast<float>(ps), static_cast<float>(ps));
    glDrawPixels(gFramebuffer.width(), gFramebuffer.height(), GL_RGB, GL_UNSIGNED_BYTE, gFramebuffer.data());

    drawHud();
    glutSwapBuffers();
}

void reshape(int w, int h) {
    gWindowW = w > 0 ? w : 1;
    gWindowH = h > 0 ? h : 1;
    glViewport(0, 0, gWindowW, gWindowH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, gWindowW, 0.0, gWindowH, -1.0, 1.0);  // 1 unit = 1 window pixel, origin bottom-left
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void applyViewMode() {
    gSettings.showSkin = gViewMode != 2;
    gSettings.showSkeleton = gViewMode != 1;
}

void keyboard(unsigned char key, int, int) {
    const int signCount = static_cast<int>(signLibrary().size());
    if (key >= '1' && key <= '9' && key - '1' < signCount) {
        gPage = -1;
        gAutoPlay = false;
        startTransition(key - '1');
    }
    switch (key) {
        case 27: std::exit(0);  // Esc
        case 'p': case 'P':
            gAutoPlay = !gAutoPlay;
            gPage = -1;
            gLastAutoMs = nowMs() - kAutoPlayIntervalMs;  // move to the next sign straight away
            if (gAutoPlay) ensureTimer();
            break;
        case 'd': case 'D':
            gSettings.lineAlgo = gSettings.lineAlgo == LineAlgo::DDA ? LineAlgo::Bresenham : LineAlgo::DDA;
            break;
        case 'f': case 'F': gSettings.filled = !gSettings.filled; break;
        case 's': case 'S': gViewMode = (gViewMode + 1) % 3; applyViewMode(); break;
        case 'x': case 'X': gSettings.showDetails = !gSettings.showDetails; break;
        case 'c': case 'C': gDetailView = !gDetailView; gPage = -1; break;
        case 'm': case 'M': gMotionTrails = !gMotionTrails; gPage = -1; break;
        case '+': case '=': if (gPixelSizeIndex < kPixelSizeCount - 1) ++gPixelSizeIndex; break;
        case '-': case '_': if (gPixelSizeIndex > 0) --gPixelSizeIndex; break;
        case 't': case 'T':
            // Hand -> page 1 -> ... -> page 5 -> hand
            gPage = (gPage + 1 >= algorithmPageCount()) ? -1 : gPage + 1;
            gAutoPlay = false;
            if (curvesPageShown()) ensureTimer();
            break;
        default: break;
    }
    glutPostRedisplay();
}

}  // namespace

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(gWindowW, gWindowH);
    glutCreateWindow("Sign Language Gesture Visualizer - Checkpoint 1");

    gToPose = signLibrary()[0];
    gFromPose = gToPose;
    gHand.setPose(gToPose);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);

    std::printf("Sign Language Gesture Visualizer - Checkpoint 1 (interactive viewer)\n");
    std::printf("1-8 sign | P play | D lines | F fill | S view | X details | C detail view | M trails | T pages | +/- pixels | Esc\n");
    glutMainLoop();
    return 0;
}
