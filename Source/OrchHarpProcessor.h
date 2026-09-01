#pragma once

#include <array>
#include <atomic>
#include <limits>
#include <vector>
#include <JuceHeader.h>

#include "OrchHarpPedalLogic.h"

class OrchHarpAudioProcessor final : public juce::AudioProcessor
{
public:
    OrchHarpAudioProcessor();
    ~OrchHarpAudioProcessor() override = default;

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
    void writeFamilyToSlot (ohrp::Family family, int variant, int baseKey, int slotIndex);
    void resetBankToFactory();

    // ---- UI status readouts (message thread; plain reads) -------------------
    int  getLastInputNoteForUi() const  { return lastInputNote.load(); }
    int  getLastOutputNoteForUi() const { return lastOutputNote.load(); }
    int  getLastOutputLetterForUi() const { return lastOutputLetter.load(); } // 0..6, -1 none
    int  getLastActionForUi() const     { return lastAction.load(); } // 0 none 1 string 2 nearest 3 dropped 4 control
    int  getLastCcForUi() const         { return lastCc.load(); }
    int  getMovesInTransitForUi() const { return movesInTransit.load(); }
    int  getLastGlissNoteForUi() const  { return lastGlissNoteUi.load(); }
    int  getGlissActiveCountForUi() const { return glissActiveCountUi.load(); }
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
    std::atomic<float>* glissLoStringParam = nullptr;
    std::atomic<float>* glissHiStringParam = nullptr;
    std::atomic<float>* glissBaseOctaveParam = nullptr;
    std::atomic<float>* glissVelCcParam = nullptr;
    std::atomic<float>* glissVelocityParam = nullptr;
    std::atomic<float>* glissRingParam = nullptr;
    std::atomic<float>* glissTrigLoParam = nullptr;
    std::atomic<float>* glissTrigHiParam = nullptr;
    std::atomic<float>* glissRunDirectionParam = nullptr;
    std::atomic<float>* glissRunSpanParam = nullptr;
    std::atomic<float>* glissRunDurationParam = nullptr;

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
        int channel = 0;      // 1..16
        int inputNote = 0;    // 0..127
        int outputNote = -1;  // emitted pitch, or -1 if dropped / consumed
    };
    std::vector<TrackedNote> activeNotes;

    // ---- Governor state ----
    ohrp::Diagram soundingDiagram { ohrp::kAllNatural };
    ohrp::MoveInfo moveInfo;
    double lastMovePpq = -1.0e12;
    double lastNudgePpq = -1.0e12;
    std::array<double, 7> lastStringSoundPpq { };
    int lastAppliedBankSlot = -1;

    double sampleRate = 44100.0;
    double integratedPpq = 0.0;
    bool wasPlaying = false;

    // ---- Glissando engine ----
    int lastGlissString = std::numeric_limits<int>::min();
    int lastGlissNote = -1;              // Monophonic contour: the ringing gliss note
    int glissVelValue = 96;              // latched from glissVelCc
    std::vector<int> glissRingNotes;     // Ring mode: every ringing gliss note

    struct PendingGlissEvent
    {
        double ppq = 0.0;
        int stringIndex = 0;
        juce::uint8 velocity = 96;
    };
    std::vector<PendingGlissEvent> pendingGliss; // sorted by ppq
    int runLastNote = -1;                // Monophonic trigger-run: last emitted note
    double runOffPpq = -1.0e12;          // when to release runLastNote

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
    int  glissBaseOctave() const;

    void resetNoteMap();
    void snapSoundingToRequested (const ohrp::Diagram& requested);
    void runGovernor (const ohrp::Diagram& requested, double blockPpq);

    void handleControlCc (const juce::MidiMessage& message);
    bool tryConsumeControlNote (int noteNumber); // true if consumed as a control-zone note
    // Contour-follower CC -> gliss notes. Returns true if the CC was consumed.
    bool handleGlissCc (const juce::MidiMessage& message, int samplePosition, juce::MidiBuffer& output);
    bool tryStartTriggerRun (int noteNumber, juce::uint8 velocity, double eventPpq, double ppqPerSample);
    void drainPendingGliss (juce::MidiBuffer& output, double blockStartPpq, double blockEndPpq,
                            double ppqPerSample, int numSamples);
    void flushGlissNotes (juce::MidiBuffer& output, int samplePosition);
    void handleNoteOn (const juce::MidiMessage& message, int samplePosition, juce::MidiBuffer& output,
                       double blockPpq, double ppqPerSample);
    void handleNoteOff (const juce::MidiMessage& message, int samplePosition, juce::MidiBuffer& output);

    void storeReadoutDiagrams (const ohrp::Diagram& requested);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchHarpAudioProcessor)
};
