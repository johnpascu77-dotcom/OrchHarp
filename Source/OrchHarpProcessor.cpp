#include "OrchHarpProcessor.h"
#include "OrchHarpEditor.h"

#include <algorithm>

namespace
{
    // Pedal choice index <-> pedal offset. Index 0 Flat / 1 Natural / 2 Sharp.
    int offsetFromChoiceIndex (int index) noexcept { return juce::jlimit (-1, 1, index - 1); }
    int choiceIndexFromOffset (int offset) noexcept { return juce::jlimit (0, 2, offset + 1); }

    // minChangeInterval choice -> beats (4/4 assumed; a time-signature-aware
    // version is a later refinement - see design doc §6 / §12).
    const std::array<double, 5> kIntervalBeats { 0.5, 1.0, 2.0, 4.0, 8.0 };

    // glissRunDuration choice -> total run length in beats (4/4 assumed):
    // 1/16, 1/8, 1/4, 1/2, 1 bar.
    const std::array<double, 5> kRunDurationBeats { 0.25, 0.5, 1.0, 2.0, 4.0 };

    constexpr int kMaxRingingGliss = 256;

    // Factory bank: the "universal glissando" starting points (design §5).
    // Authored as target pitch-class sets and fitted onto the 7 pedals by the
    // same best-fit logic the family helper uses, so a slot always sounds
    // exactly its set.
    struct FactorySlot { const char* name; std::vector<int> pcs; juce::uint32 colour; };

    const std::array<FactorySlot, OrchHarpAudioProcessor::kNumBankSlots>& factoryBank()
    {
        static const std::array<FactorySlot, 12> slots {{
            { "C Major",            { 0, 2, 4, 5, 7, 9, 11 },      0xff5fc8f5 },
            { "A Natural Minor",    { 9, 11, 0, 2, 4, 5, 7 },      0xff6fd0a0 },
            { "A Melodic Minor",    { 9, 11, 0, 2, 4, 6, 8 },      0xff7fd070 },
            { "A Harmonic Minor",   { 9, 11, 0, 2, 4, 5, 8 },      0xff9fd060 },
            { "C Whole Tone",       { 0, 2, 4, 6, 8, 10 },         0xfff5c85f },
            { "C Major Pentatonic", { 0, 2, 4, 7, 9 },             0xfff5a05f },
            { "A Minor Pentatonic", { 9, 0, 2, 4, 7 },             0xfff57f7f },
            { "C Octatonic H-W",    { 0, 1, 3, 4, 6, 7, 9, 10 },   0xffc87ff5 },
            { "C Octatonic W-H",    { 0, 2, 3, 5, 6, 8, 9, 11 },   0xffa07ff5 },
            { "C Quartal",          { 0, 5, 10, 3, 8 },            0xff7f9ff5 },
            { "Hexachord 012678",   { 0, 1, 2, 6, 7, 8 },          0xffc0c0c0 },
            { "Hexachord 014589",   { 0, 1, 4, 5, 8, 9 },          0xffd0b090 },
        }};
        return slots;
    }
}

// ============================================================================

OrchHarpAudioProcessor::OrchHarpAudioProcessor()
    : AudioProcessor (BusesProperties()),
      parameters (*this, nullptr, "OrchHarpParameters", createParameterLayout())
{
    modeParam = parameters.getRawParameterValue ("mode");
    static const std::array<const char*, 7> pedalIds { "pedalC", "pedalD", "pedalE", "pedalF", "pedalG", "pedalA", "pedalB" };
    for (int i = 0; i < 7; ++i)
    {
        pedalParam[static_cast<size_t> (i)]  = parameters.getRawParameterValue (pedalIds[static_cast<size_t> (i)]);
        pedalChoice[static_cast<size_t> (i)] = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter (pedalIds[static_cast<size_t> (i)]));
    }
    bankSlotParam        = parameters.getRawParameterValue ("bankSlot");
    blackKeyModeParam    = parameters.getRawParameterValue ("blackKeyMode");
    playabilityParam     = parameters.getRawParameterValue ("playability");
    minChangeIntervalParam = parameters.getRawParameterValue ("minChangeInterval");
    changesAtRestsOnlyParam = parameters.getRawParameterValue ("changesAtRestsOnly");
    avoidRingingParam    = parameters.getRawParameterValue ("avoidRingingPedalChange");
    ccBankSelectParam    = parameters.getRawParameterValue ("ccBankSelect");
    ccChannelParam       = parameters.getRawParameterValue ("ccChannel");
    ctrlDirectLoParam    = parameters.getRawParameterValue ("ctrlDirectLo");
    ctrlDirectHiParam    = parameters.getRawParameterValue ("ctrlDirectHi");
    ctrlStepDownParam    = parameters.getRawParameterValue ("ctrlStepDownNote");
    ctrlStepUpParam      = parameters.getRawParameterValue ("ctrlStepUpNote");

    glissCcParam           = parameters.getRawParameterValue ("glissCc");
    glissLoNoteParam       = parameters.getRawParameterValue ("glissLoNote");
    glissHiNoteParam       = parameters.getRawParameterValue ("glissHiNote");
    glissVelCcParam        = parameters.getRawParameterValue ("glissVelCc");
    glissVelocityParam     = parameters.getRawParameterValue ("glissVelocity");
    glissRingParam         = parameters.getRawParameterValue ("glissRing");
    glissReleaseParam      = parameters.getRawParameterValue ("glissRelease");
    glissTrigLoParam       = parameters.getRawParameterValue ("glissTrigLo");
    glissTrigHiParam       = parameters.getRawParameterValue ("glissTrigHi");
    glissRunLoNoteParam    = parameters.getRawParameterValue ("glissRunLoNote");
    glissRunHiNoteParam    = parameters.getRawParameterValue ("glissRunHiNote");
    glissRunDirectionParam = parameters.getRawParameterValue ("glissRunDirection");
    glissRunDurationParam  = parameters.getRawParameterValue ("glissRunDuration");

    bankSlotInt = dynamic_cast<juce::AudioParameterInt*> (parameters.getParameter ("bankSlot"));

    resetBankToFactory();
    resetNoteMap();

    soundingDiagram = readRequestedDiagram();
    storeReadoutDiagrams (soundingDiagram);
}

