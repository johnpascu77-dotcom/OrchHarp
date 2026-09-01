#include "OrchHarpPedalLogic.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ohrp
{
    namespace
    {
        int floorDiv (int a, int b) noexcept
        {
            int q = a / b;
            if ((a % b != 0) && ((a < 0) != (b < 0)))
                --q;
            return q;
        }

        // Smallest circular distance between two pitch classes (0..6).
        int pcDistance (int a, int b) noexcept
        {
            const int d = ((a - b) % 12 + 12) % 12;
            return std::min (d, 12 - d);
        }

        int nearestSetDistance (int pc, const std::vector<int>& set) noexcept
        {
            int best = 12;
            for (int member : set)
                best = std::min (best, pcDistance (pc, member));
            return best;
        }

        // Base (root == C) pitch-class sets per family / variant.
        std::vector<int> baseSet (Family family, int variant)
        {
            switch (family)
            {
                case Family::MajorMinor:
                    switch (variant)
                    {
                        case 1:  return { 0, 2, 3, 5, 7, 8, 10 }; // natural minor
                        case 2:  return { 0, 2, 3, 5, 7, 8, 11 }; // harmonic minor
                        case 3:  return { 0, 2, 3, 5, 7, 9, 11 }; // melodic minor (asc)
                        default: return { 0, 2, 4, 5, 7, 9, 11 }; // major
                    }

                case Family::SeventhChord:
                    switch (variant)
                    {
                        case 1:  return { 0, 4, 7, 11 };  // major 7
                        case 2:  return { 0, 3, 7, 10 };  // minor 7
                        case 3:  return { 0, 3, 6, 9 };   // diminished 7
                        case 4:  return { 0, 3, 6, 10 };  // half-diminished 7
                        default: return { 0, 4, 7, 10 };  // dominant 7
                    }

                case Family::WholeTone:
                    return { 0, 2, 4, 6, 8, 10 };

                case Family::Pentatonic:
                    return variant == 1 ? std::vector<int> { 0, 3, 5, 7, 10 }   // minor pentatonic
                                        : std::vector<int> { 0, 2, 4, 7, 9 };   // major pentatonic
            }
            return { 0, 2, 4, 5, 7, 9, 11 };
        }
    }

    int mod12 (int value) noexcept
    {
        return ((value % 12) + 12) % 12;
    }

    int clampNote (int note) noexcept
    {
        return std::clamp (note, 0, 127);
    }

    bool isWhiteKey (int noteNumber) noexcept
    {
        switch (mod12 (noteNumber))
        {
            case 0: case 2: case 4: case 5: case 7: case 9: case 11:
                return true;
            default:
                return false;
        }
    }

    int letterForWhiteKey (int noteNumber) noexcept
    {
        switch (mod12 (noteNumber))
        {
            case 0:  return 0; // C
            case 2:  return 1; // D
            case 4:  return 2; // E
            case 5:  return 3; // F
            case 7:  return 4; // G
            case 9:  return 5; // A
            case 11: return 6; // B
            default: return -1;
        }
    }

    int stringSemitone (int letter, const Diagram& diagram) noexcept
    {
        letter = std::clamp (letter, 0, 6);
        return kLetterBaseSemitone[static_cast<size_t> (letter)]
             + std::clamp (diagram[static_cast<size_t> (letter)], -1, 1);
    }

    int stringPitchClass (int letter, const Diagram& diagram) noexcept
    {
        return mod12 (stringSemitone (letter, diagram));
    }

    std::vector<int> diagramPitchClasses (const Diagram& diagram)
    {
        std::vector<int> pcs;
        pcs.reserve (7);
        for (int letter = 0; letter < 7; ++letter)
        {
            const int pc = stringPitchClass (letter, diagram);
            if (std::find (pcs.begin(), pcs.end(), pc) == pcs.end())
                pcs.push_back (pc);
        }
        std::sort (pcs.begin(), pcs.end());
        return pcs;
    }

    int nearestStringIndex (int inputPc, const Diagram& diagram) noexcept
    {
        inputPc = mod12 (inputPc);
        int bestLetter = 0;
        int bestDist = 13;
        for (int letter = 0; letter < 7; ++letter)
        {
            const int dist = pcDistance (inputPc, stringPitchClass (letter, diagram));
            if (dist < bestDist)
            {
                bestDist = dist;
                bestLetter = letter;
            }
        }
        return bestLetter;
    }

    int whiteKeyToStringNote (int noteNumber, const Diagram& diagram) noexcept
    {
        const int letter = letterForWhiteKey (noteNumber);
        if (letter < 0)
            return clampNote (noteNumber);

        const int octave = floorDiv (noteNumber, 12);
        return clampNote (12 * octave + stringSemitone (letter, diagram));
    }

    int nearestStringNote (int noteNumber, const Diagram& diagram) noexcept
    {
        const int letter = nearestStringIndex (mod12 (noteNumber), diagram);
        const int base = stringSemitone (letter, diagram);

        // Place that string in the octave whose pitch lands closest to the input
        // note: floor((note - base + 6) / 12) rounds to the nearest octave.
        const int octave = floorDiv (noteNumber - base + 6, 12);
        return clampNote (12 * octave + base);
    }

    Diagram bestFitDiagram (const std::vector<int>& pitchClassesIn)
    {
        std::vector<int> set;
        for (int pc : pitchClassesIn)
            set.push_back (mod12 (pc));
        std::sort (set.begin(), set.end());
        set.erase (std::unique (set.begin(), set.end()), set.end());

        if (set.empty())
            return kAllNatural;

        const auto inSet = [&set] (int pc)
        {
            return std::find (set.begin(), set.end(), pc) != set.end();
        };

        // Search every {-1,0,+1}^7 pedal assignment (2187) and score it:
        //  - a set member no string sounds is the worst (the diagram would lose
        //    a pitch of the scale),
        //  - a string sounding outside the set is next-worst (a foreign pitch),
        //  - then least pedal effort (naturals preferred),
        //  - then best-fit distance for whatever is still off.
        // For a 7-note scale this lands on the unique correct spelling; for
        // smaller sets it doubles the leftover letters onto the nearest member.
        Diagram best = kAllNatural;
        long long bestScore = -1;

        Diagram candidate { };
        std::array<int, 7> digits { };

        for (int combo = 0; combo < 2187; ++combo)
        {
            int c = combo;
            for (int letter = 0; letter < 7; ++letter)
            {
                digits[static_cast<size_t> (letter)] = (c % 3) - 1;
                c /= 3;
                candidate[static_cast<size_t> (letter)] = digits[static_cast<size_t> (letter)];
            }

            std::array<bool, 12> sounded { };
            int effort = 0;
            int fitDistance = 0;
            for (int letter = 0; letter < 7; ++letter)
            {
                const int pc = stringPitchClass (letter, candidate);
                sounded[static_cast<size_t> (pc)] = true;
                effort += std::abs (digits[static_cast<size_t> (letter)]);
                if (! inSet (pc))
                    fitDistance += nearestSetDistance (pc, set);
            }

            int uncovered = 0;
            for (int pc : set)
                if (! sounded[static_cast<size_t> (pc)])
                    ++uncovered;

            int foreign = 0;
            for (int letter = 0; letter < 7; ++letter)
                if (! inSet (stringPitchClass (letter, candidate)))
                    ++foreign;

            const long long score = static_cast<long long> (uncovered) * 100000
                                  + static_cast<long long> (foreign) * 1000
                                  + static_cast<long long> (fitDistance) * 50
                                  + effort;

            if (bestScore < 0 || score < bestScore)
            {
                bestScore = score;
                best = candidate;
            }
        }

        return best;
    }

    Diagram familyVariantKeyToDiagram (Family family, int variant, int baseKey)
    {
        return bestFitDiagram (familyVariantPitchClasses (family, variant, baseKey));
    }

    int stringIndexToNote (int stringIndex, const Diagram& diagram, int baseOctave) noexcept
    {
        const int octave = floorDiv (stringIndex, 7);
        const int letter = stringIndex - 7 * octave; // 0..6
        return clampNote (12 * (baseOctave + octave) + stringSemitone (letter, diagram));
    }

    int noteToNearestStringIndex (int noteNumber, const Diagram& diagram, int baseOctave) noexcept
    {
        int bestIndex = 0;
        int bestDist = 1 << 20;
        for (int s = -12; s <= 60; ++s)
        {
            const int dist = std::abs (stringIndexToNote (s, diagram, baseOctave) - noteNumber);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestIndex = s;
            }
        }
        return bestIndex;
    }

    int mapContour (int ccValue, int loStringIndex, int hiStringIndex) noexcept
    {
        const float t = std::clamp (ccValue, 0, 127) / 127.0f;
        return static_cast<int> (std::lround (loStringIndex + t * static_cast<float> (hiStringIndex - loStringIndex)));
    }

    int numVariants (Family family) noexcept
    {
        switch (family)
        {
            case Family::MajorMinor:   return 4;
            case Family::SeventhChord: return 5;
            case Family::WholeTone:    return 1;
            case Family::Pentatonic:   return 2;
        }
        return 1;
    }

    std::string familyName (Family family)
    {
        switch (family)
        {
            case Family::MajorMinor:   return "Major / Minor";
            case Family::SeventhChord: return "7-Chord";
            case Family::WholeTone:    return "Whole Tone";
            case Family::Pentatonic:   return "Pentatonic";
        }
        return "?";
    }

    std::string variantName (Family family, int variant)
    {
        switch (family)
        {
            case Family::MajorMinor:
                switch (variant)
                {
                    case 1:  return "Natural Minor";
                    case 2:  return "Harmonic Minor";
                    case 3:  return "Melodic Minor";
                    default: return "Major";
                }
            case Family::SeventhChord:
                switch (variant)
                {
                    case 1:  return "Major 7";
                    case 2:  return "Minor 7";
                    case 3:  return "Diminished 7";
                    case 4:  return "Half-Diminished 7";
                    default: return "Dominant 7";
                }
            case Family::WholeTone:
                return "Whole Tone";
            case Family::Pentatonic:
                return variant == 1 ? "Minor Pentatonic" : "Major Pentatonic";
        }
        return "?";
    }

    std::vector<int> familyVariantPitchClasses (Family family, int variant, int baseKey)
    {
        variant = std::clamp (variant, 0, numVariants (family) - 1);
        const int key = mod12 (baseKey);

        std::vector<int> set;
        for (int pc : baseSet (family, variant))
            set.push_back (mod12 (pc + key));

        std::sort (set.begin(), set.end());
        set.erase (std::unique (set.begin(), set.end()), set.end());
        return set;
    }

    bool isLeftFootLetter (int letter) noexcept
    {
        return letter == 6 || letter == 0 || letter == 1; // B, C, D
    }

    bool isRightFootLetter (int letter) noexcept
    {
        return letter == 2 || letter == 3 || letter == 4 || letter == 5; // E, F, G, A
    }

    bool diagramsEqual (const Diagram& a, const Diagram& b) noexcept
    {
        return a == b;
    }

    Diagram playablePedalStep (const Diagram& current,
                               const Diagram& target,
                               const MoveInfo& moveInfo,
                               const std::array<bool, 7>& lockedOut)
    {
        Diagram out = current;

        const auto pickForFoot = [&] (const std::vector<int>& letters, int lastMoved) -> int
        {
            int fallback = -1;
            for (int letter : letters)
            {
                if (lockedOut[static_cast<size_t> (letter)])
                    continue;
                if (current[static_cast<size_t> (letter)] == target[static_cast<size_t> (letter)])
                    continue;

                if (fallback < 0)
                    fallback = letter;
                if (letter != lastMoved)
                    return letter;
            }
            return fallback;
        };

        // Left foot: B, C, D. Right foot: E, F, G, A.
        const int left  = pickForFoot ({ 6, 0, 1 }, moveInfo.lastMovedLeft);
        const int right = pickForFoot ({ 2, 3, 4, 5 }, moveInfo.lastMovedRight);

        for (int letter : { left, right })
        {
            if (letter < 0)
                continue;

            const int cur = current[static_cast<size_t> (letter)];
            const int tgt = target[static_cast<size_t> (letter)];
            out[static_cast<size_t> (letter)] = cur + (cur < tgt ? 1 : -1);
        }

        return out;
    }
}
