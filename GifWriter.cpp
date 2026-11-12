#include "GifWriter.h"

#include <algorithm>
#include <unordered_map>

namespace {

void put8(std::ofstream& out, int v) { out.put(static_cast<char>(v & 0xFF)); }
void put16(std::ofstream& out, int v) {
    put8(out, v);
    put8(out, v >> 8);
}

// Writes codes of a varying number of bits, lowest bit first (GIF's order).
class BitWriter {
public:
    void write(int code, int bits) {
        buffer_ |= static_cast<std::uint32_t>(code) << count_;
        count_ += bits;
        while (count_ >= 8) {
            bytes.push_back(static_cast<std::uint8_t>(buffer_ & 0xFF));
            buffer_ >>= 8;
            count_ -= 8;
        }
    }
    void flush() {
        if (count_ > 0) bytes.push_back(static_cast<std::uint8_t>(buffer_ & 0xFF));
        buffer_ = 0;
        count_ = 0;
    }
    std::vector<std::uint8_t> bytes;

private:
    std::uint32_t buffer_ = 0;
    int count_ = 0;
};

}  // namespace

// ===========================================================================
// LZW compression
// ===========================================================================
// LZW builds a dictionary of pixel sequences while it reads them. Codes
// 0..255 are the single colours, 256 = CLEAR (start a new dictionary),
// 257 = END. Every new sequence = (an existing sequence + one more pixel)
// gets the next free code, so long runs of the same colour (most of our
// image) shrink to a few codes. Codes start 9 bits wide and grow to 12 bits
// as the dictionary fills; at 4095 entries we send CLEAR and start again.
std::vector<std::uint8_t> gifLzwEncode(const std::vector<std::uint8_t>& indices, int minCodeSize) {
    const int clearCode = 1 << minCodeSize;
    const int endCode = clearCode + 1;
    int codeSize = minCodeSize + 1;
    int maxCode = endCode;

    // Dictionary: (prefix code, next pixel) -> code.
    std::unordered_map<std::uint32_t, int> dict;
    BitWriter bits;
    bits.write(clearCode, codeSize);

    int current = -1;  // the code for the sequence read so far
    for (std::uint8_t pixel : indices) {
        if (current < 0) {
            current = pixel;
            continue;
        }
        const std::uint32_t keyValue = (static_cast<std::uint32_t>(current) << 8) | pixel;
        auto found = dict.find(keyValue);
        if (found != dict.end()) {
            current = found->second;  // the longer sequence is already known
            continue;
        }
        bits.write(current, codeSize);  // send the longest known sequence
        dict[keyValue] = ++maxCode;     // remember it plus this pixel
        if (maxCode >= (1 << codeSize)) ++codeSize;
        if (maxCode == 4095) {          // dictionary full: start again
            bits.write(clearCode, codeSize);
            dict.clear();
            codeSize = minCodeSize + 1;
            maxCode = endCode;
        }
        current = pixel;
    }
    if (current >= 0) bits.write(current, codeSize);
    bits.write(clearCode, codeSize);
    bits.write(endCode, minCodeSize + 1);
    bits.flush();
    return bits.bytes;
}

// ===========================================================================
// File structure
// ===========================================================================
bool GifWriter::open(const std::string& path, int width, int height, int scale) {
    close();
    scale_ = std::max(1, scale);
    width_ = width * scale_;
    height_ = height * scale_;
    frames_ = 0;
    out_.open(path, std::ios::binary);
    if (!out_) return false;

    out_.write("GIF89a", 6);
    put16(out_, width_);
    put16(out_, height_);
    put8(out_, 0x00);  // no global colour table (each frame has its own)
    put8(out_, 0);     // background colour index
    put8(out_, 0);     // pixel aspect ratio: not given

    // NETSCAPE2.0 application extension: repeat forever (loop count 0).
    put8(out_, 0x21);
    put8(out_, 0xFF);
    put8(out_, 11);
    out_.write("NETSCAPE2.0", 11);
    put8(out_, 3);
    put8(out_, 1);
    put16(out_, 0);
    put8(out_, 0);
    return static_cast<bool>(out_);
}

