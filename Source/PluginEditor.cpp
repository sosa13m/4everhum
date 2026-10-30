#include "PluginEditor.h"

#include "midi/MidiExporter.h"
#include "ui/Theme.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
constexpr int editorWidth  = 1000;
constexpr int editorHeight = 740;
constexpr int headerHeight = 70;

void setStyle (juce::Component& c, const char* style)
{
    c.getProperties().set (HumLookAndFeel::styleKey, style);
}

void styleCaption (juce::Label& l)
{
    l.setColour (juce::Label::textColourId, theme::c().cardInk);
    l.setFont (theme::typed (15.0f));
    l.setJustificationType (juce::Justification::centred);
}
} // namespace

FourEverHumEditor::FourEverHumEditor (FourEverHumProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // El tema es una preferencia del usuario (la misma para todos sus proyectos).
    theme::setCurrent (theme::loadSavedThemeId());
    lookAndFeel.refreshColours();
    setLookAndFeel (&lookAndFeel);

    for (int i = 0; i < (int) theme::all().size(); ++i)
        themeBox.addItem (theme::all()[(std::size_t) i].name, i + 1);
    themeBox.setTooltip (theme::ui ("Tema visual, igual que los temas de 4EVERTOOLS. Se recuerda para la próxima vez."));
    themeBox.onChange = [this]
    {
        const int idx = themeBox.getSelectedItemIndex();
        if (idx >= 0)
            applyTheme (theme::all()[(std::size_t) idx].id);
    };
    addAndMakeVisible (themeBox);

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

    statusLabel.setColour (juce::Label::textColourId, theme::c().textOnWall);
    statusLabel.setFont (theme::typed (15.0f));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setMinimumHorizontalScale (0.9f);
    addAndMakeVisible (statusLabel);

    preview.createMidiFileForDrag = [this] { return writeTempMidi(); };
    preview.onNotesEdited      = [this] (std::vector<fourever::ClipNote> edited) { processor.setEditedNotes (std::move (edited)); };
    preview.onRestoreAutomatic = [this] { processor.clearEditedNotes(); };
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

    // --- Musica: tonalidad, escala y grid ------------------------------------
    setupChoice (keyChoice, paramIds::keyRoot, "TONALIDAD", FourEverHumProcessor::keyChoices(),
                 theme::ui ("Auto detecta la tonalidad de lo que cantaste. Si ya sabes en qué tonalidad "
                            "está tu beat, elígela aquí y las notas se ajustan a ella."));
    setupChoice (scaleChoice, paramIds::scale, "ESCALA", FourEverHumProcessor::scaleChoices(),
                 theme::ui ("Las notas desafinadas caen en la nota más cercana de esta escala. "
                            "Auto usa mayor o menor según la tonalidad. \"Sin escala\" deja la altura tal cual."));
    setupChoice (quantizeChoice, paramIds::quantize, "CUANTIZAR", FourEverHumProcessor::quantizeChoices(),
                 theme::ui ("Ajusta los tiempos al grid: 1/16 para melodías rápidas, 1/8 para algo más suelto, "
                            "T = tresillos. Apagado deja tu ritmo tal cual."));
    setupKnob (strengthKnob, paramIds::strength, "FUERZA",
               theme::ui ("Cuánto se acercan las notas al grid. 100% = exacto; menos deja algo de tu groove."));

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

    precisionToggle.setButtonText (theme::ui ("Alta precisión"));
    precisionToggle.setTooltip (theme::ui ("Usa pYIN: sigue la melodía en el tiempo y evita saltos de octava, "
                                           "sobre todo con voz rasposa o grave. Desactívalo solo si una toma muy "
                                           "larga tarda en analizarse."));
    addAndMakeVisible (precisionToggle);
    precisionAttachment = std::make_unique<ButtonAttachment> (processor.params, paramIds::precision, precisionToggle);

    // Limpia los .mid temporales de arrastres de dias anteriores.
    const auto cutoff = juce::Time::getCurrentTime() - juce::RelativeTime::days (1);
    for (const auto& f : tempFolder().findChildFiles (juce::File::findFiles, false, "*.mid"))
        if (f.getLastModificationTime() < cutoff)
            f.deleteFile();

    // Redimensionable desde la esquina: al agrandar crece el piano roll.
    setResizable (true, true);
    setResizeLimits (editorWidth, 680, 1800, 1300);
    setSize (editorWidth, editorHeight);
    refreshThemedComponents();
    timerCallback();
    startTimerHz (30);
}

