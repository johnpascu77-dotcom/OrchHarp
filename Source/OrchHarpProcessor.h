#pragma once

#include <array>
#include <atomic>
#include <limits>
#include <memory>
#include <utility>
#include <vector>
#include <JuceHeader.h>

#include "OrchHarpPedalLogic.h"

class OrchHarpAudioProcessor final : public juce::AudioProcessor
{
public:
    OrchHarpAudioProcessor();
    ~OrchHarpAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ---- Bank (preset store, message thread) --------------------------------
    static constexpr int kNumBankSlots = 12;

    struct BankSlot
    {
        ohrp::Diagram offsets { ohrp::kAllNatural };
        juce::String name;
        juce::uint32 colour = 0xff5fc8f5;
    };

    BankSlot getBankSlot (int index) const;
    void recallBankSlot (int index);                 // sets bankSlot param -> pedals
    void saveCurrentDiagramToSlot (int index);       // requested diagram -> slot
    void renameBankSlot (int index, const juce::String& name);
    void recolourBankSlot (int index, juce::Colour colour);
    void setBankSlot (int index, const ohrp::Diagram& offsets, const juce::String& name);
    void resetBankToFactory();

    // One-click concert-harp hand envelope: sets hand / range / protect /
    // out-of-range for the Voicing tab (splitMode is left to the user).
    void applyHandPreset (bool left);

    // ---- UI status readouts (message thread; plain reads) -------------------
    int  getLastInputNoteForUi() const  { return lastInputNote.load(); }
    int  getLastOutputNoteForUi() const { return lastOutputNote.load(); }
    int  getLastOutputLetterForUi() const { return lastOutputLetter.load(); } // 0..6, -1 none
    int  getLastActionForUi() const     { return lastAction.load(); } // 0 none 1 string 2 nearest 3 dropped 4 control
    int  getLastCcForUi() const         { return lastCc.load(); }
    int  getMovesInTransitForUi() const { return movesInTransit.load(); }
    int  getLastGlissNoteForUi() const  { return lastGlissNoteUi.load(); }
    int  getGlissActiveCountForUi() const { return glissActiveCountUi.load(); }
    int  getVoicedKeptForUi() const  { return lastVoicedKept; }
    int  getVoicedSeenForUi() const  { return lastVoicedSeen; }
    ohrp::Diagram getSoundingDiagramForUi() const;
    ohrp::Diagram getRequestedDiagramForUi() const;

    static juce::StringArray pedalChoiceLabels();   // Flat / Natural / Sharp

private:
    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* modeParam = nullptr;
    std::array<std::atomic<float>*, 7> pedalParam { };
    std::atomic<float>* bankSlotParam = nullptr;
    std::atomic<float>* blackKeyModeParam = nullptr;
    std::atomic<float>* playabilityParam = nullptr;
    std::atomic<float>* minChangeIntervalParam = nullptr;
    std::atomic<float>* changesAtRestsOnlyParam = nullptr;
    std::atomic<float>* avoidRingingParam = nullptr;
    std::atomic<float>* ccBankSelectParam = nullptr;
    std::atomic<float>* ccChannelParam = nullptr;
    std::atomic<float>* ctrlDirectLoParam = nullptr;
    std::atomic<float>* ctrlDirectHiParam = nullptr;
    std::atomic<float>* ctrlStepDownParam = nullptr;
    std::atomic<float>* ctrlStepUpParam = nullptr;

    std::atomic<float>* glissCcParam = nullptr;
    std::atomic<float>* glissLoNoteParam = nullptr;
    std::atomic<float>* glissHiNoteParam = nullptr;
    std::atomic<float>* glissVelCcParam = nullptr;
    std::atomic<float>* glissVelocityParam = nullptr;
    std::atomic<float>* glissRingParam = nullptr;
    std::atomic<float>* glissReleaseParam = nullptr;
    std::atomic<float>* glissTrigLoParam = nullptr;
    std::atomic<float>* glissTrigHiParam = nullptr;
    std::atomic<float>* glissRunLoNoteParam = nullptr;
    std::atomic<float>* glissRunHiNoteParam = nullptr;
    std::atomic<float>* glissRunDirectionParam = nullptr;
    std::atomic<float>* glissRunDurationParam = nullptr;
    std::atomic<float>* bisbLoNoteParam = nullptr;
    std::atomic<float>* bisbHiNoteParam = nullptr;
    std::atomic<float>* bisbRateParam = nullptr;
    std::atomic<float>* bisbEnharmonicParam = nullptr;

