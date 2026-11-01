#pragma once
#include "Theme.h"
#include "AudioEngine.h"

//==============================================================================
// アイコンボタン
class IconButton : public juce::Button
{
public:
    enum Style { stripToggle, headerBox, bare };

    IconButton (Icons::Id i, Style st, juce::Colour acc = Theme::gold)
        : juce::Button ({}), icon (i), style (st), accent (acc) {}

    void setJoined (bool left, bool right) { joinLeft = left; joinRight = right; }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();

        if (style == stripToggle)
        {
            if (on)
            {
                g.setColour (accent.withAlpha (0.25f));
                g.fillRoundedRectangle (r.expanded (2), 9.0f);
                g.setGradientFill (juce::ColourGradient (accent.brighter (0.2f), 0, r.getY(), accent.darker (0.1f), 0, r.getBottom(), false));
                g.fillRoundedRectangle (r, 8.0f);
            }
            else
            {
                g.setColour (over ? Theme::row.brighter (0.1f) : Theme::panelInner);
                g.fillRoundedRectangle (r, 8.0f);
                g.setColour (Theme::borderHi);
                g.drawRoundedRectangle (r, 8.0f, 1.2f);
            }
            const float s = r.getHeight() * 0.62f;
            Icons::draw (g, icon, r.withSizeKeepingCentre (s, s), on ? juce::Colour (0xff1a1230) : Theme::subText, 2.0f);
        }
        else if (style == headerBox)
        {
            juce::Path p;
            p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 10.0f, 10.0f,
                                   ! joinLeft, ! joinRight, ! joinLeft, ! joinRight);
            g.setColour (down ? Theme::row.brighter (0.15f) : over ? Theme::row.brighter (0.07f) : Theme::panel);
            g.fillPath (p);
            g.setColour (Theme::borderHi);
            g.strokePath (p, juce::PathStrokeType (1.5f));
            const float s = r.getHeight() * 0.55f;
            Icons::draw (g, icon, r.withSizeKeepingCentre (s, s), Theme::text, 2.0f);
        }
        else
        {
            const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.8f;
            Icons::draw (g, icon, r.withSizeKeepingCentre (s, s),
                         over ? Theme::text : Theme::text.withAlpha (0.82f), strokeWidth);
        }
    }

    float strokeWidth = 2.0f;

private:
    Icons::Id icon;
    Style style;
    juce::Colour accent;
    bool joinLeft = false, joinRight = false;
};

//==============================================================================
// 縦メーター（L/R 2 列・セグメント）
class VMeter : public juce::Component
{
public:
    explicit VMeter (juce::Colour acc, juce::Colour top) : accent (acc), topColour (top) {}

    void setLevels (float l, float r)
    {
        levelL = juce::jmax (l, levelL * 0.86f);
        levelR = juce::jmax (r, levelR * 0.86f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const int n = 16;
        const float gap = 4.0f;
        const float colW = ((float) getWidth() - 6.0f) * 0.5f;
        const float segH = ((float) getHeight() - gap * (float) (n - 1)) / (float) n;

        auto toNorm = [] (float v) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -54.0f) + 54.0f) / 54.0f); };
        const float lv[2] = { toNorm (levelL), toNorm (levelR) };

        for (int c = 0; c < 2; ++c)
        {
            const float x = (float) c * (colW + 6.0f);
            for (int i = 0; i < n; ++i)
            {
                const float y = (float) getHeight() - (float) (i + 1) * segH - (float) i * gap;
                const bool lit = lv[c] > ((float) i + 0.5f) / (float) n;
                auto col = i >= n - 3 ? topColour : accent.interpolatedWith (topColour, (float) i / (float) (n * 2));
                g.setColour (lit ? col : Theme::segOff);
                g.fillRect (x, y, colW, segH);
            }
        }
    }

private:
    juce::Colour accent, topColour;
    float levelL = 0, levelR = 0;
};

//==============================================================================
// 横メーター（出力レベル）
class HMeter : public juce::Component
{
public:
    void setLevel (float v) { level = juce::jmax (v, level * 0.86f); repaint(); }

    // 画像のスケールに合わせた dB → 位置（-60..-12 を 70%、-12..0 を 30%）
    static float dbToNorm (float db)
    {
        db = juce::jlimit (-60.0f, 0.0f, db);
        return db < -12.0f ? (db + 60.0f) / 48.0f * 0.711f : 0.711f + (db + 12.0f) / 12.0f * 0.289f;
    }