juce::StringArray OrchHarpAudioProcessor::pedalChoiceLabels()
{
    return { "Flat", "Natural", "Sharp" };
}

juce::AudioProcessorValueTreeState::ParameterLayout OrchHarpAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Declaration order == Bitwig remote-control page order, 8 per page
    // (design doc §8).
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "Pedal", "Chromatic" }, 0));

    const std::array<const char*, 7> pedalIds   { "pedalC", "pedalD", "pedalE", "pedalF", "pedalG", "pedalA", "pedalB" };
    const std::array<const char*, 7> pedalNames { "Pedal C", "Pedal D", "Pedal E", "Pedal F", "Pedal G", "Pedal A", "Pedal B" };
    for (int i = 0; i < 7; ++i)
        params.push_back (std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { pedalIds[static_cast<size_t> (i)], 1 },
            pedalNames[static_cast<size_t> (i)],
            juce::StringArray { "Flat", "Natural", "Sharp" }, 1)); // Natural

    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "bankSlot", 1 }, "Bank Slot", 0, kNumBankSlots - 1, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "blackKeyMode", 1 }, "Black Key Mode",
        juce::StringArray { "Control", "Nearest", "Drop", "Nudge" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "playability", 1 }, "Playability Governor", true));

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "minChangeInterval", 1 }, "Min Change Interval",
        juce::StringArray { "1/8", "1/4", "1/2", "1 bar", "2 bars" }, 1)); // 1/4

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "changesAtRestsOnly", 1 }, "Changes At Rests Only", false));

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "avoidRingingPedalChange", 1 }, "Avoid Ringing Pedal Change", false));

    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ccBankSelect", 1 }, "CC# Bank Select (0 = off)", 0, 127, 49));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ccChannel", 1 }, "CC Channel (0 = any)", 0, 16, 0));

    // Black-key control zone. Defaults: direct-select C-1..B-1 (MIDI 0..11),
    // step down/up MIDI 12/13 - all below any folded harp playing range
    // (design doc §12).
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ctrlDirectLo", 1 }, "Ctrl Direct Lo Note", 0, 127, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ctrlDirectHi", 1 }, "Ctrl Direct Hi Note", 0, 127, 11));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ctrlStepDownNote", 1 }, "Ctrl Step Down Note", 0, 127, 12));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "ctrlStepUpNote", 1 }, "Ctrl Step Up Note", 0, 127, 13));

    // ---- Glissando engine (Phase 2a) -------------------------------------
    // Contour follower: a CC contour drives a string position between two fixed
    // notes; a note fires each time the diagram-snapped string changes.
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "glissCc", 1 }, "Gliss CC# (0 = off)", 0, 127, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "glissLoNote", 1 }, "Gliss Low Note", 0, 127, 24));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "glissHiNote", 1 }, "Gliss High Note", 0, 127, 96));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "glissVelCc", 1 }, "Gliss Velocity CC# (0 = fixed)", 0, 127, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "glissVelocity", 1 }, "Gliss Velocity", 1, 127, 96));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "glissRing", 1 }, "Gliss Ring",
        juce::StringArray { "Monophonic", "Ring" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "glissRelease", 1 }, "Gliss Release (CC idle)",
        juce::StringArray { "Hold", "1/8", "1/4", "1/2", "1 bar" }, 2)); // 1/4

    // Trigger gesture: one note in a zone fires a run across the run window in
    // the chosen direction over a tempo-synced duration; velocity scales how
    // far into the window the run reaches.
    // Trigger zone: 0 / 0 = off (set a zone to enable).
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "glissTrigLo", 1 }, "Gliss Trigger Lo Note", 0, 127, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "glissTrigHi", 1 }, "Gliss Trigger Hi Note", 0, 127, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "glissRunLoNote", 1 }, "Gliss Run Low Note", 0, 127, 24));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "glissRunHiNote", 1 }, "Gliss Run High Note", 0, 127, 103));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "glissRunDirection", 1 }, "Gliss Run Direction",
        juce::StringArray { "Up", "Down", "Up-Down", "Down-Up" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "glissRunDuration", 1 }, "Gliss Run Duration",
        juce::StringArray { "1/16", "1/8", "1/4", "1/2", "1 bar" }, 2));

    return { params.begin(), params.end() };
}

// ---- Bank ------------------------------------------------------------------

void OrchHarpAudioProcessor::resetBankToFactory()
{
    const juce::SpinLock::ScopedLockType lock (bankLock);
    const auto& factory = factoryBank();
    for (int i = 0; i < kNumBankSlots; ++i)
    {
        bank[static_cast<size_t> (i)].offsets = ohrp::bestFitDiagram (factory[static_cast<size_t> (i)].pcs);
        bank[static_cast<size_t> (i)].name    = factory[static_cast<size_t> (i)].name;
        bank[static_cast<size_t> (i)].colour  = factory[static_cast<size_t> (i)].colour;
    }
}

OrchHarpAudioProcessor::BankSlot OrchHarpAudioProcessor::getBankSlot (int index) const
{
    const juce::SpinLock::ScopedLockType lock (bankLock);
    if (index < 0 || index >= kNumBankSlots)
        return {};
    return bank[static_cast<size_t> (index)];
}

void OrchHarpAudioProcessor::recallBankSlot (int index)
{
    if (bankSlotInt != nullptr && index >= 0 && index < kNumBankSlots && bankSlotInt->get() != index)
        *bankSlotInt = index; // AudioParameterInt::operator= -> setValueNotifyingHost
}

void OrchHarpAudioProcessor::saveCurrentDiagramToSlot (int index)
{
    if (index < 0 || index >= kNumBankSlots)
        return;
    const auto requested = readRequestedDiagram();
    const juce::SpinLock::ScopedLockType lock (bankLock);
    bank[static_cast<size_t> (index)].offsets = requested;
}

void OrchHarpAudioProcessor::setBankSlot (int index, const ohrp::Diagram& offsets, const juce::String& name)
{
    if (index < 0 || index >= kNumBankSlots)
        return;
    const juce::SpinLock::ScopedLockType lock (bankLock);
    bank[static_cast<size_t> (index)].offsets = offsets;
    if (name.isNotEmpty())
        bank[static_cast<size_t> (index)].name = name;
}

