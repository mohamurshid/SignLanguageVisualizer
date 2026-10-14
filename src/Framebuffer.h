#pragma once
// Framebuffer: our own block of pixel memory.
//
// Every drawing algorithm in this project writes into this buffer one pixel
// at a time through setPixel(). The finished image is saved as a .bmp file
// (pure C++, see saveBMP) or, in the optional viewer, copied to a window.
// This keeps every graphics algorithm in our own code.

#include <cstdint>
#include <string>
#include <vector>

#include "Geometry.h"

// A colour as 8-bit red, green and blue values (0-255).
struct Color {
    std::uint8_t r, g, b;
};

inline bool operator==(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
inline bool operator!=(Color a, Color b) { return !(a == b); }

class Framebuffer {
public:
    explicit Framebuffer(int width = 1, int height = 1);

    void resize(int width, int height);
    void clear(Color c);

    // Writes one pixel. (0,0) is the BOTTOM-LEFT corner.
    // Point clipping: a pixel outside the scissor rectangle is ignored, so
    // nothing can ever be written outside the buffer.
    void setPixel(int x, int y, Color c);
    Color getPixel(int x, int y) const;
    bool inside(int x, int y) const;  // inside the whole buffer?

    // The scissor rectangle limits setPixel to part of the buffer
    // (by default the whole buffer). Used for point clipping of circles and
    // ellipses, which our geometric clipping algorithms do not handle.
    void setScissor(ClipRect r);
    void resetScissor();
    ClipRect scissor() const { return scissor_; }
    ClipRect bounds() const { return {0, 0, width_ - 1, height_ - 1}; }

    int width() const { return width_; }
    int height() const { return height_; }
    const std::uint8_t* data() const { return pixels_.data(); }

    // Saves the image as a 24-bit Windows .bmp file. Written by hand (no
    // libraries). 'scale' enlarges every pixel into a scale x scale block,
    // so the steps each algorithm produces can be seen.
    bool saveBMP(const std::string& path, int scale = 1) const;

private:
    int width_ = 1;
    int height_ = 1;
    ClipRect scissor_{0, 0, 0, 0};
    std::vector<std::uint8_t> pixels_;  // RGB triples, row 0 = bottom row
};