FourEverHumEditor::~FourEverHumEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FourEverHumEditor::applyTheme (const juce::String& id)
{
    if (id == theme::currentId())
        return;
    theme::setCurrent (id);
    theme::saveThemeId (id);
    refreshThemedComponents();
}

void FourEverHumEditor::refreshThemedComponents()
{
    const auto& t = theme::c();
    wallGrain  = theme::grainTile (t.wallGrain, 0.35f, 11);
    xeroxGrain = theme::grainTile (t.cardGrain, 0.55f, 23);

    lookAndFeel.refreshColours();
    for (auto* l : { &sensitivityKnob.label, &minNoteKnob.label, &stabilityKnob.label, &strengthKnob.label,
                     &keyChoice.label, &scaleChoice.label, &quantizeChoice.label, &octaveLabel })
        styleCaption (*l);
    for (auto* k : { &sensitivityKnob, &minNoteKnob, &stabilityKnob, &strengthKnob })
        k->slider.setColour (juce::Slider::textBoxTextColourId, t.cardInk);

    statusLabel.setColour (juce::Label::textColourId, t.textOnWall);
    statusLabel.setFont (theme::typed (15.0f));

    for (int i = 0; i < (int) theme::all().size(); ++i)
        if (theme::all()[(std::size_t) i].id == t.id)
            themeBox.setSelectedItemIndex (i, juce::dontSendNotification);

    sendLookAndFeelChange(); // los componentes vuelven a pedir sus colores y fuentes
    resized();
    repaint();
}

void FourEverHumEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& title,
                                   const juce::String& tooltip)
{
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 20);
    k.slider.setTextValueSuffix (paramId == paramIds::minNote ? " ms" : " %");
    k.slider.setColour (juce::Slider::textBoxTextColourId, theme::c().cardInk);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k.slider.setTooltip (tooltip);
    addAndMakeVisible (k.slider);

    k.label.setText (title, juce::dontSendNotification);
    styleCaption (k.label);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (processor.params, paramId, k.slider);
}

void FourEverHumEditor::setupChoice (Choice& c, const juce::String& paramId, const juce::String& title,
                                     const juce::StringArray& items, const juce::String& tooltip)
{
    c.box.addItemList (items, 1);
    c.box.setTooltip (tooltip);
    addAndMakeVisible (c.box);

    c.label.setText (title, juce::dontSendNotification);
    styleCaption (c.label);
    addAndMakeVisible (c.label);

    c.attachment = std::make_unique<ComboAttachment> (processor.params, paramId, c.box);
}

juce::String FourEverHumEditor::keyLabelForFile() const
{
    if (currentClip.notes.empty() || currentClip.scaleUsed == fourever::Scale::none)
        return {};
    return juce::String (fourever::keyName (currentClip.key));
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
    const auto file = tempFolder().getChildFile (fourever::suggestedMidiFileName (bpm, keyLabelForFile()));
    fourever::MidiExportSettings settings;
    settings.bpm = bpm;

    if (! fourever::writeMidiFile (file, processor.getClip().notes, settings))
        return {};
    return file;
}