void OrchHarpAudioProcessor::renameBankSlot (int index, const juce::String& name)
{
    const juce::SpinLock::ScopedLockType lock (bankLock);
    if (index >= 0 && index < kNumBankSlots)
        bank[static_cast<size_t> (index)].name = name;
}

void OrchHarpAudioProcessor::recolourBankSlot (int index, juce::Colour colour)
{
    const juce::SpinLock::ScopedLockType lock (bankLock);
    if (index >= 0 && index < kNumBankSlots)
        bank[static_cast<size_t> (index)].colour = colour.getARGB();
}

juce::ValueTree OrchHarpAudioProcessor::bankToTree() const
{
    juce::ValueTree tree ("bank");
    const juce::SpinLock::ScopedLockType lock (bankLock);
    for (int i = 0; i < kNumBankSlots; ++i)
    {
        const auto& slot = bank[static_cast<size_t> (i)];
        juce::ValueTree child ("slot");
        child.setProperty ("index", i, nullptr);
        child.setProperty ("name", slot.name, nullptr);
        child.setProperty ("colour", (juce::int64) slot.colour, nullptr);
        for (int p = 0; p < 7; ++p)
            child.setProperty ("o" + juce::String (p), slot.offsets[static_cast<size_t> (p)], nullptr);
        tree.appendChild (child, nullptr);
    }
    return tree;
}

void OrchHarpAudioProcessor::bankFromTree (const juce::ValueTree& tree)
{
    if (! tree.hasType ("bank"))
        return;

    const juce::SpinLock::ScopedLockType lock (bankLock);
    for (int c = 0; c < tree.getNumChildren(); ++c)
    {
        const auto child = tree.getChild (c);
        const int index = (int) child.getProperty ("index", c);
        if (index < 0 || index >= kNumBankSlots)
            continue;

        auto& slot = bank[static_cast<size_t> (index)];
        slot.name   = child.getProperty ("name", slot.name).toString();
        slot.colour = (juce::uint32) (juce::int64) child.getProperty ("colour", (juce::int64) slot.colour);
        for (int p = 0; p < 7; ++p)
            slot.offsets[static_cast<size_t> (p)] =
                juce::jlimit (-1, 1, (int) child.getProperty ("o" + juce::String (p),
                                                              slot.offsets[static_cast<size_t> (p)]));
    }
}

// ---- Diagram helpers ------------------------------------------------------

ohrp::Diagram OrchHarpAudioProcessor::readRequestedDiagram() const
{
    ohrp::Diagram d = ohrp::kAllNatural;
    for (int i = 0; i < 7; ++i)
        if (pedalParam[static_cast<size_t> (i)] != nullptr)
            d[static_cast<size_t> (i)] = offsetFromChoiceIndex (juce::roundToInt (pedalParam[static_cast<size_t> (i)]->load()));
    return d;
}

void OrchHarpAudioProcessor::applyDiagramToParams (const ohrp::Diagram& diagram)
{
    for (int i = 0; i < 7; ++i)
    {
        auto* p = pedalChoice[static_cast<size_t> (i)];
        const int want = choiceIndexFromOffset (diagram[static_cast<size_t> (i)]);
        if (p != nullptr && p->getIndex() != want)
            *p = want;
    }
}

double OrchHarpAudioProcessor::minChangeIntervalInBeats() const
{
    const int idx = minChangeIntervalParam != nullptr
        ? juce::jlimit (0, 4, juce::roundToInt (minChangeIntervalParam->load())) : 1;
    return kIntervalBeats[static_cast<size_t> (idx)];
}

double OrchHarpAudioProcessor::glissReleaseInBeats() const
{
    // Choice: Hold / 1/8 / 1/4 / 1/2 / 1 bar.
    static const std::array<double, 5> beats { 0.0, 0.5, 1.0, 2.0, 4.0 };
    const int idx = glissReleaseParam != nullptr
        ? juce::jlimit (0, 4, juce::roundToInt (glissReleaseParam->load())) : 2;
    return beats[static_cast<size_t> (idx)];
}

int OrchHarpAudioProcessor::glissWindowLoIndex (bool trigger) const
{
    auto* lo = trigger ? glissRunLoNoteParam : glissLoNoteParam;
    auto* hi = trigger ? glissRunHiNoteParam : glissHiNoteParam;
    const int loNote = lo != nullptr ? juce::jlimit (0, 127, juce::roundToInt (lo->load())) : 24;
    const int hiNote = hi != nullptr ? juce::jlimit (0, 127, juce::roundToInt (hi->load())) : 96;
    const int a = ohrp::noteToNearestStringIndex (loNote, soundingDiagram, kGlissBaseOctave);
    const int b = ohrp::noteToNearestStringIndex (hiNote, soundingDiagram, kGlissBaseOctave);
    return juce::jmin (a, b);
}

int OrchHarpAudioProcessor::glissWindowHiIndex (bool trigger) const
{
    auto* lo = trigger ? glissRunLoNoteParam : glissLoNoteParam;
    auto* hi = trigger ? glissRunHiNoteParam : glissHiNoteParam;
    const int loNote = lo != nullptr ? juce::jlimit (0, 127, juce::roundToInt (lo->load())) : 24;
    const int hiNote = hi != nullptr ? juce::jlimit (0, 127, juce::roundToInt (hi->load())) : 96;
    const int a = ohrp::noteToNearestStringIndex (loNote, soundingDiagram, kGlissBaseOctave);
    const int b = ohrp::noteToNearestStringIndex (hiNote, soundingDiagram, kGlissBaseOctave);
    return juce::jmax (a, b);
}

ohrp::Diagram OrchHarpAudioProcessor::getSoundingDiagramForUi() const
{
    ohrp::Diagram d = ohrp::kAllNatural;
    for (int i = 0; i < 7; ++i)
        d[static_cast<size_t> (i)] = juce::jlimit (-1, 1, soundingUi[static_cast<size_t> (i)].load());
    return d;
}

ohrp::Diagram OrchHarpAudioProcessor::getRequestedDiagramForUi() const
{
    ohrp::Diagram d = ohrp::kAllNatural;
    for (int i = 0; i < 7; ++i)
        d[static_cast<size_t> (i)] = juce::jlimit (-1, 1, requestedUi[static_cast<size_t> (i)].load());
    return d;
}

