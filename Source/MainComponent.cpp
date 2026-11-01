#include "MainComponent.h"
#include "Pages.h"

using namespace juce;

bool gDemoMode = false;

// 画像上の絶対座標 → ページ内座標（ページ原点 = 210, 92）
static Rectangle<int> R (int x1, int y1, int x2, int y2) { return { x1 - 210, y1 - 92, x2 - x1, y2 - y1 }; }

// 見出し＋下側が一段暗いパネル
static void drawSection (Graphics& g, Rectangle<float> b, Icons::Id icon, const String& title,
                         float titleCY, float innerTop, Colour iconCol = Theme::purple.brighter (0.25f))
{
    Theme::drawPanel (g, b.reduced (1.0f), 14.0f);
    auto inner = b.reduced (2.5f).withTrimmedTop (innerTop - 2.5f);
    Path p;
    p.addRoundedRectangle (inner.getX(), inner.getY(), inner.getWidth(), inner.getHeight(), 12.0f, 12.0f, false, false, true, true);
    g.setColour (Theme::panelInner.withAlpha (0.75f));
    g.fillPath (p);
    Icons::draw (g, icon, { b.getX() + 20.0f, titleCY - 14.0f, 28.0f, 28.0f }, iconCol, 2.2f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (22.0f, true));
    g.drawText (title, Rectangle<float> (b.getX() + 58.0f, titleCY - 18.0f, b.getWidth() - 70.0f, 36.0f), Justification::centredLeft);
}

