#include "OrchHarpPedalLogic.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool condition, const std::string& label)
    {
        if (condition)
        {
            std::cout << "[PASS] " << label << "\n";
        }
        else
        {
            std::cerr << "[FAIL] " << label << "\n";
            ++failures;
        }
    }

    void checkInt (int got, int expected, const std::string& label)
    {
        check (got == expected, label + " (got " + std::to_string (got)
                                + ", expected " + std::to_string (expected) + ")");
    }

    std::string pcsToString (const std::vector<int>& pcs)
    {
        std::string s = "{";
        for (size_t i = 0; i < pcs.size(); ++i)
            s += (i ? "," : "") + std::to_string (pcs[i]);
        return s + "}";
    }

    bool sameSet (std::vector<int> a, std::vector<int> b)
    {
        std::sort (a.begin(), a.end());
        std::sort (b.begin(), b.end());
        return a == b;
    }

    // Apply governor steps until the diagram reaches target or the step budget
    // runs out. Returns the number of moves used (or -1 if it never converged).
    int stepsToConverge (ohrp::Diagram current, const ohrp::Diagram& target, int budget = 32)
    {
        ohrp::MoveInfo info;
        for (int i = 0; i < budget; ++i)
        {
            if (ohrp::diagramsEqual (current, target))
                return i;

            const auto next = ohrp::playablePedalStep (current, target, info);

            for (int letter = 0; letter < 7; ++letter)
            {
                if (next[static_cast<size_t> (letter)] != current[static_cast<size_t> (letter)])
                {
                    if (ohrp::isLeftFootLetter (letter))  info.lastMovedLeft = letter;
                    if (ohrp::isRightFootLetter (letter)) info.lastMovedRight = letter;
                }
            }

            current = next;
        }
        return ohrp::diagramsEqual (current, target) ? budget : -1;
    }

    int maxMovesPerFoot (const ohrp::Diagram& a, const ohrp::Diagram& b)
    {
        // Largest number of pedals changed by a single step within one foot.
        int left = 0, right = 0;
        for (int letter = 0; letter < 7; ++letter)
        {
            if (a[static_cast<size_t> (letter)] == b[static_cast<size_t> (letter)])
                continue;
            if (ohrp::isLeftFootLetter (letter))  ++left;
            if (ohrp::isRightFootLetter (letter)) ++right;
        }
        return std::max (left, right);
    }
}

