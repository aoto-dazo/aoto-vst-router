#pragma once
#include <JuceHeader.h>

// ソース中の日本語文字列を UTF-8 として juce::String にする（"テキスト"_j）
inline juce::String operator""_j (const char* s, size_t n) { return juce::String (juce::CharPointer_UTF8 (s), n); }

//==============================================================================
// 1 つの VST スロット
struct PluginSlot
{
    juce::PluginDescription description;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    std::atomic<bool> enabled { true };

    // 音声スレッド用の作業バッファ（プラグインのバス数に合わせて確保）
    juce::AudioBuffer<float> work;
    juce::MidiBuffer midi;

    // エディタ窓（instance より後に宣言 → 先に破棄される）
    std::unique_ptr<juce::DocumentWindow> editorWindow;

    ~PluginSlot() { editorWindow.reset(); }

    void prepare (double sampleRate, int blockSize);
    void process (juce::AudioBuffer<float>& stereo, int numSamples) noexcept;
};

//==============================================================================
// Main / FX チャンネル
struct ChannelStrip
{
    juce::String id;
    std::vector<std::unique_ptr<PluginSlot>> slots;   // 音声スレッドは lock 中のみ参照

    std::atomic<float> gainDb  { 0.0f };    // -60 以下 = -inf
    std::atomic<float> pan     { 0.0f };    // -1..1
    std::atomic<bool>  toOutput  { true };  // スピーカーボタン（配信出力へ）
    std::atomic<bool>  toMonitor { false }; // ヘッドホンボタン（モニターへ）

    std::atomic<float> peakL { 0 }, peakR { 0 };

    juce::LinearSmoothedValue<float> smGainL, smGainR;
    juce::AudioBuffer<float> buffer;

    int totalLatency() const;
};

//==============================================================================
class AudioEngine : private juce::AudioIODeviceCallback,
                    private juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void initialise();
    void shutdown();

    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    juce::ApplicationProperties appProperties;

    ChannelStrip mainCh, fxCh;
    ChannelStrip& channel (int idx) { return idx == 0 ? mainCh : fxCh; }

    // ---- ルーティング（デバイスのチャンネル番号）----
    std::atomic<int>  inputChannel  { 0 };   // マイク入力チャンネル（0 始まり）
    std::atomic<bool> inputStereo   { false };
    std::atomic<int>  outputPair    { 0 };   // 配信出力 L の番号
    std::atomic<int>  monitorPair   { 2 };   // モニター L の番号（-1 = なし）
    std::atomic<bool> monoOutput    { false };
    std::atomic<float> monitorGainDb { -6.0f };
    std::atomic<float> outPeakL { 0 }, outPeakR { 0 };

    juce::StringArray getInputChannelNames() const;
    juce::StringArray getOutputChannelNames() const;
    juce::String getDeviceName() const;

    // ---- プラグイン操作（メッセージスレッド）----
    juce::String addPlugin (int ch, const juce::PluginDescription&, int insertIndex = -1);
    juce::String replacePlugin (int ch, int index, const juce::PluginDescription&);
    void removePlugin (int ch, int index);
    void movePlugin (int ch, int from, int to);
    void showEditor (int ch, int index);
    void setPluginEnabled (int ch, int index, bool);

    // ---- プリセット / セッション ----
    std::unique_ptr<juce::XmlElement> createStateXml();
    void restoreStateXml (const juce::XmlElement&, juce::StringArray& errors);
    juce::File getPresetFolder() const;
    juce::StringArray getPresetNames() const;
    bool savePreset (const juce::String& name);
    bool loadPreset (const juce::File& file, juce::StringArray& errors);
    juce::String currentPresetName;

    void saveSettings();
    juce::File getDeadMansPedalFile() const;

    // レイテンシー情報（サンプル）
    struct LatencyInfo { double sampleRate = 0; int buffer = 0, input = 0, output = 0, mainPlugins = 0, fxPlugins = 0; };
    LatencyInfo getLatencyInfo();

    std::function<void()> onChainChanged;   // UI 更新用

private:
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn,
                                           float* const* out, int numOut, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    std::unique_ptr<PluginSlot> createSlot (const juce::PluginDescription&, juce::String& error);
    void processChannel (ChannelStrip&, int numSamples) noexcept;
    void notifyChanged();
    void openAllChannels();

    juce::SpinLock chainLock;          // スロット配列の差し替えを保護
    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;
    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