static void drawArrow (Graphics& g, float x1, float x2, float y, Colour c)
{
    g.setColour (c);
    g.drawLine (x1, y, x2 - 2.0f, y, 2.6f);
    Path head;
    head.startNewSubPath (x2 - 12.0f, y - 9.0f);
    head.lineTo (x2, y);
    head.lineTo (x2 - 12.0f, y + 9.0f);
    g.strokePath (head, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

//==============================================================================
class TabButton : public Button
{
public:
    TabButton (const String& t, Colour acc) : Button (t), accent (acc) { setClickingTogglesState (true); }

    void paintButton (Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f, 1.5f).withTrimmedBottom (6.0f);
        const bool on = getToggleState();
        if (on)
        {
            g.setColour (accent.withAlpha (0.25f));
            g.fillRoundedRectangle (r.expanded (2.0f), 10.0f);
            g.setGradientFill (ColourGradient (accent.brighter (0.15f), 0, r.getY(), accent.darker (0.15f), 0, r.getBottom(), false));
            g.fillRoundedRectangle (r, 8.0f);
            Path notch;
            notch.addTriangle (r.getCentreX() - 9.0f, r.getBottom() - 0.5f, r.getCentreX() + 9.0f, r.getBottom() - 0.5f, r.getCentreX(), r.getBottom() + 7.0f);
            g.fillPath (notch);
        }
        else
        {
            g.setColour (over ? Theme::row.brighter (0.08f) : Theme::row);
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (accent.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 8.0f, 1.6f);
        }
        const bool darkText = on && accent == Theme::gold;
        g.setColour (darkText ? Colour (0xff1a1230) : Theme::text);
        g.setFont (Theme::font (20.0f, true));
        g.drawText (getButtonText(), r, Justification::centred);
    }

private:
    Colour accent;
};

//==============================================================================
class PluginPanel : public Component
{
public:
    PluginPanel (AudioEngine& e) : engine (e)
    {
        mainTab.setRadioGroupId (11);
        fxTab.setRadioGroupId (11);
        mainTab.setToggleState (true, dontSendNotification);
        mainTab.onClick = [this] { if (mainTab.getToggleState()) { current = 0; rebuild(); } };
        fxTab.onClick   = [this] { if (fxTab.getToggleState())   { current = 1; rebuild(); } };
        addAndMakeVisible (mainTab);
        addAndMakeVisible (fxTab);

        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (10);
        addAndMakeVisible (viewport);

        addButton.onClick = [this]
        {
            showPluginMenu (engine, &addButton, {}, [this] (const PluginDescription& d)
            {
                const auto err = engine.addPlugin (current, d);
                if (err.isNotEmpty()) showWarning ("プラグインを読み込めませんでした"_j, err);
            }, goToPlugins);
        };
        content.addAndMakeVisible (addButton);
    }

    std::function<void()> goToPlugins;

    void rebuild()
    {
        rows.clear();
        auto& slots = engine.channel (current).slots;

        auto addRow = [this] (const String& name, const String& cat, bool en)
        {
            auto* row = rows.add (new SlotRow());
            row->set (name, cat, en);
            content.addAndMakeVisible (row);
            return row;
        };

        if (gDemoMode && slots.empty())
        {
            const char* demo[2][4] = { { "EQ", "Compressor", "DeEsser", "Saturation" }, { "Echo", "Reverb", nullptr, nullptr } };
            for (auto* n : demo[current]) if (n != nullptr) addRow (n, {}, true);
        }

        for (int i = 0; i < (int) slots.size(); ++i)
        {
            auto& s = *slots[(size_t) i];
            auto* row = addRow (s.description.name, s.description.category, s.enabled.load());
            const int ch = current;
            row->onToggle = [this, ch, i] (bool on) { engine.setPluginEnabled (ch, i, on); };
            row->onEdit   = [this, ch, i] { engine.showEditor (ch, i); };
            row->onRemove = [this, ch, i] { MessageManager::callAsync ([this, ch, i] { engine.removePlugin (ch, i); }); };
            row->onSelect = [this, ch, i, row]
            {
                const auto ticked = engine.channel (ch).slots[(size_t) i]->description.createIdentifierString();
                showPluginMenu (engine, row, ticked, [this, ch, i] (const PluginDescription& d)
                {
                    const auto err = engine.replacePlugin (ch, i, d);
                    if (err.isNotEmpty()) showWarning ("プラグインを読み込めませんでした"_j, err);
                }, goToPlugins);
            };
            row->onDragEnd = [this, ch, i] (int dy)
            {
                const int to = i + roundToInt ((float) dy / (float) rowStep);
                MessageManager::callAsync ([this, ch, i, to] { engine.movePlugin (ch, i, to); rebuild(); });
            };
        }
        layoutRows();
    }

    void layoutRows()
    {
        const int w = viewport.getWidth() - (rows.size() >= 5 ? 14 : 0);
        for (int i = 0; i < rows.size(); ++i)
            rows[i]->setBounds (0, i * rowStep, w, rowH);
        addButton.setBounds (0, rows.size() * rowStep + 3, w, 87);
        content.setSize (w, addButton.getBottom() + 4);
    }

    void resized() override
    {
        mainTab.setBounds (18, 16, 278, 56);
        fxTab.setBounds (315, 16, 254, 56);
        viewport.setBounds (19, 93, 551, 452);
        layoutRows();
    }

    void paint (Graphics& g) override
    {
        Theme::drawPanel (g, getLocalBounds().toFloat().reduced (1.0f), 14.0f);
    }

private:
    AudioEngine& engine;
    int current = 0;
    static constexpr int rowH = 67, rowStep = 81;
    TabButton mainTab { "Main チャンネルのVST"_j, Theme::gold }, fxTab { "FX チャンネルのVST"_j, Theme::purple };
    Viewport viewport;
    Component content;
    OwnedArray<SlotRow> rows;
    AddPluginButton addButton;
};

//==============================================================================
class SegButton : public Button
{
public:
    SegButton (const String& t, Icons::Id i) : Button (t), icon (i) { setClickingTogglesState (true); }
    void paintButton (Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        const bool on = getToggleState();
        if (on)
        {
            g.setColour (Theme::purple.withAlpha (0.3f));
            g.fillRoundedRectangle (r.expanded (2.0f), 9.0f);
            g.setGradientFill (ColourGradient (Theme::purpleDeep, r.getX(), 0, Theme::purple.withAlpha (0.85f), r.getRight(), 0, false));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (Theme::purple.brighter (0.4f));
            g.drawRoundedRectangle (r, 7.0f, 1.3f);
        }
        else if (over)
        {
            g.setColour (Theme::row);
            g.fillRoundedRectangle (r, 7.0f);
        }
        g.setFont (Theme::font (20.0f, on));
        const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), getButtonText());
        const float total = 26.0f + 10.0f + tw;
        const float x0 = r.getCentreX() - total * 0.5f;
        Icons::draw (g, icon, { x0, r.getCentreY() - 13.0f, 26.0f, 26.0f }, on ? Colours::white : Theme::subText, 2.0f);
        g.setColour (on ? Colours::white : Theme::subText);
        g.drawText (getButtonText(), Rectangle<float> (x0 + 36.0f, r.getY(), tw + 4.0f, r.getHeight()), Justification::centredLeft);
    }
private:
    Icons::Id icon;
};

