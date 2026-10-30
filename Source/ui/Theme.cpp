#include "ui/Theme.h"

#include "BinaryData.h"

#include <atomic>

namespace theme
{
namespace
{
juce::Colour col (juce::uint32 argb) { return juce::Colour (argb); }

std::vector<Palette> makeThemes()
{
    std::vector<Palette> v;

    // SOTANO 4EVER: disqueria en un sotano (underground.css).
    {
        Palette p;
        p.id = "sotano"; p.name = juce::String::fromUTF8 ("SÓTANO 4EVER"); p.logoResource = "logo4ever_png";
        p.wall = col (0xff0d0b0a); p.wallHi = col (0xff1b1714); p.headerBar = col (0xf20b0909); p.headerLine = col (0xffd93025);
        p.glow = col (0xfff3c27a); p.textOnWall = col (0xffe3dccd); p.dimOnWall = col (0xff9b9184); p.wallGrain = col (0xffece6d8);
        p.card1 = col (0xffefe9dc); p.card2 = col (0xffe0d8c6); p.cardInk = col (0xff121010); p.cardInkSoft = col (0xff4a3b33);
        p.cardGrain = col (0xff121010); p.cardGrainAlpha = 0.30f; p.cardBorder = juce::Colours::transparentBlack;
        p.paper = col (0xffece6d8); p.paper2 = col (0xffddd5c3); p.paperLight = col (0xfff6f3ec); p.ink = col (0xff121010); p.inkSoft = col (0xff4a3b33);
        p.red = col (0xffd93025); p.redDark = col (0xffb3221a); p.redButton = col (0xffc4271d); p.redButtonHover = col (0xffa81f17);
        p.buttonText = juce::Colours::white; p.onAccent = juce::Colours::white; p.primaryShadow = juce::Colours::black;
        p.meterLit = col (0xffece6d8); p.meterHot = col (0xffd93025); p.stampInk = col (0xffd93025);
        p.knobBody = col (0xff121010); p.knobIndicator = col (0xffece6d8);
        p.tape = col (0xcce8dcae); p.useTape = true;
        p.wallStyle = WallStyle::concrete; p.cardStyle = CardStyle::photocopy;
        p.title = { Font::anton, 1.0f }; p.display = { Font::anton, 1.0f }; p.label = { Font::specialElite, 1.0f }; p.mono = { Font::spaceMono, 1.0f };
        v.push_back (p);
    }

    // LINEA 4 EVER: metro con graffiti (transit.css).
    {
        Palette p;
        p.id = "linea"; p.name = "LINEA 4 EVER"; p.logoResource = "logolinea_png";
        p.wall = col (0xff0b0910); p.wallHi = col (0xff1c1922); p.headerBar = col (0xf20b0910); p.headerLine = col (0xff7c4dff);
        p.glow = col (0xff7c4dff); p.textOnWall = col (0xffefeaff); p.dimOnWall = col (0xffa6a6b2); p.wallGrain = col (0xffffffff);
        p.card1 = col (0xff26222e); p.card2 = col (0xff1c1922); p.cardInk = col (0xfff6f0ff); p.cardInkSoft = col (0xffc9b3ff);
        p.cardGrain = col (0xffffffff); p.cardGrainAlpha = 0.06f; p.cardBorder = col (0xff7c4dff);
        p.paper = col (0xfff6f0ff); p.paper2 = col (0xffa6a6b2); p.paperLight = col (0xfff6f0ff); p.ink = col (0xff070409); p.inkSoft = col (0xff3a3150);
        p.red = col (0xff7c4dff); p.redDark = col (0xff5a2fe0); p.redButton = col (0xff7c4dff); p.redButtonHover = col (0xff6a3df0);
        p.buttonText = juce::Colours::white; p.onAccent = juce::Colours::white; p.primaryShadow = col (0xffd4ff3a);
        p.meterLit = col (0xffc9b3ff); p.meterHot = col (0xffd4ff3a); p.stampInk = col (0xff5a2fe0);
        p.knobBody = col (0xff070409); p.knobIndicator = col (0xffd4ff3a);
        p.tape = juce::Colours::transparentBlack; p.useTape = false;
        p.wallStyle = WallStyle::tiles; p.cardStyle = CardStyle::tileBoard;
        p.title = { Font::sedgwick, 1.15f }; p.display = { Font::bungee, 0.72f }; p.label = { Font::inter, 0.88f }; p.mono = { Font::spaceMono, 1.0f };
        v.push_back (p);
    }

    // 4EVERTALE: RPG de 16 bits, negro/blanco/morado (tale.css).
    {
        Palette p;
        p.id = "tale"; p.name = "4EVERTALE"; p.logoResource = "logotale_png";
        p.wall = col (0xff000000); p.wallHi = col (0xff140e22); p.headerBar = col (0xf2000000); p.headerLine = col (0xffb18cff);
        p.glow = col (0xff6a46b0); p.textOnWall = col (0xffefe9ff); p.dimOnWall = col (0xff8f86a8); p.wallGrain = col (0xffffffff);
        p.card1 = col (0xff1d1530); p.card2 = col (0xff140e22); p.cardInk = col (0xffffffff); p.cardInkSoft = col (0xffb18cff);
        p.cardGrain = col (0xffffffff); p.cardGrainAlpha = 0.0f; p.cardBorder = col (0xffffffff);
        p.paper = col (0xffffffff); p.paper2 = col (0xff5c5470); p.paperLight = col (0xffefe9ff); p.ink = col (0xff000000); p.inkSoft = col (0xff3b2d5c);
        p.red = col (0xffb18cff); p.redDark = col (0xff8a5cf0); p.redButton = col (0xffb18cff); p.redButtonHover = col (0xffc9aaff);
        p.buttonText = juce::Colours::black; p.onAccent = juce::Colours::black; p.primaryShadow = col (0xff6a46b0);
        p.meterLit = col (0xffffffff); p.meterHot = col (0xffb18cff); p.stampInk = col (0xff6a46b0);
        p.knobBody = col (0xff000000); p.knobIndicator = col (0xffffffff);
        p.tape = juce::Colours::transparentBlack; p.useTape = false;
        p.wallStyle = WallStyle::starfield; p.cardStyle = CardStyle::pixelWindow;
        p.title = { Font::pixelify, 1.0f }; p.display = { Font::pixelify, 1.0f }; p.label = { Font::pixelify, 0.95f }; p.mono = { Font::pressStart, 0.62f };
        v.push_back (p);
    }

    // LIBRETA NEGRA: el blackbook, estrictamente blanco y negro (blackbook.css).
    {
        Palette p;
        p.id = "libreta"; p.name = "LIBRETA NEGRA"; p.logoResource = "logoblackbook_png";
        p.wall = col (0xff0c0c0c); p.wallHi = col (0xff161616); p.headerBar = col (0xf2080808); p.headerLine = col (0xfff4f1e9);
        p.glow = col (0xffffffff); p.textOnWall = col (0xfff4f1e9); p.dimOnWall = col (0xff8a8a8a); p.wallGrain = col (0xffffffff);
        p.card1 = col (0xfff4f1e9); p.card2 = col (0xffe9e5da); p.cardInk = col (0xff111111); p.cardInkSoft = col (0xff5b5b5b);
        p.cardGrain = col (0xff5b5b5b); p.cardGrainAlpha = 0.22f; p.cardBorder = juce::Colours::transparentBlack;
        p.paper = col (0xfff4f1e9); p.paper2 = col (0xffcfcac0); p.paperLight = col (0xffffffff); p.ink = col (0xff111111); p.inkSoft = col (0xff5b5b5b);
        p.red = col (0xff111111); p.redDark = col (0xff000000); p.redButton = col (0xfff4f1e9); p.redButtonHover = col (0xffffffff);
        p.buttonText = col (0xff111111); p.onAccent = juce::Colours::white; p.primaryShadow = col (0xff5b5b5b);
        p.meterLit = col (0xfff4f1e9); p.meterHot = col (0xffffffff); p.stampInk = col (0xff111111);
        p.knobBody = col (0xff111111); p.knobIndicator = col (0xfff4f1e9);
        p.tape = col (0xccffffff); p.useTape = true;
        p.wallStyle = WallStyle::cover; p.cardStyle = CardStyle::sketchPage;
        p.title = { Font::permanentMarker, 1.0f }; p.display = { Font::permanentMarker, 0.9f }; p.label = { Font::caveat, 1.32f }; p.mono = { Font::spaceMono, 1.0f };
        v.push_back (p);
    }

    // 4EVERPALOOZA: festival, verde toxico y rosado (neon.css).
    {
        Palette p;
        p.id = "palooza"; p.name = "4EVERPALOOZA"; p.logoResource = "logoneon_png";
        p.wall = col (0xff050505); p.wallHi = col (0xff0d0d0d); p.headerBar = col (0xf2050505); p.headerLine = col (0xff56ff18);
        p.glow = col (0xff56ff18); p.textOnWall = col (0xfff7fff3); p.dimOnWall = col (0xff8a9a85); p.wallGrain = col (0xffffffff);
        p.card1 = col (0xff141414); p.card2 = col (0xff0d0d0d); p.cardInk = col (0xfff7fff3); p.cardInkSoft = col (0xffbfe9b0);
        p.cardGrain = col (0xffffffff); p.cardGrainAlpha = 0.04f; p.cardBorder = col (0xff56ff18);
        p.paper = col (0xfff7fff3); p.paper2 = col (0xff6b6b6b); p.paperLight = col (0xfff7fff3); p.ink = col (0xff050505); p.inkSoft = col (0xff2b2b2b);
        p.red = col (0xff56ff18); p.redDark = col (0xff3fcc0c); p.redButton = col (0xff56ff18); p.redButtonHover = col (0xff7dff4d);
        p.buttonText = col (0xff050505); p.onAccent = col (0xff050505); p.primaryShadow = col (0xffff29bd);
        p.meterLit = col (0xfff7fff3); p.meterHot = col (0xffff29bd); p.stampInk = col (0xffd4129c);
        p.knobBody = col (0xff050505); p.knobIndicator = col (0xff56ff18);
        p.tape = juce::Colours::transparentBlack; p.useTape = false;
        p.wallStyle = WallStyle::stage; p.cardStyle = CardStyle::stagePanel;
        p.title = { Font::bigShoulders, 1.25f }; p.display = { Font::bigShoulders, 1.12f }; p.label = { Font::archivo, 0.9f }; p.mono = { Font::spaceMono, 1.0f };
        v.push_back (p);
    }

    return v;
}

std::atomic<int> currentIndex { 0 };

juce::Typeface::Ptr typeface (Font f)
{
    auto load = [] (const void* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); };
    switch (f)
    {
        case Font::anton:           { static auto t = load (BinaryData::AntonRegular_ttf, BinaryData::AntonRegular_ttfSize); return t; }
        case Font::specialElite:    { static auto t = load (BinaryData::SpecialEliteRegular_ttf, BinaryData::SpecialEliteRegular_ttfSize); return t; }
        case Font::spaceMono:       { static auto t = load (BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize); return t; }
        case Font::spaceMonoBold:   { static auto t = load (BinaryData::SpaceMonoBold_ttf, BinaryData::SpaceMonoBold_ttfSize); return t; }
        case Font::sedgwick:        { static auto t = load (BinaryData::SedgwickAveDisplayRegular_ttf, BinaryData::SedgwickAveDisplayRegular_ttfSize); return t; }
        case Font::bungee:          { static auto t = load (BinaryData::BungeeRegular_ttf, BinaryData::BungeeRegular_ttfSize); return t; }
        case Font::inter:           { static auto t = load (BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize); return t; }
        case Font::pixelify:        { static auto t = load (BinaryData::PixelifySansSemiBold_ttf, BinaryData::PixelifySansSemiBold_ttfSize); return t; }
        case Font::pressStart:      { static auto t = load (BinaryData::PressStart2PRegular_ttf, BinaryData::PressStart2PRegular_ttfSize); return t; }
        case Font::permanentMarker: { static auto t = load (BinaryData::PermanentMarkerRegular_ttf, BinaryData::PermanentMarkerRegular_ttfSize); return t; }
        case Font::caveat:          { static auto t = load (BinaryData::CaveatBold_ttf, BinaryData::CaveatBold_ttfSize); return t; }
        case Font::archivo:         { static auto t = load (BinaryData::ArchivoSemiBold_ttf, BinaryData::ArchivoSemiBold_ttfSize); return t; }
        case Font::bigShoulders:    { static auto t = load (BinaryData::BigShouldersDisplayBlack_ttf, BinaryData::BigShouldersDisplayBlack_ttfSize); return t; }
    }
    return {};
}

juce::FontOptions fontFor (const FontRole& role, float height)
{
    return juce::FontOptions (typeface (role.font)).withHeight (height * role.scale);
}

std::unique_ptr<juce::PropertiesFile> openSettings()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "4EVERHUM";
    o.folderName          = "4EVERTOOLS";
    o.filenameSuffix      = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    return std::make_unique<juce::PropertiesFile> (o);
}
} // namespace

