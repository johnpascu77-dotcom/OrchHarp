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

    // ---- Glissando engine (Phase 2) --------------------------------------
    //
    // An "absolute string index" spans the whole harp: octave = floor(s / 7),
    // letter = s - 7*octave (0..6). The same index resolves to a different
    // pitch under a different diagram - that is what makes a re-pedalled
    // glissando recolour live.

    // MIDI note sounded by an absolute string index under a diagram. baseOctave
    // places string index 0 (its C): baseOctave 2 -> MIDI 24 for an all-natural
    // diagram.
    int stringIndexToNote (int stringIndex, const Diagram& diagram, int baseOctave) noexcept;

    // Absolute string index whose sounded pitch is closest to `noteNumber`
    // (tie -> lower index). Inverse of stringIndexToNote for the trigger-run
    // start position.
    int noteToNearestStringIndex (int noteNumber, const Diagram& diagram, int baseOctave) noexcept;

    // Map a 0..127 CC value onto a string index between loStringIndex and
    // hiStringIndex (lo may exceed hi for a descending contour).
    int mapContour (int ccValue, int loStringIndex, int hiStringIndex) noexcept;

    // ---- Contour mode (Phase 3) -----------------------------------------
    //
    // Preserve a melody's *shape* and rhythm but re-quantise it to the current
    // diagram's degrees: a chromatic line becomes a hexachord / whole-tone line
    // with the same gesture. The output moves by whole string indices; the
    // input interval only sets direction and how many degrees.

    enum class ContourStep
    {
        Tight = 0,  // ~1 degree per 2 input semitones (scale-step feel)
        Literal,    // 1 degree per input semitone
        Compress,   // ~1 degree per 3 input semitones
        Expand      // ~3 degrees per 2 input semitones
    };

    // Next absolute string index from the previous input note, the previous
    // output string index, and the new input note. First note of a phrase:
    // pass lastInputNote < 0 and it seeds by direction 0 (caller supplies the
    // seed index as lastOutputStringIndex).
    int contourNextIndex (int lastInputNote, int lastOutputStringIndex,
                          int newInputNote, ContourStep step) noexcept;

    // ---- Voicing (Phase 3) ---------------------------------------------
    //
    // Reduce one onset group (a "hand placement") to what a harpist could grab:
    // a per-hand slice of the chord, a polyphony cap, and a span clamp - with
    // the drops weighted by musical function (protect the outer voices, drop
    // from the inside out).

    struct VoiceConfig
    {
        int hand = 0;          // 0 Both, 1 Left, 2 Right
        int splitMode = 0;     // 0 Off, 1 Block, 2 Interlock (Channel: caller pre-filters)
        int maxVoices = 4;     // per group, for this hand
        int maxSpanSemis = 16; // a 10th
        int protect = 3;       // 0 None, 1 KeepLowest, 2 KeepHighest, 3 KeepBothEnds
        int splitNote = 60;    // Block: notes below go Left, at/above go Right. Also
                               // routes a lone note (any split mode) so a monophonic
                               // line isn't silently dropped by one instance.
    };

    // `sortedNotes` ascending (one onset group, already channel-filtered).
    // Returns the surviving indices into it, ascending. Pure & deterministic,
    // so two instances running it on the same group never disagree.
    std::vector<int> selectVoices (const std::vector<int>& sortedNotes, const VoiceConfig& config);

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