//==============================================================================
class OutputPanel : public Component
{
public:
    OutputPanel (AudioEngine& e) : engine (e)
    {
        addAndMakeVisible (combo);
        combo.onChange = [this]
        {
            const int id = combo.getSelectedId();
            if (id == 9999) { refresh(); if (goToSettings) goToSettings(); return; }
            if (id > 0) { engine.outputPair = id - 1; engine.saveSettings(); }
        };
        stereo.setRadioGroupId (21);
        mono.setRadioGroupId (21);
        stereo.onClick = [this] { engine.monoOutput = ! stereo.getToggleState(); };
        mono.onClick   = [this] { engine.monoOutput = mono.getToggleState(); };
        addAndMakeVisible (stereo);
        addAndMakeVisible (mono);
        refresh();
    }

    std::function<void()> goToSettings;

    void refresh()
    {
        fillPairCombo (combo, engine, false);
        stereo.setToggleState (! engine.monoOutput.load(), dontSendNotification);
        mono.setToggleState (engine.monoOutput.load(), dontSendNotification);
    }

    void resized() override
    {
        combo.setBounds (24, 112, 354, 49);
        stereo.setBounds (25, 219, 176, 44);
        mono.setBounds (201, 219, 176, 44);
    }

    void paint (Graphics& g) override
    {
        drawSection (g, getLocalBounds().toFloat(), Icons::Id::signal, "出力（ルーティング）"_j, 31.0f, 63.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::font (18.0f));
        g.drawText ("出力デバイス"_j, 24, 82, 300, 26, Justification::centredLeft);
        g.drawText ("出力モード"_j, 24, 182, 300, 26, Justification::centredLeft);
        g.setColour (Theme::panelInner);
        g.fillRoundedRectangle (24.0f, 218.0f, 354.0f, 46.0f, 9.0f);
        g.setColour (Theme::borderHi);
        g.drawRoundedRectangle (24.0f, 218.0f, 354.0f, 46.0f, 9.0f, 1.2f);
    }

private:
    AudioEngine& engine;
    ComboBox combo;
    SegButton stereo { "ステレオ"_j, Icons::Id::headphones }, mono { "モノラル"_j, Icons::Id::plugGeneric };
};

//==============================================================================
class MonitorPanel : public Component
{
public:
    MonitorPanel (AudioEngine& e) : engine (e)
    {
        addAndMakeVisible (combo);
        combo.onChange = [this]
        {
            const int id = combo.getSelectedId();
            if (id == 9999) { refresh(); if (goToSettings) goToSettings(); return; }
            if (id == 1000) engine.monitorPair = -1;
            else if (id > 0) engine.monitorPair = id - 1;
            engine.saveSettings();
        };

        slider.setSliderStyle (Slider::LinearHorizontal);
        slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        NormalisableRange<double> r (-60.0, 6.0);
        r.setSkewForCentre (-20.0);
        slider.setNormalisableRange (r);
        slider.setDoubleClickReturnValue (true, 0.0);
        slider.onValueChange = [this]
        {
            engine.monitorGainDb = (float) slider.getValue();
            value.setText (formatDb ((float) slider.getValue()), dontSendNotification);
        };
        addAndMakeVisible (slider);

        value.setJustificationType (Justification::centred);
        value.setFont (Theme::font (18.0f));
        value.setColour (Label::backgroundColourId, Theme::panelInner);
        value.setColour (Label::outlineColourId, Theme::borderHi);
        value.setEditable (false, true);
        value.onTextChange = [this] { slider.setValue (jlimit (-60.0, 6.0, value.getText().retainCharacters ("-0123456789.").getDoubleValue())); };
        addAndMakeVisible (value);
        refresh();
    }

    std::function<void()> goToSettings;

    void refresh()
    {
        fillPairCombo (combo, engine, true);
        slider.setValue (engine.monitorGainDb.load(), dontSendNotification);
        value.setText (formatDb (engine.monitorGainDb.load()), dontSendNotification);
    }

    void resized() override
    {
        combo.setBounds (26, 95, 352, 49);
        slider.setBounds (16, 186, 276, 32);
        value.setBounds (298, 180, 81, 39);
    }

    void paint (Graphics& g) override
    {
        drawSection (g, getLocalBounds().toFloat(), Icons::Id::headphones, "モニター"_j, 27.0f, 52.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::font (18.0f));
        g.drawText ("モニター出力"_j, 26, 66, 300, 26, Justification::centredLeft);
        g.drawText ("モニター音量"_j, 26, 156, 300, 26, Justification::centredLeft);
    }

private:
    AudioEngine& engine;
    ComboBox combo;
    Slider slider;
    Label value;
};

//==============================================================================
class SignalFlowPanel : public Component
{
public:
    explicit SignalFlowPanel (AudioEngine& e) : engine (e) {}