void OrchHarpAudioProcessor::storeReadoutDiagrams (const ohrp::Diagram& requested)
{
    int transit = 0;
    for (int i = 0; i < 7; ++i)
    {
        soundingUi[static_cast<size_t> (i)].store (soundingDiagram[static_cast<size_t> (i)]);
        requestedUi[static_cast<size_t> (i)].store (requested[static_cast<size_t> (i)]);
        if (soundingDiagram[static_cast<size_t> (i)] != requested[static_cast<size_t> (i)])
            ++transit;
    }
    movesInTransit.store (transit);
}

// ---- Lifecycle ----------------------------------------------------------

void OrchHarpAudioProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    resetNoteMap();
    integratedPpq = 0.0;
    wasPlaying = false;
    beatsSinceMove = 1.0e6;
    beatsSinceNudge = 1.0e6;
    lastAppliedBankSlot = -1;
    soundingDiagram = readRequestedDiagram();
    moveInfo = {};
    storeReadoutDiagrams (soundingDiagram);
}

void OrchHarpAudioProcessor::flushGlissNotes (juce::MidiBuffer& output, int samplePosition)
{
    if (lastGlissNote >= 0)
        output.addEvent (juce::MidiMessage::noteOff (1, lastGlissNote), samplePosition);
    if (runLastNote >= 0 && runLastNote != lastGlissNote)
        output.addEvent (juce::MidiMessage::noteOff (1, runLastNote), samplePosition);
    for (int note : glissRingNotes)
        if (note != lastGlissNote && note != runLastNote)
            output.addEvent (juce::MidiMessage::noteOff (1, note), samplePosition);

    lastGlissNote = -1;
    runLastNote = -1;
    lastGlissString = std::numeric_limits<int>::min();
    glissRingNotes.clear();
    pendingGliss.clear();
    glissIdleBeats = 0.0;

    lastGlissNoteUi.store (-1);
    glissActiveCountUi.store (0);
}

void OrchHarpAudioProcessor::releaseIdleGlissNotes (juce::MidiBuffer& output, int samplePosition)
{
    const double releaseBeats = glissReleaseInBeats();
    if (releaseBeats <= 0.0)              // "Hold"
        return;
    if (! pendingGliss.empty())           // a run is still playing out
        return;
    if (glissIdleBeats < releaseBeats)
        return;

    const bool anything = lastGlissNote >= 0 || runLastNote >= 0 || ! glissRingNotes.empty();
    if (! anything)
        return;

    if (lastGlissNote >= 0)
        output.addEvent (juce::MidiMessage::noteOff (1, lastGlissNote), samplePosition);
    if (runLastNote >= 0 && runLastNote != lastGlissNote)
        output.addEvent (juce::MidiMessage::noteOff (1, runLastNote), samplePosition);
    for (int note : glissRingNotes)
        if (note != lastGlissNote && note != runLastNote)
            output.addEvent (juce::MidiMessage::noteOff (1, note), samplePosition);

    lastGlissNote = -1;
    runLastNote = -1;
    lastGlissString = std::numeric_limits<int>::min();
    glissRingNotes.clear();
    lastGlissNoteUi.store (-1);
    glissActiveCountUi.store (0);
}

void OrchHarpAudioProcessor::releaseResources() {}

bool OrchHarpAudioProcessor::isBusesLayoutSupported (const BusesLayout&) const { return true; }

void OrchHarpAudioProcessor::resetNoteMap()
{
    activeNotes.clear();
    stringQuietBeats.fill (1.0e6);

    lastGlissNote = -1;
    runLastNote = -1;
    lastGlissString = std::numeric_limits<int>::min();
    glissRingNotes.clear();
    pendingGliss.clear();
    glissIdleBeats = 0.0;
    lastGlissNoteUi.store (-1);
    glissActiveCountUi.store (0);
}

// ---- Governor ---------------------------------------------------------

void OrchHarpAudioProcessor::snapSoundingToRequested (const ohrp::Diagram& requested)
{
    soundingDiagram = requested;
    moveInfo = {};
}

void OrchHarpAudioProcessor::runGovernor (const ohrp::Diagram& requested, double blockBeats)
{
    // Elapsed-beat accumulators, not host-ppq comparisons: a loop region or a
    // transport jump used to leave blockPpq < lastMovePpq and wedge the governor
    // until stop. Accumulators can't do that.
    beatsSinceMove = juce::jmin (beatsSinceMove + blockBeats, 1.0e6);
    beatsSinceNudge = juce::jmin (beatsSinceNudge + blockBeats, 1.0e6);
    for (auto& q : stringQuietBeats)
        q = juce::jmin (q + blockBeats, 1.0e6);

    const bool governed = playabilityParam == nullptr || playabilityParam->load() >= 0.5f;

    if (! governed)
    {
        soundingDiagram = requested;
        return;
    }

    if (ohrp::diagramsEqual (soundingDiagram, requested))
        return;

    // Hold every move until the track is silent, when asked.
    const bool restsOnly = changesAtRestsOnlyParam != nullptr && changesAtRestsOnlyParam->load() >= 0.5f;
    if (restsOnly && ! activeNotes.empty())
        return;

    const double intervalBeats = minChangeIntervalInBeats();
    if (beatsSinceMove < intervalBeats)
        return;

    // Buzz avoidance: skip a pedal whose string sounded within the last
    // interval - but only as a *soft* delay. On a dense passage every string is
    // always sounding; once the move is well overdue, let it through anyway so
    // the governor can never wedge (design §6 keeps this "irrelevant for the
    // score").
    std::array<bool, 7> lockedOut { };
    const bool overdue = beatsSinceMove > intervalBeats * 3.0;
    if (! overdue && avoidRingingParam != nullptr && avoidRingingParam->load() >= 0.5f)
        for (int i = 0; i < 7; ++i)
            if (stringQuietBeats[static_cast<size_t> (i)] < intervalBeats)
                lockedOut[static_cast<size_t> (i)] = true;

    const auto next = ohrp::playablePedalStep (soundingDiagram, requested, moveInfo, lockedOut);
    if (ohrp::diagramsEqual (next, soundingDiagram))
        return; // nothing moved (all disagreeing pedals locked out) - try again later

    for (int i = 0; i < 7; ++i)
    {
        if (next[static_cast<size_t> (i)] != soundingDiagram[static_cast<size_t> (i)])
        {
            if (ohrp::isLeftFootLetter (i))  moveInfo.lastMovedLeft = i;
            if (ohrp::isRightFootLetter (i)) moveInfo.lastMovedRight = i;
        }
    }

    soundingDiagram = next;
    beatsSinceMove = 0.0;
}

