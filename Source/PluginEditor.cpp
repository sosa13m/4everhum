#include "PluginEditor.h"

#include "midi/MidiExporter.h"
#include "ui/Theme.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
constexpr int editorWidth  = 800;
constexpr int editorHeight = 600;
constexpr int headerHeight = 70;

void setStyle (juce::Component& c, const char* style)
{
    c.getProperties().set (HumLookAndFeel::styleKey, style);
}

void styleCaption (juce::Label& l)
{
    l.setColour (juce::Label::textColourId, theme::ink);
    l.setFont (theme::typed (15.0f));
    l.setJustificationType (juce::Justification::centred);
}
} // namespace

FourEverHumEditor::FourEverHumEditor (FourEverHumProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    wallGrain  = theme::grainTile (theme::paper, 0.35f, 11);
    xeroxGrain = theme::grainTile (theme::ink, 0.55f, 23);

    setStyle (recordButton, "primary");
    recordButton.onClick = [this] { toggleRecording(); };
    addAndMakeVisible (recordButton);

    setStyle (saveMidiButton, "paper");
    saveMidiButton.setButtonText (theme::ui ("Guardar .mid…"));
    saveMidiButton.setTooltip (theme::ui ("Guarda la melodía como archivo MIDI en la carpeta que elijas."));
    saveMidiButton.onClick = [this] { saveMidiAs(); };
    addAndMakeVisible (saveMidiButton);

    setStyle (saveWavButton, "paper");
    saveWavButton.setButtonText (theme::ui ("Guardar toma .wav…"));
    saveWavButton.setTooltip (theme::ui ("Guarda tu voz grabada. Sirve para probarla con hum2midi o para compartirla si la detección falla."));
    saveWavButton.onClick = [this] { saveTakeAs(); };
    addAndMakeVisible (saveWavButton);

    statusLabel.setColour (juce::Label::textColourId, theme::textOnWall);
    statusLabel.setFont (theme::typed (15.0f));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setMinimumHorizontalScale (0.9f);
    addAndMakeVisible (statusLabel);

    preview.createMidiFileForDrag = [this] { return writeTempMidi(); };
    addAndMakeVisible (preview);

    // --- Perillas -----------------------------------------------------------
    setupKnob (sensitivityKnob, paramIds::sensitivity, "SENSIBILIDAD",
               theme::ui ("Qué tan suave puede ser tu voz para que la detecte. "
                          "Súbela si se pierden notas; bájala si aparecen notas que no cantaste."));
    setupKnob (minNoteKnob, paramIds::minNote, theme::ui ("NOTA MÍNIMA"),
               theme::ui ("Duración mínima de una nota. Súbela si aparecen notas cortitas sueltas."));
    setupKnob (stabilityKnob, paramIds::stability, "ESTABILIDAD",
               theme::ui ("Cuánto ignora el vibrato y las desafinaciones. "
                          "Súbela si una nota larga se parte en varias."));

    // --- Octava: selector segmentado, como los de 4EVERTOOLS ------------------
    octaveLabel.setText ("OCTAVA", juce::dontSendNotification);
    styleCaption (octaveLabel);
    addAndMakeVisible (octaveLabel);

    const char* octaveNames[] = { "-2", "-1", "0", "+1", "+2" };
    for (int i = 0; i < (int) octaveButtons.size(); ++i)
    {
        auto& b = octaveButtons[(size_t) i];
        setStyle (b, "segment");
        b.setButtonText (octaveNames[i]);
        b.setTooltip (theme::ui ("Sube o baja la melodía completa. Útil si tarareas grave "
                                 "y la quieres en el registro de un piano o un lead."));
        b.onClick = [this, i] { if (octaveAttachment) octaveAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    if (auto* octaveParam = processor.params.getParameter (paramIds::octave))
    {
        octaveAttachment = std::make_unique<juce::ParameterAttachment> (*octaveParam, [this] (float index)
        {
            for (int i = 0; i < (int) octaveButtons.size(); ++i)
                octaveButtons[(size_t) i].setToggleState (i == juce::roundToInt (index), juce::dontSendNotification);
        });
        octaveAttachment->sendInitialUpdate();
    }

    autoGateToggle.setButtonText ("Filtro de ruido");
    autoGateToggle.setTooltip (theme::ui ("Mide el ruido de fondo de cada toma (por ejemplo el beat "
                                          "que se filtra por los audífonos) e ignora todo lo que esté a ese nivel."));
    addAndMakeVisible (autoGateToggle);
    autoGateAttachment = std::make_unique<ButtonAttachment> (processor.params, paramIds::autoGate, autoGateToggle);

    // Limpia los .mid temporales de arrastres de dias anteriores.
    const auto cutoff = juce::Time::getCurrentTime() - juce::RelativeTime::days (1);
    for (const auto& f : tempFolder().findChildFiles (juce::File::findFiles, false, "*.mid"))
        if (f.getLastModificationTime() < cutoff)
            f.deleteFile();

    setSize (editorWidth, editorHeight);
    timerCallback();
    startTimerHz (30);
}

FourEverHumEditor::~FourEverHumEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FourEverHumEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& title,
                                   const juce::String& tooltip)
{
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 20);
    k.slider.setTextValueSuffix (paramId == paramIds::minNote ? " ms" : " %");
    k.slider.setColour (juce::Slider::textBoxTextColourId, theme::ink);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k.slider.setTooltip (tooltip);
    addAndMakeVisible (k.slider);

    k.label.setText (title, juce::dontSendNotification);
    styleCaption (k.label);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (processor.params, paramId, k.slider);
}

