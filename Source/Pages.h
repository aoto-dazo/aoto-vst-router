#pragma once
#include "Widgets.h"

// 共通ヘルパー
void showWarning (const juce::String& title, const juce::String& message);
void showPluginMenu (AudioEngine&, juce::Component* target,
                     const juce::String& tickedId,
                     std::function<void (const juce::PluginDescription&)> onChosen,
                     std::function<void()> goToPluginsPage);
void fillPairCombo (juce::ComboBox&, AudioEngine&, bool isMonitor);
void fillInputCombo (juce::ComboBox&, AudioEngine&);

// 見出し付きパネル
class TitledPanel : public juce::Component
{
public:
    TitledPanel (Icons::Id i, const juce::String& t) : icon (i), title (t) {}
    void paint (juce::Graphics&) override;
    Icons::Id icon;
    juce::String title;
    int headerH = 62;
};

//==============================================================================
class PluginsPage : public juce::Component, private juce::ChangeListener
{
public:
    explicit PluginsPage (AudioEngine&);
    ~PluginsPage() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    AudioEngine& engine;
    TitledPanel panel { Icons::Id::plug, "プラグイン（VST3）"_j };
    juce::TextButton scanButton { "VST3 をスキャン"_j };
    std::unique_ptr<juce::PluginListComponent> list;
    juce::Label info;
};

//==============================================================================
class RoutingPage : public juce::Component
{
public:
    explicit RoutingPage (AudioEngine&);
    void refresh();
    std::function<void()> goToSettings;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    AudioEngine& engine;
    TitledPanel panel { Icons::Id::route, "ルーティング"_j };
    juce::Label inLabel, outLabel, monLabel, help;
    juce::ComboBox inCombo, outCombo, monCombo;
    juce::ToggleButton monoToggle { "モノラルで出力する（L/R 両方に同じ音）"_j };
};

//==============================================================================
class PresetsPage : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetsPage (AudioEngine&);
    void refresh();
    void resized() override;
    std::function<void()> onPresetLoaded;
    std::function<void()> requestSaveAs;

private:
    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override;
    void loadSelected();

    AudioEngine& engine;
    TitledPanel panel { Icons::Id::list, "プリセット"_j };
    juce::ListBox listBox { "presets", this };
    juce::StringArray names;
    juce::TextButton loadBtn { "読み込む"_j }, saveBtn { "名前を付けて保存"_j }, delBtn { "削除"_j }, folderBtn { "フォルダを開く"_j };
    juce::Label help;
};

//==============================================================================
class SettingsPage : public juce::Component, private juce::Timer
{
public:
    explicit SettingsPage (AudioEngine&);
    void resized() override;

private:
    void timerCallback() override;
    AudioEngine& engine;
    TitledPanel devPanel { Icons::Id::gear, "オーディオデバイス"_j };
    TitledPanel latPanel { Icons::Id::signal, "レイテンシー"_j };
    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
    juce::Label latency, tips;
};
