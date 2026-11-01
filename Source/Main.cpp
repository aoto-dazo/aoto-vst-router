#include "MainComponent.h"
#include <BinaryData.h>

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (AudioEngine& engine)
        : DocumentWindow ("Aoto VST Router", Theme::bg1, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (false);
        setTitleBarHeight (0);                 // 自前のタイトルバー（ヘッダー）を使う
        setContentOwned (new MainComponent (engine), true);
        const auto icon = juce::ImageCache::getFromMemory (BinaryData::icon_png, BinaryData::icon_pngSize);
        setIcon (icon);
        setResizable (true, false);
        setResizeLimits (960, 540, 4000, 2400);
        centreWithSize (1400, 788);
        setVisible (true);
        if (auto* peer = getPeer())
            peer->setIcon (icon);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

//==============================================================================
class AotoVSTRouterApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Aoto VST Router"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&laf);
        gDemoMode = commandLine.contains ("--demo");

        engine = std::make_unique<AudioEngine>();
        engine->initialise();
        window = std::make_unique<MainWindow> (*engine);

        // 開発用：--screenshot <path> で UI を画像に保存して終了
        auto args = juce::StringArray::fromTokens (commandLine, true);
        const int si = args.indexOf ("--screenshot");
        if (si >= 0 && si + 1 < args.size())
        {
            const juce::File out (args[si + 1].unquoted());
            const int page = args.contains ("--page") ? args[args.indexOf ("--page") + 1].getIntValue() : 0;
            juce::Timer::callAfterDelay (1500, [this, out, page]
            {
                auto* mc = dynamic_cast<MainComponent*> (window->getContentComponent());
                auto& canvas = mc->getCanvas();
                if (page > 0)
                    if (auto* sb = canvas.getChildComponent (1))
                        if (auto* b = dynamic_cast<juce::Button*> (sb->getChildComponent (page)))
                            b->triggerClick();
                juce::Timer::callAfterDelay (600, [this, out, &canvas]
                {
                    canvas.setTransform ({});
                    auto img = canvas.createComponentSnapshot (canvas.getLocalBounds(), true, 1.0f);
                    juce::FileOutputStream fos (out);
                    fos.setPosition (0); fos.truncate();
                    juce::PNGImageFormat().writeImageToStream (img, fos);
                    fos.flush();
                    quit();
                });
            });
        }
    }

    void shutdown() override
    {
        if (engine) engine->saveSettings();
        window.reset();
        engine.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

    void anotherInstanceStarted (const juce::String&) override
    {
        if (window) { window->setMinimised (false); window->toFront (true); }
    }

private:
    AotoLookAndFeel laf;
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (AotoVSTRouterApp)