    void paint (Graphics& g) override
    {
        drawSection (g, getLocalBounds().toFloat(), Icons::Id::flow, "シグナルフロー"_j, 23.0f, 47.0f);

        auto caption = [&] (const String& a, const String& b, float cx, float y1, float y2)
        {
            g.setColour (Theme::text);
            g.setFont (Theme::font (18.0f));
            g.drawText (a, Rectangle<float> (cx - 100, y1 - 12, 200, 24), Justification::centred);
            g.setColour (Theme::subText);
            g.setFont (Theme::font (15.0f));
            g.drawText (b, Rectangle<float> (cx - 120, y2 - 11, 240, 22), Justification::centred);
        };

        Icons::draw (g, Icons::Id::mic, { 62.0f, 70.0f, 40.0f, 40.0f }, Theme::text, 1.6f);
        caption ("入力"_j, "(マイク)"_j, 82.0f, 132.0f, 155.0f);
        drawArrow (g, 147.0f, 190.0f, 103.0f, Theme::text);

        const int nMain = (int) engine.mainCh.slots.size();
        auto box = [&] (Rectangle<float> r, Colour c, const String& t1, const String& t2)
        {
            g.setColour (c.withAlpha (0.08f));
            g.fillRoundedRectangle (r, 9.0f);
            Theme::drawGlowBorder (g, r, 9.0f, c, 2.0f);
            g.setColour (Theme::text);
            g.setFont (Theme::font (20.0f));
            g.drawText (t1, r.withTrimmedBottom (r.getHeight() * 0.45f).withTrimmedTop (8), Justification::centred);
            g.setFont (Theme::font (16.0f));
            g.drawText (t2, r.withTrimmedTop (r.getHeight() * 0.5f).withTrimmedBottom (8), Justification::centred);
        };
        box ({ 214.0f, 67.0f, 147.0f, 79.0f }, Theme::gold, "Main", nMain > 0 ? "(VST ×"_j + String (nMain) + ")" : "(VST)");

        g.setColour (Theme::text);
        g.drawLine (380.0f, 103.0f, 406.0f, 103.0f, 2.4f);
        g.drawLine (393.0f, 90.0f, 393.0f, 116.0f, 2.4f);

        box ({ 429.0f, 67.0f, 152.0f, 79.0f }, Theme::purple, "FX", "(エコー/リバーブ)"_j);
        drawArrow (g, 602.0f, 644.0f, 103.0f, Theme::text);

        Icons::draw (g, Icons::Id::broadcast, { 706.0f, 66.0f, 48.0f, 44.0f }, Theme::text, 1.8f);
        caption ("出力"_j, "(配信 / ループバック)"_j, 730.0f, 128.0f, 153.0f);
    }

private:
    AudioEngine& engine;
};

//==============================================================================
class OutputLevelPanel : public Component
{
public:
    OutputLevelPanel (AudioEngine& e) : engine (e)
    {
        addAndMakeVisible (left);
        addAndMakeVisible (right);
    }

    void tick()
    {
        left.setLevel (engine.outPeakL.exchange (0.0f));
        right.setLevel (engine.outPeakR.exchange (0.0f));

        const auto li = engine.getLatencyInfo();
        String t;
        if (li.sampleRate > 0)
            t = "レイテンシー "_j + String ((double) (li.input + li.output + li.mainPlugins) * 1000.0 / li.sampleRate, 1) + " ms";
        if (t != latencyText) { latencyText = t; repaint (getWidth() - 260, 0, 260, 46); }
    }

    void resized() override
    {
        left.setBounds (61, 70, 466, 23);
        right.setBounds (61, 108, 466, 23);
    }

    void paint (Graphics& g) override
    {
        drawSection (g, getLocalBounds().toFloat(), Icons::Id::levels, "出力レベル"_j, 23.0f, 47.0f);
        g.setColour (Theme::subText);
        g.setFont (Theme::font (15.0f));
        g.drawText (latencyText, getWidth() - 260, 6, 236, 34, Justification::centredRight);

        g.setColour (Theme::text);
        g.setFont (Theme::font (18.0f));
        g.drawText ("L", 24, 70, 20, 23, Justification::centred);
        g.drawText ("R", 24, 108, 20, 23, Justification::centred);

        g.setFont (Theme::font (16.0f));
        const int marks[] = { -60, -48, -36, -24, -12, -6, 0 };
        for (auto m : marks)
        {
            const float x = 61.0f + HMeter::dbToNorm ((float) m) * 466.0f;
            const String t = m == 0 ? "0 dB" : String (m);
            if (m == 0) g.drawText (t, Rectangle<float> (x - 20.0f, 143.0f, 60.0f, 24.0f), Justification::centredLeft);
            else        g.drawText (t, Rectangle<float> (x - 30.0f, 143.0f, 60.0f, 24.0f), Justification::centred);
        }
    }

private:
    AudioEngine& engine;
    HMeter left, right;
    String latencyText;
};