bool GifWriter::addFrame(const Framebuffer& fb, int delayHundredths) {
    if (!out_) return false;

    // 1. Colour table: give each different colour in the frame an index.
    std::vector<Color> table;
    std::unordered_map<std::uint32_t, std::uint8_t> lookup;
    bool exact = true;
    for (int y = 0; y < fb.height() && exact; ++y) {
        for (int x = 0; x < fb.width(); ++x) {
            const Color c = fb.getPixel(x, y);
            const std::uint32_t k = (static_cast<std::uint32_t>(c.r) << 16) | (c.g << 8) | c.b;
            if (lookup.count(k)) continue;
            if (table.size() == 256) {
                exact = false;
                break;
            }
            lookup[k] = static_cast<std::uint8_t>(table.size());
            table.push_back(c);
        }
    }
    if (!exact) {  // fallback: 6 x 7 x 6 colour cube
        table.clear();
        for (int r = 0; r < 6; ++r)
            for (int g = 0; g < 7; ++g)
                for (int b = 0; b < 6; ++b)
                    table.push_back({static_cast<std::uint8_t>(r * 51), static_cast<std::uint8_t>(g * 42),
                                     static_cast<std::uint8_t>(b * 51)});
    }
    table.resize(256, Color{0, 0, 0});

    // 2. Pixel indices, TOP row first (our framebuffer stores the bottom row first).
    std::vector<std::uint8_t> indices;
    indices.reserve(static_cast<size_t>(width_) * height_);
    for (int y = height_ - 1; y >= 0; --y) {
        for (int x = 0; x < width_; ++x) {
            const Color c = fb.getPixel(x / scale_, y / scale_);
            if (exact) {
                indices.push_back(lookup[(static_cast<std::uint32_t>(c.r) << 16) | (c.g << 8) | c.b]);
            } else {
                const int r = (c.r * 5 + 127) / 255, g = (c.g * 6 + 127) / 255, b = (c.b * 5 + 127) / 255;
                indices.push_back(static_cast<std::uint8_t>((r * 7 + g) * 6 + b));
            }
        }
    }

    // 3. Graphic control extension: the frame's display time.
    put8(out_, 0x21);
    put8(out_, 0xF9);
    put8(out_, 4);
    put8(out_, 1 << 2);  // disposal: leave the frame in place
    put16(out_, delayHundredths);
    put8(out_, 0);  // no transparent colour
    put8(out_, 0);

    // 4. Image descriptor with a 256-entry local colour table.
    put8(out_, 0x2C);
    put16(out_, 0);
    put16(out_, 0);
    put16(out_, width_);
    put16(out_, height_);
    put8(out_, 0x80 | 7);  // local table present, 2^(7+1) = 256 entries
    for (const Color& c : table) {
        put8(out_, c.r);
        put8(out_, c.g);
        put8(out_, c.b);
    }

    // 5. LZW data in sub-blocks of at most 255 bytes, then a 0-length block.
    const int minCodeSize = 8;
    put8(out_, minCodeSize);
    const std::vector<std::uint8_t> data = gifLzwEncode(indices, minCodeSize);
    for (size_t i = 0; i < data.size(); i += 255) {
        const size_t n = std::min<size_t>(255, data.size() - i);
        put8(out_, static_cast<int>(n));
        out_.write(reinterpret_cast<const char*>(&data[i]), static_cast<std::streamsize>(n));
    }
    put8(out_, 0);
    ++frames_;
    return static_cast<bool>(out_);
}

bool GifWriter::close() {
    if (!out_.is_open()) return true;
    put8(out_, 0x3B);  // trailer
    const bool ok = static_cast<bool>(out_);
    out_.close();
    return ok;
}