// ---- Control CC / notes ----------------------------------------------

void OrchHarpAudioProcessor::handleControlCc (const juce::MidiMessage& message)
{
    const int ccNum = ccBankSelectParam != nullptr
        ? juce::jlimit (0, 127, juce::roundToInt (ccBankSelectParam->load())) : 49;
    if (ccNum == 0 || message.getControllerNumber() != ccNum)
        return;

    const int value = juce::jlimit (0, 127, message.getControllerValue());
    const int slot = juce::roundToInt (value / 127.0f * (kNumBankSlots - 1));
    recallBankSlot (juce::jlimit (0, kNumBankSlots - 1, slot));
    lastCc.store (ccNum);
}

bool OrchHarpAudioProcessor::tryConsumeControlNote (int noteNumber)
{
    const auto readNote = [] (std::atomic<float>* p, int fallback)
    {
        return p != nullptr ? juce::jlimit (0, 127, juce::roundToInt (p->load())) : fallback;
    };

    const int directLo = readNote (ctrlDirectLoParam, 0);
    const int directHi = readNote (ctrlDirectHiParam, 11);
    const int stepDown = readNote (ctrlStepDownParam, 12);
    const int stepUp   = readNote (ctrlStepUpParam, 13);

    const int lo = juce::jmin (directLo, directHi);
    const int hi = juce::jmax (directLo, directHi);

    const int current = bankSlotInt != nullptr ? bankSlotInt->get() : 0;

    if (noteNumber >= lo && noteNumber <= hi)
    {
        recallBankSlot (juce::jlimit (0, kNumBankSlots - 1, noteNumber - lo));
        return true;
    }
    if (noteNumber == stepDown)
    {
        recallBankSlot (juce::jlimit (0, kNumBankSlots - 1, current - 1));
        return true;
    }
    if (noteNumber == stepUp)
    {
        recallBankSlot (juce::jlimit (0, kNumBankSlots - 1, current + 1));
        return true;
    }

    return false; // not a control-zone note - caller handles it (re-pedal)
}

// ---- Glissando engine ------------------------------------------------

bool OrchHarpAudioProcessor::handleGlissCc (const juce::MidiMessage& message, int samplePosition,
                                            juce::MidiBuffer& output)
{
    const auto readNum = [] (std::atomic<float>* p, int fallback)
    {
        return p != nullptr ? juce::jlimit (0, 127, juce::roundToInt (p->load())) : fallback;
    };

    const int glissCc  = readNum (glissCcParam, 0);
    const int velCc     = readNum (glissVelCcParam, 0);
    const int cc        = message.getControllerNumber();
    const int value     = juce::jlimit (0, 127, message.getControllerValue());

    if (velCc != 0 && cc == velCc)
    {
        glissVelValue = juce::jmax (1, value);
        return true; // consumed
    }

    if (glissCc == 0 || cc != glissCc)
        return false;

    lastCc.store (cc);

    // CC back to 0 damps the run.
    if (value == 0)
    {
        flushGlissNotes (output, samplePosition);
        return true;
    }

    glissIdleBeats = 0.0; // the contour is moving

    // The string window comes from Gliss Low Note / Gliss High Note, resolved
    // to string indices against the current diagram. Low may map above High for
    // a descending contour.
    const int loNote = readNum (glissLoNoteParam, 24);
    const int hiNote = readNum (glissHiNoteParam, 96);
    const int loIdx = ohrp::noteToNearestStringIndex (loNote, soundingDiagram, kGlissBaseOctave);
    const int hiIdx = ohrp::noteToNearestStringIndex (hiNote, soundingDiagram, kGlissBaseOctave);
    const int s = ohrp::mapContour (value, loIdx, hiIdx);

    if (s == lastGlissString)
        return true;

    lastGlissString = s;

    const int note = ohrp::stringIndexToNote (s, soundingDiagram, kGlissBaseOctave);
    const int vel = velCc != 0
        ? glissVelValue
        : juce::jlimit (1, 127, readNum (glissVelocityParam, 96));

    const bool ring = glissRingParam != nullptr && glissRingParam->load() >= 0.5f;

    if (! ring)
    {
        if (lastGlissNote >= 0 && lastGlissNote != note)
            output.addEvent (juce::MidiMessage::noteOff (1, lastGlissNote), samplePosition);
        if (lastGlissNote != note)
            output.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (vel)), samplePosition);
        lastGlissNote = note;
    }
    else
    {
        if (glissRingNotes.size() >= static_cast<size_t> (kMaxRingingGliss))
        {
            output.addEvent (juce::MidiMessage::noteOff (1, glissRingNotes.front()), samplePosition);
            glissRingNotes.erase (glissRingNotes.begin());
        }
        output.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (vel)), samplePosition);
        glissRingNotes.push_back (note);
        lastGlissNote = note;
    }

    lastGlissNoteUi.store (note);
    glissActiveCountUi.store (ring ? static_cast<int> (glissRingNotes.size()) : 1);
    return true;
}

