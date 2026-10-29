#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Identidad visual de 4EVERHUM, tomada del tema "SOTANO 4EVER" de 4EVERTOOLS:
// una disqueria en un sotano. Pared de concreto bajo una ampolleta, flyers
// fotocopiados, cinta de enmascarar y el rojo de la tienda.
namespace theme
{
// --- Paleta (mismos valores que underground.css de 4EVERTOOLS) --------------
inline const juce::Colour wall       { 0xff0d0b0a }; // pared
inline const juce::Colour wallHi     { 0xff1b1714 };
inline const juce::Colour headerBar  { 0xf20b0909 };
inline const juce::Colour paper      { 0xffece6d8 }; // papel fotocopia
inline const juce::Colour paper2     { 0xffddd5c3 };
inline const juce::Colour paperLight { 0xfff6f3ec };
inline const juce::Colour ink        { 0xff121010 }; // tinta xerox
inline const juce::Colour inkSoft    { 0xff4a3b33 }; // texto secundario sobre papel
inline const juce::Colour red        { 0xffd93025 }; // rojo de la tienda
inline const juce::Colour redDark    { 0xffb3221a };
inline const juce::Colour redButton  { 0xffc4271d };
inline const juce::Colour tape       { 0xcce8dcae }; // cinta de enmascarar
inline const juce::Colour bulb       { 0xfff3c27a }; // luz de la ampolleta
inline const juce::Colour textOnWall { 0xffe3dccd };
inline const juce::Colour dimOnWall  { 0xff9b9184 };

// Textos de la UI escritos en UTF-8 (para tildes).
inline juce::String ui (const char* utf8) { return juce::String::fromUTF8 (utf8); }

// --- Tipografias embebidas ---------------------------------------------------
// Anton: titulares de flyer · Special Elite: etiquetas a maquina · Space Mono: numeros/recibos
juce::FontOptions anton (float height);
juce::FontOptions typed (float height);
juce::FontOptions mono (float height, bool bold = false);

juce::Image logo();

// --- Texturas ------------------------------------------------------------------
// Grano tipo fotocopia/concreto, generado una vez (semilla fija = siempre igual).
juce::Image grainTile (juce::Colour grain, float density, int seed);

// Rectangulo con la "cinta de enmascarar" pegada encima.
void drawTape (juce::Graphics&, juce::Rectangle<float> area, float angleRadians);

// Sombra dura desplazada (el "sticker" de 4EVERTOOLS).
void drawHardShadow (juce::Graphics&, juce::Rectangle<float> area, juce::Point<float> offset, juce::Colour);
} // namespace theme