    void paint (juce::Graphics& g) override
    {
        const int n = 46;
        const float gap = 3.0f;
        const float segW = ((float) getWidth() - gap * (float) (n - 1)) / (float) n;
        const float norm = dbToNorm (juce::Decibels::gainToDecibels (level, -100.0f));

        for (int i = 0; i < n; ++i)
        {
            const float x = (float) i * (segW + gap);
            const float t = (float) i / (float) (n - 1);
            const bool lit = norm > ((float) i + 0.5f) / (float) n;
            auto col = t < 0.45f ? Theme::gold : t < 0.65f ? Theme::gold.interpolatedWith (Theme::pink, (t - 0.45f) / 0.2f)
                                                           : Theme::pink.interpolatedWith (Theme::purple, juce::jmin (1.0f, (t - 0.65f) / 0.2f));
            g.setColour (lit ? col : Theme::segOff);
            g.fillRoundedRectangle (x, 0, segW, (float) getHeight(), 1.5f);
        }
    }

private:
    float level = 0;
};

//==============================================================================
// フェーダー（dB）
class Fader : public juce::Slider
{
public:
    explicit Fader (juce::Colour accent)
        : juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox)
    {
        // 画像の位置感に合わせた区分線形カーブ（-12dB ≒ 47%、0dB ≒ 67%）
        juce::NormalisableRange<double> r (-60.0, 6.0,
            [] (double, double, double p)
            {
                if (p < 0.47) return -60.0 + p / 0.47 * 48.0;
                if (p < 0.67) return -12.0 + (p - 0.47) / 0.20 * 12.0;
                return (p - 0.67) / 0.33 * 6.0;
            },
            [] (double, double, double v)
            {
                if (v < -12.0) return (v + 60.0) / 48.0 * 0.47;
                if (v < 0.0)   return 0.47 + (v + 12.0) / 12.0 * 0.20;
                return 0.67 + v / 6.0 * 0.33;
            });
        r.interval = 0.0;
        setNormalisableRange (r);
        setDoubleClickReturnValue (true, 0.0);
        getProperties().set ("accent", (juce::int64) accent.getARGB());
        setMouseDragSensitivity (300);
        setVelocityBasedMode (false);
    }
};

class PanKnob : public juce::Slider
{
public:
    explicit PanKnob (juce::Colour accent)
        : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox)
    {
        setRange (-1.0, 1.0, 0.01);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setDoubleClickReturnValue (true, 0.0);
        getProperties().set ("accent", (juce::int64) accent.getARGB());
        setMouseDragSensitivity (160);
    }
};

inline juce::String formatDb (float db)
{
    if (db <= -60.0f) return juce::String (juce::CharPointer_UTF8 ("-\xe2\x88\x9e dB"));
    return juce::String (db, 1) + " dB";
}

//==============================================================================
// プラグイン 1 行
class SlotRow : public juce::Component
{
public:
    SlotRow()
    {
        enable.getProperties().set ("switch", true);
        enable.onClick = [this] { if (onToggle) onToggle (enable.getToggleState()); };
        addAndMakeVisible (enable);

        edit.onClick = [this] { if (onEdit) onEdit(); };
        remove.onClick = [this] { if (onRemove) onRemove(); };
        edit.strokeWidth = 2.2f;
        remove.strokeWidth = 2.2f;
        edit.setTooltip ("プラグインの画面を開く"_j);
        remove.setTooltip ("このプラグインを外す"_j);
        addAndMakeVisible (edit);
        addAndMakeVisible (remove);
    }

    void set (const juce::String& n, const juce::String& category, bool enabled)
    {
        name = n;
        icon = iconFor (n + " " + category);
        enable.setToggleState (enabled, juce::dontSendNotification);
        repaint();
    }

    static Icons::Id iconFor (const juce::String& s)
    {
        auto t = s.toLowerCase();
        if (t.contains ("esser") || t.contains ("de-ess") || t.contains ("sibil")) return Icons::Id::letterS;
        if (t.contains ("eq"))                                      return Icons::Id::wave;
        if (t.contains ("comp") || t.contains ("limit") || t.contains ("dynamic") || t.contains ("gate")) return Icons::Id::gauge;
        if (t.contains ("satur") || t.contains ("tube") || t.contains ("tape") || t.contains ("dist")) return Icons::Id::tube;
        if (t.contains ("reverb") || t.contains ("verb") || t.contains ("echo") || t.contains ("delay")) return Icons::Id::sparkle;
        return Icons::Id::plugGeneric;
    }

    std::function<void (bool)> onToggle;
    std::function<void()> onSelect, onEdit, onRemove;
    std::function<void (int)> onDragging;
    std::function<void (int)> onDragEnd;

    void resized() override
    {
        enable.setBounds (52, 18, 56, 32);
        selector = { 128, 9, 308, 49 };
        edit.setBounds (458, 15, 38, 38);
        remove.setBounds (509, 17, 34, 34);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.75f);
        g.setColour (dragging ? Theme::row.brighter (0.08f) : Theme::row);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (dragging ? Theme::purple : Theme::border);
        g.drawRoundedRectangle (r, 8.0f, 1.2f);