juce::File FourEverHumEditor::tempFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("4EVERHUM");
    dir.createDirectory();
    return dir;
}

void FourEverHumEditor::toggleRecording()
{
    if (processor.getState() == FourEverHumProcessor::State::recording)
        processor.stopRecording();
    else
        processor.startRecording();
    transientUntilMs = 0;
    timerCallback();
}

void FourEverHumEditor::showStatus (const juce::String& text)
{
    transientStatus  = text;
    transientUntilMs = juce::Time::getMillisecondCounter() + 4000;
    statusLabel.setText (text, juce::dontSendNotification);
}

juce::File FourEverHumEditor::writeTempMidi()
{
    const auto bpm  = processor.getResultBpm();
    const auto file = tempFolder().getChildFile (fourever::suggestedMidiFileName (bpm));
    fourever::MidiExportSettings settings;
    settings.bpm = bpm;

    if (! fourever::writeMidiFile (file, processor.getNotes(), settings))
        return {};
    return file;
}

void FourEverHumEditor::saveMidiAs()
{
    if (processor.getNotes().empty())
        return;

    const auto start = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                           .getChildFile (fourever::suggestedMidiFileName (processor.getResultBpm()));

    fileChooser = std::make_unique<juce::FileChooser> (theme::ui ("Guardar melodía como MIDI"), start, "*.mid");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto file = fc.getResult();
                                  if (file == juce::File{})
                                      return;
                                  file = file.withFileExtension ("mid");

                                  fourever::MidiExportSettings settings;
                                  settings.bpm = processor.getResultBpm();
                                  const bool ok = fourever::writeMidiFile (file, processor.getNotes(), settings);
                                  showStatus (ok ? theme::ui ("Guardado: ") + file.getFileName()
                                                 : theme::ui ("No se pudo guardar el archivo."));
                              });
}

void FourEverHumEditor::saveTakeAs()
{
    auto take = processor.getLastTake();
    if (take == nullptr || take->samples.empty())
        return;

    const auto name  = "4EVERHUM_toma_" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".wav";
    const auto start = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (name);

    fileChooser = std::make_unique<juce::FileChooser> (theme::ui ("Guardar toma como WAV"), start, "*.wav");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, take] (const juce::FileChooser& fc)
                              {
                                  auto file = fc.getResult();
                                  if (file == juce::File{})
                                      return;
                                  file = file.withFileExtension ("wav");
                                  file.deleteFile();

                                  bool ok = false;
                                  if (auto out = file.createOutputStream())
                                  {
                                      juce::WavAudioFormat wav;
                                      std::unique_ptr<juce::OutputStream> stream (std::move (out));
                                      const auto options = juce::AudioFormatWriterOptions{}
                                                               .withSampleRate (take->sampleRate)
                                                               .withNumChannels (1)
                                                               .withBitsPerSample (24);
                                      if (auto writer = wav.createWriterFor (stream, options))
                                      {
                                          const float* channels[] = { take->samples.data() };
                                          ok = writer->writeFromFloatArrays (channels, 1, (int) take->samples.size());
                                      }
                                  }
                                  showStatus (ok ? theme::ui ("Toma guardada: ") + file.getFileName()
                                                 : theme::ui ("No se pudo guardar la toma."));
                              });
}

