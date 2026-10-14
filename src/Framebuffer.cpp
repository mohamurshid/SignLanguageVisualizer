#include "Framebuffer.h"

#include <algorithm>
#include <fstream>

Framebuffer::Framebuffer(int width, int height) { resize(width, height); }

void Framebuffer::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    pixels_.assign(static_cast<size_t>(width_) * height_ * 3, 0);
    resetScissor();
}

void Framebuffer::clear(Color c) {
    for (size_t i = 0; i < pixels_.size(); i += 3) {
        pixels_[i] = c.r;
        pixels_[i + 1] = c.g;
        pixels_[i + 2] = c.b;
    }
}

bool Framebuffer::inside(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

void Framebuffer::setScissor(ClipRect r) {
    // Keep the scissor inside the buffer so setPixel only needs one test.
    scissor_.xmin = std::max(r.xmin, 0);
    scissor_.ymin = std::max(r.ymin, 0);
    scissor_.xmax = std::min(r.xmax, width_ - 1);
    scissor_.ymax = std::min(r.ymax, height_ - 1);
}

void Framebuffer::resetScissor() { scissor_ = bounds(); }

void Framebuffer::setPixel(int x, int y, Color c) {
    if (!scissor_.contains(x, y)) return;  // point clipping
    size_t i = (static_cast<size_t>(y) * width_ + x) * 3;
    pixels_[i] = c.r;
    pixels_[i + 1] = c.g;
    pixels_[i + 2] = c.b;
}

Color Framebuffer::getPixel(int x, int y) const {
    if (!inside(x, y)) return {0, 0, 0};
    size_t i = (static_cast<size_t>(y) * width_ + x) * 3;
    return {pixels_[i], pixels_[i + 1], pixels_[i + 2]};
}

namespace {
// BMP files store numbers little-endian (lowest byte first).
void put16(std::ofstream& out, std::uint16_t v) {
    out.put(static_cast<char>(v & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
}
void put32(std::ofstream& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.put(static_cast<char>((v >> (8 * i)) & 0xFF));
}
}  // namespace

bool Framebuffer::saveBMP(const std::string& path, int scale) const {
    scale = std::max(1, scale);
    const int w = width_ * scale;
    const int h = height_ * scale;
    const int rowBytes = (w * 3 + 3) & ~3;  // each row is padded to a multiple of 4 bytes
    const std::uint32_t imageBytes = static_cast<std::uint32_t>(rowBytes) * h;

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    // File header (14 bytes)
    out.put('B');
    out.put('M');
    put32(out, 54 + imageBytes);  // total file size
    put32(out, 0);                // reserved
    put32(out, 54);               // where the pixel data starts

    // Info header (40 bytes)
    put32(out, 40);
    put32(out, static_cast<std::uint32_t>(w));
    put32(out, static_cast<std::uint32_t>(h));  // positive height = rows stored bottom-up
    put16(out, 1);                              // colour planes
    put16(out, 24);                             // bits per pixel
    put32(out, 0);                              // no compression
    put32(out, imageBytes);
    put32(out, 2835);  // 72 DPI
    put32(out, 2835);
    put32(out, 0);
    put32(out, 0);

    // Pixel rows, bottom row first - the same order as our buffer.
    // BMP stores each pixel as blue, green, red.
    std::vector<char> row(static_cast<size_t>(rowBytes), 0);
    for (int y = 0; y < h; ++y) {
        const int sy = y / scale;
        for (int x = 0; x < w; ++x) {
            const Color c = getPixel(x / scale, sy);
            row[static_cast<size_t>(x) * 3] = static_cast<char>(c.b);
            row[static_cast<size_t>(x) * 3 + 1] = static_cast<char>(c.g);
            row[static_cast<size_t>(x) * 3 + 2] = static_cast<char>(c.r);
        }
        out.write(row.data(), rowBytes);
    }
    return static_cast<bool>(out);
}
