#pragma once
// Speller: type a word or a sentence, and the hands sign it (final project).
//
//   1. Split the text into words ("Nice to meet you!" -> nice, to, meet, you).
//   2. Look each word (or two- and three-word phrase, such as "thank you")
//      up in the sign dictionary (Signs3D).
//   3. A word ASL leaves out (a, the, is, to, ...) is skipped.
//   4. A word with a sign is signed; any other word is FINGERSPELLED letter by
//      letter, which is what signers do for names and words with no sign.
//      Numbers 0-10 have their own signs; larger numbers are shown digit by
//      digit.
//   5. All the keyframes are joined into ONE timeline with short blends
//      between signs, starting and ending with the hands at rest.
//
// The result also lists each part (word or spelled letter) with its start and
// end time, so the screen can highlight what is being signed.
//
// This is English word order with ASL signs, not ASL grammar - the project
// scope leaves grammar (and facial expressions) out.

#include <string>
#include <vector>

#include "Timeline.h"

struct PerformancePart {
    std::string label;     // "HELLO", or "A" for one spelled letter
    std::string word;      // the word it belongs to ("ABDALLA")
    bool spelled = false;  // true = one letter of a fingerspelled word
    double start = 0.0;    // seconds
    double end = 0.0;
};

struct Performance {
    std::string text;                    // what was typed
    std::vector<Key3D> keys;             // the whole timeline
    std::vector<PerformancePart> parts;  // in order
    std::vector<std::string> gloss;      // e.g. {"HELLO", "MY", "NAME", "A-B-D-A-L-L-A"}

    double duration() const { return keys.empty() ? 0.0 : keys.back().time; }
    // The part being signed at time t (-1 = none: moving between signs).
    int partAt(double t) const;
};

struct SpellerTiming {
    double betweenSigns = 0.34;    // blend from one sign to the next
    double betweenLetters = 0.18;  // blend between fingerspelled letters
    double fromRest = 0.5;         // raising the hands at the start
    double toRest = 0.6;           // lowering them at the end
    double doubleLetterSlide = 0.22;  // a repeated letter slides sideways
};

Performance perform(const std::string& text, const SpellerTiming& timing = SpellerTiming{});

// Splits text into lower-case words (letters and digits only).
std::vector<std::string> tokenize(const std::string& text);

// True for words that ASL does not sign as separate signs.
bool isSkippedWord(const std::string& word);
