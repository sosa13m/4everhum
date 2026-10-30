#pragma once

#include "ui/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

// Estilo SOTANO 4EVER para los controles de JUCE.
//
// Los botones eligen su estilo con una propiedad del componente:
//   button.getProperties().set (HumLookAndFeel::styleKey, "primary");
//   - "primary": sticker rojo con sombra dura (Grabar)
//   - "paper":   papel con borde de tinta (Guardar...)
//   - "segment": selector; activado = tinta con sombra roja (Octava)
class HumLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    static inline const juce::Identifier styleKey { "humStyle" };

    HumLookAndFeel();
    void refreshColours(); // re-aplica los colores del tema activo

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isMouseOver, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOver, bool isButtonDown) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool isMouseOver, bool isButtonDown) override;

    // Menus desplegables (Tonalidad, Escala, Cuantizar).
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;

    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

    // Margen que dejan los botones para su sombra dura.
    static constexpr float shadowSpace = 4.0f;

private:
    static juce::String styleOf (const juce::Component&);
    static juce::TextLayout tooltipLayout (const juce::String& text);
};
