#pragma once
#include "AudioEngine.h"

//==============================================================================
namespace Theme
{
    using juce::Colour;
    const Colour bg0        { 0xff0c0818 };
    const Colour bg1        { 0xff140e25 };
    const Colour panel      { 0xff1b1431 };
    const Colour panelInner { 0xff120d21 };
    const Colour row        { 0xff1e1636 };
    const Colour border     { 0xff3a2b60 };
    const Colour borderHi   { 0xff5a4590 };
    const Colour gold       { 0xffffd75e };
    const Colour goldDeep   { 0xffe9b53d };
    const Colour purple     { 0xffb784ff };
    const Colour purpleDeep { 0xff7e4fe0 };
    const Colour pink       { 0xffe58df0 };
    const Colour text       { 0xfff1ebff };
    const Colour subText    { 0xffbcb0da };
    const Colour dim        { 0xff6f6491 };
    const Colour segOff     { 0xff2a2142 };

    inline juce::String uiFontName()
    {
       #if JUCE_WINDOWS
        return "Yu Gothic UI";
       #elif JUCE_MAC
        return "Hiragino Sans";
       #else
        if (auto* f = std::getenv ("AOTO_FONT")) return f;
        return "Noto Sans CJK JP";
       #endif
    }

    inline juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (uiFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::Font titleFont (float size)
    {
       #if JUCE_WINDOWS
        return juce::Font (juce::FontOptions ("Palatino Linotype", size, juce::Font::bold));
       #else
        return juce::Font (juce::FontOptions ("Noto Serif CJK JP", size, juce::Font::bold));
       #endif
    }

    // パネル（角丸＋細い枠）
    inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 14.0f,
                           Colour fill = panel, Colour stroke = border, float strokeW = 1.5f)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r, radius);
        g.setColour (stroke);
        g.drawRoundedRectangle (r.reduced (strokeW * 0.5f), radius, strokeW);
    }

    // 発光する枠
    inline void drawGlowBorder (juce::Graphics& g, juce::Rectangle<float> r, float radius, Colour c, float width = 2.0f)
    {
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (c.withAlpha (0.06f * (float) (5 - i)));
            g.drawRoundedRectangle (r.expanded ((float) i * 1.5f), radius + (float) i, 3.0f);
        }
        g.setColour (c);
        g.drawRoundedRectangle (r, radius, width);
    }
}

//==============================================================================
// ベクターアイコン（24x24 の座標系で作ってから枠に合わせて描く）
namespace Icons
{
    enum class Id { mic, sparkle, home, plug, route, list, gear, speaker, headphones, folder, save,
                    chevron, close, minimise, maximise, plus, wave, gauge, letterS, tube, plugGeneric,
                    signal, flow, levels, broadcast, dots };

    inline juce::Path gearPath (float cx, float cy, float rOuter, float rInner, int teeth)
    {
        juce::Path p;
        const int n = teeth * 4;
        for (int i = 0; i < n; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) n - juce::MathConstants<float>::halfPi;
            const float r = (i % 4 == 0 || i % 4 == 1) ? rOuter : rInner;
            const float x = cx + r * std::cos (a), y = cy + r * std::sin (a);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        p.closeSubPath();
        return p;
    }