bool OrchHarpAudioProcessor::tryStartTriggerRun (int noteNumber, juce::uint8 velocity,
                                                 double eventPpq, double /*ppqPerSample*/)
{
    const auto readNum = [] (std::atomic<float>* p, int fallback)
    {
        return p != nullptr ? juce::roundToInt (p->load()) : fallback;
    };

    const int trigLoRaw = juce::jlimit (0, 127, readNum (glissTrigLoParam, 0));
    const int trigHiRaw = juce::jlimit (0, 127, readNum (glissTrigHiParam, 0));
    const int trigLo = juce::jmin (trigLoRaw, trigHiRaw);
    const int trigHi = juce::jmax (trigLoRaw, trigHiRaw);

    if (trigHi <= 0 || noteNumber < trigLo || noteNumber > trigHi)
        return false; // 0/0 = trigger engine off

    // The run lives inside the run window (Gliss Run Low/High Note -> string
    // indices). The trigger note's pitch is irrelevant - it just fires.
    const int loNote = juce::jlimit (0, 127, readNum (glissRunLoNoteParam, 24));
    const int hiNote = juce::jlimit (0, 127, readNum (glissRunHiNoteParam, 103));
    const int a = ohrp::noteToNearestStringIndex (loNote, soundingDiagram, kGlissBaseOctave);
    const int b = ohrp::noteToNearestStringIndex (hiNote, soundingDiagram, kGlissBaseOctave);
    const int winLo = juce::jmin (a, b);
    const int winHi = juce::jmax (a, b);
    const int windowLen = winHi - winLo;
    if (windowLen < 1)
        return true;

    // Velocity scales how far into the window the run reaches from its start
    // end (soft note = a short run, full velocity = the whole window).
    const double reach = 0.25 + 0.75 * velocity / 127.0;
    const int span = juce::jmax (1, juce::roundToInt (windowLen * reach));

    const int dir = juce::jlimit (0, 3, readNum (glissRunDirectionParam, 0));

    std::vector<int> indices;
    auto ramp = [&indices, winLo, winHi] (int from, int to)
    {
        const int step = to >= from ? 1 : -1;
        for (int s = from; s != to + step; s += step)
        {
            const int clamped = juce::jlimit (winLo, winHi, s);
            if (indices.empty() || indices.back() != clamped)
                indices.push_back (clamped);
        }
    };

    switch (dir)
    {
        case 1:  ramp (winHi, winHi - span); break;                                        // Down: from the top
        case 2:  ramp (winLo, winLo + span); ramp (winLo + span - 1, winLo); break;        // Up-Down
        case 3:  ramp (winHi, winHi - span); ramp (winHi - span + 1, winHi); break;        // Down-Up
        default: ramp (winLo, winLo + span); break;                                        // Up: from the bottom
    }

    if (indices.size() < 2)
        return true;

    const int durIdx = juce::jlimit (0, 4, readNum (glissRunDurationParam, 2));
    const double durBeats = kRunDurationBeats[static_cast<size_t> (durIdx)];
    const int n = static_cast<int> (indices.size());

    for (int k = 0; k < n; ++k)
    {
        const double frac = n > 1 ? static_cast<double> (k) / (n - 1) : 0.0;
        pendingGliss.push_back ({ eventPpq + frac * durBeats, indices[static_cast<size_t> (k)], velocity });
    }
    // Terminal release marker for the Monophonic run.
    pendingGliss.push_back ({ eventPpq + durBeats + durBeats / juce::jmax (1, n) * 0.5, -1, 0 });

    std::sort (pendingGliss.begin(), pendingGliss.end(),
               [] (const PendingGlissEvent& a, const PendingGlissEvent& b) { return a.ppq < b.ppq; });
    return true;
}

void OrchHarpAudioProcessor::drainPendingGliss (juce::MidiBuffer& output, double blockStartPpq,
                                                double blockEndPpq, double ppqPerSample, int numSamples)
{
    if (pendingGliss.empty())
        return;

    const bool ring = glissRingParam != nullptr && glissRingParam->load() >= 0.5f;

    auto samplePosFor = [&] (double ppq)
    {
        if (ppqPerSample <= 0.0)
            return 0;
        return juce::jlimit (0, juce::jmax (0, numSamples - 1),
                             juce::roundToInt ((ppq - blockStartPpq) / ppqPerSample));
    };

    size_t drained = 0;
    for (auto& ev : pendingGliss)
    {
        if (ev.ppq >= blockEndPpq)
            break;
        ++drained;

        const int samplePos = samplePosFor (ev.ppq);

        if (ev.velocity == 0)
        {
            // Terminal release marker (pushed as {ppq, -1, 0}).
            if (! ring && runLastNote >= 0)
            {
                output.addEvent (juce::MidiMessage::noteOff (1, runLastNote), samplePos);
                runLastNote = -1;
            }
            continue;
        }

        const int note = ohrp::stringIndexToNote (ev.stringIndex, soundingDiagram, kGlissBaseOctave);
        glissIdleBeats = 0.0; // a run note just fired

        if (! ring)
        {
            if (runLastNote >= 0 && runLastNote != note)
                output.addEvent (juce::MidiMessage::noteOff (1, runLastNote), samplePos);
            output.addEvent (juce::MidiMessage::noteOn (1, note, ev.velocity), samplePos);
            runLastNote = note;
        }
        else
        {
            if (glissRingNotes.size() >= static_cast<size_t> (kMaxRingingGliss))
            {
                output.addEvent (juce::MidiMessage::noteOff (1, glissRingNotes.front()), samplePos);
                glissRingNotes.erase (glissRingNotes.begin());
            }
            output.addEvent (juce::MidiMessage::noteOn (1, note, ev.velocity), samplePos);
            glissRingNotes.push_back (note);
        }

        lastGlissNoteUi.store (note);
    }

    pendingGliss.erase (pendingGliss.begin(), pendingGliss.begin() + static_cast<std::ptrdiff_t> (drained));
    glissActiveCountUi.store (ring ? static_cast<int> (glissRingNotes.size())
                                   : (runLastNote >= 0 ? 1 : 0));
}

