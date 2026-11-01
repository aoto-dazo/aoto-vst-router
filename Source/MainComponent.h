#pragma once
#include "Theme.h"
#include "AudioEngine.h"

class Canvas;

// ウィンドウの中身。デザイン解像度 1672x941 の Canvas をウィンドウサイズに合わせて拡大縮小する
class MainComponent : public juce::Component, private juce::Timer
{
public:
    explicit MainComponent (AudioEngine&);
    ~MainComponent() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Component& getCanvas();

    static constexpr int designW = 1672, designH = 941;

private:
    void timerCallback() override;
    AudioEngine& engine;
    std::unique_ptr<Canvas> canvas;
    int ticks = 0;
};

extern bool gDemoMode;