const std::vector<Palette>& all()
{
    static const auto themes = makeThemes();
    return themes;
}

const Palette& c()
{
    return all()[(std::size_t) juce::jlimit (0, (int) all().size() - 1, currentIndex.load())];
}

void setCurrent (const juce::String& id)
{
    for (std::size_t i = 0; i < all().size(); ++i)
        if (all()[i].id == id)
        {
            currentIndex.store ((int) i);
            return;
        }
    currentIndex.store (0);
}

juce::String currentId() { return c().id; }

juce::String loadSavedThemeId()
{
    return openSettings()->getValue ("theme", "sotano");
}

void saveThemeId (const juce::String& id)
{
    auto props = openSettings();
    props->setValue ("theme", id);
    props->saveIfNeeded();
}

juce::FontOptions title (float h)   { return fontFor (c().title, h); }
juce::FontOptions display (float h) { return fontFor (c().display, h); }
juce::FontOptions label (float h)   { return fontFor (c().label, h); }

juce::FontOptions mono (float h, bool bold)
{
    auto role = c().mono;
    if (bold && role.font == Font::spaceMono)
        role.font = Font::spaceMonoBold;
    return fontFor (role, h);
}

juce::Image logo()
{
    int size = 0;
    if (const auto* data = BinaryData::getNamedResource (c().logoResource.toRawUTF8(), size))
        return juce::ImageCache::getFromMemory (data, size);
    return {};
}

