#include "OrchHarpEditor.h"

namespace
{
    const juce::Colour kBackground = juce::Colour::fromRGB (18, 24, 31);
    const juce::Colour kAccent     = juce::Colour::fromRGB (95, 200, 245);
    const juce::Colour kAmber      = juce::Colour::fromRGB (245, 195, 90);
    const juce::Colour kSlotOff    = juce::Colour::fromRGB (40, 50, 60);
    const juce::Colour kPanel      = juce::Colour::fromRGB (28, 36, 46);

    const char* kLetters = "CDEFGAB";
    // Harp pedal order, left to right: D C B | E F G A.
    const std::array<int, 7> kPedalDrawOrder { 1, 0, 6, 2, 3, 4, 5 };

    void styleLabel (juce::Label& label, float size, bool bold = false)
    {
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    void styleBox (juce::ComboBox& box)
    {
        box.setColour (juce::ComboBox::backgroundColourId, kPanel);
        box.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        box.setColour (juce::ComboBox::outlineColourId, kAccent);
    }

    void styleSlider (juce::Slider& slider)
    {
        slider.setColour (juce::Slider::backgroundColourId, kPanel);
        slider.setColour (juce::Slider::trackColourId, kAccent);
        slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, kPanel);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB (70, 85, 95));
    }

    juce::String noteName (int n)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
    }

    // Compact ASCII spelling of a diagram, e.g. "C# D Eb F# G Ab B#".
    // (Kept ASCII on purpose - the music glyphs mojibake through JUCE's
    // system-codepage char* handling on Windows.)
    juce::String diagramText (const ohrp::Diagram& d)
    {
        juce::String s;
        for (int i = 0; i < 7; ++i)
        {
            const int o = d[static_cast<size_t> (i)];
            s << (i ? " " : "") << kLetters[i] << (o < 0 ? "b" : o > 0 ? "#" : "");
        }
        return s;
    }

    const std::array<juce::Colour, 8> kSwatch {
        juce::Colour::fromRGB (95, 200, 245),  juce::Colour::fromRGB (111, 208, 160),
        juce::Colour::fromRGB (127, 208, 112), juce::Colour::fromRGB (245, 200, 95),
        juce::Colour::fromRGB (245, 160, 95),  juce::Colour::fromRGB (245, 127, 127),
        juce::Colour::fromRGB (200, 127, 245), juce::Colour::fromRGB (192, 192, 192) };
}

// ============================ PedalDiagramComponent ============================

void PedalDiagramComponent::setDiagrams (const ohrp::Diagram& sounding, const ohrp::Diagram& requested)
{
    if (sounding != soundingDiagram || requested != requestedDiagram)
    {
        soundingDiagram = sounding;
        requestedDiagram = requested;
        repaint();
    }
}

void PedalDiagramComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (6.0f);
    g.setColour (kPanel);
    g.fillRoundedRectangle (area, 6.0f);

    const float colW = area.getWidth() / 7.0f;
    const float topY = area.getY() + 18.0f;
    const float botY = area.getBottom() - 18.0f;
    const std::array<float, 3> rowY { topY, (topY + botY) * 0.5f, botY }; // flat / natural / sharp

    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("b", juce::Rectangle<float> (area.getX() - 2, rowY[0] - 8, 16, 16), juce::Justification::centred);
    g.drawText ("n", juce::Rectangle<float> (area.getX() - 2, rowY[1] - 8, 16, 16), juce::Justification::centred);
    g.drawText ("#", juce::Rectangle<float> (area.getX() - 2, rowY[2] - 8, 16, 16), juce::Justification::centred);

    for (int slot = 0; slot < 7; ++slot)
    {
        const int letter = kPedalDrawOrder[static_cast<size_t> (slot)];
        const float cx = area.getX() + colW * (slot + 0.5f);

        // Foot divider after the third pedal (left foot: D C B).
        if (slot == 3)
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawLine (area.getX() + colW * slot, area.getY() + 4, area.getX() + colW * slot, area.getBottom() - 4, 1.0f);
        }

        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawLine (cx, rowY[0], cx, rowY[2], 1.5f);

        const int reqOffset = requestedDiagram[static_cast<size_t> (letter)];
        const int soundOffset = soundingDiagram[static_cast<size_t> (letter)];
        const bool inTransit = reqOffset != soundOffset;

        auto dotAt = [&] (int offset, bool filled)
        {
            const float y = rowY[static_cast<size_t> (juce::jlimit (0, 2, offset + 1))];
            const juce::Rectangle<float> r (cx - 7.0f, y - 7.0f, 14.0f, 14.0f);
            if (filled)
                g.fillEllipse (r);
            else
                g.drawEllipse (r, 2.0f);
        };

        g.setColour (inTransit ? kAmber : kAccent);
        dotAt (reqOffset, false);          // requested: outline
        dotAt (soundOffset, true);         // sounding: filled

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (juce::String::charToString ((juce::juce_wchar) kLetters[letter]),
                    juce::Rectangle<float> (cx - 10.0f, area.getBottom() - 16.0f, 20.0f, 14.0f),
                    juce::Justification::centred);
    }
}