void FourEverHumEditor::timerCallback()
{
    using State = FourEverHumProcessor::State;
    const auto state = processor.getState();

    // El buffer de grabacion se lleno solo: cerramos la toma.
    if (state == State::recording && ! processor.isCapturing())
        processor.stopRecording();

    // Medidor con caida suave.
    meterLevel = juce::jmax (processor.getInputLevel(), meterLevel * 0.9f);
    repaint (meterArea.expanded (2));

    const auto now   = processor.getState();
    const bool isRec = now == State::recording;
    recordButton.setButtonText (isRec ? theme::ui ("■  Detener") : theme::ui ("●  Grabar"));
    recordButton.setEnabled (now != State::analyzing);

    if (processor.getResultVersion() != shownVersion)
    {
        shownVersion = processor.getResultVersion();
        preview.setNotes (processor.getNotes(), processor.getResultBpm());
    }
    saveMidiButton.setEnabled (preview.hasNotes() && ! isRec);
    saveWavButton.setEnabled (processor.getLastTake() != nullptr && ! isRec);

    if (juce::Time::getMillisecondCounter() < transientUntilMs && ! isRec)
    {
        statusLabel.setText (transientStatus, juce::dontSendNotification);
    }
    else
    {
        juce::String status;
        switch (now)
        {
            case State::idle:      status = theme::ui ("Listo. Pon el plugin en el track donde entra tu micrófono."); break;
            case State::recording: status = theme::ui ("Grabando… ") + juce::String (processor.getRecordedSeconds(), 1) + " s"; break;
            case State::analyzing: status = theme::ui ("Analizando tu voz…"); break;
            case State::ready:
            {
                const auto n = preview.hasNotes() ? processor.getNotes().size() : (size_t) 0;
                if (n == 0)
                    status = theme::ui ("No se detectaron notas. Sube la Sensibilidad o acércate al micrófono.");
                else
                    status = juce::String ((int) n) + theme::ui (n == 1 ? " nota detectada." : " notas detectadas.")
                           + (processor.getLastTake() != nullptr ? theme::ui (" Gira las perillas para afinar el resultado.") : juce::String());
                break;
            }
        }
        statusLabel.setText (status, juce::dontSendNotification);
    }

    if (! juce::exactlyEqual (processor.getHostBpm(), shownBpm))
    {
        shownBpm = processor.getHostBpm();
        repaint (bpmBadgeArea.expanded (4));
    }
}

// =============================================================================
// Pintura: pared del sotano, cartel de la tienda y la hoja fotocopiada.
// =============================================================================

void FourEverHumEditor::paint (juce::Graphics& g)
{
    paintWall (g);
    paintHeader (g);
    paintMeter (g);
    paintCard (g);
}

void FourEverHumEditor::paintWall (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    g.setGradientFill ({ theme::wallHi, 0.0f, 0.0f, theme::wall, 0.0f, b.getHeight() * 0.7f, false });
    g.fillAll();

    // Grano de concreto.
    g.setTiledImageFill (wallGrain, 0, 0, 0.10f);
    g.fillRect (b);

    // Luz calida de la ampolleta bajo el cartel.
    g.setGradientFill ({ theme::bulb.withAlpha (0.13f), b.getCentreX(), (float) headerHeight,
                         theme::bulb.withAlpha (0.0f), b.getCentreX() + 380.0f, (float) headerHeight + 60.0f, true });
    g.fillRect (b);

    // Vineta: las esquinas se pierden en la oscuridad.
    g.setGradientFill ({ juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                         juce::Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true });
    g.fillRect (b);
}

