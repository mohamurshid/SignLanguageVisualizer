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
// Colour quantization (median cut) - added for the 3D version
// ===========================================================================
// A shaded 3D picture has tens of thousands of colours, a GIF frame at most
// 256. Median cut chooses 256 colours that fit THIS picture:
//   1. put every pixel's colour in one box;
//   2. repeatedly take the box with the widest spread of red, green or blue
//      and cut it in two at the median along that colour;
//   3. each of the final 256 boxes becomes one palette colour (its average).
// Colours are first reduced to 5 bits per channel (32 x 32 x 32 = 32768
// possible values) and counted, so the boxes hold colours, not pixels.
std::vector<Color> medianCutPalette(const Framebuffer& fb, int size) {
    struct Entry {
        int r, g, b, count;
    };
    std::vector<int> histogram(32768, 0);
    for (int y = 0; y < fb.height(); ++y)
        for (int x = 0; x < fb.width(); ++x) {
            const Color c = fb.getPixel(x, y);
            ++histogram[((c.r >> 3) << 10) | ((c.g >> 3) << 5) | (c.b >> 3)];
        }
    std::vector<Entry> colours;
    for (int i = 0; i < 32768; ++i)
        if (histogram[i]) colours.push_back({(i >> 10) & 31, (i >> 5) & 31, i & 31, histogram[i]});

    struct Box {
        size_t begin, end;  // range in 'colours'
    };
    auto spread = [&](const Box& b, int& channel) {
        int lo[3] = {31, 31, 31}, hi[3] = {0, 0, 0};
        for (size_t i = b.begin; i < b.end; ++i) {
            const int v[3] = {colours[i].r, colours[i].g, colours[i].b};
            for (int k = 0; k < 3; ++k) lo[k] = std::min(lo[k], v[k]), hi[k] = std::max(hi[k], v[k]);
        }
        channel = 0;
        for (int k = 1; k < 3; ++k)
            if (hi[k] - lo[k] > hi[channel] - lo[channel]) channel = k;
        return hi[channel] - lo[channel];
    };
    std::vector<Box> boxes{{0, colours.size()}};
    while (static_cast<int>(boxes.size()) < size) {
        int best = -1, bestSpread = 0, bestChannel = 0;
        for (size_t i = 0; i < boxes.size(); ++i) {
            if (boxes[i].end - boxes[i].begin < 2) continue;
            int ch;
            const int sp = spread(boxes[i], ch);
            if (sp > bestSpread) best = static_cast<int>(i), bestSpread = sp, bestChannel = ch;
        }
        if (best < 0) break;  // every box is a single colour
        Box b = boxes[best];
        std::sort(colours.begin() + static_cast<long>(b.begin), colours.begin() + static_cast<long>(b.end),
                  [&](const Entry& p, const Entry& q) {
                      const int a = bestChannel == 0 ? p.r : bestChannel == 1 ? p.g : p.b;
                      const int c = bestChannel == 0 ? q.r : bestChannel == 1 ? q.g : q.b;
                      return a < c;
                  });
        // The median by pixel count, so busy colours get more boxes.
        long total = 0, half = 0;
        for (size_t i = b.begin; i < b.end; ++i) total += colours[i].count;
        size_t cut = b.begin + 1;
        for (size_t i = b.begin; i < b.end - 1; ++i) {
            half += colours[i].count;
            if (half * 2 >= total) {
                cut = i + 1;
                break;
            }
        }
        boxes[best] = {b.begin, cut};
        boxes.push_back({cut, b.end});
    }
    std::vector<Color> palette;
    for (const Box& b : boxes) {
        double r = 0, g = 0, bl = 0, n = 0;
        for (size_t i = b.begin; i < b.end; ++i) {
            r += colours[i].r * colours[i].count;
            g += colours[i].g * colours[i].count;
            bl += colours[i].b * colours[i].count;
            n += colours[i].count;
        }
        if (n == 0) continue;
        auto to8 = [](double v5) { return static_cast<std::uint8_t>(std::min(255.0, v5 * 255.0 / 31.0 + 0.5)); };
        palette.push_back({to8(r / n), to8(g / n), to8(bl / n)});
    }
    return palette;
}

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
    // More than 256 colours (shaded 3D): choose 256 by median cut, and map
    // each 5-bit colour to its nearest palette entry once (a lookup table).
    std::vector<std::uint8_t> nearest;
    if (!exact) {
        table = medianCutPalette(fb, 256);
        nearest.assign(32768, 0);
        for (int i = 0; i < 32768; ++i) {
            const int r = ((i >> 10) & 31) * 255 / 31, g = ((i >> 5) & 31) * 255 / 31, b = (i & 31) * 255 / 31;
            int best = 0, bestD = 1 << 30;
            for (size_t k = 0; k < table.size(); ++k) {
                const int dr = r - table[k].r, dg = g - table[k].g, db = b - table[k].b;
                const int d = 2 * dr * dr + 4 * dg * dg + 3 * db * db;  // the eye is most sensitive to green
                if (d < bestD) bestD = d, best = static_cast<int>(k);
            }
            nearest[i] = static_cast<std::uint8_t>(best);
        }
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
                indices.push_back(nearest[((c.r >> 3) << 10) | ((c.g >> 3) << 5) | (c.b >> 3)]);
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