// ============================ BankCellComponent ============================

void BankCellComponent::setContent (const juce::String& name, juce::Colour colour, bool active)
{
    if (name != slotName || colour != slotColour || active != isActive)
    {
        slotName = name;
        slotColour = colour;
        isActive = active;
        repaint();
    }
}

void BankCellComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (isActive ? slotColour.withAlpha (0.35f) : kSlotOff);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (isActive ? slotColour : slotColour.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 4.0f, isActive ? 2.0f : 1.0f);

    g.setColour (slotColour);
    g.fillRoundedRectangle (r.removeFromLeft (8.0f).reduced (2.0f), 2.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (11.0f, isActive ? juce::Font::bold : juce::Font::plain));
    g.drawText (slotName, r.reduced (4.0f, 0.0f), juce::Justification::centredLeft, true);
}

void BankCellComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, "Rename...");
        m.addItem (2, "Recolour...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
            [this] (int r)
            {
                if (r == 1 && onRename)   onRename();
                if (r == 2 && onRecolour) onRecolour();
            });
        return;
    }

    if (e.mods.isShiftDown())
    {
        if (onSave) onSave();
        return;
    }

    if (onRecall) onRecall();
}

// ============================ Editor ============================

OrchHarpAudioProcessorEditor::OrchHarpAudioProcessorEditor (OrchHarpAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setResizable (true, true);
    setResizeLimits (680, 760, 1200, 1200);
    setSize (780, 880);

    auto& params = audioProcessor.getParameters();

    titleLabel.setText ("OrchHarp", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    styleLabel (titleLabel, 26.0f, true);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Pedal-Harp Pitch Transform", juce::dontSendNotification);
    subtitleLabel.setJustificationType (juce::Justification::centred);
    styleLabel (subtitleLabel, 13.0f);
    addAndMakeVisible (subtitleLabel);

    buildLabel.setText ("Build: Phase 1", juce::dontSendNotification);
    buildLabel.setJustificationType (juce::Justification::centred);
    buildLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (140, 160, 180));
    buildLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (buildLabel);

    modeLabel.setText ("Mode", juce::dontSendNotification);
    styleLabel (modeLabel, 13.0f, true);
    addAndMakeVisible (modeLabel);
    modeBox.addItemList ({ "Pedal", "Chromatic" }, 1);
    styleBox (modeBox);
    addAndMakeVisible (modeBox);
    modeAttachment = std::make_unique<ComboBoxAttachment> (params, "mode", modeBox);

    blackKeyModeLabel.setText ("Black Keys", juce::dontSendNotification);
    styleLabel (blackKeyModeLabel, 13.0f, true);
    addAndMakeVisible (blackKeyModeLabel);
    blackKeyModeBox.addItemList ({ "Control", "Nearest", "Drop", "Nudge" }, 1);
    styleBox (blackKeyModeBox);
    addAndMakeVisible (blackKeyModeBox);
    blackKeyModeAttachment = std::make_unique<ComboBoxAttachment> (params, "blackKeyMode", blackKeyModeBox);

    addAndMakeVisible (pedalDiagram);

    pedalsLabel.setText ("Pedal Diagram (C D E F G A B)", juce::dontSendNotification);
    styleLabel (pedalsLabel, 13.0f, true);
    addAndMakeVisible (pedalsLabel);

    static const std::array<const char*, 7> pedalIds { "pedalC", "pedalD", "pedalE", "pedalF", "pedalG", "pedalA", "pedalB" };
    for (int i = 0; i < 7; ++i)
    {
        auto& letterLabel = pedalLetterLabels[static_cast<size_t> (i)];
        letterLabel.setText (juce::String::charToString ((juce::juce_wchar) kLetters[i]), juce::dontSendNotification);
        letterLabel.setJustificationType (juce::Justification::centred);
        styleLabel (letterLabel, 12.0f, true);
        addAndMakeVisible (letterLabel);

        auto& box = pedalBoxes[static_cast<size_t> (i)];
        box.addItemList ({ "Flat", "Natural", "Sharp" }, 1);
        styleBox (box);
        addAndMakeVisible (box);
        pedalAttachments[static_cast<size_t> (i)] =
            std::make_unique<ComboBoxAttachment> (params, pedalIds[static_cast<size_t> (i)], box);
    }

    bankLabel.setText ("Bank   (click = recall   /   shift-click = save   /   right-click = rename + colour)", juce::dontSendNotification);
    styleLabel (bankLabel, 13.0f, true);
    addAndMakeVisible (bankLabel);

    for (int i = 0; i < OrchHarpAudioProcessor::kNumBankSlots; ++i)
    {
        auto& cell = bankCells[static_cast<size_t> (i)];
        cell.onRecall = [this, i] { audioProcessor.recallBankSlot (i); };
        cell.onSave   = [this, i] { audioProcessor.saveCurrentDiagramToSlot (i); refreshBankCells(); };
        cell.onRename = [this, i]
        {
            auto* aw = new juce::AlertWindow ("Rename slot " + juce::String (i + 1),
                                              "Slot name:", juce::MessageBoxIconType::NoIcon);
            aw->addTextEditor ("name", audioProcessor.getBankSlot (i).name);
            aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            aw->enterModalState (true, juce::ModalCallbackFunction::create (
                [this, aw, i] (int r)
                {
                    if (r == 1)
                        audioProcessor.renameBankSlot (i, aw->getTextEditorContents ("name"));
                    refreshBankCells();
                    delete aw;
                }), false);
        };
        cell.onRecolour = [this, i]
        {
            juce::PopupMenu m;
            for (int c = 0; c < (int) kSwatch.size(); ++c)
                m.addColouredItem (c + 1, "Colour " + juce::String (c + 1), kSwatch[static_cast<size_t> (c)]);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&bankCells[static_cast<size_t> (i)]),
                [this, i] (int r)
                {
                    if (r >= 1 && r <= (int) kSwatch.size())
                    {
                        audioProcessor.recolourBankSlot (i, kSwatch[static_cast<size_t> (r - 1)]);
                        refreshBankCells();
                    }
                });
        };
        addAndMakeVisible (cell);
    }

    helperLabel.setText ("Family Helper", juce::dontSendNotification);
    styleLabel (helperLabel, 13.0f, true);
    addAndMakeVisible (helperLabel);

    for (int f = 0; f < 4; ++f)
        familyBox.addItem (ohrp::familyName (static_cast<ohrp::Family> (f)), f + 1);
    styleBox (familyBox);
    addAndMakeVisible (familyBox);
    familyBox.setSelectedId (1, juce::dontSendNotification);
    familyBox.onChange = [this] { rebuildVariantBox(); };

    styleBox (variantBox);
    addAndMakeVisible (variantBox);

    for (int k = 0; k < 12; ++k)
        baseKeyBox.addItem (noteName (60 + k).dropLastCharacters (1), k + 1);
    styleBox (baseKeyBox);
    addAndMakeVisible (baseKeyBox);
    baseKeyBox.setSelectedId (1, juce::dontSendNotification);

    helperSlotSlider.setSliderStyle (juce::Slider::IncDecButtons);
    helperSlotSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 22);
    helperSlotSlider.setRange (1, OrchHarpAudioProcessor::kNumBankSlots, 1);
    helperSlotSlider.setValue (1, juce::dontSendNotification);
    styleSlider (helperSlotSlider);
    addAndMakeVisible (helperSlotSlider);

    helperWriteButton.setColour (juce::TextButton::buttonColourId, kPanel);
    helperWriteButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible (helperWriteButton);
    helperWriteButton.onClick = [this]
    {
        const auto family = static_cast<ohrp::Family> (juce::jlimit (0, 3, familyBox.getSelectedId() - 1));
        const int variant = juce::jmax (0, variantBox.getSelectedId() - 1);
        const int baseKey = juce::jlimit (0, 11, baseKeyBox.getSelectedId() - 1);
        const int slot = juce::jlimit (0, OrchHarpAudioProcessor::kNumBankSlots - 1,
                                       (int) helperSlotSlider.getValue() - 1);
        audioProcessor.writeFamilyToSlot (family, variant, baseKey, slot);
        refreshBankCells();
    };

    rebuildVariantBox();

    governorLabel.setText ("Playability Governor", juce::dontSendNotification);
    styleLabel (governorLabel, 13.0f, true);
    addAndMakeVisible (governorLabel);

    playabilityButton.setButtonText ("On");
    playabilityButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible (playabilityButton);
    playabilityAttachment = std::make_unique<ButtonAttachment> (params, "playability", playabilityButton);

    minChangeIntervalBox.addItemList ({ "1/8", "1/4", "1/2", "1 bar", "2 bars" }, 1);
    styleBox (minChangeIntervalBox);
    addAndMakeVisible (minChangeIntervalBox);
    minChangeIntervalAttachment = std::make_unique<ComboBoxAttachment> (params, "minChangeInterval", minChangeIntervalBox);

    changesAtRestsOnlyButton.setButtonText ("Changes at rests only");
    changesAtRestsOnlyButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible (changesAtRestsOnlyButton);
    changesAtRestsOnlyAttachment = std::make_unique<ButtonAttachment> (params, "changesAtRestsOnly", changesAtRestsOnlyButton);

    avoidRingingButton.setButtonText ("Avoid ringing pedal change");
    avoidRingingButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible (avoidRingingButton);
    avoidRingingAttachment = std::make_unique<ButtonAttachment> (params, "avoidRingingPedalChange", avoidRingingButton);

    triggersLabel.setText ("Triggers", juce::dontSendNotification);
    styleLabel (triggersLabel, 13.0f, true);
    addAndMakeVisible (triggersLabel);

    auto initIncDec = [] (juce::Slider& s, int lo, int hi)
    {
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 52, 22);
        s.setRange (lo, hi, 1);
        styleSlider (s);
    };

    ccBankLabel.setText ("CC# Bank (0=off, 49=Harp slot)", juce::dontSendNotification);
    styleLabel (ccBankLabel, 12.0f);
    addAndMakeVisible (ccBankLabel);
    initIncDec (ccBankSlider, 0, 127);
    addAndMakeVisible (ccBankSlider);
    ccBankAttachment = std::make_unique<SliderAttachment> (params, "ccBankSelect", ccBankSlider);

    ccChannelLabel.setText ("CC Channel (0=any)", juce::dontSendNotification);
    styleLabel (ccChannelLabel, 12.0f);
    addAndMakeVisible (ccChannelLabel);
    initIncDec (ccChannelSlider, 0, 16);
    addAndMakeVisible (ccChannelSlider);
    ccChannelAttachment = std::make_unique<SliderAttachment> (params, "ccChannel", ccChannelSlider);

    ctrlDirectLabel.setText ("Ctrl direct-select note range", juce::dontSendNotification);
    styleLabel (ctrlDirectLabel, 12.0f);
    addAndMakeVisible (ctrlDirectLabel);
    initIncDec (ctrlDirectLoSlider, 0, 127);
    initIncDec (ctrlDirectHiSlider, 0, 127);
    addAndMakeVisible (ctrlDirectLoSlider);
    addAndMakeVisible (ctrlDirectHiSlider);
    ctrlDirectLoAttachment = std::make_unique<SliderAttachment> (params, "ctrlDirectLo", ctrlDirectLoSlider);
    ctrlDirectHiAttachment = std::make_unique<SliderAttachment> (params, "ctrlDirectHi", ctrlDirectHiSlider);

    ctrlStepLabel.setText ("Ctrl step down / up note", juce::dontSendNotification);
    styleLabel (ctrlStepLabel, 12.0f);
    addAndMakeVisible (ctrlStepLabel);
    initIncDec (ctrlStepDownSlider, 0, 127);
    initIncDec (ctrlStepUpSlider, 0, 127);
    addAndMakeVisible (ctrlStepDownSlider);
    addAndMakeVisible (ctrlStepUpSlider);
    ctrlStepDownAttachment = std::make_unique<SliderAttachment> (params, "ctrlStepDownNote", ctrlStepDownSlider);
    ctrlStepUpAttachment = std::make_unique<SliderAttachment> (params, "ctrlStepUpNote", ctrlStepUpSlider);

    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour (juce::Label::textColourId, kAmber);
    statusLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (statusLabel);

    refreshBankCells();
    updateStatus();
    startTimerHz (12);
}