juce::Image grainTile (juce::Colour grain, float density, int seed)
{
    constexpr int size = 192;
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Random rng (seed);
    juce::Image::BitmapData px (img, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            // Ruido con algo de "grumo": la mayoria del grano es fino y unos pocos
            // puntos son mas oscuros, como el toner de una fotocopia.
            const float r = rng.nextFloat();
            const float a = r < density ? (0.25f + 0.75f * rng.nextFloat()) * (r < density * 0.15f ? 1.0f : 0.45f) : 0.0f;
            px.setPixelColour (x, y, grain.withMultipliedAlpha (a));
        }
    return img;
}

void drawTape (juce::Graphics& g, juce::Rectangle<float> area, float angle)
{
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::rotation (angle, area.getCentreX(), area.getCentreY()));

    // Contorno con los extremos en zigzag: cinta cortada con los dedos.
    juce::Path shape;
    const float step = 3.0f;
    shape.startNewSubPath (area.getX(), area.getY());
    shape.lineTo (area.getRight(), area.getY());
    for (float y = area.getY(); y < area.getBottom(); y += step)
        shape.lineTo (area.getRight() - ((int) ((y - area.getY()) / step) % 2 == 0 ? 2.5f : 0.0f), juce::jmin (y + step, area.getBottom()));
    shape.lineTo (area.getX(), area.getBottom());
    for (float y = area.getBottom(); y > area.getY(); y -= step)
        shape.lineTo (area.getX() + ((int) ((area.getBottom() - y) / step) % 2 == 0 ? 2.5f : 0.0f), juce::jmax (y - step, area.getY()));
    shape.closeSubPath();

    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillPath (shape, juce::AffineTransform::translation (0.0f, 1.2f));
    g.setColour (c().tape);
    g.fillPath (shape);

    // Fibras de la cinta.
    g.reduceClipRegion (shape);
    g.setColour (juce::Colours::white.withAlpha (0.13f));
    for (float x = area.getX() + 3.0f; x < area.getRight(); x += 4.0f)
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
}

void drawHardShadow (juce::Graphics& g, juce::Rectangle<float> area, juce::Point<float> offset, juce::Colour colour)
{
    g.setColour (colour);
    g.fillRect (area.translated (offset.x, offset.y));
}
} // namespace theme