void FourEverHumEditor::saveMidiAs()
{
    if (currentClip.notes.empty())
        return;

    const auto start = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                           .getChildFile (fourever::suggestedMidiFileName (processor.getResultBpm(), keyLabelForFile()));

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
                                  const bool ok = fourever::writeMidiFile (file, processor.getClip().notes, settings);
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
        currentClip  = processor.getClip();
        preview.setClip (currentClip, processor.getResultBeatsPerBar());
        repaint (stampArea.expanded (12));
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
                const auto n = currentClip.notes.size();
                if (currentClip.edited)
                    status = theme::ui ("Melodía editada a mano (") + juce::String ((int) n)
                           + theme::ui (" notas). Las perillas ya no la cambian: AUTOMÁTICO para volver.");
                else if (n == 0)
                    status = theme::ui ("No se detectaron notas. Sube la Sensibilidad o acércate al micrófono.");
                else if (currentClip.alignedToProject)
                    status = juce::String ((int) n) + theme::ui (" notas, alineadas desde el compás ")
                           + juce::String (juce::roundToInt (currentClip.originBeat / juce::jmax (1, processor.getResultBeatsPerBar())) + 1)
                           + theme::ui (" del proyecto.");
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
    paintRowTab (g, detectionRow, theme::ui ("DETECCIÓN"));
    paintRowTab (g, musicRow, theme::ui ("MÚSICA"));
    paintKeyStamp (g);
}

void FourEverHumEditor::paintRowTab (juce::Graphics& g, juce::Rectangle<int> row, const juce::String& title)
{
    // Pestana de tinta con el titulo de la fila, como las pestanas de un flyer.
    const auto tab = row.removeFromLeft (24).toFloat().reduced (0.0f, 4.0f);
    g.setColour (theme::c().cardInk); // oscura en hojas claras, clara en hojas oscuras
    g.fillRect (tab);

    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, tab.getCentreX(), tab.getCentreY()));
    g.setColour (theme::c().card1);
    g.setFont (theme::mono (10.0f, true));
    g.drawText (title, juce::Rectangle<float> (tab.getHeight(), tab.getWidth()).withCentre (tab.getCentre()),
                juce::Justification::centred);
}

void FourEverHumEditor::paintKeyStamp (juce::Graphics& g)
{
    if (currentClip.notes.empty())
        return;

    // Timbre de goma rojo con la tonalidad, estampado sobre la linea de corte.
    const auto r = stampArea.toFloat();
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::rotation (-0.04f, r.getCentreX(), r.getCentreY()));

    const auto ink = theme::c().stampInk.withAlpha (0.92f);
    g.setColour (theme::c().paperLight.withAlpha (0.85f));
    g.fillRect (r);
    g.setColour (ink);
    g.drawRect (r, 2.5f);
    g.drawRect (r.reduced (4.0f), 1.0f);

    const bool known = currentClip.scaleUsed != fourever::Scale::none;
    const bool chosen = processor.currentClipSettings().keyRoot >= 0;

    auto inner = r.reduced (12.0f, 6.0f);
    g.setFont (theme::display (20.0f));
    if (known)
    {
        g.drawText (juce::String (fourever::keyNameSpanish (currentClip.key)).toUpperCase(),
                    inner.removeFromTop (22.0f), juce::Justification::centredLeft);
        g.setFont (theme::mono (11.0f, true));
        // El nombre de la escala solo aparece si no es la mayor/menor de la propia tonalidad.
        const bool plainMode = currentClip.scaleUsed == (currentClip.key.minor ? fourever::Scale::minor : fourever::Scale::major);
        const auto scaleName = plainMode ? juce::String()
                                         : "  " + FourEverHumProcessor::scaleChoices()[(int) currentClip.scaleUsed].toUpperCase();
        g.drawText (juce::String (fourever::keyName (currentClip.key)) + theme::ui (" · ") + juce::String (fourever::camelot (currentClip.key))
                        + scaleName + (currentClip.edited ? theme::ui (" · EDITADO A MANO")
                                                          : chosen ? theme::ui (" · ELEGIDA") : theme::ui (" · DETECTADA")),
                    inner, juce::Justification::centredLeft);
    }
    else
    {
        g.drawText ("SIN ESCALA", inner.removeFromTop (22.0f), juce::Justification::centredLeft);
        g.setFont (theme::mono (11.0f, true));
        g.drawText (currentClip.detected.valid ? theme::ui ("alturas tal cual las cantaste")
                                               : theme::ui ("muy pocas notas para detectar tonalidad"),
                    inner, juce::Justification::centredLeft);
    }
}