OrchHarpAudioProcessorEditor::~OrchHarpAudioProcessorEditor()
{
    stopTimer();
}

void OrchHarpAudioProcessorEditor::rebuildVariantBox()
{
    const auto family = static_cast<ohrp::Family> (juce::jlimit (0, 3, familyBox.getSelectedId() - 1));
    const int keepId = variantBox.getSelectedId();
    variantBox.clear (juce::dontSendNotification);
    const int n = ohrp::numVariants (family);
    for (int v = 0; v < n; ++v)
        variantBox.addItem (ohrp::variantName (family, v), v + 1);
    variantBox.setSelectedId (keepId >= 1 && keepId <= n ? keepId : 1, juce::dontSendNotification);
}

void OrchHarpAudioProcessorEditor::refreshBankCells()
{
    const int active = juce::roundToInt (audioProcessor.getParameters().getRawParameterValue ("bankSlot")->load());
    for (int i = 0; i < OrchHarpAudioProcessor::kNumBankSlots; ++i)
    {
        const auto slot = audioProcessor.getBankSlot (i);
        bankCells[static_cast<size_t> (i)].setContent (
            juce::String (i + 1) + ". " + slot.name,
            juce::Colour (slot.colour), i == active);
    }
}

void OrchHarpAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (kAccent);
    g.drawRoundedRectangle (bounds, 8.0f, 2.0f);
}

void OrchHarpAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20, 14);

    titleLabel.setBounds (area.removeFromTop (30));
    subtitleLabel.setBounds (area.removeFromTop (16));
    buildLabel.setBounds (area.removeFromTop (14));
    area.removeFromTop (8);

    {
        auto row = area.removeFromTop (26);
        modeLabel.setBounds (row.removeFromLeft (48));
        modeBox.setBounds (row.removeFromLeft (130));
        row.removeFromLeft (24);
        blackKeyModeLabel.setBounds (row.removeFromLeft (78));
        blackKeyModeBox.setBounds (row.removeFromLeft (130));
    }
    area.removeFromTop (8);

    pedalDiagram.setBounds (area.removeFromTop (150));
    area.removeFromTop (6);

    pedalsLabel.setBounds (area.removeFromTop (18));
    {
        auto row = area.removeFromTop (48);
        const int w = row.getWidth() / 7;
        for (int i = 0; i < 7; ++i)
        {
            auto col = row.removeFromLeft (w).reduced (3, 0);
            pedalLetterLabels[static_cast<size_t> (i)].setBounds (col.removeFromTop (16));
            pedalBoxes[static_cast<size_t> (i)].setBounds (col.removeFromTop (26));
        }
    }
    area.removeFromTop (8);

    bankLabel.setBounds (area.removeFromTop (18));
    {
        auto grid = area.removeFromTop (2 * 26 + 4);
        for (int rowIdx = 0; rowIdx < 2; ++rowIdx)
        {
            auto row = grid.removeFromTop (26);
            const int w = row.getWidth() / 6;
            for (int col = 0; col < 6; ++col)
                bankCells[static_cast<size_t> (rowIdx * 6 + col)].setBounds (row.removeFromLeft (w));
            grid.removeFromTop (4);
        }
    }
    area.removeFromTop (8);

    helperLabel.setBounds (area.removeFromTop (18));
    {
        auto row = area.removeFromTop (26);
        familyBox.setBounds (row.removeFromLeft (130));
        row.removeFromLeft (6);
        variantBox.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (6);
        baseKeyBox.setBounds (row.removeFromLeft (60));
        row.removeFromLeft (6);
        helperSlotSlider.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (6);
        helperWriteButton.setBounds (row.removeFromLeft (110));
    }
    area.removeFromTop (10);

    governorLabel.setBounds (area.removeFromTop (18));
    {
        auto row = area.removeFromTop (26);
        playabilityButton.setBounds (row.removeFromLeft (70));
        row.removeFromLeft (10);
        minChangeIntervalBox.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (16);
        changesAtRestsOnlyButton.setBounds (row.removeFromLeft (180));
        avoidRingingButton.setBounds (row.removeFromLeft (210));
    }
    area.removeFromTop (10);

    triggersLabel.setBounds (area.removeFromTop (18));
    {
        auto row = area.removeFromTop (26);
        ccBankLabel.setBounds (row.removeFromLeft (200));
        ccBankSlider.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (16);
        ccChannelLabel.setBounds (row.removeFromLeft (130));
        ccChannelSlider.setBounds (row.removeFromLeft (110));
    }
    area.removeFromTop (4);
    {
        auto row = area.removeFromTop (26);
        ctrlDirectLabel.setBounds (row.removeFromLeft (200));
        ctrlDirectLoSlider.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (8);
        ctrlDirectHiSlider.setBounds (row.removeFromLeft (110));
    }
    area.removeFromTop (4);
    {
        auto row = area.removeFromTop (26);
        ctrlStepLabel.setBounds (row.removeFromLeft (200));
        ctrlStepDownSlider.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (8);
        ctrlStepUpSlider.setBounds (row.removeFromLeft (110));
    }

    auto footer = getLocalBounds().reduced (20, 0);
    footer.removeFromBottom (10);
    statusLabel.setBounds (footer.removeFromBottom (22));
}

