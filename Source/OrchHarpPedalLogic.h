#pragma once

#include <array>
#include <string>
#include <vector>

// Pure pedal-harp pitch logic for OrchHarp. No JUCE, no randomness, no MIDI
// buffer or note-tracking concerns - those live in the processor. This is the
// part the pedal-logic check tool exercises directly.
//
// Shares DNA with OrchNoteFilter's field logic (pitch-class constraint,
// nearest-snap) but the vocabulary here is *pedal diagrams*: 7 strings, one per
// letter name, each retuned flat / natural / sharp by a foot pedal.
namespace ohrp
{
    // Letter index: C=0 D=1 E=2 F=3 G=4 A=5 B=6.
    inline constexpr std::array<int, 7> kLetterBaseSemitone { 0, 2, 4, 5, 7, 9, 11 };

    // The 7 pedal offsets, one per letter, each -1 (flat) / 0 (natural) / +1
    // (sharp). This is a full pedal diagram. The 7 resulting string pitches may
    // collide (E#=F): that is the harpist's "double" pedalling - the way a pitch
    // class is muted in real playing and writing. Strict 7, there is no "off".
    using Diagram = std::array<int, 7>;

    enum class BlackKeyMode
    {
        Control = 0,   // consumed, routed to the control zone (recall / step a slot)
        Nearest,       // sounds, snapped to the nearest string in the current diagram
        Drop,          // filtered out
        Nudge          // bends the nearest pedal one step toward the note
    };

    enum class Family
    {
        MajorMinor = 0, // 7-letter, clean
        SeventhChord,   // 4-letter, best-fit doubling
        WholeTone,      // 6-letter, best-fit doubling
        Pentatonic      // 5-letter, best-fit doubling
    };

    // Heuristic input for playablePedalStep - which pedal moved last, so the
    // governor prefers to spread work across pedals rather than pump one.
    struct MoveInfo
    {
        int lastMovedLeft = -1;   // letter index, or -1
        int lastMovedRight = -1;
    };

    inline constexpr Diagram kAllNatural { 0, 0, 0, 0, 0, 0, 0 };

    int mod12 (int value) noexcept;
    int clampNote (int note) noexcept;

    bool isWhiteKey (int noteNumber) noexcept;   // pc in {C,D,E,F,G,A,B}
    int  letterForWhiteKey (int noteNumber) noexcept; // 0..6, or -1 if black

    // Absolute semitone of a string (may be -1..12 before mod).
    int stringSemitone (int letter, const Diagram& diagram) noexcept;
    // Sounding pitch class 0..11 of a string.
    int stringPitchClass (int letter, const Diagram& diagram) noexcept;

    // The (up-to-7) distinct sounding pitch classes of a diagram, sorted asc.
    std::vector<int> diagramPitchClasses (const Diagram& diagram);

    // Letter (0..6) whose string pitch class is closest, mod-12, to inputPc.
    // Tie -> lower letter index.
    int nearestStringIndex (int inputPc, const Diagram& diagram) noexcept;

    // Given a white-key input note, the output MIDI note on that letter's string
    // in the same octave, sounding its pedal-tuned pitch.
    int whiteKeyToStringNote (int noteNumber, const Diagram& diagram) noexcept;

    // Given any input note, the nearest string's pitch to it (same octave region
    // as the input): used by BlackKeyMode::Nearest.
    int nearestStringNote (int noteNumber, const Diagram& diagram) noexcept;

    // Fit a pedal diagram onto an arbitrary target pitch-class set: every
    // set member should be sounded by some string, no string should sound
    // outside the set, then least pedal effort. For a 7-note set this is the
    // unique correct spelling; for smaller sets the leftover letters
    // enharmonically double the nearest member.
    Diagram bestFitDiagram (const std::vector<int>& pitchClasses);

    // Build a diagram from a scale family / variant / base key. 7-letter
    // families are exact; smaller families assign their members and best-fit
    // (nearest sounding pc) the leftover pedals, so extra letters enharmonically
    // double a member rather than introducing a foreign pitch.
    Diagram familyVariantKeyToDiagram (Family family, int variant, int baseKey);

    // Variant count / names for a family (UI helper; also drives the check tool).
    int numVariants (Family family) noexcept;
    std::string familyName (Family family);
    std::string variantName (Family family, int variant);

    // The pitch-class set a family/variant/key resolves to (before it is fitted
    // onto the 7 letters). Exposed for tests and the UI readout.
    std::vector<int> familyVariantPitchClasses (Family family, int variant, int baseKey);

    // One governor move toward `target` from `current`: at most one pedal
    // changed per foot (left {B,C,D}, right {E,F,G,A}), each stepped a single
    // notch toward its target. A pedal in `lockedOut` (letter index) is skipped
    // for this move (buzz avoidance). Returns `current` unchanged once it equals
    // `target` or every disagreeing pedal is locked out.
    Diagram playablePedalStep (const Diagram& current,
                               const Diagram& target,
                               const MoveInfo& moveInfo,
                               const std::array<bool, 7>& lockedOut = { });

    // True when two diagrams are pedal-for-pedal identical.
    bool diagramsEqual (const Diagram& a, const Diagram& b) noexcept;

    // Foot membership.
    bool isLeftFootLetter (int letter) noexcept;   // B, C, D
    bool isRightFootLetter (int letter) noexcept;  // E, F, G, A
}