    std::atomic<float>* pitchModeParam = nullptr;
    std::atomic<float>* contourStepParam = nullptr;
    std::atomic<float>* contourChordsParam = nullptr;
    std::atomic<float>* contourLoNoteParam = nullptr;
    std::atomic<float>* contourHiNoteParam = nullptr;

    std::atomic<float>* voicingEnableParam = nullptr;
    std::atomic<float>* handParam = nullptr;
    std::atomic<float>* splitModeParam = nullptr;
    std::atomic<float>* splitChanLeftParam = nullptr;
    std::atomic<float>* splitChanRightParam = nullptr;
    std::atomic<float>* splitNoteParam = nullptr;
    std::atomic<float>* maxVoicesParam = nullptr;
    std::atomic<float>* onsetWindowMsParam = nullptr;
    std::atomic<float>* rangeModeParam = nullptr;
    std::atomic<float>* handLoNoteParam = nullptr;
    std::atomic<float>* handHiNoteParam = nullptr;
    std::atomic<float>* handCenterParam = nullptr;
    std::atomic<float>* handSpanParam = nullptr;
    std::atomic<float>* outOfRangeParam = nullptr;
    std::atomic<float>* maxSpanParam = nullptr;
    std::atomic<float>* overSpanParam = nullptr;
    std::atomic<float>* rollRateParam = nullptr;
    std::atomic<float>* protectParam = nullptr;
    std::atomic<float>* outChannelParam = nullptr;
    std::atomic<float>* dampSuccessiveParam = nullptr;

    // Typed handles for guarded write-back from CC / bank recall / nudge.
    std::array<juce::AudioParameterChoice*, 7> pedalChoice { };
    juce::AudioParameterInt* bankSlotInt = nullptr;

    // ---- Bank ----
    std::array<BankSlot, kNumBankSlots> bank;
    mutable juce::SpinLock bankLock;

    juce::ValueTree bankToTree() const;
    void bankFromTree (const juce::ValueTree& tree);

    // ---- Note tracking (ONF pattern) ----
    struct TrackedNote
    {
        int channel = 0;       // input channel, 1..16 (note-off is matched on this)
        int inputNote = 0;     // 0..127
        int outputNote = -1;   // emitted pitch, or -1 if dropped / consumed
        int outputChannel = 0; // channel the note-on went out on (for the note-off)
    };
    std::vector<TrackedNote> activeNotes;

    // ---- Voicing (Phase 3) ----
    struct ResolvedNote
    {
        int channel = 1;
        int inputNote = 0;
        juce::uint8 velocity = 100;
        int outputNote = -1;   // -1 = drop
        int letter = -1;       // string letter 0..6, or -1
        int action = 0;        // 0 none 1 string 2 nearest 3 dropped 4 control 5 trigger 6 contour
        bool consumed = false; // swallow note-on AND note-off, emit nothing
        int samplePos = 0;
    };
    std::vector<ResolvedNote> currentGroup;   // open onset group (voicing on)
    int currentGroupStartSample = 0;
    std::atomic<int> lastVoicedKept { 0 };
    std::atomic<int> lastVoicedSeen { 0 };

    // ---- Contour mode state ----
    int lastContourInput = -1;
    int contourClusterTopIdx = 0;
    int contourStackDepth = 0;
    double contourClusterPpq = -1.0e12;

    // ---- Governor state ----
    // Timing is beat-accumulator based (not host-ppq compared) so a loop or a
    // transport jump can't wedge it.
    ohrp::Diagram soundingDiagram { ohrp::kAllNatural };
    ohrp::MoveInfo moveInfo;
    double beatsSinceMove = 1.0e6;
    double beatsSinceNudge = 1.0e6;
    std::array<double, 7> stringQuietBeats { };
    int lastAppliedBankSlot = -1;

    double sampleRate = 44100.0;
    double integratedPpq = 0.0;
    bool wasPlaying = false;
    double beatsPerBar = 4.0; // host time signature in quarter-note beats; latched at transport start

    // ---- Pedal-marker sidecar --------------------------------------------
    // On transport stop OrchHarp writes every requested-diagram change of the
    // take to a well-known temp file as "bar:label" lines; OrchCapture folds
    // them into its section markers so the pedal changes land in the Dorico
    // score. Written off the audio thread by a dedicated worker.
    struct MarkerWriter;
    std::unique_ptr<MarkerWriter> markerWriter;
    juce::String pedalMarkerTag;
    ohrp::Diagram lastLoggedRequested { -9, -9, -9, -9, -9, -9, -9 };
    std::vector<std::pair<double, juce::String>> pedalMarkerLog;
    juce::CriticalSection pedalMarkerLock;
    void writePedalMarkerFile();   // worker thread only