void OrchHarpAudioProcessorEditor::timerCallback()
{
    pedalDiagram.setDiagrams (audioProcessor.getSoundingDiagramForUi(),
                              audioProcessor.getRequestedDiagramForUi());

    const bool governed = audioProcessor.getParameters().getRawParameterValue ("playability")->load() >= 0.5f;
    minChangeIntervalBox.setEnabled (governed);
    changesAtRestsOnlyButton.setEnabled (governed);
    avoidRingingButton.setEnabled (governed);

    const bool pedalMode = audioProcessor.getParameters().getRawParameterValue ("mode")->load() < 0.5f;
    for (auto& b : pedalBoxes) b.setEnabled (pedalMode);

    refreshBankCells();
    updateStatus();
}

void OrchHarpAudioProcessorEditor::updateStatus()
{
    const int in = audioProcessor.getLastInputNoteForUi();
    const int out = audioProcessor.getLastOutputNoteForUi();
    const int letter = audioProcessor.getLastOutputLetterForUi();
    const int action = audioProcessor.getLastActionForUi();

    juce::String text;

    if (in >= 0)
    {
        text << "In " << noteName (in) << " -> ";
        if (action == 3)
            text << "(dropped)";
        else if (action == 4)
            text << "(control)";
        else
        {
            text << noteName (out);
            if (letter >= 0 && letter < 7)
                text << " " << juce::String::charToString ((juce::juce_wchar) "CDEFGAB"[letter]) << " string";
        }
    }
    else
    {
        text << "no notes yet";
    }

    text << "   |   sounding " << diagramText (audioProcessor.getSoundingDiagramForUi());
    const int transit = audioProcessor.getMovesInTransitForUi();
    if (transit > 0)
        text << "   |   " << transit << " pedal" << (transit == 1 ? "" : "s") << " in transit";

    const int cc = audioProcessor.getLastCcForUi();
    if (cc >= 0)
        text << "   |   last CC" << cc;

    statusLabel.setText (text, juce::dontSendNotification);
}