//==============================================================================
class MixerPage : public Component
{
public:
    MixerPage (AudioEngine& e)
        : mainStrip (e, 0), fxStrip (e, 1), plugins (e), output (e), monitor (e), flow (e), level (e)
    {
        for (auto* c : std::initializer_list<Component*> { &mainStrip, &fxStrip, &plugins, &output, &monitor, &flow, &level })
            addAndMakeVisible (c);
    }

    void refresh()
    {
        mainStrip.syncFromEngine();
        fxStrip.syncFromEngine();
        plugins.rebuild();
        output.refresh();
        monitor.refresh();
        flow.repaint();
    }

    void tick()
    {
        mainStrip.tick();
        fxStrip.tick();
        level.tick();
    }

    void resized() override
    {
        mainStrip.setBounds (R (238, 128, 420, 690));
        fxStrip.setBounds   (R (434, 128, 615, 690));
        plugins.setBounds   (R (641, 127, 1229, 690));
        output.setBounds    (R (1253, 127, 1655, 427));
        monitor.setBounds   (R (1253, 449, 1655, 690));
        flow.setBounds      (R (238, 720, 1063, 908));
        level.setBounds     (R (1089, 720, 1655, 908));
    }

    ChannelStripComp mainStrip, fxStrip;
    PluginPanel plugins;
    OutputPanel output;
    MonitorPanel monitor;
    SignalFlowPanel flow;
    OutputLevelPanel level;
};

//==============================================================================
class NavButton : public Button
{
public:
    NavButton (const String& t, Icons::Id i) : Button (t), icon (i) { setClickingTogglesState (true); setRadioGroupId (1); }
    void paintButton (Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const bool on = getToggleState();
        if (on)
        {
            g.setGradientFill (ColourGradient (Theme::purpleDeep.withAlpha (0.85f), r.getX(), 0, Theme::purple.withAlpha (0.35f), r.getRight(), 0, false));
            g.fillRoundedRectangle (r, 10.0f);
            Theme::drawGlowBorder (g, r, 10.0f, Theme::purple.brighter (0.2f), 1.8f);
        }
        else if (over)
        {
            g.setColour (Theme::row);
            g.fillRoundedRectangle (r, 10.0f);
        }
        const auto col = on ? Colours::white : Theme::text.withAlpha (0.9f);
        Icons::draw (g, icon, { r.getX() + 18.0f, r.getCentreY() - 16.0f, 32.0f, 32.0f }, on ? Colours::white : Theme::subText, 2.2f);
        g.setColour (col);
        g.setFont (Theme::font (21.0f, on));
        g.drawText (getButtonText(), r.withTrimmedLeft (70.0f), Justification::centredLeft);
    }
private:
    Icons::Id icon;
};

class Sidebar : public Component
{
public:
    Sidebar()
    {
        const char* names[] = { "ミキサー", "プラグイン", "ルーティング", "プリセット", "設定" };
        const Icons::Id icons[] = { Icons::Id::home, Icons::Id::plug, Icons::Id::route, Icons::Id::list, Icons::Id::gear };
        for (int i = 0; i < 5; ++i)
        {
            auto* b = buttons.add (new NavButton (String::fromUTF8 (names[i]), icons[i]));
            b->onClick = [this, i] { if (buttons[i]->getToggleState() && onSelect) onSelect (i); };
            addAndMakeVisible (b);
        }
        buttons[0]->setToggleState (true, dontSendNotification);
    }

    void select (int i)
    {
        buttons[i]->setToggleState (true, dontSendNotification);
        if (onSelect) onSelect (i);
    }

    std::function<void (int)> onSelect;

    void resized() override
    {
        const int cy[] = { 95, 176, 255, 331, 406 };
        for (int i = 0; i < 5; ++i)
            buttons[i]->setBounds (12, cy[i] - 32, 186, 64);
    }

