#pragma once
// StudioCapture: renders the documentation images and animations of the 3D
// version (screenshots, alphabet chart, GIFs), run as
//     sign_studio --capture
// which writes into the folder output_final.
//
// It drives StudioCore exactly as a user would (choose a sign, a camera, a
// shading mode, a moment in time), reads the finished OpenGL picture back
// with glReadPixels into OUR Framebuffer (Checkpoint 1), and saves it with
// our own BMP writer and GIF writer (Checkpoint 2). So the final project
// reuses every earlier checkpoint.

#include <string>

#include "StudioCore.h"

// The window's drawing area must be width x height pixels (and core.resize
// must have been called with that size). Returns the number of files written.
int runCapture(StudioCore& core, const std::string& folder, int width, int height);