        Icons::draw (g, Icons::Id::dots, { 12.0f, 21.0f, 24.0f, 24.0f }, Theme::subText);

        // セレクター
        auto s = selector.toFloat();
        g.setColour (Theme::panelInner);
        g.fillRoundedRectangle (s, 8.0f);
        g.setColour (overSelector ? Theme::purple : Theme::borderHi);
        g.drawRoundedRectangle (s, 8.0f, 1.2f);

        auto iconBox = juce::Rectangle<float> (s.getX() + 9.0f, s.getY() + 7.0f, 36.0f, 36.0f);
        g.setColour (Theme::row.brighter (0.06f));
        g.fillRoundedRectangle (iconBox, 6.0f);
        Icons::draw (g, icon, iconBox.reduced (5.0f), Theme::purple.brighter (0.2f), 1.8f);

        g.setColour (enable.getToggleState() ? Theme::text : Theme::dim);
        g.setFont (Theme::font (20.0f));
        g.drawFittedText (name, juce::Rectangle<int> ((int) s.getX() + 56, (int) s.getY(), (int) s.getWidth() - 100, (int) s.getHeight()),
                          juce::Justification::centredLeft, 1);
        Icons::draw (g, Icons::Id::chevron, { s.getRight() - 40.0f, s.getCentreY() - 11.0f, 22.0f, 22.0f }, Theme::text, 2.2f);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool o = selector.contains (e.getPosition());
        if (o != overSelector) { overSelector = o; repaint(); }
        setMouseCursor (e.x < 44 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { overSelector = false; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < 44) { dragging = true; dragStartY = getY(); repaint(); }
        else if (selector.contains (e.getPosition()) && onSelect) onSelect();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        setTopLeftPosition (getX(), dragStartY + e.getDistanceFromDragStartY());
        toFront (false);
        if (onDragging) onDragging (e.getDistanceFromDragStartY());
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        dragging = false;
        repaint();
        if (onDragEnd) onDragEnd (e.getDistanceFromDragStartY());
    }

private:
    juce::ToggleButton enable;
    IconButton edit { Icons::Id::gear, IconButton::bare }, remove { Icons::Id::close, IconButton::bare };
    juce::String name;
    Icons::Id icon = Icons::Id::plugGeneric;
    juce::Rectangle<int> selector;
    bool overSelector = false, dragging = false;
    int dragStartY = 0;
};

//==============================================================================
// 「＋ VSTプラグインを追加」
class AddPluginButton : public juce::Button
{
public:
    AddPluginButton() : juce::Button ("add") {}
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        if (over) { g.setColour (Theme::purple.withAlpha (0.06f)); g.fillRoundedRectangle (r, 8.0f); }
        juce::Path p; p.addRoundedRectangle (r, 8.0f);
        juce::Path dashed;
        const float dash[] = { 7.0f, 5.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, p, dash, 2);
        g.setColour (over ? Theme::purple : Theme::dim);
        g.fillPath (dashed);

        g.setFont (Theme::font (20.0f));
        const juce::String t = "VSTプラグインを追加"_j;
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t);
        const float total = 28.0f + 18.0f + tw;
        const float x0 = r.getCentreX() - total * 0.5f;
        Icons::draw (g, Icons::Id::plus, { x0, r.getCentreY() - 14.0f, 28.0f, 28.0f }, Theme::text, 2.0f);
        g.setColour (Theme::text);
        g.drawText (t, juce::Rectangle<float> (x0 + 46.0f, r.getY(), tw + 4.0f, r.getHeight()), juce::Justification::centredLeft);
    }
};

