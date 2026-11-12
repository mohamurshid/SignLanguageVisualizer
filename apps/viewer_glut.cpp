// Sign Language Gesture Visualizer - alternative interactive viewer using
// OpenGL + freeglut.
//
// OpenGL is used only to open the window, copy our finished framebuffer to
// the screen (glDrawPixels) and draw the text labels. Everything else -
// drawing, transformations, animation, Build mode - is in ViewerCore and
// src/, shared with the Windows viewer.

#include <GL/glut.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "ViewerCore.h"

namespace {

ViewerCore* gCore = nullptr;
int gWindowW = 960;
int gWindowH = 720;

void drawText(const HudText& t) {
    int x = t.x;
    const int width = static_cast<int>(t.text.size()) * 9;
    if (t.centred) {
        // Keep the whole text on screen: OpenGL skips text whose start
        // position falls outside the window.
        x = t.x - width / 2;
        if (x + width > gWindowW - 4) x = gWindowW - 4 - width;
        if (x < 4) x = 4;
    }
    glColor3ub(0, 0, 0);
    glRasterPos2i(x + 1, t.yFromBottom - 1);
    for (char ch : t.text) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, ch);
    glColor3ub(t.color.r, t.color.g, t.color.b);
    glRasterPos2i(x, t.yFromBottom);
    for (char ch : t.text) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, ch);
}

void display() {
    gCore->render();
    const Framebuffer& fb = gCore->framebuffer();
    const int ps = gCore->pixelSize();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glRasterPos2i(0, 0);
    glPixelZoom(static_cast<float>(ps), static_cast<float>(ps));
    glDrawPixels(fb.width(), fb.height(), GL_RGB, GL_UNSIGNED_BYTE, fb.data());
    for (const HudText& t : gCore->hud()) drawText(t);
    glutSwapBuffers();
}

void reshape(int w, int h) {
    gWindowW = w > 0 ? w : 1;
    gWindowH = h > 0 ? h : 1;
    gCore->resize(gWindowW, gWindowH);
    glViewport(0, 0, gWindowW, gWindowH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, gWindowW, 0.0, gWindowH, -1.0, 1.0);  // 1 unit = 1 window pixel, origin bottom-left
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void afterInput() {
    if (gCore->wantsQuit()) std::exit(0);
    glutPostRedisplay();
}

void keyboard(unsigned char key, int, int) {
    gCore->onChar(static_cast<char>(key));
    afterInput();
}

void special(int key, int, int) {
    if (key == GLUT_KEY_LEFT) gCore->onSpecialKey(SpecialKey::Left);
    if (key == GLUT_KEY_RIGHT) gCore->onSpecialKey(SpecialKey::Right);
    if (key == GLUT_KEY_UP) gCore->onSpecialKey(SpecialKey::Up);
    if (key == GLUT_KEY_DOWN) gCore->onSpecialKey(SpecialKey::Down);
    afterInput();
}

void mouse(int button, int state, int x, int y) {
    if (button != GLUT_LEFT_BUTTON) return;
    if (state == GLUT_DOWN) {
        gCore->onMouseDown(x, y);
    } else {
        gCore->onMouseUp();
    }
    afterInput();
}

void motion(int x, int y) {
    gCore->onMouseMove(x, y);
    afterInput();
}

void tick(int) {
    if (gCore->update(glutGet(GLUT_ELAPSED_TIME) / 1000.0)) glutPostRedisplay();
    glutTimerFunc(16, tick, 0);  // about 60 updates per second
}

}  // namespace

int main(int argc, char** argv) {
    ViewerCore core;
    gCore = &core;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(gWindowW, gWindowH);
    glutCreateWindow(ViewerCore::windowTitle());
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutTimerFunc(16, tick, 0);

    std::printf("Sign Language Gesture Visualizer - Checkpoint 2 (interactive viewer, OpenGL)\n");
    std::printf("W = watch an animated sign, B = build mode (drag joints with the mouse).\n");
    glutMainLoop();
    return 0;
}
