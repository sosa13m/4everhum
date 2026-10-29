#include "ui/HumLookAndFeel.h"

HumLookAndFeel::HumLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,    theme::ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, theme::red.withAlpha (0.35f));
    setColour (juce::TextEditor::textColourId,       theme::ink);
    setColour (juce::TextEditor::backgroundColourId, theme::paperLight);
    setColour (juce::TextEditor::highlightColourId,  theme::red.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId,  theme::red);
    setColour (juce::Label::textColourId,            theme::ink);
    setColour (juce::ToggleButton::textColourId,     theme::ink);
    setColour (juce::TooltipWindow::backgroundColourId, theme::paperLight);
    setColour (juce::TooltipWindow::textColourId,    theme::ink);
    setColour (juce::TooltipWindow::outlineColourId, theme::ink);
}

juce::String HumLookAndFeel::styleOf (const juce::Component& c)
{
    return c.getProperties().getWithDefault (styleKey, "paper").toString();
}

void HumLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                           bool isMouseOver, bool isButtonDown)
{
    const auto style   = styleOf (button);
    const bool enabled = button.isEnabled();
    const bool on      = button.getToggleState();

    auto face = button.getLocalBounds().toFloat().reduced (1.0f);
    face.removeFromRight (shadowSpace);
    face.removeFromBottom (shadowSpace);

    // Al presionar, el sticker "baja" y la sombra se achica (como en la web).
    const float press = (isButtonDown && enabled) ? 2.5f : 0.0f;

    juce::Colour fill, shadow;
    float shadowOffset = 0.0f;

    if (style == "primary")
    {
        fill   = ! enabled ? juce::Colour (0xff4b2b27)
               : isMouseOver ? juce::Colour (0xffa81f17) : theme::redButton;
        shadow = juce::Colours::black;
        shadowOffset = enabled ? 4.0f : 0.0f;
    }
    else if (style == "segment" && on)
    {
        fill   = theme::ink;
        shadow = theme::red;
        shadowOffset = 2.5f;
    }
    else // "paper" o "segment" sin activar
    {
        fill   = ! enabled ? theme::paper2 : (isMouseOver ? juce::Colours::white : theme::paperLight);
        shadow = theme::ink.withAlpha (0.4f);
        shadowOffset = enabled ? (style == "segment" ? 2.0f : 3.0f) : 0.0f;
    }

    if (shadowOffset > 0.0f)
        theme::drawHardShadow (g, face, { shadowOffset, shadowOffset }, shadow);

    face.translate (juce::jmin (press, shadowOffset), juce::jmin (press, shadowOffset));
    g.setColour (fill);
    g.fillRect (face);
    g.setColour (enabled ? theme::ink : theme::ink.withAlpha (0.35f));
    g.drawRect (face, 2.0f);
}

void HumLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool isButtonDown)
{
    const auto style   = styleOf (button);
    const bool enabled = button.isEnabled();
    const bool on      = button.getToggleState();

    auto area = button.getLocalBounds().toFloat().reduced (1.0f);
    area.removeFromRight (shadowSpace);
    area.removeFromBottom (shadowSpace);
    if (isButtonDown && enabled)
        area.translate (2.5f, 2.5f);

    juce::Colour colour;
    if (style == "primary")
    {
        g.setFont (theme::anton (juce::jmin (22.0f, area.getHeight() * 0.5f)));
        colour = enabled ? juce::Colours::white : juce::Colour (0xfff3e7e4).withAlpha (0.6f);
    }
    else if (style == "segment")
    {
        g.setFont (theme::mono (14.0f, true));
        colour = on ? theme::paper : theme::ink;
    }
    else
    {
        g.setFont (theme::typed (15.0f));
        colour = enabled ? theme::ink : theme::ink.withAlpha (0.4f);
    }

    g.setColour (colour);
    g.drawFittedText (style == "primary" ? button.getButtonText().toUpperCase() : button.getButtonText(),
                      area.toNearestInt().reduced (6, 0), juce::Justification::centred, 1, 0.85f);
}

void HumLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    const float angle  = startAngle + pos * (endAngle - startAngle);
    const bool hot     = slider.isMouseOverOrDragging();

    // Marcas alrededor, como en un aparato de los 80.
    g.setColour (theme::ink.withAlpha (0.55f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float len = (i % 5 == 0) ? 5.0f : 3.0f;
        g.drawLine ({ centre.getPointOnCircumference (radius, a),
                      centre.getPointOnCircumference (radius - len, a) },
                    (i % 5 == 0) ? 1.6f : 1.0f);
    }

    // Arco del valor en rojo.
    const float arcR   = radius - 9.0f;
    const float stroke = 4.0f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (theme::ink.withAlpha (0.12f));
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    if (pos > 0.0f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (slider.isEnabled() ? theme::red : theme::inkSoft);
        g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    // Cuerpo de tinta con sombra dura.
    const float bodyR = arcR - 6.0f;
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    g.setColour (hot ? theme::red : theme::ink.withAlpha (0.35f));
    g.fillEllipse (body.translated (2.5f, 2.5f));
    g.setColour (theme::ink);
    g.fillEllipse (body);

    // Indicador de papel.
    g.setColour (theme::paper);
    g.drawLine ({ centre.getPointOnCircumference (bodyR * 0.2f, angle),
                  centre.getPointOnCircumference (bodyR * 0.82f, angle) }, 3.0f);
}

juce::Label* HumLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (theme::mono (13.0f, true));
    l->setColour (juce::Label::textColourId, theme::ink);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineWhenEditingColourId, theme::red);
    l->setColour (juce::Label::backgroundWhenEditingColourId, theme::paperLight);
    return l;
}

juce::Font HumLookAndFeel::getLabelFont (juce::Label& label)
{
    // Los valores de las perillas van en Space Mono, como los numeros de un recibo.
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
        return theme::mono (14.0f, true);
    return LookAndFeel_V4::getLabelFont (label);
}

void HumLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool isMouseOver, bool)
{
    const float boxSize = 20.0f;
    auto area = button.getLocalBounds().toFloat();
    const auto box = area.removeFromLeft (boxSize + 2.0f).withSizeKeepingCentre (boxSize, boxSize);

    theme::drawHardShadow (g, box, { 2.0f, 2.0f }, theme::ink.withAlpha (0.35f));
    g.setColour (isMouseOver ? juce::Colours::white : theme::paperLight);
    g.fillRect (box);
    g.setColour (theme::ink);
    g.drawRect (box, 2.0f);

    if (button.getToggleState())
    {
        // Visto rojo hecho "a mano".
        juce::Path tick;
        tick.startNewSubPath (box.getX() + 4.0f, box.getCentreY() + 1.0f);
        tick.lineTo (box.getX() + 8.5f, box.getBottom() - 4.0f);
        tick.lineTo (box.getRight() + 2.0f, box.getY() - 3.0f);
        g.setColour (theme::red);
        g.strokePath (tick, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    area.removeFromLeft (8.0f);
    g.setColour (button.isEnabled() ? theme::ink : theme::inkSoft);
    g.setFont (theme::typed (15.0f));
    g.drawFittedText (button.getButtonText(), area.toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);
}

juce::TextLayout HumLookAndFeel::tooltipLayout (const juce::String& text)
{
    juce::AttributedString s;
    s.setJustification (juce::Justification::topLeft);
    s.append (text, theme::typed (14.0f), theme::ink);

    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, 300.0f);
    return layout;
}

juce::Rectangle<int> HumLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> screenPos,
                                                       juce::Rectangle<int> parentArea)
{
    const auto layout = tooltipLayout (text);
    const int w = (int) std::ceil (layout.getWidth()) + 22;
    const int h = (int) std::ceil (layout.getHeight()) + 18;

    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)  : screenPos.y + 6,
                                 w, h)
        .constrainedWithin (parentArea);
}

void HumLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (theme::paperLight);
    g.fillRect (r);
    g.setColour (theme::red);
    g.fillRect (r.withWidth (4.0f));
    g.setColour (theme::ink);
    g.drawRect (r, 1.5f);

    tooltipLayout (text).draw (g, r.reduced (12.0f, 9.0f).withTrimmedLeft (2.0f));
}