    void paint (Graphics& g) override
    {
        g.setColour (Theme::bg0.withAlpha (0.6f));
        g.fillRect (getLocalBounds());
        g.setColour (Theme::border);
        g.drawLine ((float) getWidth() - 0.5f, 0.0f, (float) getWidth() - 0.5f, (float) getHeight(), 1.0f);

        // 飾り：キラキラ
        Icons::draw (g, Icons::Id::sparkle, { 156.0f, 540.0f, 24.0f, 24.0f }, Theme::purple);
        Icons::draw (g, Icons::Id::sparkle, { 46.0f, 606.0f, 28.0f, 28.0f }, Theme::gold);

        // 飾り：ストライプ
        {
            Graphics::ScopedSaveState ss (g);
            Path tri;
            tri.addTriangle (0.0f, 700.0f, 0.0f, (float) getHeight(), 210.0f, (float) getHeight());
            g.reduceClipRegion (tri);
            for (int i = 0; i < 14; ++i)
            {
                const float x = -150.0f + (float) i * 26.0f;
                Path s;
                s.startNewSubPath (x, (float) getHeight());
                s.lineTo (x + 150.0f, (float) getHeight() - 150.0f);
                s.lineTo (x + 162.0f, (float) getHeight() - 150.0f);
                s.lineTo (x + 12.0f, (float) getHeight());
                s.closeSubPath();
                g.setColour ((i % 2 == 0 ? Theme::purple : Theme::pink).withAlpha (0.55f));
                g.fillPath (s);
            }
        }

        // 飾り：ジェスターハット
        Path hat;
        hat.startNewSubPath (30.0f, 742.0f);
        hat.quadraticTo (18.0f, 650.0f, 110.0f, 628.0f);
        hat.quadraticTo (160.0f, 618.0f, 182.0f, 642.0f);
        hat.quadraticTo (140.0f, 640.0f, 150.0f, 706.0f);
        hat.closeSubPath();
        g.setGradientFill (ColourGradient (Theme::purple.brighter (0.2f), 60.0f, 640.0f, Theme::purpleDeep, 140.0f, 720.0f, false));
        g.fillPath (hat);
        Path band;
        band.startNewSubPath (24.0f, 736.0f);
        band.lineTo (150.0f, 698.0f);
        band.lineTo (157.0f, 720.0f);
        band.lineTo (31.0f, 760.0f);
        band.closeSubPath();
        g.setColour (Theme::gold);
        g.fillPath (band);
        g.fillEllipse (172.0f, 632.0f, 20.0f, 20.0f);
        g.setColour (Theme::bg0);
        g.fillEllipse (17.0f, 744.0f, 18.0f, 18.0f);
        g.setColour (Theme::purple);
        g.drawEllipse (17.0f, 744.0f, 18.0f, 18.0f, 3.0f);
    }

private:
    OwnedArray<NavButton> buttons;
};

//==============================================================================
class Header : public Component
{
public:
    Header (AudioEngine& e) : engine (e)
    {
        presetCombo.getProperties().set ("frameless", true);
        presetCombo.setColour (ComboBox::backgroundColourId, Colours::transparentBlack);
        presetCombo.setTextWhenNothingSelected ("（未保存）"_j);
        presetCombo.onChange = [this]
        {
            const auto name = presetCombo.getText();
            if (presetCombo.getSelectedId() > 0 && name != engine.currentPresetName && onLoadPreset)
                onLoadPreset (name);
        };
        addAndMakeVisible (presetCombo);

        openBtn.setJoined (false, true);
        saveBtn.setJoined (true, false);
        openBtn.setTooltip ("プリセットファイルを開く"_j);
        saveBtn.setTooltip ("プリセットを保存"_j);
        settingsBtn.setTooltip ("オーディオ設定"_j);
        for (auto* b : { &openBtn, &saveBtn, &settingsBtn, &minBtn, &maxBtn, &closeBtn })
            addAndMakeVisible (b);
        minBtn.strokeWidth = maxBtn.strokeWidth = 2.0f;
        closeBtn.strokeWidth = 2.4f;

        minBtn.onClick = [this] { if (auto* w = dynamic_cast<DocumentWindow*> (getTopLevelComponent())) w->setMinimised (true); };
        maxBtn.onClick = [this] { if (auto* w = dynamic_cast<DocumentWindow*> (getTopLevelComponent())) w->setFullScreen (! w->isFullScreen()); };
        closeBtn.onClick = [] { JUCEApplication::getInstance()->systemRequestedQuit(); };
    }

    void refreshPresets()
    {
        presetCombo.clear (dontSendNotification);
        auto names = engine.getPresetNames();
        for (int i = 0; i < names.size(); ++i)
            presetCombo.addItem (names[i], i + 1);
        const int idx = names.indexOf (engine.currentPresetName);
        if (idx >= 0) presetCombo.setSelectedId (idx + 1, dontSendNotification);
        else          presetCombo.setText (engine.currentPresetName, dontSendNotification);
    }

    std::function<void (const String&)> onLoadPreset;
    IconButton openBtn { Icons::Id::folder, IconButton::headerBox },
               saveBtn { Icons::Id::save, IconButton::headerBox },
               settingsBtn { Icons::Id::gear, IconButton::headerBox },
               minBtn { Icons::Id::minimise, IconButton::bare },
               maxBtn { Icons::Id::maximise, IconButton::bare },
               closeBtn { Icons::Id::close, IconButton::bare };

