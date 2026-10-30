// 4EVERHUM - tararea -> MIDI
// Copyright (C) 2026 sosa13m / colectivo 4EVER
// SPDX-License-Identifier: AGPL-3.0-or-later
// Este programa es software libre: ver el archivo LICENSE.

#pragma once

#include "PluginProcessor.h"
#include "ui/HumLookAndFeel.h"
#include "ui/NotePreview.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

class FourEverHumEditor final : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit FourEverHumEditor (FourEverHumProcessor&);
    ~FourEverHumEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Menu desplegable + su etiqueta.
    struct Choice
    {
        juce::ComboBox box;
        juce::Label    label;
        std::unique_ptr<ComboAttachment> attachment;
    };

    // Perilla + su etiqueta.
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& title, const juce::String& tooltip);
    void setupChoice (Choice&, const juce::String& paramId, const juce::String& title,
                      const juce::StringArray& items, const juce::String& tooltip);
    juce::String keyLabelForFile() const;
    void toggleRecording();
    void saveMidiAs();
    void saveTakeAs();
    void showStatus (const juce::String& text);
    juce::File writeTempMidi();
    static juce::File tempFolder();

    void applyTheme (const juce::String& id); // cambia el tema y re-aplica colores y tipografias
    void refreshThemedComponents();
    void paintWall (juce::Graphics&);
    void paintHeader (juce::Graphics&);
    void paintMeter (juce::Graphics&);
    void paintCard (juce::Graphics&);
    void paintKeyStamp (juce::Graphics&);
    void paintRowTab (juce::Graphics&, juce::Rectangle<int> row, const juce::String& title);

    FourEverHumProcessor& humProcessor;
    HumLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::ComboBox   themeBox;
    juce::HyperlinkButton aboutLink; // aviso de licencia (AGPLv3) y link al codigo
    juce::TextButton recordButton;
    juce::TextButton saveMidiButton;
    juce::TextButton saveWavButton;
    juce::Label      statusLabel;
    NotePreview      preview;

    Knob sensitivityKnob, minNoteKnob, stabilityKnob, strengthKnob;
    Choice keyChoice, scaleChoice, quantizeChoice;

    juce::Label octaveLabel;
    std::array<juce::TextButton, 5> octaveButtons;
    std::unique_ptr<juce::ParameterAttachment> octaveAttachment;

    juce::ToggleButton autoGateToggle, precisionToggle;
    std::unique_ptr<ButtonAttachment> autoGateAttachment, precisionAttachment;

    // Texturas generadas una vez.
    juce::Image wallGrain, xeroxGrain;

    juce::Rectangle<int> headerArea, meterArea, cardArea, previewArea, controlsArea, bpmBadgeArea;
    juce::Rectangle<int> detectionRow, musicRow, stampArea;
    fourever::Clip currentClip;
    float  meterLevel   = 0.0f;
    int    shownVersion = -1;
    double shownBpm     = -1.0;
    juce::String transientStatus;           // "Guardado: ..." por unos segundos
    juce::uint32 transientUntilMs = 0;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourEverHumEditor)
};