int main()
{
    using namespace ohrp;

    std::cout << "OrchHarpPedalLogicCheck\n-----------------------\n";

    // --- String model -----------------------------------------------------
    {
        Diagram natural = kAllNatural;
        checkInt (stringPitchClass (0, natural), 0,  "C natural string = C");
        checkInt (stringPitchClass (6, natural), 11, "B natural string = B");

        Diagram bFlat = kAllNatural; bFlat[6] = -1;
        checkInt (stringPitchClass (6, bFlat), 10, "B flat string = Bb");

        Diagram eSharp = kAllNatural; eSharp[2] = +1;
        checkInt (stringPitchClass (2, eSharp), 5, "E# string = F (enharmonic double)");
        // E# and F natural now collide on pitch class 5.
        check (stringPitchClass (2, eSharp) == stringPitchClass (3, eSharp),
               "E# and F natural collide on the same pitch class");
    }

    // --- diagramPitchClasses dedupe -------------------------------------
    {
        Diagram d = kAllNatural; d[2] = +1; // E# == F
        const auto pcs = diagramPitchClasses (d);
        check (pcs.size() == 6, "diagram with one enharmonic double yields 6 distinct pcs");
        check (std::is_sorted (pcs.begin(), pcs.end()), "diagramPitchClasses is sorted");
    }

    // --- nearestStringIndex incl. tie ----------------------------------
    {
        Diagram cMajor = kAllNatural;
        checkInt (nearestStringIndex (1, cMajor), 0, "C# nearest string -> C (tie prefers lower letter)");
        checkInt (nearestStringIndex (6, cMajor), 3, "F# nearest string -> F");
        checkInt (nearestStringIndex (10, cMajor), 5, "A# nearest string -> A");
    }

    // --- white-key transform -------------------------------------------
    {
        Diagram d = kAllNatural; d[6] = -1; // B flat
        checkInt (whiteKeyToStringNote (71, d), 70, "white B4 (71) on a Bb diagram -> 70");
        checkInt (whiteKeyToStringNote (72, d), 72, "white C5 (72) unaffected -> 72");

        Diagram cFlat = kAllNatural; cFlat[0] = -1;
        checkInt (whiteKeyToStringNote (60, cFlat), 59, "white C4 (60) on a Cb diagram -> 59 (octave below)");
    }

    // --- nearest-string note for a black key --------------------------
    {
        Diagram cMajor = kAllNatural;
        checkInt (nearestStringNote (61, cMajor), 60, "black C#4 (61), Nearest -> C4 (60)");
        checkInt (nearestStringNote (66, cMajor), 65, "black F#4 (66), Nearest -> F4 (65)");
    }

    // --- family / variant / key --------------------------------------
    {
        check (sameSet (familyVariantPitchClasses (Family::MajorMinor, 0, 0),
                        { 0, 2, 4, 5, 7, 9, 11 }), "C major pc set");

        const auto cMajorDiagram = familyVariantKeyToDiagram (Family::MajorMinor, 0, 0);
        check (diagramsEqual (cMajorDiagram, kAllNatural), "C major diagram is all-natural");

        // A natural minor is also all white notes -> all-natural diagram.
        const auto aMinorDiagram = familyVariantKeyToDiagram (Family::MajorMinor, 1, 9);
        check (diagramsEqual (aMinorDiagram, kAllNatural), "A natural minor diagram is all-natural");

        // E major: F#, G#, C#, D# -> those four pedals sharp, others natural.
        const auto eMajorDiagram = familyVariantKeyToDiagram (Family::MajorMinor, 0, 4);
        check (eMajorDiagram[0] == 1 && eMajorDiagram[1] == 1   // C#, D#
                 && eMajorDiagram[3] == 1 && eMajorDiagram[4] == 1 // F#, G#
                 && eMajorDiagram[2] == 0 && eMajorDiagram[5] == 0 && eMajorDiagram[6] == 0,
               "E major diagram sharps C D F G, leaves E A B natural");
        check (sameSet (diagramPitchClasses (eMajorDiagram),
                        familyVariantPitchClasses (Family::MajorMinor, 0, 4)),
               "E major diagram sounds exactly the E major pc set");

        // Whole tone on C -> the diagram must sound exactly the whole-tone set.
        const auto wtDiagram = familyVariantKeyToDiagram (Family::WholeTone, 0, 0);
        check (sameSet (diagramPitchClasses (wtDiagram),
                        std::vector<int> { 0, 2, 4, 6, 8, 10 }),
               "whole-tone diagram sounds only whole-tone pitch classes " + pcsToString (diagramPitchClasses (wtDiagram)));

        // Major pentatonic: fewer than 7 letters, leftovers double a member.
        const auto pentDiagram = familyVariantKeyToDiagram (Family::Pentatonic, 0, 0);
        const std::vector<int> pentSet { 0, 2, 4, 7, 9 };
        bool pentClean = true;
        for (int pc : diagramPitchClasses (pentDiagram))
            if (std::find (pentSet.begin(), pentSet.end(), pc) == pentSet.end())
                pentClean = false;
        check (pentClean, "C major pentatonic diagram introduces no foreign pitch class "
                          + pcsToString (diagramPitchClasses (pentDiagram)));
    }

    // --- playability governor ---------------------------------------
    {
        Diagram start = kAllNatural;

        // Target: everything flat. That is 3 left-foot + 4 right-foot changes.
        Diagram allFlat; allFlat.fill (-1);

        const auto oneStep = playablePedalStep (start, allFlat, MoveInfo {});
        check (maxMovesPerFoot (start, oneStep) <= 1, "a single governor move changes at most one pedal per foot");

        int totalChanged = 0;
        for (int letter = 0; letter < 7; ++letter)
            if (oneStep[static_cast<size_t> (letter)] != start[static_cast<size_t> (letter)])
                ++totalChanged;
        check (totalChanged == 2, "first move toward all-flat changes exactly two pedals (one per foot)");

        const int steps = stepsToConverge (start, allFlat);
        check (steps >= 0, "governor converges to an all-flat target");
        check (steps == 4, "all-natural -> all-flat takes 4 moves (max(3 left, 4 right))");

        // flat -> sharp is ONE governor move: the pedal swings the whole way
        // (through the natural notch) in one foot gesture.
        Diagram cFlat = kAllNatural;  cFlat[0]  = -1;
        Diagram cSharp = kAllNatural; cSharp[0] = +1;
        checkInt (stepsToConverge (cFlat, cSharp), 1, "one pedal flat -> sharp is a single governor move");
        checkInt (playablePedalStep (cFlat, cSharp, MoveInfo {})[0], +1, "flat -> sharp lands on sharp in one step");

        // A new target arriving mid-transition re-aims from the current diagram.
        Diagram halfway = playablePedalStep (start, allFlat, MoveInfo {});
        Diagram newTarget = kAllNatural; newTarget[4] = +1; // G sharp only
        const int reaim = stepsToConverge (halfway, newTarget);
        check (reaim >= 0 && reaim <= 6, "governor re-aims to a new target from a partial diagram");

        // Locking out a pedal keeps the governor off it for that move.
        std::array<bool, 7> locked { };
        locked[2] = true; // lock E
        Diagram eTarget = kAllNatural; eTarget[2] = +1;
        const auto lockedStep = playablePedalStep (start, eTarget, MoveInfo {}, locked);
        check (diagramsEqual (lockedStep, start), "a locked-out pedal does not move this step");
    }

    // --- Glissando string-index model -----------------------------
    {
        Diagram natural = kAllNatural;
        checkInt (stringIndexToNote (0, natural, 2), 24, "string index 0 (baseOct 2) = MIDI 24 (C1)");
        checkInt (stringIndexToNote (7, natural, 2), 36, "string index 7 = one octave up = MIDI 36");
        checkInt (stringIndexToNote (4, natural, 2), 31, "string index 4 = G string = MIDI 31");

        Diagram bFlat = kAllNatural; bFlat[6] = -1;
        checkInt (stringIndexToNote (6, bFlat, 2), 34, "string index 6 on a Bb diagram = MIDI 34 (Bb1)");

        // Round-trip: an exact string note maps back to its index.
        checkInt (noteToNearestStringIndex (36, natural, 2), 7, "MIDI 36 -> string index 7");
        checkInt (noteToNearestStringIndex (26, natural, 2), 1, "MIDI 26 (D1) -> string index 1");
        checkInt (noteToNearestStringIndex (31, natural, 2), 4, "MIDI 31 (G1) -> string index 4");

        // Contour map endpoints and midpoint.
        checkInt (mapContour (0, 0, 35), 0,   "contour cc 0 -> lo string");
        checkInt (mapContour (127, 0, 35), 35, "contour cc 127 -> hi string");
        checkInt (mapContour (64, 0, 36), 18, "contour cc 64 -> midpoint");
        checkInt (mapContour (127, 35, 0), 0,  "contour with lo>hi descends");
    }

    // --- Contour mode ----------------------------------------------
    {
        // Rising chromatic input, Tight: ~1 degree per 2 semitones.
        int idx = 20;
        idx = contourNextIndex (60, idx, 62, ContourStep::Tight);  // +2 semitones -> +1
        checkInt (idx, 21, "contour Tight: +2 semitones -> +1 degree");
        idx = contourNextIndex (62, idx, 66, ContourStep::Tight);  // +4 -> +2
        checkInt (idx, 23, "contour Tight: +4 semitones -> +2 degrees");

        // Literal: 1 degree per semitone.
        checkInt (contourNextIndex (60, 10, 63, ContourStep::Literal), 13, "contour Literal: +3 semitones -> +3 degrees");

        // Direction reversal and the minimum-1 rule.
        checkInt (contourNextIndex (60, 10, 61, ContourStep::Tight), 11, "contour Tight: +1 semitone still moves +1 degree");
        checkInt (contourNextIndex (60, 10, 59, ContourStep::Tight), 9,  "contour Tight: -1 semitone -> -1 degree");

        // Repeated input note -> no move.
        checkInt (contourNextIndex (60, 10, 60, ContourStep::Literal), 10, "contour: repeated input note holds the degree");

        // Seed (no previous input).
        checkInt (contourNextIndex (-1, 7, 60, ContourStep::Tight), 7, "contour seed: returns the seed index");

        // Clamped walk (the processor clamps the walk POSITION each step, not
        // just the output): a long descent parks at the floor and recovers on
        // the next ascent instead of running the internal index away.
        {
            const int lo = 0, hi = 28;
            int pos = 20, prevIn = 127;
            for (int i = 0; i < 40; ++i) // a long steady descent
            {
                const int inNote = prevIn - 3;
                pos = std::max (lo, std::min (hi, contourNextIndex (prevIn, pos, inNote, ContourStep::Tight)));
                prevIn = inNote;
            }
            checkInt (pos, lo, "clamped contour walk parks at the floor on a long descent");
            pos = std::max (lo, std::min (hi, contourNextIndex (prevIn, pos, prevIn + 30, ContourStep::Tight)));
            check (pos > lo, "clamped contour walk climbs off the floor on the next ascent");
        }
    }

    // --- Voicing --------------------------------------------------
    {
        auto eq = [] (const std::vector<int>& got, const std::vector<int>& want) { return got == want; };

        std::vector<int> chord4 { 48, 55, 62, 69 }; // C3 G3 D4 A4, straddles the C4 split

        VoiceConfig block { };
        block.splitMode = 1; block.maxVoices = 4; block.maxSpanSemis = 36; block.splitNote = 60;

        block.hand = 1; // Left
        check (eq (selectVoices (chord4, block), { 0, 1 }), "Block split by register: Left keeps notes below the split");
        block.hand = 2; // Right
        check (eq (selectVoices (chord4, block), { 2, 3 }), "Block split by register: Right keeps notes at/above the split");

        // Register split, not count split: a lopsided chord divides where the
        // notes actually sit, and the two halves cover the group with no gap.
        std::vector<int> chord5 { 48, 52, 59, 64, 67 };
        block.hand = 1;
        check (eq (selectVoices (chord5, block), { 0, 1, 2 }), "Block split: Left keeps the three below C4");
        block.hand = 2;
        check (eq (selectVoices (chord5, block), { 3, 4 }), "Block split: Right keeps the two at/above C4");

        // A lone note is routed by register, not dropped by one instance.
        block.hand = 1;
        check (eq (selectVoices ({ 55 }, block), { 0 }), "lone note below split -> Left keeps it");
        check (selectVoices ({ 72 }, block).empty(),       "lone note above split -> Left drops it");
        block.hand = 2;
        check (selectVoices ({ 55 }, block).empty(),       "lone note below split -> Right drops it");
        check (eq (selectVoices ({ 72 }, block), { 0 }),   "lone note above split -> Right keeps it");
        block.hand = 1; block.splitNote = 84;
        check (eq (selectVoices ({ 72 }, block), { 0 }),   "raising the split sends a high lone note to Left");
        block.splitNote = 60;

        VoiceConfig inter { };
        inter.splitMode = 2; inter.maxVoices = 4; inter.maxSpanSemis = 36;
        inter.hand = 1;
        check (eq (selectVoices (chord4, inter), { 0, 2 }), "Interlock, Left -> even indices");
        inter.hand = 2;
        check (eq (selectVoices (chord4, inter), { 1, 3 }), "Interlock, Right -> odd indices");

        // Poly cap, protect both ends: drop from the inside.
        VoiceConfig cap { };
        cap.hand = 0; cap.splitMode = 0; cap.maxVoices = 3; cap.maxSpanSemis = 36; cap.protect = 3;
        const auto capped = selectVoices (chord5, cap);
        check (capped.size() == 3 && capped.front() == 0 && capped.back() == 4,
               "poly cap 3, KeepBothEnds: keeps the two ends + one interior");

        // Span clamp: 20-semitone spread, limit 16, both ends protected -> ends win.
        std::vector<int> wide { 48, 55, 60, 68 }; // span 20
        VoiceConfig sp { };
        sp.hand = 0; sp.maxVoices = 12; sp.maxSpanSemis = 16; sp.protect = 3;
        const auto spanned = selectVoices (wide, sp);
        check (spanned.front() == 0 && spanned.back() == 3,
               "span clamp with both ends protected: protect wins, ends kept");

        // travelNote: the "hand travels" primitive - stepwise along the scale.
        {
            const Diagram nat = kAllNatural; // C D E F G A B
            checkInt (travelNote (60, 60, 60, nat), 60, "travelNote: centre at anchor is identity");
            checkInt (travelNote (60, 64, 60, nat), 64, "travelNote: +4 of centre -> E (a scale step, not an octave)");
            checkInt (travelNote (60, 67, 60, nat), 67, "travelNote: +7 -> G");
            checkInt (travelNote (60, 72, 60, nat), 72, "travelNote: +12 -> the octave, reached through the degrees");
            checkInt (travelNote (48, 55, 48, nat), 55, "travelNote: input tracks the centre offset");
            check (travelNote (127, 127, 0, nat) <= 127, "travelNote: stays in MIDI range");
        }

        // Board-order pedal-marker spelling for OrchCapture.
        check (harpPedalText (kAllNatural) == "D C B | E F G A",
               "harpPedalText: all-natural is board order D C B | E F G A");
        {
            Diagram d = kAllNatural;
            d[0] = -1; // C flat
            d[3] = 1;  // F sharp
            d[5] = -1; // A flat
            check (harpPedalText (d) == "D Cb B | E F# G Ab",
                   "harpPedalText: flats/sharps spelled, feet split by |");
        }

        // hand=Both ignores splitMode.
        VoiceConfig both { };
        both.hand = 0; both.splitMode = 1; both.maxVoices = 12; both.maxSpanSemis = 36;
        check (selectVoices (chord4, both).size() == 4, "hand=Both keeps the whole group regardless of splitMode");
    }

    std::cout << "-----------------------\n";
    if (failures == 0)
    {
        std::cout << "[PASS] OrchHarpPedalLogicCheck passed.\n";
        return 0;
    }

    std::cerr << "[FAIL] " << failures << " failure(s).\n";
    return 1;
}