    inline void draw (juce::Graphics& g, Id id, juce::Rectangle<float> area, juce::Colour c, float stroke = 1.8f)
    {
        using juce::Path;
        Path s;   // 線で描く部分
        Path f;   // 塗る部分
        const float pi = juce::MathConstants<float>::pi;

        switch (id)
        {
            case Id::mic:
                s.addRoundedRectangle (8.5f, 2.5f, 7.0f, 12.0f, 3.5f);
                s.startNewSubPath (5.0f, 11.0f);
                s.addCentredArc (12.0f, 11.0f, 7.0f, 6.5f, 0.0f, -pi * 0.5f, pi * 0.5f);
                s.startNewSubPath (12.0f, 17.5f); s.lineTo (12.0f, 21.0f);
                s.startNewSubPath (8.5f, 21.5f);  s.lineTo (15.5f, 21.5f);
                for (int i = 0; i < 3; ++i) { s.startNewSubPath (10.5f, 6.0f + (float) i * 2.5f); s.lineTo (13.5f, 6.0f + (float) i * 2.5f); }
                break;

            case Id::sparkle:
                f.startNewSubPath (12, 1);
                f.quadraticTo (13, 11, 23, 12); f.quadraticTo (13, 13, 12, 23);
                f.quadraticTo (11, 13, 1, 12);  f.quadraticTo (11, 11, 12, 1);
                f.closeSubPath();
                break;

            case Id::home:
                s.startNewSubPath (2.5f, 11.5f); s.lineTo (12, 3); s.lineTo (21.5f, 11.5f);
                s.startNewSubPath (5.5f, 9.5f); s.lineTo (5.5f, 21); s.lineTo (18.5f, 21); s.lineTo (18.5f, 9.5f);
                s.startNewSubPath (10, 21); s.lineTo (10, 15); s.lineTo (14, 15); s.lineTo (14, 21);
                break;

            case Id::plug:
            {
                Path p;
                p.addRoundedRectangle (6.0f, 8.0f, 12.0f, 9.0f, 2.0f);
                p.startNewSubPath (9.5f, 3.5f); p.lineTo (9.5f, 8.0f);
                p.startNewSubPath (14.5f, 3.5f); p.lineTo (14.5f, 8.0f);
                p.startNewSubPath (12.0f, 17.0f); p.lineTo (12.0f, 22.0f);
                p.startNewSubPath (9.0f, 12.5f); p.lineTo (15.0f, 12.5f);
                p.applyTransform (juce::AffineTransform::rotation (pi * 0.25f, 12.0f, 12.0f));
                s = p;
                break;
            }

            case Id::route:
                s.addEllipse (2.5f, 15.5f, 5.0f, 5.0f);
                s.addEllipse (16.5f, 3.5f, 5.0f, 5.0f);
                s.startNewSubPath (7.5f, 18.0f);
                s.cubicTo (15.0f, 18.0f, 9.0f, 6.0f, 16.5f, 6.0f);
                s.startNewSubPath (13.0f, 19.0f); s.lineTo (19.0f, 19.0f);
                s.startNewSubPath (16.0f, 16.0f); s.lineTo (16.0f, 22.0f);
                break;

            case Id::list:
                s.addRoundedRectangle (3.0f, 3.0f, 18.0f, 18.0f, 3.0f);
                for (int i = 0; i < 3; ++i)
                {
                    const float y = 8.0f + (float) i * 4.0f;
                    f.addEllipse (6.5f, y - 1.0f, 2.0f, 2.0f);
                    s.startNewSubPath (10.5f, y); s.lineTo (17.5f, y);
                }
                break;

            case Id::gear:
                s = gearPath (12, 12, 10.5f, 8.0f, 8);
                s.addEllipse (8.5f, 8.5f, 7.0f, 7.0f);
                break;

            case Id::speaker:
                s.startNewSubPath (3.5f, 9.0f); s.lineTo (7.5f, 9.0f); s.lineTo (12.5f, 4.5f);
                s.lineTo (12.5f, 19.5f); s.lineTo (7.5f, 15.0f); s.lineTo (3.5f, 15.0f); s.closeSubPath();
                s.startNewSubPath (15.5f, 9.0f);  s.quadraticTo (17.5f, 12.0f, 15.5f, 15.0f);
                s.startNewSubPath (18.0f, 6.5f);  s.quadraticTo (22.0f, 12.0f, 18.0f, 17.5f);
                break;

            case Id::headphones:
                s.startNewSubPath (4.0f, 15.0f);
                s.addCentredArc (12.0f, 13.0f, 8.0f, 8.5f, 0.0f, -pi * 0.5f, pi * 0.5f);
                s.addRoundedRectangle (3.0f, 13.5f, 4.5f, 7.0f, 1.8f);
                s.addRoundedRectangle (16.5f, 13.5f, 4.5f, 7.0f, 1.8f);
                break;

            case Id::folder:
                s.startNewSubPath (2.5f, 6.0f); s.lineTo (9.0f, 6.0f); s.lineTo (11.0f, 8.5f);
                s.lineTo (21.5f, 8.5f); s.lineTo (21.5f, 19.5f); s.lineTo (2.5f, 19.5f); s.closeSubPath();
                s.startNewSubPath (2.5f, 11.5f); s.lineTo (21.5f, 11.5f);
                break;

            case Id::save:
                s.startNewSubPath (3.5f, 3.5f); s.lineTo (17.0f, 3.5f); s.lineTo (20.5f, 7.0f);
                s.lineTo (20.5f, 20.5f); s.lineTo (3.5f, 20.5f); s.closeSubPath();
                s.addRectangle (7.5f, 3.5f, 8.0f, 5.0f);
                s.addRoundedRectangle (7.0f, 13.0f, 10.0f, 7.5f, 1.0f);
                break;

            case Id::chevron:
                s.startNewSubPath (5.5f, 9.0f); s.lineTo (12.0f, 15.5f); s.lineTo (18.5f, 9.0f);
                break;

            case Id::close:
                s.startNewSubPath (5, 5); s.lineTo (19, 19);
                s.startNewSubPath (19, 5); s.lineTo (5, 19);
                break;

            case Id::minimise:
                s.startNewSubPath (4, 12); s.lineTo (20, 12);
                break;

            case Id::maximise:
                s.addRectangle (4.5f, 4.5f, 15.0f, 15.0f);
                break;

            case Id::plus:
                s.startNewSubPath (12, 3); s.lineTo (12, 21);
                s.startNewSubPath (3, 12); s.lineTo (21, 12);
                break;

            case Id::wave:
                s.startNewSubPath (2.5f, 12.5f); s.lineTo (6.5f, 12.5f); s.lineTo (9.0f, 5.5f);
                s.lineTo (12.5f, 19.0f); s.lineTo (15.5f, 8.0f); s.lineTo (17.5f, 12.5f); s.lineTo (21.5f, 12.5f);
                break;

            case Id::gauge:
                s.startNewSubPath (3.0f, 17.0f);
                s.addCentredArc (12.0f, 15.0f, 9.0f, 9.0f, 0.0f, -pi * 0.5f, pi * 0.5f);
                s.startNewSubPath (12.0f, 15.0f); s.lineTo (16.5f, 9.5f);
                for (int i = 0; i < 4; ++i)
                {
                    const float a = -pi * 0.5f + pi * (float) i / 3.0f;
                    s.startNewSubPath (12.0f + 6.5f * std::sin (a), 15.0f - 6.5f * std::cos (a));
                    s.lineTo (12.0f + 8.0f * std::sin (a), 15.0f - 8.0f * std::cos (a));
                }
                break;

            case Id::tube:
                s.addRoundedRectangle (7.5f, 2.5f, 9.0f, 15.0f, 4.5f);
                s.startNewSubPath (9.0f, 17.5f); s.lineTo (9.0f, 21.5f);
                s.startNewSubPath (15.0f, 17.5f); s.lineTo (15.0f, 21.5f);
                s.startNewSubPath (10.5f, 12.0f); s.lineTo (13.5f, 12.0f);
                s.startNewSubPath (12.0f, 6.0f); s.lineTo (12.0f, 12.0f);
                break;

            case Id::plugGeneric:
                s.addRoundedRectangle (4.0f, 4.0f, 16.0f, 16.0f, 3.0f);
                s.addEllipse (8.0f, 8.0f, 8.0f, 8.0f);
                s.startNewSubPath (12.0f, 8.0f); s.lineTo (12.0f, 11.0f);
                break;

            case Id::signal:
            {
                const float hs[] = { 6, 12, 18, 10, 14 };
                for (int i = 0; i < 5; ++i)
                {
                    const float x = 3.0f + (float) i * 4.5f;
                    s.startNewSubPath (x, 12.0f - hs[i] * 0.5f); s.lineTo (x, 12.0f + hs[i] * 0.5f);
                }
                break;
            }

            case Id::flow:
                s.addEllipse (9.0f, 2.5f, 6.0f, 6.0f);
                s.addEllipse (2.5f, 15.5f, 6.0f, 6.0f);
                s.addEllipse (15.5f, 15.5f, 6.0f, 6.0f);
                s.startNewSubPath (10.5f, 8.0f); s.lineTo (6.5f, 15.8f);
                s.startNewSubPath (13.5f, 8.0f); s.lineTo (17.5f, 15.8f);
                s.startNewSubPath (8.5f, 18.5f); s.lineTo (15.5f, 18.5f);
                break;

            case Id::levels:
                for (int i = 0; i < 3; ++i)
                {
                    const float x = 5.0f + (float) i * 7.0f;
                    const float y = i == 1 ? 8.0f : 14.0f;
                    s.startNewSubPath (x, 3.0f); s.lineTo (x, y - 2.5f);
                    s.startNewSubPath (x, y + 2.5f); s.lineTo (x, 21.0f);
                    s.addEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
                }
                break;

            case Id::broadcast:
                f.addEllipse (9.5f, 9.5f, 5.0f, 5.0f);
                s.startNewSubPath (7.5f, 7.5f);  s.quadraticTo (4.5f, 12.0f, 7.5f, 16.5f);
                s.startNewSubPath (16.5f, 7.5f); s.quadraticTo (19.5f, 12.0f, 16.5f, 16.5f);
                s.startNewSubPath (4.5f, 4.5f);  s.quadraticTo (-0.5f, 12.0f, 4.5f, 19.5f);
                s.startNewSubPath (19.5f, 4.5f); s.quadraticTo (24.5f, 12.0f, 19.5f, 19.5f);
                break;

            case Id::dots:
                for (int r = 0; r < 3; ++r)
                    for (int col = 0; col < 2; ++col)
                        f.addEllipse (8.0f + (float) col * 6.0f - 1.6f, 6.0f + (float) r * 6.0f - 1.6f, 3.2f, 3.2f);
                break;

            case Id::letterS:
            {
                g.setColour (c);
                g.setFont (Theme::font (area.getHeight() * 0.95f, true));
                g.drawText ("S", area, juce::Justification::centred);
                return;
            }
        }

        const auto t = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                           .getTransformToFit ({ 0, 0, 24, 24 }, area);
        g.setColour (c);
        if (! s.isEmpty())
        {
            s.applyTransform (t);
            g.strokePath (s, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        if (! f.isEmpty())
        {
            f.applyTransform (t);
            g.fillPath (f);
        }
    }

    // ロゴの王冠
    inline void drawCrown (juce::Graphics& g, juce::Rectangle<float> area)
    {
        juce::Path p;
        p.startNewSubPath (12, 66); p.lineTo (4, 22); p.lineTo (30, 42); p.lineTo (50, 6);
        p.lineTo (70, 42); p.lineTo (96, 22); p.lineTo (88, 66); p.closeSubPath();
        p.addRoundedRectangle (10.0f, 69.0f, 80.0f, 11.0f, 3.0f);

        const auto t = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                           .getTransformToFit ({ 0, 0, 100, 84 }, area);
        p.applyTransform (t);
        auto b = p.getBounds();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffe9a0), b.getX(), b.getY(),
                                                 juce::Colour (0xffd9982a), b.getRight(), b.getBottom(), false));
        g.fillPath (p);

