#include "Pages.h"

using namespace juce;

//==============================================================================
void showWarning (const String& title, const String& message)
{
    AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, title, message, "OK");
}

void showPluginMenu (AudioEngine& engine, Component* target, const String& tickedId,
                     std::function<void (const PluginDescription&)> onChosen,
                     std::function<void()> goToPluginsPage)
{
    auto types = engine.knownPlugins.getTypes();
    PopupMenu menu;

    if (types.isEmpty())
    {
        menu.addSectionHeader ("VST がまだ登録されていません"_j);
        menu.addItem (1, "プラグイン画面でスキャンする…"_j);
        menu.showMenuAsync (PopupMenu::Options().withTargetComponent (target),
                            [goToPluginsPage] (int r) { if (r == 1 && goToPluginsPage) goToPluginsPage(); });
        return;
    }

    KnownPluginList::addToMenu (menu, types, KnownPluginList::sortByManufacturer, tickedId);
    menu.addSeparator();
    menu.addItem (0x7fff0001, "プラグインをスキャン…"_j);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (target).withMinimumWidth (target->getWidth()),
                        [types, onChosen, goToPluginsPage] (int r)
                        {
                            if (r == 0) return;
                            if (r == 0x7fff0001) { if (goToPluginsPage) goToPluginsPage(); return; }
                            const int i = KnownPluginList::getIndexChosenByMenu (types, r);
                            if (isPositiveAndBelow (i, types.size()) && onChosen)
                                onChosen (types.getReference (i));
                        });
}

void fillPairCombo (ComboBox& box, AudioEngine& engine, bool isMonitor)
{
    box.clear (dontSendNotification);
    box.setTextWhenNothingSelected ("出力を選択…"_j);
    box.setTextWhenNoChoicesAvailable ("オーディオデバイスがありません"_j);
    const auto names = engine.getOutputChannelNames();
    const auto dev = engine.getDeviceName();

    if (isMonitor)
        box.addItem ("なし（モニターしない）"_j, 1000);

    for (int i = 0; i < names.size(); i += 2)
    {
        const String pair = i + 1 < names.size() ? names[i] + " / " + names[i + 1] : names[i];
        box.addItem (dev + " (" + pair + ")", i + 1);
    }
    box.addSeparator();
    box.addItem ("オーディオデバイスの設定…"_j, 9999);

    const int sel = isMonitor ? engine.monitorPair.load() : engine.outputPair.load();
    box.setSelectedId (sel < 0 ? 1000 : sel + 1, dontSendNotification);
}

void fillInputCombo (ComboBox& box, AudioEngine& engine)
{
    box.clear (dontSendNotification);
    const auto names = engine.getInputChannelNames();
    const auto dev = engine.getDeviceName();
    box.addSectionHeading ("モノラル入力（マイク向け）"_j);
    for (int i = 0; i < names.size(); ++i)
        box.addItem (dev + " - " + names[i], i + 1);
    box.addSectionHeading ("ステレオ入力"_j);
    for (int i = 0; i + 1 < names.size(); i += 2)
        box.addItem (dev + " - " + names[i] + " / " + names[i + 1], 1000 + i + 1);

    const int ch = engine.inputChannel.load();
    box.setSelectedId (engine.inputStereo ? 1000 + ch + 1 : ch + 1, dontSendNotification);
}

//==============================================================================
void TitledPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    Theme::drawPanel (g, r, 14.0f);
    Icons::draw (g, icon, { 22.0f, 18.0f, 28.0f, 28.0f }, Theme::purple.brighter (0.25f), 2.2f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (22.0f, true));
    g.drawText (title, 62, 12, getWidth() - 80, 40, Justification::centredLeft);
    g.setColour (Theme::border);
    g.drawLine (16.0f, (float) headerH, (float) getWidth() - 16.0f, (float) headerH, 1.0f);
}

static void styleHelp (Label& l, const String& text, float size = 16.0f)
{
    l.setText (text, dontSendNotification);
    l.setFont (Theme::font (size));
    l.setColour (Label::textColourId, Theme::subText);
    l.setJustificationType (Justification::topLeft);
}

static void styleFieldLabel (Label& l, const String& text)
{
    l.setText (text, dontSendNotification);
    l.setFont (Theme::font (18.0f));
    l.setColour (Label::textColourId, Theme::text);
}