    // ---- Glissando engine ----
    static constexpr int kGlissBaseOctave = 2; // absolute string index 0 -> ~MIDI 24
    int glissEmitChannel = 1;            // latched per block: outChannel if set, else 1
    int lastGlissString = std::numeric_limits<int>::min();
    int lastGlissNote = -1;              // Monophonic contour: the ringing gliss note
    int glissVelValue = 96;              // latched from glissVelCc
    std::vector<int> glissRingNotes;     // Ring mode: every ringing gliss note
    double glissIdleBeats = 0.0;         // since the last gliss note; drives idle release

    struct PendingGlissEvent
    {
        double ppq = 0.0;
        int stringIndex = 0;
        juce::uint8 velocity = 96;
    };
    std::vector<PendingGlissEvent> pendingGliss; // sorted by ppq
    int runLastNote = -1;                // Monophonic trigger-run: last emitted note

    // ---- Bisbigliando ----
    // A note in the bisb zone is consumed and re-emitted as a measured tremolo
    // (a rustling repeat, optionally rocking to an enharmonic neighbour string)
    // until its note-off. A "Bisbigliando" marker is logged to the sidecar.
    struct BisbVoice
    {
        int channel = 1;
        int inputNote = 0;
        int noteA = -1;
        int noteB = -1;
        int cur = -1;           // the pitch currently sounding
        juce::uint8 velocity = 96;
        double nextPpq = 0.0;
        bool onB = false;
        bool sounding = false;
    };
    std::vector<BisbVoice> bisbVoices;

    // ---- Readouts ----
    std::atomic<int> lastInputNote { -1 };
    std::atomic<int> lastOutputNote { -1 };
    std::atomic<int> lastOutputLetter { -1 };
    std::atomic<int> lastAction { 0 };
    std::atomic<int> lastCc { -1 };
    std::atomic<int> movesInTransit { 0 };
    std::atomic<int> lastGlissNoteUi { -1 };
    std::atomic<int> glissActiveCountUi { 0 };
    std::array<std::atomic<int>, 7> soundingUi { };
    std::array<std::atomic<int>, 7> requestedUi { };

    ohrp::Diagram readRequestedDiagram() const;
    void applyDiagramToParams (const ohrp::Diagram& diagram);
    double minChangeIntervalInBeats() const;
    double glissReleaseInBeats() const;  // 0 = Hold (never idle-release)
    int  glissWindowLoIndex (bool trigger) const;
    int  glissWindowHiIndex (bool trigger) const;

    void resetNoteMap();
    void snapSoundingToRequested (const ohrp::Diagram& requested);
    void runGovernor (const ohrp::Diagram& requested, double blockBeats);

    void handleControlCc (const juce::MidiMessage& message);
    bool tryConsumeControlNote (int noteNumber); // true if consumed as a control-zone note
    // Contour-follower CC -> gliss notes. Returns true if the CC was consumed.
    bool handleGlissCc (const juce::MidiMessage& message, int samplePosition, juce::MidiBuffer& output);
    bool tryStartTriggerRun (int noteNumber, juce::uint8 velocity, double eventPpq, double ppqPerSample);
    void drainPendingGliss (juce::MidiBuffer& output, double blockStartPpq, double blockEndPpq,
                            double ppqPerSample, int numSamples);
    void releaseIdleGlissNotes (juce::MidiBuffer& output, int samplePosition);
    void flushGlissNotes (juce::MidiBuffer& output, int samplePosition);

    bool tryStartBisb (int noteNumber, int channel, juce::uint8 velocity, double eventPpq);
    bool stopBisb (int channel, int noteNumber, juce::MidiBuffer& output, int samplePosition);
    void drainBisb (juce::MidiBuffer& output, double blockStartPpq, double blockEndPpq,
                    double ppqPerSample, int numSamples);
    void flushBisb (juce::MidiBuffer& output, int samplePosition);
    double bisbRateInBeats() const;

    // The pitch transform for one note-on, with side effects (trigger scheduling,
    // re-pedal, nudge, contour state) but NO emission.
    ResolvedNote resolveNoteOn (const juce::MidiMessage& message, int samplePosition,
                                double blockPpq, double ppqPerSample);
    void emitResolved (const ResolvedNote& r, juce::MidiBuffer& output);
    void flushVoiceGroup (std::vector<ResolvedNote>& group, juce::MidiBuffer& output,
                          double ppqPerSample, int numSamples);
    void handleNoteOff (const juce::MidiMessage& message, int samplePosition, juce::MidiBuffer& output);

    int  contourWindowLo() const;
    int  contourWindowHi() const;

    void storeReadoutDiagrams (const ohrp::Diagram& requested);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchHarpAudioProcessor)
};