void FourEverHumEditor::paintWall (juce::Graphics& g)
{
    const auto& t = theme::c();
    const auto b = getLocalBounds().toFloat();
    const float top = (float) headerHeight;

    g.setGradientFill ({ t.wallHi, 0.0f, 0.0f, t.wall, 0.0f, b.getHeight() * 0.7f, false });
    g.fillAll();

    auto radial = [&] (juce::Colour colour, float alpha, float cx, float cy, float radius)
    {
        g.setGradientFill ({ colour.withAlpha (alpha), cx, cy, colour.withAlpha (0.0f), cx + radius, cy, true });
        g.fillRect (b);
    };

    switch (t.wallStyle)
    {
        case theme::WallStyle::concrete:
            // Pared de concreto con grano y la luz calida de la ampolleta.
            g.setTiledImageFill (wallGrain, 0, 0, 0.10f);
            g.fillRect (b);
            radial (t.glow, 0.13f, b.getCentreX(), top, 380.0f);
            break;

        case theme::WallStyle::tiles:
        {
            // Azulejos del anden (en ladrillo) y el morado del spray.
            g.setColour (t.textOnWall.withAlpha (0.045f));
            const float tw = 46.0f, th = 22.0f;
            for (float y = top; y < b.getBottom(); y += th)
            {
                g.drawHorizontalLine ((int) y, 0.0f, b.getWidth());
                const float offset = std::fmod (y - top, 2.0f * th) < th ? 0.0f : tw * 0.5f;
                for (float x = -offset; x < b.getWidth(); x += tw)
                    g.drawVerticalLine ((int) x, y, y + th);
            }
            radial (t.glow, 0.22f, b.getWidth() * 0.12f, top + 40.0f, 420.0f);
            radial (juce::Colour (0xffd4ff3a), 0.07f, b.getWidth() * 0.9f, b.getBottom(), 360.0f);
            break;
        }

        case theme::WallStyle::starfield:
        {
            // Cielo pixelado: estrellas cuadradas y una luna morada.
            juce::Random rng (7);
            for (int i = 0; i < 140; ++i)
            {
                const float x = rng.nextFloat() * b.getWidth(), y = top + rng.nextFloat() * (b.getHeight() - top);
                const float sz = rng.nextFloat() < 0.15f ? 3.0f : 2.0f;
                g.setColour ((rng.nextFloat() < 0.2f ? t.red : juce::Colours::white).withAlpha (0.25f + 0.6f * rng.nextFloat()));
                g.fillRect (std::floor (x), std::floor (y), sz, sz);
            }
            radial (t.glow, 0.25f, b.getWidth() * 0.86f, top + 30.0f, 260.0f);
            break;
        }

        case theme::WallStyle::cover:
        {
            // Tapa de la libreta negra: grano fino y el elastico al costado.
            g.setTiledImageFill (wallGrain, 0, 0, 0.06f);
            g.fillRect (b);
            const juce::Rectangle<float> band (b.getRight() - 46.0f, top, 12.0f, b.getHeight() - top);
            g.setColour (juce::Colour (0xff1d1d1d));
            g.fillRect (band);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (band.withWidth (2.0f).translated (2.0f, 0.0f));
            radial (t.glow, 0.05f, b.getCentreX(), top, 420.0f);
            break;
        }

        case theme::WallStyle::stage:
            // Escenario negro con dos focos: verde toxico y rosado.
            g.setTiledImageFill (wallGrain, 0, 0, 0.05f);
            g.fillRect (b);
            radial (t.glow, 0.20f, b.getWidth() * 0.15f, top, 420.0f);
            radial (t.primaryShadow, 0.16f, b.getWidth() * 0.85f, top, 420.0f);
            break;
    }

    // Vineta: las esquinas se pierden en la oscuridad.
    g.setGradientFill ({ juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                         juce::Colours::black.withAlpha (0.5f), 0.0f, 0.0f, true });
    g.fillRect (b);
}