//==============================================================================
PluginsPage::PluginsPage (AudioEngine& e) : engine (e)
{
    addAndMakeVisible (panel);
    list = std::make_unique<PluginListComponent> (engine.formatManager, engine.knownPlugins,
                                                  engine.getDeadMansPedalFile(),
                                                  engine.appProperties.getUserSettings(), true);
    list->setOptionsButtonText ("その他の操作…"_j);
    list->setScanDialogText ("VST3 をスキャン中"_j, "VST3 プラグインを探しています…"_j);
    list->getTableListBox().setRowHeight (30);
    addAndMakeVisible (*list);

    scanButton.onClick = [this]
    {
        for (auto* f : engine.formatManager.getFormats())
            if (f->getName() == "VST3") { list->scanFor (*f); break; }
    };
    addAndMakeVisible (scanButton);

    styleHelp (info, "「VST3 をスキャン」で C:\\Program Files\\Common Files\\VST3 などを探します（フォルダは最初の画面で追加可）。\n"_j
                     "見つかった VST はミキサー画面の「＋ VSTプラグインを追加」から使えます。"_j, 17.0f);
    addAndMakeVisible (info);
    engine.knownPlugins.addChangeListener (this);
}

PluginsPage::~PluginsPage() { engine.knownPlugins.removeChangeListener (this); }

void PluginsPage::changeListenerCallback (ChangeBroadcaster*) { engine.saveSettings(); }

void PluginsPage::paint (Graphics&) {}

void PluginsPage::resized()
{
    auto r = getLocalBounds().reduced (28, 36);
    panel.setBounds (r);
    auto in = r.reduced (24, 0).withTrimmedTop (82).withTrimmedBottom (24);
    auto top = in.removeFromTop (60);
    scanButton.setBounds (top.removeFromLeft (220).reduced (0, 6));
    top.removeFromLeft (20);
    info.setBounds (top);
    in.removeFromTop (14);
    list->setBounds (in);
}

//==============================================================================
RoutingPage::RoutingPage (AudioEngine& e) : engine (e)
{
    addAndMakeVisible (panel);
    for (auto* l : { &inLabel, &outLabel, &monLabel }) addAndMakeVisible (*l);
    styleFieldLabel (inLabel, "マイク入力"_j);
    styleFieldLabel (outLabel, "配信出力（ループバックする出力）"_j);
    styleFieldLabel (monLabel, "モニター出力（ヘッドホン）"_j);

    for (auto* c : { &inCombo, &outCombo, &monCombo }) addAndMakeVisible (*c);

    inCombo.onChange = [this]
    {
        const int id = inCombo.getSelectedId();
        if (id <= 0) return;
        engine.inputStereo = id > 1000;
        engine.inputChannel = (id > 1000 ? id - 1000 : id) - 1;
        engine.saveSettings();
    };
    outCombo.onChange = [this]
    {
        const int id = outCombo.getSelectedId();
        if (id == 9999) { refresh(); if (goToSettings) goToSettings(); return; }
        if (id > 0) { engine.outputPair = id - 1; engine.saveSettings(); }
    };
    monCombo.onChange = [this]
    {
        const int id = monCombo.getSelectedId();
        if (id == 9999) { refresh(); if (goToSettings) goToSettings(); return; }
        if (id == 1000) engine.monitorPair = -1;
        else if (id > 0) engine.monitorPair = id - 1;
        engine.saveSettings();
    };

    monoToggle.onClick = [this] { engine.monoOutput = monoToggle.getToggleState(); engine.saveSettings(); };
    addAndMakeVisible (monoToggle);

    styleHelp (help,
        "■ 信号の流れ\n"_j
        "  マイク入力 → Main の VST（上から順番）→ Main フェーダー → 配信出力\n"_j
        "                    └→ FX の VST（エコー・リバーブ等）→ FX フェーダー → 配信出力\n"_j
        "  FX には Main の VST を通った後の声が送られます。FX 側のリバーブ / ディレイは Mix（Dry/Wet）を 100% Wet にすると扱いやすいです。\n\n"_j
        "■ RME（TotalMix）でのループバック配信\n"_j
        "  1. 「配信出力」を TotalMix で Loopback を ON にした出力（例：AN 1/2）にする\n"_j
        "  2. OBS / TikTok LIVE Studio のマイクは、その出力と同じ番号の録音デバイス（例：Analog 1+2）を選ぶ\n"_j
        "  3. マイクの生音がループバックに混ざらないよう、TotalMix 側でその出力へのマイク直送りは下げておく\n\n"_j
        "■ モニター\n"_j
        "  チャンネルのヘッドホンボタンを ON にしたものだけがモニター出力に流れます（Studio One のように声を確認したい時に）。"_j,
        16.5f);
    addAndMakeVisible (help);
    refresh();
}