    void resized() override
    {
        presetCombo.setBounds (982, 22, 236, 46);
        openBtn.setBounds (1240, 18, 64, 55);
        saveBtn.setBounds (1303, 18, 64, 55);
        settingsBtn.setBounds (1396, 18, 61, 55);
        minBtn.setBounds (1514, 21, 32, 32);
        maxBtn.setBounds (1570, 21, 32, 32);
        closeBtn.setBounds (1624, 21, 32, 32);
    }

    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (Colour (0xff1c1335), 0, 0, Theme::bg1, 0, (float) getHeight(), false));
        g.fillRect (getLocalBounds());

        // 斜めストライプ
        for (int i = 0; i < 4; ++i)
        {
            const float x = 470.0f + (float) i * 38.0f;
            Path p;
            p.startNewSubPath (x, (float) getHeight());
            p.lineTo (x + 72.0f, 0.0f);
            p.lineTo (x + 72.0f + (i == 0 ? 60.0f : 16.0f), 0.0f);
            p.lineTo (x + (i == 0 ? 60.0f : 16.0f), (float) getHeight());
            p.closeSubPath();
            g.setColour (Theme::purple.withAlpha (i == 0 ? 0.5f : 0.38f - 0.07f * (float) i));
            g.fillPath (p);
        }

        // ロゴ
        Icons::drawCrown (g, { 20.0f, 8.0f, 86.0f, 72.0f });
        g.setFont (Theme::titleFont (44.0f));
        g.setGradientFill (ColourGradient (Colour (0xfffff6e2), 0, 14, Colour (0xffe9d9b8), 0, 58, false));
        g.drawText ("Aoto VST Router", 135, 10, 420, 50, Justification::centredLeft);
        g.setColour (Theme::subText);
        g.setFont (Theme::font (18.0f));
        g.drawText ("VST Plugin Host for Live", 137, 56, 320, 24, Justification::centredLeft);

        // プリセット枠
        auto box = Rectangle<float> (848.0f, 18.0f, 373.0f, 55.0f);
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (box, 10.0f);
        g.setColour (Theme::borderHi);
        g.drawRoundedRectangle (box, 10.0f, 1.5f);
        g.setColour (Theme::subText);
        g.setFont (Theme::font (20.0f));
        g.drawText ("プリセット："_j, 868, 18, 120, 55, Justification::centredLeft);

        // 下の光るライン
        g.setGradientFill (ColourGradient (Theme::purple.withAlpha (0.9f), 0, 0, Theme::purple.withAlpha (0.1f), (float) getWidth(), 0, false));
        g.fillRect (0, getHeight() - 2, getWidth(), 2);
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (auto* top = getTopLevelComponent()) dragger.startDraggingComponent (top, e.getEventRelativeTo (top));
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (auto* w = dynamic_cast<DocumentWindow*> (getTopLevelComponent()))
            if (! w->isFullScreen())
                dragger.dragComponent (w, e.getEventRelativeTo (w), nullptr);
    }
    void mouseDoubleClick (const MouseEvent&) override { maxBtn.triggerClick(); }

private:
    AudioEngine& engine;
    ComboBox presetCombo;
    ComponentDragger dragger;
};

//==============================================================================
class Canvas : public Component
{
public:
    Canvas (AudioEngine& e)
        : engine (e), header (e), mixer (e), pluginsPage (e), routingPage (e), presetsPage (e), settingsPage (e)
    {
        addAndMakeVisible (header);
        addAndMakeVisible (sidebar);
        for (auto* p : pages()) addChildComponent (p);
        mixer.setVisible (true);

        sidebar.onSelect = [this] (int i) { showPage (i); };
        auto goPlugins  = [this] { sidebar.select (1); };
        auto goSettings = [this] { sidebar.select (4); };
        mixer.plugins.goToPlugins = goPlugins;
        mixer.output.goToSettings = goSettings;
        mixer.monitor.goToSettings = goSettings;
        routingPage.goToSettings = goSettings;
        header.settingsBtn.onClick = goSettings;

        header.onLoadPreset = [this] (const String& name)
        {
            StringArray errors;
            engine.loadPreset (engine.getPresetFolder().getChildFile (name + ".xml"), errors);
            if (! errors.isEmpty()) showWarning ("一部のプラグインを読み込めませんでした"_j, errors.joinIntoString ("\n"));
            refreshAll();
        };
        header.saveBtn.onClick = [this] { showSaveMenu(); };
        header.openBtn.onClick = [this] { openPresetFile(); };
        presetsPage.onPresetLoaded = [this] { refreshAll(); };
        presetsPage.requestSaveAs = [this] { savePresetAs(); };

        header.refreshPresets();
        mixer.refresh();
    }