void FourEverHumEditor::paintHeader (juce::Graphics& g)
{
    const auto h = headerArea.toFloat();

    // Cartel de la tienda sobre la escalera: barra negra con borde rojo.
    g.setColour (theme::headerBar);
    g.fillRect (h);
    g.setGradientFill ({ juce::Colours::black.withAlpha (0.6f), 0.0f, h.getBottom() + 4.0f,
                         juce::Colours::transparentBlack, 0.0f, h.getBottom() + 18.0f, false });
    g.fillRect (h.getX(), h.getBottom() + 4.0f, h.getWidth(), 14.0f);
    g.setColour (theme::red);
    g.fillRect (h.getX(), h.getBottom(), h.getWidth(), 4.0f);

    // Logo 4ever con su sombra dura.
    const auto logo = theme::logo();
    const float logoH = 50.0f;
    const juce::Rectangle<float> logoArea (20.0f, (h.getHeight() - logoH) * 0.5f,
                                           logoH * (float) logo.getWidth() / (float) juce::jmax (1, logo.getHeight()), logoH);
    g.setColour (juce::Colours::black);
    g.drawImage (logo, logoArea.translated (3.0f, 4.0f), juce::RectanglePlacement::centred, true);
    g.setOpacity (1.0f);
    g.drawImage (logo, logoArea, juce::RectanglePlacement::centred);

    // "HUM" en tipografia de flyer.
    g.setColour (theme::paper);
    g.setFont (theme::anton (46.0f));
    const juce::Rectangle<float> humArea (logoArea.getRight() + 10.0f, 4.0f, 90.0f, h.getHeight() - 6.0f);
    g.drawText ("HUM", humArea, juce::Justification::centredLeft);

    // Etiqueta de cinta: "tararea -> MIDI", torcida como pegada a mano.
    {
        const juce::Rectangle<float> tag (humArea.getX() + 88.0f, 22.0f, 150.0f, 26.0f);
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (-0.04f, tag.getCentreX(), tag.getCentreY()));
        theme::drawHardShadow (g, tag, { 3.0f, 3.0f }, juce::Colours::black);
        g.setColour (theme::paper);
        g.fillRect (tag);
        g.setColour (theme::ink);
        g.setFont (theme::typed (15.0f));
        g.drawText (theme::ui ("TARAREA → MIDI"), tag, juce::Justification::centred);
    }

    // Insignia del tempo del proyecto.
    {
        const auto badge = bpmBadgeArea.toFloat();
        theme::drawHardShadow (g, badge, { 2.0f, 2.0f }, juce::Colours::black);
        g.setColour (theme::paper);
        g.fillRect (badge);
        g.setColour (theme::red);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ badge.getX() + 14.0f, badge.getCentreY() }));
        g.setColour (theme::ink);
        g.setFont (theme::typed (14.0f));
        g.drawText ("TEMPO " + juce::String (processor.getHostBpm(), 1) + " BPM",
                    badge.withTrimmedLeft (24.0f).withTrimmedRight (8.0f), juce::Justification::centred);
    }
}

void FourEverHumEditor::paintMeter (juce::Graphics& g)
{
    // Medidor tipo "barra de carga" de 4EVERTOOLS: bloques de 6 px con 2 px de aire.
    const auto m = meterArea.toFloat();
    g.setColour (theme::dimOnWall);
    g.setFont (theme::mono (10.0f, true));
    g.drawText ("MIC", m.withY (m.getY() - 15.0f).withHeight (12.0f), juce::Justification::left);

    const float db   = juce::Decibels::gainToDecibels (meterLevel, -60.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);

    const int blocks = (int) (m.getWidth() / 8.0f);
    for (int i = 0; i < blocks; ++i)
    {
        const float pos = (float) (i + 1) / (float) blocks;
        const auto r = juce::Rectangle<float> (m.getX() + (float) i * 8.0f, m.getY(), 6.0f, m.getHeight());
        const bool lit = pos <= frac;
        const bool hot = pos > 0.9f; // ultimos bloques: cerca de saturar
        g.setColour (lit ? (hot ? theme::red : theme::paper) : juce::Colours::white.withAlpha (0.08f));
        g.fillRect (r);
    }
}