        g.setColour (juce::Colour (0xffffe9a0));
        for (auto pt : { juce::Point<float> (4, 20), { 50, 4 }, { 96, 20 } })
        {
            auto q = pt.transformedBy (t);
            g.fillEllipse (q.x - 4.5f, q.y - 4.5f, 9.0f, 9.0f);
        }
        // 紫の宝石
        auto gem = juce::Point<float> (50, 52).transformedBy (t);
        g.setColour (Theme::purpleDeep);
        g.fillEllipse (gem.x - 6, gem.y - 6, 12, 12);
        g.setColour (Theme::purple.brighter (0.4f));
        g.fillEllipse (gem.x - 2.5f, gem.y - 4, 4, 4);
    }
}

//==============================================================================
class AotoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AotoLookAndFeel()
    {
        using namespace juce;
        setDefaultSansSerifTypefaceName (Theme::uiFontName());

        setColour (ResizableWindow::backgroundColourId, Theme::bg0);
        setColour (DocumentWindow::textColourId, Theme::text);
        setColour (Label::textColourId, Theme::text);
        setColour (ComboBox::backgroundColourId, Theme::panelInner);
        setColour (ComboBox::textColourId, Theme::text);
        setColour (ComboBox::outlineColourId, Theme::borderHi);
        setColour (ComboBox::arrowColourId, Theme::text);
        setColour (PopupMenu::backgroundColourId, Theme::panel);
        setColour (PopupMenu::textColourId, Theme::text);
        setColour (PopupMenu::highlightedBackgroundColourId, Theme::purpleDeep);
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (PopupMenu::headerTextColourId, Theme::gold);
        setColour (TextButton::buttonColourId, Theme::row);
        setColour (TextButton::buttonOnColourId, Theme::purpleDeep);
        setColour (TextButton::textColourOffId, Theme::text);
        setColour (TextButton::textColourOnId, Colours::white);
        setColour (ToggleButton::textColourId, Theme::text);
        setColour (ToggleButton::tickColourId, Theme::gold);
        setColour (ToggleButton::tickDisabledColourId, Theme::dim);
        setColour (ListBox::backgroundColourId, Theme::panelInner);
        setColour (ListBox::outlineColourId, Theme::border);
        setColour (ListBox::textColourId, Theme::text);
        setColour (TableHeaderComponent::backgroundColourId, Theme::row);
        setColour (TableHeaderComponent::textColourId, Theme::subText);
        setColour (TableHeaderComponent::outlineColourId, Theme::border);
        setColour (TableHeaderComponent::highlightColourId, Theme::purpleDeep);
        setColour (TableListBox::backgroundColourId, Theme::panelInner);
        setColour (ScrollBar::thumbColourId, Theme::borderHi);
        setColour (TextEditor::backgroundColourId, Theme::panelInner);
        setColour (TextEditor::textColourId, Theme::text);
        setColour (TextEditor::outlineColourId, Theme::borderHi);
        setColour (TextEditor::focusedOutlineColourId, Theme::purple);
        setColour (TextEditor::highlightColourId, Theme::purpleDeep.withAlpha (0.6f));
        setColour (CaretComponent::caretColourId, Theme::gold);
        setColour (AlertWindow::backgroundColourId, Theme::panel);
        setColour (AlertWindow::textColourId, Theme::text);
        setColour (AlertWindow::outlineColourId, Theme::borderHi);
        setColour (Slider::thumbColourId, Theme::gold);
        setColour (Slider::trackColourId, Theme::purple);
        setColour (Slider::backgroundColourId, Theme::segOff);
        setColour (Slider::textBoxTextColourId, Theme::text);
        setColour (Slider::textBoxBackgroundColourId, Theme::panelInner);
        setColour (Slider::textBoxOutlineColourId, Theme::border);
        setColour (ProgressBar::foregroundColourId, Theme::purple);
        setColour (ProgressBar::backgroundColourId, Theme::panelInner);
        setColour (TooltipWindow::backgroundColourId, Theme::panel);
        setColour (TooltipWindow::textColourId, Theme::text);
        setColour (TooltipWindow::outlineColourId, Theme::borderHi);

        getCurrentColourScheme().setUIColour (ColourScheme::windowBackground, Theme::panel);
        getCurrentColourScheme().setUIColour (ColourScheme::widgetBackground, Theme::panelInner);
        getCurrentColourScheme().setUIColour (ColourScheme::menuBackground, Theme::panel);
        getCurrentColourScheme().setUIColour (ColourScheme::outline, Theme::borderHi);
        getCurrentColourScheme().setUIColour (ColourScheme::defaultText, Theme::text);
        getCurrentColourScheme().setUIColour (ColourScheme::defaultFill, Theme::purpleDeep);
        getCurrentColourScheme().setUIColour (ColourScheme::highlightedText, Colours::white);
        getCurrentColourScheme().setUIColour (ColourScheme::highlightedFill, Theme::purpleDeep);
        getCurrentColourScheme().setUIColour (ColourScheme::menuText, Theme::text);
    }

    juce::Font getComboBoxFont (juce::ComboBox& b) override       { return Theme::font (juce::jmin (19.0f, (float) b.getHeight() * 0.42f)); }
    juce::Font getPopupMenuFont() override                         { return Theme::font (17.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return Theme::font (juce::jmin (17.0f, (float) h * 0.5f)); }
    juce::Font getLabelFont (juce::Label& l) override              { return Theme::font (l.getFont().getHeight()); }
    juce::Font getAlertWindowMessageFont() override                { return Theme::font (16.0f); }
    juce::Font getAlertWindowTitleFont() override                  { return Theme::font (19.0f, true); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.75f);
        if (box.getProperties().getWithDefault ("frameless", false))
        {
            const float s = juce::jmin (24.0f, (float) h * 0.55f);
            Icons::draw (g, Icons::Id::chevron, { (float) w - s - 8.0f, ((float) h - s) * 0.5f, s, s }, Theme::text, 2.2f);
            return;
        }
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (box.hasKeyboardFocus (true) ? Theme::purple : Theme::borderHi);
        g.drawRoundedRectangle (r, 8.0f, 1.3f);
        const float s = juce::jmin (22.0f, (float) h * 0.5f);
        Icons::draw (g, Icons::Id::chevron, { (float) w - s - 14.0f, ((float) h - s) * 0.5f, s, s }, Theme::text, 2.2f);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 48, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (Theme::panel);
        g.setColour (Theme::borderHi);
        g.drawRect (0, 0, w, h, 1);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto base = b.getToggleState() ? Theme::purpleDeep : Theme::row;
        if (down) base = base.brighter (0.15f); else if (over) base = base.brighter (0.08f);
        g.setColour (base);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (b.getToggleState() ? Theme::purple : Theme::borderHi);
        g.drawRoundedRectangle (r, 8.0f, 1.2f);
    }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbPos, int thumbSize, bool over, bool) override
    {
        juce::Rectangle<float> t = vertical ? juce::Rectangle<float> ((float) x + (float) w * 0.3f, (float) thumbPos, (float) w * 0.4f, (float) thumbSize)
                                            : juce::Rectangle<float> ((float) thumbPos, (float) y + (float) h * 0.3f, (float) thumbSize, (float) h * 0.4f);
        g.setColour ((over ? Theme::purple : Theme::borderHi).withAlpha (0.8f));
        g.fillRoundedRectangle (t, juce::jmin (t.getWidth(), t.getHeight()) * 0.5f);
    }

    //==== フェーダー / 横スライダー =============================================
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        const auto accent = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("accent", (juce::int64) Theme::gold.getARGB()));

        if (style == juce::Slider::LinearVertical)
        {
            const float cx = (float) x + (float) w * 0.5f;
            const float top = (float) y, bottom = (float) (y + h);
            // 溝
            g.setColour (juce::Colour (0xff0a0714));
            g.fillRoundedRectangle (cx - 4.0f, top, 8.0f, bottom - top, 4.0f);
            g.setColour (Theme::border);
            g.drawRoundedRectangle (cx - 4.0f, top, 8.0f, bottom - top, 4.0f, 1.0f);
            // 下からの塗り
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.9f), cx, pos, accent.withAlpha (0.25f), cx, bottom, false));
            g.fillRoundedRectangle (cx - 2.0f, pos, 4.0f, bottom - pos - 2.0f, 2.0f);
            // つまみ
            auto cap = juce::Rectangle<float> (cx - 21.0f, pos - 13.0f, 42.0f, 26.0f);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillRoundedRectangle (cap.translated (0, 3), 5.0f);
            g.setGradientFill (juce::ColourGradient (accent.brighter (0.35f), cap.getX(), cap.getY(),
                                                     accent.darker (0.25f), cap.getX(), cap.getBottom(), false));
            g.fillRoundedRectangle (cap, 5.0f);
            g.setColour (accent.darker (0.6f));
            g.drawLine (cap.getX() + 6, cap.getCentreY(), cap.getRight() - 6, cap.getCentreY(), 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawRoundedRectangle (cap.reduced (0.5f), 5.0f, 1.0f);
        }
        else
        {
            const float cy = (float) y + (float) h * 0.5f;
            const float left = (float) x, right = (float) (x + w);
            g.setColour (Theme::segOff);
            g.fillRoundedRectangle (left, cy - 4.0f, right - left, 8.0f, 4.0f);
            g.setGradientFill (juce::ColourGradient (Theme::pink, left, cy, Theme::purple, pos, cy, false));
            g.fillRoundedRectangle (left, cy - 4.0f, pos - left, 8.0f, 4.0f);
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (Theme::purple.withAlpha (0.12f * (float) (4 - i)));
                g.fillEllipse (pos - 11.0f - (float) i * 2, cy - 11.0f - (float) i * 2, 22.0f + (float) i * 4, 22.0f + (float) i * 4);
            }
            g.setColour (juce::Colours::white);
            g.fillEllipse (pos - 11.0f, cy - 11.0f, 22.0f, 22.0f);
        }
    }

    //==== パンノブ ================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& s) override
    {
        const auto accent = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("accent", (juce::int64) Theme::gold.getARGB()));
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f - 7.0f;
        const auto c = r.getCentre();

        // 外周の目盛り
        g.setColour (Theme::dim);
        for (int i = 0; i <= 8; ++i)
        {
            const float a = startA + (endA - startA) * (float) i / 8.0f;
            g.fillEllipse (c.x + (radius + 5.0f) * std::sin (a) - 1.3f, c.y - (radius + 5.0f) * std::cos (a) - 1.3f, 2.6f, 2.6f);
        }
        // 本体
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - radius, c.y - radius + 3, radius * 2, radius * 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3448), c.x, c.y - radius,
                                                 juce::Colour (0xff09070e), c.x, c.y + radius, false));
        g.fillEllipse (c.x - radius, c.y - radius, radius * 2, radius * 2);
        g.setColour (juce::Colour (0xff4b445c));
        g.drawEllipse (c.x - radius, c.y - radius, radius * 2, radius * 2, 1.2f);
        // 指針
        const float a = startA + pos * (endA - startA);
        g.setColour (juce::Colours::white);
        g.drawLine (c.x + radius * 0.15f * std::sin (a), c.y - radius * 0.15f * std::cos (a),
                    c.x + radius * 0.85f * std::sin (a), c.y - radius * 0.85f * std::cos (a), 3.0f);
        juce::ignoreUnused (accent);
    }

    //==== トグルスイッチ =========================================================
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down) override
    {
        if (! b.getProperties().getWithDefault ("switch", false))
        {
            LookAndFeel_V4::drawToggleButton (g, b, over, down);
            return;
        }
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        const float rad = r.getHeight() * 0.5f;
        g.setColour (on ? Theme::gold : Theme::segOff);
        g.fillRoundedRectangle (r, rad);
        if (on)
        {
            g.setColour (Theme::gold.withAlpha (0.25f));
            g.drawRoundedRectangle (r.expanded (2.0f), rad + 2.0f, 2.0f);
        }
        auto knob = juce::Rectangle<float> (r.getHeight() - 6.0f, r.getHeight() - 6.0f)
                        .withCentre ({ on ? r.getRight() - rad : r.getX() + rad, r.getCentreY() });
        g.setColour (juce::Colours::black.withAlpha (0.2f));
        g.fillEllipse (knob.translated (0, 1.5f));
        g.setColour (on ? juce::Colours::white : Theme::dim.brighter (0.4f));
        g.fillEllipse (knob);
    }
};
