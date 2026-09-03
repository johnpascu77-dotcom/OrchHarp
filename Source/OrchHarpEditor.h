#pragma once

#include <array>
#include <JuceHeader.h>
#include "OrchHarpProcessor.h"

// Live readout of the 7 pedals plus direct editing: requested diagram as an
// outline, sounding diagram as a solid dash, pedals still in transit amber.
// Click a pedal column at flat / natural / sharp height to set it.
class PedalDiagramComponent final : public juce::Component
{
public:
    PedalDiagramComponent() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void setDiagrams (const ohrp::Diagram& sounding, const ohrp::Diagram& requested);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    // Called with letter 0..6 (C..B) and offset -1 / 0 / +1 when a pedal is
    // clicked. The editor turns it into a parameter write.
    std::function<void (int letter, int offset)> onPedalEdit;

private:
    ohrp::Diagram soundingDiagram { ohrp::kAllNatural };
    ohrp::Diagram requestedDiagram { ohrp::kAllNatural };
};

// One bank cell: left-click recalls, shift-click saves, right-click menu.
class BankCellComponent final : public juce::Component
{
public:
    std::function<void()> onRecall;
    std::function<void()> onSave;
    std::function<void()> onRename;
    std::function<void()> onRecolour;

    void setContent (const juce::String& name, juce::Colour colour, bool active);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String slotName;
    juce::Colour slotColour { juce::Colours::grey };
    bool isActive = false;
};

// A plain panel whose layout is delegated back to the editor via a lambda, so
// the editor can keep all its controls as members and just split resized().
class LayoutPanel final : public juce::Component
{
public:
    std::function<void()> onLayout;
    void resized() override { if (onLayout) onLayout(); }
};

class OrchHarpAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit OrchHarpAudioProcessorEditor (OrchHarpAudioProcessor&);
    ~OrchHarpAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshBankCells();
    void rebuildVariantBox();
    void updateStatus();
    void layoutHarpTab();
    void layoutMotionTab();
    void layoutVoicingTab();

    OrchHarpAudioProcessor& audioProcessor;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    LayoutPanel harpPanel, motionPanel, voicingPanel;

    juce::Label titleLabel, subtitleLabel, buildLabel;

    juce::Label modeLabel, blackKeyModeLabel;
    juce::ComboBox modeBox, blackKeyModeBox;

    PedalDiagramComponent pedalDiagram;

    juce::Label pedalsLabel;
    std::array<juce::Label, 7> pedalLetterLabels;
    std::array<juce::ComboBox, 7> pedalBoxes;

    juce::Label bankLabel;
    std::array<BankCellComponent, OrchHarpAudioProcessor::kNumBankSlots> bankCells;

    juce::Label helperLabel;
    juce::ComboBox familyBox, variantBox, baseKeyBox;
    juce::Slider helperSlotSlider;
    juce::TextButton helperWriteButton { "Write to slot" };
    juce::Label pcSetLabel;
    juce::TextEditor pcSetEditor;
    juce::TextButton pcSetWriteButton { "Write PC set" };

    juce::Label governorLabel;
    juce::ToggleButton playabilityButton;
    juce::ComboBox minChangeIntervalBox;
    juce::ToggleButton changesAtRestsOnlyButton, avoidRingingButton;

    juce::Label triggersLabel;
    juce::Label ccBankLabel, ccChannelLabel, ctrlDirectLabel, ctrlStepLabel;
    juce::Slider ccBankSlider, ccChannelSlider;
    juce::Slider ctrlDirectLoSlider, ctrlDirectHiSlider, ctrlStepDownSlider, ctrlStepUpSlider;

    juce::Label contourGlissLabel;
    juce::Label glissCcLabel, glissNoteRangeLabel, glissVelLabel, glissReleaseLabel;
    juce::Slider glissCcSlider, glissLoNoteSlider, glissHiNoteSlider;
    juce::Slider glissVelCcSlider, glissVelocitySlider;
    juce::ComboBox glissRingBox, glissReleaseBox;

    juce::Label triggerGlissLabel;
    juce::Label glissTrigZoneLabel, glissRunWindowLabel, glissRunDirLabel;
    juce::Slider glissTrigLoSlider, glissTrigHiSlider, glissRunLoNoteSlider, glissRunHiNoteSlider;
    juce::ComboBox glissRunDirectionBox, glissRunDurationBox;

    juce::Label bisbLabel, bisbZoneLabel;
    juce::Slider bisbLoNoteSlider, bisbHiNoteSlider;
    juce::ComboBox bisbRateBox;
    juce::ToggleButton bisbEnharmonicButton;

    // ---- Contour (Harp tab) ----
    juce::Label contourLabel, contourStepLabel, contourWinLabel;
    juce::ComboBox pitchModeBox, contourStepBox, contourChordsBox;
    juce::Slider contourLoNoteSlider, contourHiNoteSlider;

    // ---- Voicing tab ----
    juce::Label voicingHandLabel, voicingSplitLabel, voicingCapLabel, voicingRangeLabel,
                voicingCenterLabel, voicingSpanLabel, voicingProtectLabel;
    juce::ToggleButton voicingEnableButton, dampSuccessiveButton;
    juce::TextButton leftHandPresetButton { "Left hand" }, rightHandPresetButton { "Right hand" };
    juce::ComboBox handBox, splitModeBox, rangeModeBox, outOfRangeBox, overSpanBox, rollRateBox, protectBox;
    juce::Slider splitChanLeftSlider, splitChanRightSlider, splitNoteSlider, maxVoicesSlider, onsetWindowSlider,
                 handLoNoteSlider, handHiNoteSlider, handCenterSlider, handSpanSlider, maxSpanSlider, outChannelSlider;

    juce::Label statusLabel;

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ComboBoxAttachment> modeAttachment, blackKeyModeAttachment, minChangeIntervalAttachment;
    std::array<std::unique_ptr<ComboBoxAttachment>, 7> pedalAttachments;
    std::unique_ptr<ButtonAttachment> playabilityAttachment, changesAtRestsOnlyAttachment, avoidRingingAttachment;
    std::unique_ptr<SliderAttachment> ccBankAttachment, ccChannelAttachment;
    std::unique_ptr<SliderAttachment> ctrlDirectLoAttachment, ctrlDirectHiAttachment, ctrlStepDownAttachment, ctrlStepUpAttachment;
    std::unique_ptr<SliderAttachment> glissCcAttachment, glissLoNoteAttachment, glissHiNoteAttachment;
    std::unique_ptr<SliderAttachment> glissVelCcAttachment, glissVelocityAttachment;
    std::unique_ptr<SliderAttachment> glissTrigLoAttachment, glissTrigHiAttachment, glissRunLoNoteAttachment, glissRunHiNoteAttachment;
    std::unique_ptr<ComboBoxAttachment> glissRingAttachment, glissReleaseAttachment, glissRunDirectionAttachment, glissRunDurationAttachment;
    std::unique_ptr<SliderAttachment> bisbLoNoteAttachment, bisbHiNoteAttachment;
    std::unique_ptr<ComboBoxAttachment> bisbRateAttachment;
    std::unique_ptr<ButtonAttachment> bisbEnharmonicAttachment;

    std::unique_ptr<ComboBoxAttachment> pitchModeAttachment, contourStepAttachment, contourChordsAttachment;
    std::unique_ptr<SliderAttachment> contourLoNoteAttachment, contourHiNoteAttachment;

    std::unique_ptr<ButtonAttachment> voicingEnableAttachment, dampSuccessiveAttachment;
    std::unique_ptr<ComboBoxAttachment> handAttachment, splitModeAttachment, rangeModeAttachment, outOfRangeAttachment,
                                        overSpanAttachment, rollRateAttachment, protectAttachment;
    std::unique_ptr<SliderAttachment> splitChanLeftAttachment, splitChanRightAttachment, splitNoteAttachment, maxVoicesAttachment,
                                      onsetWindowAttachment, handLoNoteAttachment, handHiNoteAttachment,
                                      handCenterAttachment, handSpanAttachment,
                                      maxSpanAttachment, outChannelAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchHarpAudioProcessorEditor)
};
