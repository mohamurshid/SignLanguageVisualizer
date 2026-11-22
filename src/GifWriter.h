#pragma once
// GifWriter: saves a sequence of framebuffers as an animated .gif file.
//
// Written by hand in pure C++ (no libraries), like the BMP writer. A GIF
// stores up to 256 colours per frame in a colour table, then the pixels as
// colour-table indices compressed with LZW:
//
//   "GIF89a" header, screen size
//   NETSCAPE2.0 block               -> loop forever
//   for every frame:
//     graphic control extension     -> how long to show the frame
//     image descriptor + colour table
//     LZW-compressed pixel indices, in blocks of at most 255 bytes
//   ';' trailer
//
// The 2D frames use only a few dozen colours, so every colour is stored
// exactly. A shaded 3D frame has thousands: then 256 colours are chosen for
// that frame by MEDIAN CUT (medianCutPalette) and each pixel uses the nearest.

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "Framebuffer.h"

// Chooses up to 'size' colours that represent the picture (median cut).
std::vector<Color> medianCutPalette(const Framebuffer& fb, int size);

class GifWriter {
public:
    // 'scale' enlarges every pixel into a scale x scale block, like saveBMP.
    bool open(const std::string& path, int width, int height, int scale = 1);
    // 'delay' is in hundredths of a second (5 = 20 frames per second).
    bool addFrame(const Framebuffer& fb, int delayHundredths);
    bool close();
    int frameCount() const { return frames_; }

    ~GifWriter() { close(); }

private:
    std::ofstream out_;
    int width_ = 0;
    int height_ = 0;
    int scale_ = 1;
    int frames_ = 0;
};

// LZW compression as used by GIF (exposed so it can be tested on its own).
std::vector<std::uint8_t> gifLzwEncode(const std::vector<std::uint8_t>& indices, int minCodeSize);
