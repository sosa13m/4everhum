#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

// Temas de 4EVERHUM, tomados de los temas de 4EVERTOOLS (misma paleta,
// tipografias y logo de cada uno). El tema activo es una preferencia global del
// usuario: lo usan todas las ventanas del plugin.
namespace theme
{
enum class WallStyle { concrete, tiles, starfield, cover, stage };
enum class CardStyle { photocopy, tileBoard, pixelWindow, sketchPage, stagePanel };
enum class Font { anton, specialElite, spaceMono, spaceMonoBold, sedgwick, bungee, inter, pixelify, pressStart,
                  permanentMarker, caveat, archivo, bigShoulders };

struct FontRole
{
    Font  font  = Font::anton;
    float scale = 1.0f; // compensa diferencias de tamano entre tipografias
};

struct Palette
{
    juce::String id, name;
    juce::String logoResource; // nombre del recurso embebido

    // Pared y cartel (fondo de la ventana)
    juce::Colour wall, wallHi, headerBar, headerLine, glow, textOnWall, dimOnWall;
    juce::Colour wallGrain;

    // Hoja de trabajo (donde van el piano roll y los controles)
    juce::Colour card1, card2, cardInk, cardInkSoft, cardGrain;
    float        cardGrainAlpha = 0.3f;
    juce::Colour cardBorder;      // transparente = sin borde

    // Controles (botones de papel, combos, casillas, pestanas)
    juce::Colour paper, paper2, paperLight, ink, inkSoft;

    // Acento (notas, perillas, sello, boton principal)
    juce::Colour red, redDark, redButton, redButtonHover, buttonText, onAccent, primaryShadow, meterLit, meterHot;
    juce::Colour stampInk; // tinta del sello de tonalidad (sobre papel claro)

    // Perillas
    juce::Colour knobBody, knobIndicator;

    juce::Colour tape;
    bool useTape = true;

    WallStyle wallStyle = WallStyle::concrete;
    CardStyle cardStyle = CardStyle::photocopy;

    FontRole title, display, label, mono;
};

// --- Tema activo -------------------------------------------------------------------
const std::vector<Palette>& all();
const Palette& c();                         // el tema activo
void setCurrent (const juce::String& id);   // id desconocido -> SOTANO
juce::String currentId();

// Preferencia guardada (AppData/4EVERTOOLS/4EVERHUM.settings).
juce::String loadSavedThemeId();
void saveThemeId (const juce::String& id);

// Textos de la UI escritos en UTF-8 (para tildes).
inline juce::String ui (const char* utf8) { return juce::String::fromUTF8 (utf8); }

// --- Tipografias del tema activo ------------------------------------------------------
juce::FontOptions title (float height);   // titulares grandes (HUM, marca de agua)
juce::FontOptions display (float height); // botones principales, sellos
juce::FontOptions label (float height);   // etiquetas y textos
juce::FontOptions mono (float height, bool bold = false); // numeros

// Compatibilidad con el codigo del tema SOTANO.
inline juce::FontOptions anton (float h) { return display (h); }
inline juce::FontOptions typed (float h) { return label (h); }

juce::Image logo();

// --- Texturas ------------------------------------------------------------------
juce::Image grainTile (juce::Colour grain, float density, int seed);
void drawTape (juce::Graphics&, juce::Rectangle<float> area, float angleRadians);
void drawHardShadow (juce::Graphics&, juce::Rectangle<float> area, juce::Point<float> offset, juce::Colour);
} // namespace theme