void FourEverHumEditor::paintHeader (juce::Graphics& g)
{
    const auto h = headerArea.toFloat();

    // Cartel de la tienda sobre la escalera: barra negra con borde rojo.
    g.setColour (theme::c().headerBar);
    g.fillRect (h);
    g.setGradientFill ({ juce::Colours::black.withAlpha (0.6f), 0.0f, h.getBottom() + 4.0f,
                         juce::Colours::transparentBlack, 0.0f, h.getBottom() + 18.0f, false });
    g.fillRect (h.getX(), h.getBottom() + 4.0f, h.getWidth(), 14.0f);
    g.setColour (theme::c().headerLine);
    g.fillRect (h.getX(), h.getBottom(), h.getWidth(), 4.0f);

    // Logo del tema con su sombra dura (los logos anchos se achican para caber).
    const auto logo = theme::logo();
    const float ratio = (float) logo.getWidth() / (float) juce::jmax (1, logo.getHeight());
    const float logoW = juce::jmin (50.0f * ratio, 210.0f);
    const float logoH = logoW / ratio;
    const juce::Rectangle<float> logoArea (20.0f, (h.getHeight() - logoH) * 0.5f, logoW, logoH);
    g.setColour (juce::Colours::black);
    g.drawImage (logo, logoArea.translated (3.0f, 4.0f), juce::RectanglePlacement::centred, true);
    g.setOpacity (1.0f);
    g.drawImage (logo, logoArea, juce::RectanglePlacement::centred);

    // "HUM" con la tipografia de titulares del tema.
    g.setColour (theme::c().textOnWall);
    g.setFont (theme::title (46.0f));
    const juce::Rectangle<float> humArea (logoArea.getRight() + 10.0f, 4.0f, 90.0f, h.getHeight() - 6.0f);
    g.drawText ("HUM", humArea, juce::Justification::centredLeft);

    // Etiqueta de cinta: "tararea -> MIDI", torcida como pegada a mano.
    {
        const juce::Rectangle<float> tag (humArea.getX() + 88.0f, 22.0f, 150.0f, 26.0f);
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (-0.04f, tag.getCentreX(), tag.getCentreY()));
        theme::drawHardShadow (g, tag, { 3.0f, 3.0f }, juce::Colours::black);
        g.setColour (theme::c().paper);
        g.fillRect (tag);
        g.setColour (theme::c().ink);
        g.setFont (theme::typed (15.0f));
        g.drawText (theme::ui ("TARAREA → MIDI"), tag, juce::Justification::centred);
    }

    // Version, chiquita bajo la etiqueta.
    g.setColour (theme::c().dimOnWall);
    g.setFont (theme::mono (9.0f));
    g.drawText ("v" JucePlugin_VersionString, juce::Rectangle<float> (humArea.getX() + 92.0f, h.getBottom() - 15.0f, 120.0f, 12.0f),
                juce::Justification::left);

    // Insignia del tempo del proyecto.
    {
        const auto badge = bpmBadgeArea.toFloat();
        theme::drawHardShadow (g, badge, { 2.0f, 2.0f }, juce::Colours::black);
        g.setColour (theme::c().paper);
        g.fillRect (badge);
        g.setColour (theme::c().red);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ badge.getX() + 14.0f, badge.getCentreY() }));
        g.setColour (theme::c().ink);
        g.setFont (theme::typed (14.0f));
        g.drawText ("TEMPO " + juce::String (processor.getHostBpm(), 1) + " BPM",
                    badge.withTrimmedLeft (24.0f).withTrimmedRight (8.0f), juce::Justification::centred);
    }
}

void FourEverHumEditor::paintMeter (juce::Graphics& g)
{
    // Medidor tipo "barra de carga" de 4EVERTOOLS: bloques de 6 px con 2 px de aire.
    const auto m = meterArea.toFloat();
    g.setColour (theme::c().dimOnWall);
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
        g.setColour (lit ? (hot ? theme::c().meterHot : theme::c().meterLit) : theme::c().textOnWall.withAlpha (0.08f));
        g.fillRect (r);
    }
}