void FourEverHumEditor::paintCard (juce::Graphics& g)
{
    const auto c = cardArea.toFloat();

    // Hoja fotocopiada pegada en la pared, con su sombra.
    juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 36, { 0, 18 }).drawForRectangle (g, cardArea);
    g.setGradientFill ({ juce::Colour (0xffefe9dc), c.getX(), c.getY(),
                         juce::Colour (0xffe0d8c6), c.getX() + c.getWidth() * 0.2f, c.getBottom(), false });
    g.fillRect (c);
    g.setTiledImageFill (xeroxGrain, 0, 0, 0.30f);
    g.fillRect (c);

    // Pliegue diagonal de la fotocopia.
    g.setColour (theme::ink.withAlpha (0.045f));
    g.drawLine (c.getX() + c.getWidth() * 0.62f, c.getY(), c.getX() + c.getWidth() * 0.48f, c.getBottom(), 3.0f);

    // Marca de agua gigante, como en las tool-cards de 4EVERTOOLS.
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (cardArea);
        g.addTransform (juce::AffineTransform::rotation (0.14f, c.getRight() - 200.0f, c.getY() + 70.0f));
        g.setColour (theme::ink.withAlpha (0.035f));
        g.setFont (theme::anton (124.0f));
        g.drawText ("4EVERHUM", juce::Rectangle<float> (c.getRight() - 520.0f, c.getY() + 4.0f, 560.0f, 140.0f),
                    juce::Justification::centredRight);
    }

    // Cinta que sostiene la hoja.
    theme::drawTape (g, juce::Rectangle<float> (120.0f, 26.0f).withCentre ({ c.getCentreX(), c.getY() + 1.0f }), -0.035f);

    // Linea de corte punteada entre el piano roll y los controles, con su tijera.
    const float y = (float) controlsArea.getY() - 13.0f;
    const juce::Rectangle<float> tag (c.getX() + 34.0f, y - 7.0f, 92.0f, 14.0f);

    juce::Path cut;
    cut.startNewSubPath (c.getX() + 22.0f, y);
    cut.lineTo (c.getRight() - 22.0f, y);
    juce::Path dashed;
    const float dashes[] = { 5.0f, 4.0f };
    juce::PathStrokeType (1.4f).createDashedStroke (dashed, cut, dashes, 2);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.excludeClipRegion (tag.toNearestInt());
        g.setColour (theme::ink.withAlpha (0.45f));
        g.fillPath (dashed);
    }

    g.setColour (theme::inkSoft);
    g.setFont (theme::mono (10.0f, true));
    g.drawText (theme::ui ("✂ AJUSTES"), tag, juce::Justification::centred);
}

void FourEverHumEditor::resized()
{
    auto area = getLocalBounds();

    headerArea   = area.removeFromTop (headerHeight);
    bpmBadgeArea = juce::Rectangle<int> (headerArea.getRight() - 196, 21, 174, 28);

    // Fila de grabacion sobre la pared.
    auto row = juce::Rectangle<int> (22, headerHeight + 22, getWidth() - 44, 56);
    const auto rec = row.removeFromLeft (176);
    recordButton.setTransform ({});
    recordButton.setBounds (rec);
    recordButton.setTransform (juce::AffineTransform::rotation (-0.014f, (float) rec.getCentreX(), (float) rec.getCentreY()));
    row.removeFromLeft (26);
    meterArea = row.removeFromLeft (184).withSizeKeepingCentre (184, 12).translated (0, 6);
    row.removeFromLeft (24);
    statusLabel.setBounds (row);

    // Hoja de trabajo.
    cardArea = juce::Rectangle<int> (22, headerHeight + 100, getWidth() - 44, 0)
                   .withBottom (getHeight() - 22);
    auto inner = cardArea.reduced (22, 0).withTrimmedTop (28).withTrimmedBottom (18);

    controlsArea = inner.removeFromBottom (128);
    inner.removeFromBottom (30);
    previewArea = inner;
    preview.setBounds (previewArea);

    // Controles: perillas | octava + filtro | exportar
    auto controls = controlsArea;
    auto buttons = controls.removeFromRight (184);
    saveMidiButton.setBounds (buttons.removeFromTop (60).withTrimmedTop (8));
    saveWavButton.setBounds (buttons.removeFromTop (60).withTrimmedTop (8));
    controls.removeFromRight (16);

    for (auto* k : { &sensitivityKnob, &minNoteKnob, &stabilityKnob })
    {
        auto col = controls.removeFromLeft (100);
        k->label.setBounds (col.removeFromTop (20));
        k->slider.setBounds (col);
        controls.removeFromLeft (4);
    }

    controls.removeFromLeft (12);
    auto octave = controls.removeFromLeft (212);
    octaveLabel.setBounds (octave.removeFromTop (20));
    auto seg = octave.removeFromTop (46).withSizeKeepingCentre (5 * 42, 40);
    for (auto& b : octaveButtons)
        b.setBounds (seg.removeFromLeft (42));
    octave.removeFromTop (12);
    autoGateToggle.setBounds (octave.removeFromTop (28).withSizeKeepingCentre (170, 28));
}