void RoutingPage::refresh()
{
    fillInputCombo (inCombo, engine);
    fillPairCombo (outCombo, engine, false);
    fillPairCombo (monCombo, engine, true);
    monoToggle.setToggleState (engine.monoOutput.load(), dontSendNotification);
}

void RoutingPage::paint (Graphics&) {}

void RoutingPage::resized()
{
    auto r = getLocalBounds().reduced (28, 36);
    panel.setBounds (r);
    auto in = r.reduced (32, 0).withTrimmedTop (90);

    auto left = in.removeFromLeft (560);
    auto field = [&] (Label& l, ComboBox& c)
    {
        l.setBounds (left.removeFromTop (30));
        left.removeFromTop (6);
        c.setBounds (left.removeFromTop (50));
        left.removeFromTop (26);
    };
    field (inLabel, inCombo);
    field (outLabel, outCombo);
    field (monLabel, monCombo);
    monoToggle.setBounds (left.removeFromTop (36));

    in.removeFromLeft (40);
    help.setBounds (in.withTrimmedBottom (30));
}

//==============================================================================
PresetsPage::PresetsPage (AudioEngine& e) : engine (e)
{
    addAndMakeVisible (panel);
    listBox.setRowHeight (48);
    listBox.setColour (ListBox::backgroundColourId, Theme::panelInner);
    listBox.setOutlineThickness (1);
    addAndMakeVisible (listBox);

    for (auto* b : { &loadBtn, &saveBtn, &delBtn, &folderBtn }) addAndMakeVisible (*b);

    loadBtn.onClick = [this] { loadSelected(); };
    saveBtn.onClick = [this] { if (requestSaveAs) requestSaveAs(); };
    folderBtn.onClick = [this] { engine.getPresetFolder().startAsProcess(); };
    delBtn.onClick = [this]
    {
        const int row = listBox.getSelectedRow();
        if (! isPositiveAndBelow (row, names.size())) return;
        const String name = names[row];
        AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "プリセットを削除"_j,
                                      "「"_j + name + "」を削除しますか？"_j, "削除"_j, "キャンセル"_j, nullptr,
                                      ModalCallbackFunction::create ([this, name] (int r)
                                      {
                                          if (r != 1) return;
                                          engine.getPresetFolder().getChildFile (name + ".xml").moveToTrash();
                                          refresh();
                                      }));
    };

    styleHelp (help, "ダブルクリックで読み込み。プリセットには Main / FX の VST・各設定・フェーダー・パンが保存されます。\n"_j
                     "保存場所：ドキュメント\\Aoto VST Router\\Presets\n"_j
                     "前回終了時の状態は自動で復元されるので、起動するだけですぐ配信できます。"_j);
    addAndMakeVisible (help);
    refresh();
}

void PresetsPage::refresh()
{
    names = engine.getPresetNames();
    listBox.updateContent();
    listBox.repaint();
}

int PresetsPage::getNumRows() { return names.size(); }

void PresetsPage::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (selected)
    {
        g.setColour (Theme::purpleDeep.withAlpha (0.5f));
        g.fillRoundedRectangle (4.0f, 3.0f, (float) w - 8.0f, (float) h - 6.0f, 8.0f);
    }
    const bool current = names[row] == engine.currentPresetName;
    Icons::draw (g, Icons::Id::list, { 18.0f, (float) h * 0.5f - 11.0f, 22.0f, 22.0f }, current ? Theme::gold : Theme::subText);
    g.setColour (current ? Theme::gold : Theme::text);
    g.setFont (Theme::font (19.0f, current));
    g.drawText (names[row] + (current ? "  （使用中）"_j : ""), 54, 0, w - 60, h, Justification::centredLeft);
}

void PresetsPage::listBoxItemDoubleClicked (int, const MouseEvent&) { loadSelected(); }

