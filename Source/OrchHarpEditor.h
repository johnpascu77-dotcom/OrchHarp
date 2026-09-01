#pragma once

#include <array>
#include <JuceHeader.h>
#include "OrchHarpProcessor.h"

// Live readout of the 7 pedals: requested diagram as an outline, sounding
// diagram filled, pedals still in transit highlighted amber.
class PedalDiagramComponent final : public juce::Component
{
public:
    void setDiagrams (const ohrp::Diagram& sounding, const ohrp::Diagram& requested);
    void paint (juce::Graphics&) override;

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

    OrchHarpAudioProcessor& audioProcessor;

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

    juce::Label governorLabel;
    juce::ToggleButton playabilityButton;
    juce::ComboBox minChangeIntervalBox;
    juce::ToggleButton changesAtRestsOnlyButton, avoidRingingButton;

    juce::Label triggersLabel;
    juce::Label ccBankLabel, ccChannelLabel, ctrlDirectLabel, ctrlStepLabel;
    juce::Slider ccBankSlider, ccChannelSlider;
    juce::Slider ctrlDirectLoSlider, ctrlDirectHiSlider, ctrlStepDownSlider, ctrlStepUpSlider;

    juce::Label contourGlissLabel;
    juce::Label glissCcLabel, glissRangeLabel, glissBaseOctaveLabel, glissVelLabel, glissRingLabel;
    juce::Slider glissCcSlider, glissLoStringSlider, glissHiStringSlider, glissBaseOctaveSlider;
    juce::Slider glissVelCcSlider, glissVelocitySlider;
    juce::ComboBox glissRingBox;

    juce::Label triggerGlissLabel;
    juce::Label glissTrigZoneLabel, glissRunDirLabel, glissRunSpanLabel, glissRunDurLabel;
    juce::Slider glissTrigLoSlider, glissTrigHiSlider, glissRunSpanSlider;
    juce::ComboBox glissRunDirectionBox, glissRunDurationBox;

    juce::Label statusLabel;

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ComboBoxAttachment> modeAttachment, blackKeyModeAttachment, minChangeIntervalAttachment;
    std::array<std::unique_ptr<ComboBoxAttachment>, 7> pedalAttachments;
    std::unique_ptr<ButtonAttachment> playabilityAttachment, changesAtRestsOnlyAttachment, avoidRingingAttachment;
    std::unique_ptr<SliderAttachment> ccBankAttachment, ccChannelAttachment;
    std::unique_ptr<SliderAttachment> ctrlDirectLoAttachment, ctrlDirectHiAttachment, ctrlStepDownAttachment, ctrlStepUpAttachment;
    std::unique_ptr<SliderAttachment> glissCcAttachment, glissLoStringAttachment, glissHiStringAttachment, glissBaseOctaveAttachment;
    std::unique_ptr<SliderAttachment> glissVelCcAttachment, glissVelocityAttachment, glissRunSpanAttachment;
    std::unique_ptr<SliderAttachment> glissTrigLoAttachment, glissTrigHiAttachment;
    std::unique_ptr<ComboBoxAttachment> glissRingAttachment, glissRunDirectionAttachment, glissRunDurationAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchHarpAudioProcessorEditor)
};