void FourEverHumEditor::paintCard (juce::Graphics& g)
{
    const auto& t = theme::c();
    const auto c = cardArea.toFloat();

    // La hoja de trabajo con su sombra (o su brillo, en el escenario).
    if (t.cardStyle == theme::CardStyle::stagePanel)
        juce::DropShadow (t.cardBorder.withAlpha (0.35f), 28, { 0, 0 }).drawForRectangle (g, cardArea);
    juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 36, { 0, 18 }).drawForRectangle (g, cardArea);

    if (t.cardStyle == theme::CardStyle::pixelWindow)
        g.setColour (t.card1);
    else
        g.setGradientFill ({ t.card1, c.getX(), c.getY(), t.card2, c.getX() + c.getWidth() * 0.2f, c.getBottom(), false });
    g.fillRect (c);
    if (t.cardGrainAlpha > 0.0f)
    {
        g.setTiledImageFill (xeroxGrain, 0, 0, t.cardGrainAlpha);
        g.fillRect (c);
    }

    switch (t.cardStyle)
    {
        case theme::CardStyle::photocopy:
            // Pliegue diagonal de la fotocopia.
            g.setColour (t.cardInk.withAlpha (0.045f));
            g.drawLine (c.getX() + c.getWidth() * 0.62f, c.getY(), c.getX() + c.getWidth() * 0.48f, c.getBottom(), 3.0f);
            break;

        case theme::CardStyle::tileBoard:
        {
            // Letrero del metro: azulejos finos, borde morado y la franja de la linea.
            g.setColour (t.cardInk.withAlpha (0.035f));
            for (float y = c.getY() + 24.0f; y < c.getBottom(); y += 24.0f)
                g.drawHorizontalLine ((int) y, c.getX(), c.getRight());
            for (float x = c.getX() + 48.0f; x < c.getRight(); x += 48.0f)
                g.drawVerticalLine ((int) x, c.getY(), c.getBottom());
            g.setColour (juce::Colour (0xffd4ff3a));
            g.fillRect (c.getX(), c.getY(), c.getWidth(), 5.0f);
            g.setColour (t.cardBorder);
            g.drawRect (c, 3.0f);
            break;
        }

        case theme::CardStyle::pixelWindow:
        {
            // Ventana de RPG: doble borde pixelado y esquinas escalonadas.
            g.setColour (t.cardBorder);
            g.drawRect (c, 4.0f);
            g.setColour (t.cardInkSoft);
            g.drawRect (c.reduced (7.0f), 2.0f);
            g.setColour (t.wall);
            for (const auto& corner : { c.getTopLeft(), c.getTopRight().translated (-4.0f, 0.0f),
                                        c.getBottomLeft().translated (0.0f, -4.0f), c.getBottomRight().translated (-4.0f, -4.0f) })
                g.fillRect (corner.x, corner.y, 4.0f, 4.0f);
            break;
        }

        case theme::CardStyle::sketchPage:
        {
            // Pagina de libreta: los agujeros del espiral a la izquierda.
            g.setColour (t.wall);
            for (float y = c.getY() + 26.0f; y < c.getBottom() - 16.0f; y += 30.0f)
                g.fillEllipse (c.getX() + 8.0f, y, 9.0f, 9.0f);
            break;
        }

        case theme::CardStyle::stagePanel:
            g.setColour (t.cardBorder);
            g.drawRect (c, 2.0f);
            break;
    }

    // Marca de agua gigante, como en las tool-cards de 4EVERTOOLS.
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (cardArea);
        g.addTransform (juce::AffineTransform::rotation (0.14f, c.getRight() - 200.0f, c.getY() + 70.0f));
        g.setColour (t.cardInk.withAlpha (0.035f));
        g.setFont (theme::title (124.0f));
        g.drawText ("4EVERHUM", juce::Rectangle<float> (c.getRight() - 520.0f, c.getY() + 4.0f, 560.0f, 140.0f),
                    juce::Justification::centredRight);
    }

    // Cinta que sostiene la hoja.
    if (t.useTape)
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
        g.setColour (t.cardInk.withAlpha (0.45f));
        g.fillPath (dashed);
    }

    g.setColour (t.cardInkSoft);
    g.setFont (theme::mono (10.0f, true));
    g.drawText (theme::ui ("✂ AJUSTES"), tag, juce::Justification::centred);
}