//==============================================================================
// チャンネルストリップ（Main / FX）
class ChannelStripComp : public juce::Component
{
public:
    ChannelStripComp (AudioEngine& e, int index)
        : engine (e), idx (index),
          accent (index == 0 ? Theme::gold : Theme::purple),
          fader (accent), pan (accent),
          meter (index == 0 ? Theme::gold : Theme::purple, index == 0 ? juce::Colour (0xfff2a07a) : Theme::pink),
          toOut (Icons::Id::speaker, IconButton::stripToggle, accent),
          toMon (Icons::Id::headphones, IconButton::stripToggle, accent)
    {
        auto& ch = engine.channel (idx);
        addAndMakeVisible (fader);
        addAndMakeVisible (meter);
        addAndMakeVisible (pan);
        addAndMakeVisible (dbLabel);
        addAndMakeVisible (toOut);
        addAndMakeVisible (toMon);

        fader.onValueChange = [this, &ch] { ch.gainDb = (float) fader.getValue(); dbLabel.setText (formatDb ((float) fader.getValue()), juce::dontSendNotification); };
        pan.onValueChange   = [this, &ch] { ch.pan = (float) pan.getValue(); };

        dbLabel.setJustificationType (juce::Justification::centred);
        dbLabel.setFont (Theme::font (18.0f));
        dbLabel.setEditable (false, true);
        dbLabel.setColour (juce::Label::backgroundColourId, Theme::panelInner);
        dbLabel.setColour (juce::Label::outlineColourId, Theme::border);
        dbLabel.onTextChange = [this]
        {
            fader.setValue (juce::jlimit (-60.0, 6.0, dbLabel.getText().retainCharacters ("-0123456789.").getDoubleValue()));
            dbLabel.setText (formatDb ((float) fader.getValue()), juce::dontSendNotification);
        };

        toOut.setClickingTogglesState (true);
        toMon.setClickingTogglesState (true);
        toOut.setTooltip ("配信出力に送る"_j);
        toMon.setTooltip ("モニター（ヘッドホン）に送る"_j);
        toOut.onClick = [this, &ch] { ch.toOutput = toOut.getToggleState(); };
        toMon.onClick = [this, &ch] { ch.toMonitor = toMon.getToggleState(); };

        pan.setTooltip ("パン（ダブルクリックでセンター）"_j);
        fader.setTooltip ("ダブルクリックで 0 dB"_j);
        syncFromEngine();
    }

    void syncFromEngine()
    {
        auto& ch = engine.channel (idx);
        fader.setValue (ch.gainDb.load(), juce::dontSendNotification);
        pan.setValue (ch.pan.load(), juce::dontSendNotification);
        toOut.setToggleState (ch.toOutput.load(), juce::dontSendNotification);
        toMon.setToggleState (ch.toMonitor.load(), juce::dontSendNotification);
        dbLabel.setText (formatDb (ch.gainDb.load()), juce::dontSendNotification);
    }

    void tick()
    {
        auto& ch = engine.channel (idx);
        meter.setLevels (ch.peakL.exchange (0.0f), ch.peakR.exchange (0.0f));
    }

    void resized() override
    {
        fader.setBounds (49, 120, 56, 254);
        meter.setBounds (120, 162, 34, 190);
        dbLabel.setBounds (52, 372, 80, 28);
        toOut.setBounds (19, 418, 65, 42);
        toMon.setBounds (99, 418, 65, 42);
        pan.setBounds (51, 469, 80, 80);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.10f), 0, 0, Theme::panel, 0, (float) getHeight() * 0.6f, false));
        g.fillRoundedRectangle (r, 14.0f);
        Theme::drawGlowBorder (g, r, 14.0f, accent.withAlpha (0.9f), 2.0f);

        // タイトル
        const juce::String title = idx == 0 ? "Main" : "FX";
        g.setFont (Theme::font (31.0f, true));
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), title);
        const float total = 30.0f + 12.0f + tw;
        const float x0 = (float) getWidth() * 0.5f - total * 0.5f;
        Icons::draw (g, idx == 0 ? Icons::Id::mic : Icons::Id::sparkle, { x0, 20.0f, 30.0f, 30.0f },
                     idx == 0 ? Theme::text : Theme::purple.brighter (0.3f), 2.0f);
        if (idx == 1)
            Icons::draw (g, Icons::Id::sparkle, { x0 + 20.0f, 14.0f, 13.0f, 13.0f }, Theme::purple.brighter (0.3f));
        g.setColour (Theme::text);
        g.drawText (title, juce::Rectangle<float> (x0 + 42.0f, 14.0f, tw + 4.0f, 42.0f), juce::Justification::centredLeft);

        g.setColour (Theme::subText);
        g.setFont (Theme::font (15.0f));
        g.drawText (idx == 0 ? "メインチャンネル"_j : "FXチャンネル"_j, 0, 61, getWidth(), 22, juce::Justification::centred);
        g.drawText (idx == 0 ? "(声・配信出力)"_j : "(エコー・リバーブ等)"_j, 0, 81, getWidth(), 22, juce::Justification::centred);

        g.setColour (Theme::border);
        g.drawLine (16.0f, 114.0f, (float) getWidth() - 16.0f, 114.0f, 1.0f);

        // パンの L C R
        g.setColour (Theme::text);
        g.setFont (Theme::font (16.0f));
        g.drawText ("L", 40, 530, 20, 20, juce::Justification::centred);
        g.drawText ("C", 81, 540, 20, 20, juce::Justification::centred);
        g.drawText ("R", 123, 530, 20, 20, juce::Justification::centred);
    }

private:
    AudioEngine& engine;
    int idx;
    juce::Colour accent;
    Fader fader;
    PanKnob pan;
    VMeter meter;
    juce::Label dbLabel;
    IconButton toOut, toMon;
};