void OrchHarpAudioProcessor::handleNoteOn (const juce::MidiMessage& message, int samplePosition,
                                           juce::MidiBuffer& output, double blockPpq, double ppqPerSample)
{
    const int channel = juce::jlimit (1, 16, message.getChannel());
    const int inputNote = juce::jlimit (0, 127, message.getNoteNumber());
    const auto velocity = message.getVelocity();

    const auto track = [&] (int outNote)
    {
        if (activeNotes.size() >= 2048)
            activeNotes.erase (activeNotes.begin());
        activeNotes.push_back ({ channel, inputNote, outNote });
    };

    const auto emit = [&] (int outNote, int letter, int action)
    {
        track (outNote);
        if (outNote == inputNote)
            output.addEvent (message, samplePosition);
        else
            output.addEvent (juce::MidiMessage::noteOn (channel, outNote, velocity), samplePosition);

        if (letter >= 0 && letter < 7)
            stringQuietBeats[static_cast<size_t> (letter)] = 0.0;

        lastInputNote.store (inputNote);
        lastOutputNote.store (outNote);
        lastOutputLetter.store (letter);
        lastAction.store (action);
    };

    const auto drop = [&] (int action)
    {
        track (-1);
        lastInputNote.store (inputNote);
        lastOutputNote.store (-1);
        lastOutputLetter.store (-1);
        lastAction.store (action);
    };

    // Trigger-gesture glissando: a note in the trigger zone is consumed and
    // sprays a scheduled run (design §9). Checked before white/black routing.
    {
        const double eventPpq = blockPpq + samplePosition * ppqPerSample;
        if (tryStartTriggerRun (inputNote, velocity, eventPpq, ppqPerSample))
        {
            track (-1); // swallow this note-on and its matching note-off
            lastInputNote.store (inputNote);
            lastOutputNote.store (-1);
            lastOutputLetter.store (-1);
            lastAction.store (5); // trigger run
            return;
        }
    }

    const auto blackMode = static_cast<ohrp::BlackKeyMode> (
        blackKeyModeParam != nullptr ? juce::jlimit (0, 3, juce::roundToInt (blackKeyModeParam->load())) : 0);

    // White key -> its string, same octave.
    if (ohrp::isWhiteKey (inputNote))
    {
        const int letter = ohrp::letterForWhiteKey (inputNote);
        const int out = ohrp::whiteKeyToStringNote (inputNote, soundingDiagram);

        // Same-string collision: a live note already sounds this pitch on this
        // channel -> drop the later one (a section harpist has one string).
        for (const auto& n : activeNotes)
            if (n.channel == channel && n.outputNote == out)
            {
                drop (3);
                return;
            }

        emit (out, letter, 1);
        return;
    }

    // Black key.
    switch (blackMode)
    {
        case ohrp::BlackKeyMode::Control:
        {
            if (tryConsumeControlNote (inputNote))
            {
                drop (4); // consumed by the control zone (bank recall / step)
                return;
            }

            // Out-of-zone black key: the music's own accidental re-pedals the
            // harp so its pitch class becomes playable, then sounds on that
            // string. Subsequent naturals on that letter stay re-tuned until
            // another change (the emergent Synchron-Harp behaviour, design §4).
            const int p = ohrp::mod12 (inputNote);
            const int letter = ohrp::nearestStringIndex (p, soundingDiagram);
            const int base = ohrp::kLetterBaseSemitone[static_cast<size_t> (letter)];
            int delta = ((p - base) % 12 + 12) % 12;
            if (delta > 6) delta -= 12;
            const int wantOffset = juce::jlimit (-1, 1, delta);

            if (soundingDiagram[static_cast<size_t> (letter)] != wantOffset)
            {
                soundingDiagram[static_cast<size_t> (letter)] = wantOffset;
                auto* pp = pedalChoice[static_cast<size_t> (letter)];
                const int want = choiceIndexFromOffset (wantOffset);
                if (pp != nullptr && pp->getIndex() != want)
                    *pp = want;
            }

            const int out = juce::jlimit (0, 127,
                12 * (inputNote / 12) + ohrp::stringSemitone (letter, soundingDiagram));
            emit (out, letter, 1);
            return;
        }

        case ohrp::BlackKeyMode::Drop:
            drop (3);
            return;

        case ohrp::BlackKeyMode::Nudge:
        {
            // Bend the nearest pedal one notch toward the note, rate-limited by
            // the governor interval, then sound it on the nearest string.
            const int letter = ohrp::nearestStringIndex (ohrp::mod12 (inputNote), soundingDiagram);
            const int wantPc = ohrp::mod12 (inputNote);
            const int havePc = ohrp::stringPitchClass (letter, soundingDiagram);
            if (wantPc != havePc && beatsSinceNudge >= minChangeIntervalInBeats())
            {
                auto* p = pedalChoice[static_cast<size_t> (letter)];
                if (p != nullptr)
                {
                    const int base = ohrp::kLetterBaseSemitone[static_cast<size_t> (letter)];
                    const int cur = ohrp::mod12 (base);
                    const int up = ohrp::mod12 (wantPc - cur + 6) - 6; // signed shortest direction
                    const int deltaOffset = up > 0 ? 1 : -1;
                    const int newIndex = juce::jlimit (0, 2, p->getIndex() + deltaOffset);
                    if (newIndex != p->getIndex())
                        *p = newIndex;
                }
                beatsSinceNudge = 0.0;
            }
            const int out = ohrp::nearestStringNote (inputNote, soundingDiagram);
            const int outLetter = ohrp::nearestStringIndex (ohrp::mod12 (inputNote), soundingDiagram);
            emit (out, outLetter, 2);
            return;
        }

        case ohrp::BlackKeyMode::Nearest:
        default:
        {
            const int out = ohrp::nearestStringNote (inputNote, soundingDiagram);
            const int letter = ohrp::nearestStringIndex (ohrp::mod12 (inputNote), soundingDiagram);
            for (const auto& n : activeNotes)
                if (n.channel == channel && n.outputNote == out)
                {
                    drop (3);
                    return;
                }
            emit (out, letter, 2);
            return;
        }
    }
}

void OrchHarpAudioProcessor::handleNoteOff (const juce::MidiMessage& message, int samplePosition, juce::MidiBuffer& output)
{
    const int channel = juce::jlimit (1, 16, message.getChannel());
    const int inputNote = juce::jlimit (0, 127, message.getNoteNumber());

    const auto it = std::find_if (activeNotes.begin(), activeNotes.end(),
        [&] (const TrackedNote& n) { return n.channel == channel && n.inputNote == inputNote; });

    if (it == activeNotes.end())
    {
        output.addEvent (message, samplePosition); // untracked - pass through
        return;
    }

    const int outputNote = it->outputNote;
    activeNotes.erase (it);

    if (outputNote < 0)
        return; // matching note-on was dropped / consumed

    if (outputNote == inputNote)
        output.addEvent (message, samplePosition);
    else
        output.addEvent (juce::MidiMessage::noteOff (channel, outputNote, message.getVelocity()), samplePosition);
}