void FourEverHumEditor::resized()
{
    auto area = getLocalBounds();

    headerArea   = area.removeFromTop (headerHeight);
    bpmBadgeArea = juce::Rectangle<int> (headerArea.getRight() - 196, 21, 174, 28);
    themeBox.setBounds (bpmBadgeArea.getX() - 214, 16, 196, 40);

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
    cardArea = juce::Rectangle<int> (22, headerHeight + 92, getWidth() - 44, 0)
                   .withBottom (getHeight() - 22);
    auto inner = cardArea.reduced (22, 0).withTrimmedTop (28).withTrimmedBottom (16);

    controlsArea = inner.removeFromBottom (210);
    inner.removeFromBottom (36);
    previewArea = inner;
    preview.setBounds (previewArea);

    const int cutY = controlsArea.getY() - 13;
    stampArea = juce::Rectangle<int> (controlsArea.getRight() - 236, cutY - 24, 236, 48);

    // Controles: dos filas (DETECCION y MUSICA) | botones de exportar
    auto controls = controlsArea;
    auto buttons = controls.removeFromRight (176).withSizeKeepingCentre (176, 128);
    saveMidiButton.setBounds (buttons.removeFromTop (60).withTrimmedTop (4));
    buttons.removeFromTop (8);
    saveWavButton.setBounds (buttons.removeFromTop (60).withTrimmedTop (4));
    controls.removeFromRight (16);

    detectionRow = controls.removeFromTop (100);
    controls.removeFromTop (10);
    musicRow = controls.removeFromTop (100);

    auto placeKnob = [] (Knob& k, juce::Rectangle<int> col)
    {
        k.label.setBounds (col.removeFromTop (20));
        k.slider.setBounds (col);
    };
    auto placeChoice = [] (Choice& c, juce::Rectangle<int> col)
    {
        c.label.setBounds (col.removeFromTop (20));
        c.box.setBounds (col.withTrimmedTop (10).removeFromTop (42));
    };

    // Fila 1: DETECCION
    auto ctl = detectionRow.withTrimmedLeft (34);
    for (auto* k : { &sensitivityKnob, &minNoteKnob, &stabilityKnob })
    {
        placeKnob (*k, ctl.removeFromLeft (100));
        ctl.removeFromLeft (6);
    }
    ctl.removeFromLeft (20);
    auto toggles = ctl.removeFromLeft (190).withSizeKeepingCentre (190, 68).translated (0, 6);
    autoGateToggle.setBounds (toggles.removeFromTop (28));
    toggles.removeFromTop (12);
    precisionToggle.setBounds (toggles.removeFromTop (28));

    // Fila 2: MUSICA
    ctl = musicRow.withTrimmedLeft (34);
    placeChoice (keyChoice, ctl.removeFromLeft (100));
    ctl.removeFromLeft (8);
    placeChoice (scaleChoice, ctl.removeFromLeft (168));
    ctl.removeFromLeft (8);
    placeChoice (quantizeChoice, ctl.removeFromLeft (100));
    ctl.removeFromLeft (4);
    placeKnob (strengthKnob, ctl.removeFromLeft (96));
    ctl.removeFromLeft (4);
    auto octave = ctl.removeFromLeft (5 * 34);
    octaveLabel.setBounds (octave.removeFromTop (20));
    auto seg = octave.withTrimmedTop (10).removeFromTop (42);
    for (auto& b : octaveButtons)
        b.setBounds (seg.removeFromLeft (34));
}