    std::vector<Component*> pages() { return { &mixer, &pluginsPage, &routingPage, &presetsPage, &settingsPage }; }

    void showPage (int i)
    {
        auto ps = pages();
        for (int k = 0; k < (int) ps.size(); ++k) ps[(size_t) k]->setVisible (k == i);
        if (i == 0) mixer.refresh();
        if (i == 2) routingPage.refresh();
        if (i == 3) presetsPage.refresh();
    }

    void refreshAll()
    {
        mixer.refresh();
        header.refreshPresets();
        presetsPage.refresh();
        routingPage.refresh();
    }

    void tick() { if (mixer.isVisible()) mixer.tick(); }

    void showSaveMenu()
    {
        PopupMenu m;
        const bool hasName = engine.currentPresetName.isNotEmpty();
        m.addItem (1, hasName ? "上書き保存（"_j + engine.currentPresetName + "）"_j : "上書き保存"_j, hasName);
        m.addItem (2, "名前を付けて保存…"_j);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&header.saveBtn), [this] (int r)
        {
            if (r == 1) { engine.savePreset (engine.currentPresetName); refreshAll(); }
            if (r == 2) savePresetAs();
        });
    }

    void savePresetAs()
    {
        auto* aw = new AlertWindow ("プリセットを保存"_j, "プリセット名を入力してください"_j, MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", engine.currentPresetName.isNotEmpty() ? engine.currentPresetName : "歌枠（エコー）"_j);
        aw->addButton ("保存"_j, 1, KeyPress (KeyPress::returnKey));
        aw->addButton ("キャンセル"_j, 0, KeyPress (KeyPress::escapeKey));
        aw->enterModalState (true, ModalCallbackFunction::create ([this, aw] (int r)
        {
            if (r != 1) return;
            const auto name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;
            if (! engine.savePreset (name)) showWarning ("保存できませんでした"_j, name);
            refreshAll();
        }), true);
    }

    void openPresetFile()
    {
        chooser = std::make_unique<FileChooser> ("プリセットを開く"_j, engine.getPresetFolder(), "*.xml");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
        {
            auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            StringArray errors;
            engine.loadPreset (f, errors);
            if (! errors.isEmpty()) showWarning ("一部のプラグインを読み込めませんでした"_j, errors.joinIntoString ("\n"));
            refreshAll();
        });
    }

    void resized() override
    {
        header.setBounds (0, 0, MainComponent::designW, 92);
        sidebar.setBounds (0, 92, 210, MainComponent::designH - 92);
        for (auto* p : pages()) p->setBounds (210, 92, MainComponent::designW - 210, MainComponent::designH - 92);
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Theme::bg1);
        g.setGradientFill (ColourGradient (Theme::purpleDeep.withAlpha (0.10f), 900.0f, 400.0f,
                                           Colours::transparentBlack, 1700.0f, 950.0f, true));
        g.fillRect (getLocalBounds());
    }

private:
    AudioEngine& engine;
    Header header;
    Sidebar sidebar;
    MixerPage mixer;
    PluginsPage pluginsPage;
    RoutingPage routingPage;
    PresetsPage presetsPage;
    SettingsPage settingsPage;
    std::unique_ptr<FileChooser> chooser;
};

//==============================================================================
MainComponent::MainComponent (AudioEngine& e) : engine (e)
{
    canvas = std::make_unique<Canvas> (engine);
    canvas->setBounds (0, 0, designW, designH);
    addAndMakeVisible (*canvas);
    engine.onChainChanged = [this] { canvas->refreshAll(); };
    setSize (1400, 788);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    engine.onChainChanged = nullptr;
}

Component& MainComponent::getCanvas() { return *canvas; }

void MainComponent::paint (Graphics& g) { g.fillAll (Theme::bg1); }

void MainComponent::resized()
{
    const float s = jmin ((float) getWidth() / (float) designW, (float) getHeight() / (float) designH);
    const float ox = ((float) getWidth() - (float) designW * s) * 0.5f;
    const float oy = ((float) getHeight() - (float) designH * s) * 0.5f;
    canvas->setTransform (AffineTransform::scale (s).translated (ox, oy));
}

void MainComponent::timerCallback()
{
    canvas->tick();
    if (++ticks >= 30 * 60) { ticks = 0; engine.saveSettings(); }   // 1 分ごとに自動保存
}
