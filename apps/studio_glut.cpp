// Sign Language Gesture Visualizer - 3D application, OpenGL + freeglut window.
//
// Everything is in StudioCore (apps) and src/: this file opens the window,
// passes keys, mouse and time on, and draws text with GLUT's bitmap fonts.
// The Windows build uses studio_win32.cpp instead, which needs no freeglut.
//
//   sign_studio_glut              the interactive program
//   sign_studio_glut --capture    renders the documentation images into output_final/

#include <GL/glut.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "StudioCapture.h"
#include "StudioCore.h"

namespace {

StudioCore* gCore = nullptr;
int gW = 1100, gH = 760;

void* fontFor(int size) { return size ? GLUT_BITMAP_HELVETICA_18 : GLUT_BITMAP_9_BY_15; }

void drawText(const OverlayText& t, int size) {
    glColor3f(t.r, t.g, t.b);
    glRasterPos2i(t.x, t.y);
    for (char c : t.text) glutBitmapCharacter(fontFor(size), static_cast<unsigned char>(c));
}

int measureText(const std::string& s, int size) {
    int w = 0;
    for (char c : s) w += glutBitmapWidth(fontFor(size), static_cast<unsigned char>(c));
    return w;
}

void display() {
    gCore->render();
    glutSwapBuffers();
}

void reshape(int w, int h) {
    gW = w > 0 ? w : 1;
    gH = h > 0 ? h : 1;
    gCore->resize(gW, gH);
}

void afterInput() {
    if (gCore->wantsQuit()) std::exit(0);
    glutPostRedisplay();
}

void keyboard(unsigned char key, int, int) {
    if (key == 8 || key == 127) gCore->onKey(StudioKey::Backspace);
    else if (key == 13) gCore->onChar('\r');
    else gCore->onChar(static_cast<char>(key));
    afterInput();
}

void special(int key, int, int) {
    if (key == GLUT_KEY_LEFT) gCore->onKey(StudioKey::Left);
    if (key == GLUT_KEY_RIGHT) gCore->onKey(StudioKey::Right);
    if (key == GLUT_KEY_UP) gCore->onKey(StudioKey::Up);
    if (key == GLUT_KEY_DOWN) gCore->onKey(StudioKey::Down);
    afterInput();
}

void mouse(int button, int state, int x, int y) {
    if (button == 3 || button == 4) {  // freeglut reports the wheel as buttons 3 and 4
        if (state == GLUT_DOWN) gCore->onWheel(button == 3 ? 1 : -1);
    } else if (state == GLUT_DOWN) {
        gCore->onMouseDown(x, y, button == GLUT_RIGHT_BUTTON);
    } else {
        gCore->onMouseUp();
    }
    afterInput();
}

void motion(int x, int y) {
    gCore->onMouseMove(x, y);
    glutPostRedisplay();
}

void tick(int) {
    if (gCore->update(glutGet(GLUT_ELAPSED_TIME) / 1000.0)) glutPostRedisplay();
    glutTimerFunc(16, tick, 0);
}

}  // namespace

int main(int argc, char** argv) {
    const bool capture = argc > 1 && std::strcmp(argv[1], "--capture") == 0;
    StudioCore core;
    gCore = &core;
    core.setText(drawText, measureText);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    if (capture) gW = 960, gH = 720;
    glutInitWindowSize(gW, gH);
    glutCreateWindow(StudioCore::windowTitle());
    core.resize(gW, gH);

    if (capture) return runCapture(core, "output_final", gW, gH) > 0 ? 0 : 1;

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutPassiveMotionFunc(motion);
    glutTimerFunc(16, tick, 0);
    glutMainLoop();
    return 0;
}
