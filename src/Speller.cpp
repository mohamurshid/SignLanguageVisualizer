#include "Speller.h"

#include <cctype>
#include <algorithm>

#include "Signs3D.h"

namespace {

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// The keys of one sign with every key shifted sideways by dx.
std::vector<Key3D> shiftedKeys(const std::vector<Key3D>& keys, double dx) {
    std::vector<Key3D> out = keys;
    for (Key3D& k : out) k.right.place.position.x += dx;
    return out;
}

}  // namespace

std::vector<std::string> tokenize(const std::string& text) {
    // Common contractions first, so "I'm" is "I" and "what's" is "what".
    std::vector<std::string> words;
    std::string cur;
    auto flush = [&] {
        if (cur.empty()) return;
        if (cur == "im") words.push_back("i");
        else if (cur == "whats") words.push_back("what");
        else if (cur == "wheres") words.push_back("where");
        else if (cur == "whos") words.push_back("who");
        else if (cur == "youre") words.push_back("you");
        else words.push_back(cur);
        cur.clear();
    };
    for (char ch : text) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c)) cur += static_cast<char>(std::tolower(c));
        else if (ch == '\'' || c >= 0x80) continue;  // apostrophes (also curly ones) join the word
        else flush();
    }
    flush();
    return words;
}

bool isSkippedWord(const std::string& w) {
    static const char* skipped[] = {"a", "an", "the", "is", "am", "are", "be", "to", "of", "do", "does", "did", "it"};
    for (const char* s : skipped)
        if (w == s) return true;
    return false;
}

int Performance::partAt(double t) const {
    for (size_t i = 0; i < parts.size(); ++i)
        if (t >= parts[i].start - 1e-9 && t <= parts[i].end + 1e-9) return static_cast<int>(i);
    return -1;
}

Performance perform(const std::string& text, const SpellerTiming& timing) {
    Performance perf;
    perf.text = text;
    const std::vector<std::string> words = tokenize(text);

    // The timeline starts with both hands at rest.
    perf.keys.push_back({0.0, restRight(), restLeft()});
    bool first = true;
    auto addSign = [&](const std::vector<Key3D>& keys, double blend, const std::string& label, const std::string& word,
                       bool spelled) {
        appendKeys(perf.keys, keys, first ? timing.fromRest : blend);
        first = false;
        PerformancePart part;
        part.label = label;
        part.word = word;
        part.spelled = spelled;
        part.end = perf.keys.back().time;
        part.start = part.end - (keys.back().time - keys.front().time);
        perf.parts.push_back(part);
    };
    auto spell = [&](const std::string& word) {
        const std::string up = upper(word);
        std::string glossed;
        char previous = 0;
        for (size_t i = 0; i < up.size(); ++i) {
            const char c = up[i];
            const Sign3D* s = findSign(std::string(1, c));
            if (!s) continue;
            // A doubled letter (the LL in ABDALLA) slides a little to the side.
            const double dx = c == previous ? -timing.doubleLetterSlide : 0.0;
            addSign(shiftedKeys(s->keys, dx), i == 0 ? timing.betweenSigns : timing.betweenLetters, std::string(1, c),
                    up, true);
            glossed += (glossed.empty() ? "" : "-") + std::string(1, c);
            previous = c == previous ? 0 : c;  // a triple letter slides back
        }
        if (!glossed.empty()) perf.gloss.push_back(glossed);
    };

    for (size_t i = 0; i < words.size();) {
        // Longest phrase first: "i love you", "thank you".
        const Sign3D* sign = nullptr;
        size_t used = 0;
        for (size_t n = 3; n >= 1 && !sign; --n) {
            if (i + n > words.size()) continue;
            std::string phrase = words[i];
            for (size_t k = 1; k < n; ++k) phrase += " " + words[i + k];
            // A single letter only counts as a sign when it is the whole text
            // ("a" on its own is the letter A; inside a sentence it is skipped).
            const Sign3D* s = findSign(phrase);
            if (s && s->kind == SignKind::Letter && words.size() > 1) s = nullptr;
            if (s) {
                sign = s;
                used = n;
            }
        }
        if (sign) {
            addSign(sign->keys, timing.betweenSigns, sign->gloss, sign->gloss, false);
            perf.gloss.push_back(sign->gloss);
            i += used;
            continue;
        }
        const std::string& w = words[i];
        if (isSkippedWord(w)) {
            ++i;
            continue;
        }
        bool digits = !w.empty();
        for (char c : w) digits = digits && std::isdigit(static_cast<unsigned char>(c));
        if (digits && w.size() > 1) {
            // A number above 10: digit by digit.
            std::string glossed;
            for (char c : w) {
                const Sign3D* d = findSign(std::string(1, c));
                if (!d) continue;
                addSign(d->keys, timing.betweenLetters, std::string(1, c), w, true);
                glossed += (glossed.empty() ? "" : "-") + std::string(1, c);
            }
            perf.gloss.push_back(glossed);
        } else {
            spell(w);
        }
        ++i;
    }

    // Back to rest at the end.
    if (!perf.parts.empty()) {
        const Key3D last = perf.keys.back();
        perf.keys.push_back({last.time + 0.4, last.right, last.left});
        perf.keys.push_back({last.time + 0.4 + timing.toRest, restRight(), restLeft()});
    }
    return perf;
}
