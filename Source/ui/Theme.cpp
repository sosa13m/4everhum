#include "ui/Theme.h"

#include "BinaryData.h"

namespace theme
{
namespace
{
juce::Typeface::Ptr load (const void* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}
} // namespace

juce::FontOptions anton (float height)
{
    static auto tf = load (BinaryData::AntonRegular_ttf, BinaryData::AntonRegular_ttfSize);
    return juce::FontOptions (tf).withHeight (height);
}

juce::FontOptions typed (float height)
{
    static auto tf = load (BinaryData::SpecialEliteRegular_ttf, BinaryData::SpecialEliteRegular_ttfSize);
    return juce::FontOptions (tf).withHeight (height);
}

juce::FontOptions mono (float height, bool bold)
{
    static auto regular = load (BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize);
    static auto heavy   = load (BinaryData::SpaceMonoBold_ttf,    BinaryData::SpaceMonoBold_ttfSize);
    return juce::FontOptions (bold ? heavy : regular).withHeight (height);
}

juce::Image logo()
{
    static auto img = juce::ImageCache::getFromMemory (BinaryData::logo4ever_png, BinaryData::logo4ever_pngSize);
    return img;
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
    g.setColour (tape);
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