// ---- processBlock ----------------------------------------------------

void OrchHarpAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    bool playing = false;
    bool havePpq = false;
    double bpm = 120.0;
    double blockStartPpq = integratedPpq;

    if (auto* transport = getPlayHead())
    {
        if (const auto pos = transport->getPosition())
        {
            playing = pos->getIsPlaying();
            if (const auto hostBpm = pos->getBpm(); hostBpm && *hostBpm > 0.0)
                bpm = *hostBpm;
            if (const auto ppq = pos->getPpqPosition())
            {
                blockStartPpq = *ppq;
                havePpq = true;
            }
        }
    }

    const double ppqPerSample = sampleRate > 0.0 ? (bpm / 60.0) / sampleRate : 0.0;
    integratedPpq = blockStartPpq + numSamples * ppqPerSample;

    const double blockEndPpq = blockStartPpq + numSamples * ppqPerSample;
    const double blockBeats = numSamples * ppqPerSample;
    glissIdleBeats = juce::jmin (glissIdleBeats + blockBeats, 1.0e6);

    // Chromatic = total bypass: every message passes through untouched.
    const bool chromatic = modeParam != nullptr && modeParam->load() >= 0.5f;
    if (chromatic)
    {
        // Release any ringing gliss notes into the untouched stream before we
        // bow out, so nothing hangs.
        flushGlissNotes (midiMessages, 0);
        if (! activeNotes.empty())
            resetNoteMap();
        wasPlaying = playing;
        storeReadoutDiagrams (readRequestedDiagram());
        return;
    }

    // Bank-slot recall: writing the slot's 7 offsets onto the pedal params.
    const int slot = bankSlotParam != nullptr
        ? juce::jlimit (0, kNumBankSlots - 1, juce::roundToInt (bankSlotParam->load())) : 0;
    if (slot != lastAppliedBankSlot)
    {
        if (const juce::SpinLock::ScopedTryLockType tryLock (bankLock); tryLock.isLocked())
        {
            applyDiagramToParams (bank[static_cast<size_t> (slot)].offsets);
            lastAppliedBankSlot = slot;
        }
    }

    const ohrp::Diagram requested = readRequestedDiagram();

    // Pedals are set during the rest before playing: snap on the transport edge.
    juce::MidiBuffer output;

    if (! playing)
    {
        if (wasPlaying)
            flushGlissNotes (output, 0); // transport stop: damp everything
        snapSoundingToRequested (requested);
    }
    else if (! wasPlaying)
    {
        snapSoundingToRequested (requested);
    }
    else
    {
        runGovernor (requested, blockBeats);
    }

    wasPlaying = playing;

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        const int samplePosition = metadata.samplePosition;

        if (message.isController())
        {
            const int ccChannel = ccChannelParam != nullptr
                ? juce::jlimit (0, 16, juce::roundToInt (ccChannelParam->load())) : 0;
            const bool channelOk = ccChannel == 0 || message.getChannel() == ccChannel;

            if (channelOk && handleGlissCc (message, samplePosition, output))
                continue; // gliss CCs are consumed, not passed downstream

            if (channelOk)
                handleControlCc (message);

            output.addEvent (message, samplePosition); // control CCs pass through
            continue;
        }

        if (message.isNoteOn())
        {
            handleNoteOn (message, samplePosition, output, blockStartPpq, ppqPerSample);
            continue;
        }

        if (message.isNoteOff())
        {
            handleNoteOff (message, samplePosition, output);
            continue;
        }

        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            flushGlissNotes (output, samplePosition);
            resetNoteMap();
            output.addEvent (message, samplePosition);
            continue;
        }

        output.addEvent (message, samplePosition);
    }

    // Emit any scheduled trigger-run notes that fall in this block.
    drainPendingGliss (output, blockStartPpq, blockEndPpq, ppqPerSample, numSamples);

    // Release a held contour note once the CC has been still long enough
    // (so the last note of a run gets a real duration for notation).
    releaseIdleGlissNotes (output, juce::jmax (0, numSamples - 1));

    midiMessages.swapWith (output);

    storeReadoutDiagrams (requested);
}

// ---- Boilerplate ----------------------------------------------------

juce::AudioProcessorEditor* OrchHarpAudioProcessor::createEditor()
{
    return new OrchHarpAudioProcessorEditor (*this);
}

bool OrchHarpAudioProcessor::hasEditor() const { return true; }
const juce::String OrchHarpAudioProcessor::getName() const { return JucePlugin_Name; }
bool OrchHarpAudioProcessor::acceptsMidi() const { return true; }
bool OrchHarpAudioProcessor::producesMidi() const { return true; }
bool OrchHarpAudioProcessor::isMidiEffect() const { return true; }
double OrchHarpAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int OrchHarpAudioProcessor::getNumPrograms() { return 1; }
int OrchHarpAudioProcessor::getCurrentProgram() { return 0; }
void OrchHarpAudioProcessor::setCurrentProgram (int) {}
const juce::String OrchHarpAudioProcessor::getProgramName (int) { return {}; }
void OrchHarpAudioProcessor::changeProgramName (int, const juce::String&) {}

void OrchHarpAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.removeChild (state.getChildWithName ("bank"), nullptr);
    state.appendChild (bankToTree(), nullptr);

    if (auto xml = std::unique_ptr<juce::XmlElement> (state.createXml()))
        copyXmlToBinary (*xml, destData);
}

void OrchHarpAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = std::unique_ptr<juce::XmlElement> (getXmlFromBinary (data, sizeInBytes)))
    {
        if (xml->hasTagName (parameters.state.getType()))
        {
            const auto tree = juce::ValueTree::fromXml (*xml);
            const auto bankChild = tree.getChildWithName ("bank");

            parameters.replaceState (tree);

            if (bankChild.isValid())
                bankFromTree (bankChild);
        }
    }

    resetNoteMap();
    lastAppliedBankSlot = -1;
    soundingDiagram = readRequestedDiagram();
    moveInfo = {};
    storeReadoutDiagrams (soundingDiagram);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OrchHarpAudioProcessor();
}