void PresetsPage::loadSelected()
{
    const int row = listBox.getSelectedRow();
    if (! isPositiveAndBelow (row, names.size())) return;
    StringArray errors;
    engine.loadPreset (engine.getPresetFolder().getChildFile (names[row] + ".xml"), errors);
    if (! errors.isEmpty())
        showWarning ("一部のプラグインを読み込めませんでした"_j, errors.joinIntoString ("\n"));
    refresh();
    if (onPresetLoaded) onPresetLoaded();
}

void PresetsPage::resized()
{
    auto r = getLocalBounds().reduced (28, 36);
    panel.setBounds (r);
    auto in = r.reduced (28, 0).withTrimmedTop (86).withTrimmedBottom (28);
    auto side = in.removeFromRight (380);
    in.removeFromRight (28);
    listBox.setBounds (in);
    for (auto* b : { &loadBtn, &saveBtn, &delBtn, &folderBtn })
    {
        b->setBounds (side.removeFromTop (52));
        side.removeFromTop (12);
    }
    side.removeFromTop (16);
    help.setBounds (side);
}

//==============================================================================
SettingsPage::SettingsPage (AudioEngine& e) : engine (e)
{
    addAndMakeVisible (devPanel);
    addAndMakeVisible (latPanel);
    selector = std::make_unique<AudioDeviceSelectorComponent> (engine.deviceManager, 0, 0, 0, 0, false, false, false, false);
    selector->setItemHeight (34);
    addAndMakeVisible (*selector);

    latency.setFont (Theme::font (19.0f));
    latency.setJustificationType (Justification::topLeft);
    addAndMakeVisible (latency);

    styleHelp (tips,
        "■ Studio One と同じくらい（約 9ms）にするには\n"_j
        "  ・デバイスの種類は「ASIO」、デバイスは「ASIO Fireface USB」等の RME ドライバを選ぶ\n"_j
        "  ・バッファサイズを 96 サンプル（48kHz）にする\n"_j
        "    RME はドライバ側で決まるので「Control Panel」ボタン → Buffer Size を 96 に\n"_j
        "  ・WASAPI だと遅くなるので ASIO 推奨\n"_j
        "  ・先読み（ルックアヘッド）するプラグインはその分遅れます（下の数字に表示）\n"_j
        "  ・このアプリはプロセスブロックの追加分割をしないので、遅延はドライバ＋プラグインだけです"_j);
    addAndMakeVisible (tips);
    startTimerHz (4);
    timerCallback();
}

void SettingsPage::timerCallback()
{
    const auto li = engine.getLatencyInfo();
    if (li.sampleRate <= 0) { latency.setText ("オーディオデバイスが開いていません"_j, dontSendNotification); return; }

    auto ms = [&] (int s) { return String ((double) s * 1000.0 / li.sampleRate, 2) + " ms"; };
    const int rt = li.input + li.output;
    String t;
    t << "サンプルレート："_j << String (li.sampleRate, 0) << " Hz\n"
      << "バッファサイズ："_j << li.buffer << " サンプル（"_j << ms (li.buffer) << "）\n\n"_j
      << "入力："_j << ms (li.input) << " / " << li.input << " サンプル\n"_j
      << "出力："_j << ms (li.output) << " / " << li.output << " サンプル\n"_j
      << "オーディオラウンドトリップ："_j << ms (rt) << " / " << rt << " サンプル\n\n"_j
      << "Main の VST による遅延："_j << li.mainPlugins << " サンプル\n"_j
      << "FX の VST による遅延："_j << li.fxPlugins << " サンプル\n"_j
      << "合計（Main の声）："_j << ms (rt + li.mainPlugins) << " / " << (rt + li.mainPlugins) << " サンプル"_j;
    latency.setText (t, dontSendNotification);
}

void SettingsPage::resized()
{
    auto r = getLocalBounds().reduced (28, 36);
    auto left = r.removeFromLeft (r.getWidth() * 55 / 100);
    r.removeFromLeft (24);
    devPanel.setBounds (left);
    latPanel.setBounds (r);
    selector->setBounds (left.reduced (20, 0).withTrimmedTop (80).withTrimmedBottom (20));
    auto lr = r.reduced (28, 0).withTrimmedTop (84).withTrimmedBottom (24);
    latency.setBounds (lr.removeFromTop (300));
    tips.setBounds (lr);
}
