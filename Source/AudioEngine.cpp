#include "AudioEngine.h"

using namespace juce;

static float dbToGain (float db) { return db <= -60.0f ? 0.0f : Decibels::decibelsToGain (db); }

//==============================================================================
void PluginSlot::prepare (double sampleRate, int blockSize)
{
    auto& p = *instance;
    p.setRateAndBufferSizeDetails (sampleRate, blockSize);
    p.prepareToPlay (sampleRate, blockSize);
    const int ch = jmax (2, p.getTotalNumInputChannels(), p.getTotalNumOutputChannels());
    work.setSize (ch, jmax (blockSize, 32), false, true, false);
    midi.ensureSize (256);
}

void PluginSlot::process (AudioBuffer<float>& stereo, int n) noexcept
{
    if (! enabled.load (std::memory_order_relaxed) || instance == nullptr)
        return;

    auto& p = *instance;
    const int nIn  = p.getTotalNumInputChannels();
    const int nOut = p.getTotalNumOutputChannels();
    const int nCh  = work.getNumChannels();

    if (n > work.getNumSamples() || nCh < jmax (nIn, nOut))
        return;

    AudioBuffer<float> buf (work.getArrayOfWritePointers(), nCh, n);
    buf.clear();

    if (nIn >= 2)
    {
        buf.copyFrom (0, 0, stereo, 0, 0, n);
        buf.copyFrom (1, 0, stereo, 1, 0, n);
    }
    else if (nIn == 1)
    {
        buf.addFrom (0, 0, stereo, 0, 0, n, 0.5f);
        buf.addFrom (0, 0, stereo, 1, 0, n, 0.5f);
    }

    midi.clear();
    p.processBlock (buf, midi);

    if (nOut >= 2)
    {
        stereo.copyFrom (0, 0, buf, 0, 0, n);
        stereo.copyFrom (1, 0, buf, 1, 0, n);
    }
    else if (nOut == 1)
    {
        stereo.copyFrom (0, 0, buf, 0, 0, n);
        stereo.copyFrom (1, 0, buf, 0, 0, n);
    }
}

int ChannelStrip::totalLatency() const
{
    int total = 0;
    for (auto& s : slots)
        if (s->enabled && s->instance != nullptr)
            total += s->instance->getLatencySamples();
    return total;
}

//==============================================================================
AudioEngine::AudioEngine()
{
    mainCh.id = "main";
    fxCh.id   = "fx";
    fxCh.gainDb = -12.0f;
    fxCh.toOutput = true;

    PropertiesFile::Options opts;
    opts.applicationName     = "AotoVSTRouter";
    opts.filenameSuffix      = ".settings";
    opts.folderName          = "AotoVSTRouter";
    opts.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (opts);

    addDefaultFormatsToManager (formatManager);
}

AudioEngine::~AudioEngine() { shutdown(); }

File AudioEngine::getDeadMansPedalFile() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
             .getChildFile ("AotoVSTRouter").getChildFile ("scan-crash-check.txt");
}

File AudioEngine::getPresetFolder() const
{
    auto f = File::getSpecialLocation (File::userDocumentsDirectory)
               .getChildFile ("Aoto VST Router").getChildFile ("Presets");
    f.createDirectory();
    return f;
}

void AudioEngine::initialise()
{
    auto* props = appProperties.getUserSettings();

    if (auto xml = props->getXmlValue ("knownPlugins"))
        knownPlugins.recreateFromXml (*xml);

    inputChannel  = props->getIntValue ("inputChannel", 0);
    inputStereo   = props->getBoolValue ("inputStereo", false);
    outputPair    = props->getIntValue ("outputPair", 0);
    monitorPair   = props->getIntValue ("monitorPair", 2);
    monoOutput    = props->getBoolValue ("monoOutput", false);
    monitorGainDb = (float) props->getDoubleValue ("monitorGainDb", -6.0);

    auto savedDevice = props->getXmlValue ("audioDevice");
    deviceManager.initialise (64, 64, savedDevice.get(), savedDevice == nullptr);

    // PC 起動直後などで Babyface がまだ準備できていない時は、別のデバイスに切り替えず再試行する
    if (savedDevice != nullptr && deviceManager.getCurrentAudioDevice() == nullptr)
    {
        pendingDeviceXml = std::move (savedDevice);
        startTimer (2000);
    }

    // 初回起動：ASIO があれば ASIO（Babyface 優先）＋小さいバッファを選ぶ
    if (savedDevice == nullptr && pendingDeviceXml == nullptr)
    {
        for (auto* type : deviceManager.getAvailableDeviceTypes())
        {
            if (type->getTypeName() != "ASIO")
                continue;

            type->scanForDevices();
            auto names = type->getDeviceNames();
            if (names.isEmpty())
                break;

            String chosen = names[0];
            for (auto& n : names)
                if (n.containsIgnoreCase ("Babyface") || n.containsIgnoreCase ("RME"))
                    chosen = n;

            deviceManager.setCurrentAudioDeviceType ("ASIO", true);
            auto setup = deviceManager.getAudioDeviceSetup();
            setup.outputDeviceName = setup.inputDeviceName = chosen;
            setup.bufferSize = 96;
            deviceManager.setAudioDeviceSetup (setup, true);
            break;
        }
    }

    openAllChannels();

    if (getOutputChannelNames().size() < 4 && monitorPair >= 2)
        monitorPair = -1;

    if (auto session = props->getXmlValue ("session"))
    {
        StringArray errors;
        restoreStateXml (*session, errors);
    }
    currentPresetName = props->getValue ("presetName", {});

    setAutoStart (isAutoStartEnabled());

    deviceManager.addChangeListener (this);
    deviceManager.addAudioCallback (this);
}

void AudioEngine::timerCallback()
{
    if (pendingDeviceXml == nullptr) { stopTimer(); return; }
    deviceManager.initialise (64, 64, pendingDeviceXml.get(), false);
    if (deviceManager.getCurrentAudioDevice() != nullptr)
    {
        stopTimer();
        pendingDeviceXml.reset();
        openAllChannels();
        notifyChanged();
    }
}

static const char* runKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\AotoVSTRouter";

bool AudioEngine::isAutoStartEnabled()
{
    return appProperties.getUserSettings()->getBoolValue ("autoStart", true);
}

void AudioEngine::setAutoStart (bool on)
{
    appProperties.getUserSettings()->setValue ("autoStart", on);
   #if JUCE_WINDOWS
    if (on)
    {
        const auto exe = File::getSpecialLocation (File::currentExecutableFile).getFullPathName();
        WindowsRegistry::setValue (runKey, "\"" + exe + "\" --autostart");
    }
    else
    {
        WindowsRegistry::deleteValue (runKey);
    }
   #endif
}

void AudioEngine::shutdown()
{
    stopTimer();
    deviceManager.removeAudioCallback (this);
    deviceManager.removeChangeListener (this);
    deviceManager.closeAudioDevice();

    const SpinLock::ScopedLockType sl (chainLock);
    for (auto* ch : { &mainCh, &fxCh })
        ch->slots.clear();
}

void AudioEngine::openAllChannels()
{
    // 全チャンネルを有効化しておくと、コールバックの配列番号 = デバイスのチャンネル番号 になる
    auto* dev = deviceManager.getCurrentAudioDevice();
    if (dev == nullptr) return;

    auto setup = deviceManager.getAudioDeviceSetup();
    const int nIn  = dev->getInputChannelNames().size();
    const int nOut = dev->getOutputChannelNames().size();

    BigInteger ins, outs;
    ins.setRange (0, nIn, true);
    outs.setRange (0, nOut, true);

    if (setup.inputChannels == ins && setup.outputChannels == outs
        && ! setup.useDefaultInputChannels && ! setup.useDefaultOutputChannels)
        return;

    setup.inputChannels = ins;
    setup.outputChannels = outs;
    setup.useDefaultInputChannels = setup.useDefaultOutputChannels = false;
    deviceManager.setAudioDeviceSetup (setup, true);
}

void AudioEngine::changeListenerCallback (ChangeBroadcaster*)
{
    openAllChannels();
    notifyChanged();
}

void AudioEngine::notifyChanged()
{
    if (onChainChanged) onChainChanged();
}

StringArray AudioEngine::getInputChannelNames() const
{
    if (auto* d = deviceManager.getCurrentAudioDevice()) return d->getInputChannelNames();
    return {};
}

StringArray AudioEngine::getOutputChannelNames() const
{
    if (auto* d = deviceManager.getCurrentAudioDevice()) return d->getOutputChannelNames();
    return {};
}

String AudioEngine::getDeviceName() const
{
    if (auto* d = deviceManager.getCurrentAudioDevice()) return d->getName();
    return "（デバイスなし）"_j;
}

AudioEngine::LatencyInfo AudioEngine::getLatencyInfo()
{
    LatencyInfo li;
    if (auto* d = deviceManager.getCurrentAudioDevice())
    {
        li.sampleRate = d->getCurrentSampleRate();
        li.buffer = d->getCurrentBufferSizeSamples();
        li.input  = d->getInputLatencyInSamples();
        li.output = d->getOutputLatencyInSamples();
    }
    li.mainPlugins = mainCh.totalLatency();
    li.fxPlugins   = fxCh.totalLatency();
    return li;
}

//==============================================================================
std::unique_ptr<PluginSlot> AudioEngine::createSlot (const PluginDescription& desc, String& error)
{
    auto inst = formatManager.createPluginInstance (desc, currentSampleRate, currentBlockSize, error);
    if (inst == nullptr)
    {
        if (error.isEmpty()) error = desc.name + " を読み込めませんでした"_j;
        return nullptr;
    }

    // メイン入出力をステレオに、サイドチェーン等は無効に
    inst->enableAllBuses();
    auto layout = inst->getBusesLayout();
    if (! layout.inputBuses.isEmpty())  layout.inputBuses.getReference (0)  = AudioChannelSet::stereo();
    if (! layout.outputBuses.isEmpty()) layout.outputBuses.getReference (0) = AudioChannelSet::stereo();
    for (int i = 1; i < layout.inputBuses.size(); ++i)  layout.inputBuses.getReference (i)  = AudioChannelSet::disabled();
    for (int i = 1; i < layout.outputBuses.size(); ++i) layout.outputBuses.getReference (i) = AudioChannelSet::disabled();

    if (inst->checkBusesLayoutSupported (layout))
        inst->setBusesLayout (layout);

    auto slot = std::make_unique<PluginSlot>();
    slot->description = desc;
    slot->instance = std::move (inst);
    slot->prepare (currentSampleRate, currentBlockSize);
    return slot;
}

String AudioEngine::addPlugin (int ch, const PluginDescription& desc, int insertIndex)
{
    String err;
    auto slot = createSlot (desc, err);
    if (slot == nullptr) return err;

    {
        const SpinLock::ScopedLockType sl (chainLock);
        auto& v = channel (ch).slots;
        if (insertIndex < 0 || insertIndex > (int) v.size()) insertIndex = (int) v.size();
        v.insert (v.begin() + insertIndex, std::move (slot));
    }
    notifyChanged();
    return {};
}

String AudioEngine::replacePlugin (int ch, int index, const PluginDescription& desc)
{
    auto& v = channel (ch).slots;
    if (! isPositiveAndBelow (index, (int) v.size())) return addPlugin (ch, desc);

    String err;
    auto slot = createSlot (desc, err);
    if (slot == nullptr) return err;

    slot->enabled = v[(size_t) index]->enabled.load();
    std::unique_ptr<PluginSlot> old;
    {
        const SpinLock::ScopedLockType sl (chainLock);
        old = std::move (v[(size_t) index]);
        v[(size_t) index] = std::move (slot);
    }
    old.reset();
    notifyChanged();
    return {};
}

void AudioEngine::removePlugin (int ch, int index)
{
    auto& v = channel (ch).slots;
    if (! isPositiveAndBelow (index, (int) v.size())) return;

    std::unique_ptr<PluginSlot> old;
    {
        const SpinLock::ScopedLockType sl (chainLock);
        old = std::move (v[(size_t) index]);
        v.erase (v.begin() + index);
    }
    if (old && old->instance) old->instance->releaseResources();
    old.reset();
    notifyChanged();
}

void AudioEngine::movePlugin (int ch, int from, int to)
{
    auto& v = channel (ch).slots;
    const int n = (int) v.size();
    if (! isPositiveAndBelow (from, n)) return;
    to = jlimit (0, n - 1, to);
    if (from == to) return;

    {
        const SpinLock::ScopedLockType sl (chainLock);
        auto item = std::move (v[(size_t) from]);
        v.erase (v.begin() + from);
        v.insert (v.begin() + to, std::move (item));
    }
    notifyChanged();
}

void AudioEngine::setPluginEnabled (int ch, int index, bool e)
{
    auto& v = channel (ch).slots;
    if (isPositiveAndBelow (index, (int) v.size()))
        v[(size_t) index]->enabled = e;
}

namespace
{
    struct PluginWindow : public DocumentWindow
    {
        PluginWindow (const String& name)
            : DocumentWindow (name, Colour (0xff1a1230), DocumentWindow::closeButton | DocumentWindow::minimiseButton)
        {
            setUsingNativeTitleBar (true);
        }

        void closeButtonPressed() override
        {
            setVisible (false);
            clearContentComponent();   // エディタを破棄（次に開く時に作り直す）
        }
    };
}

void AudioEngine::showEditor (int ch, int index)
{
    auto& v = channel (ch).slots;
    if (! isPositiveAndBelow (index, (int) v.size())) return;
    auto& slot = *v[(size_t) index];
    if (slot.instance == nullptr) return;

    if (slot.editorWindow == nullptr)
        slot.editorWindow = std::make_unique<PluginWindow> (slot.description.name
                                                            + (ch == 0 ? "  [Main]" : "  [FX]"));

    auto& w = *slot.editorWindow;
    if (w.getContentComponent() == nullptr)
    {
        AudioProcessorEditor* ed = slot.instance->hasEditor() ? slot.instance->createEditorIfNeeded() : nullptr;
        if (ed == nullptr) ed = new GenericAudioProcessorEditor (*slot.instance);
        w.setContentOwned (ed, true);
        w.setResizable (ed->isResizable(), false);
        w.centreWithSize (w.getWidth(), w.getHeight());
    }
    w.setVisible (true);
    w.toFront (true);
}

//==============================================================================
std::unique_ptr<XmlElement> AudioEngine::createStateXml()
{
    auto root = std::make_unique<XmlElement> ("AotoVSTRouterPreset");
    root->setAttribute ("version", 1);

    for (auto* ch : { &mainCh, &fxCh })
    {
        auto* c = root->createNewChildElement ("Channel");
        c->setAttribute ("id", ch->id);
        c->setAttribute ("gainDb", ch->gainDb.load());
        c->setAttribute ("pan", ch->pan.load());
        c->setAttribute ("toOutput", ch->toOutput.load());
        c->setAttribute ("toMonitor", ch->toMonitor.load());

        for (auto& s : ch->slots)
        {
            auto* e = c->createNewChildElement ("Slot");
            e->setAttribute ("enabled", s->enabled.load());
            e->addChildElement (s->description.createXml().release());

            if (s->instance != nullptr)
            {
                MemoryBlock mb;
                s->instance->getStateInformation (mb);
                e->createNewChildElement ("State")->addTextElement (mb.toBase64Encoding());
            }
        }
    }
    return root;
}

void AudioEngine::restoreStateXml (const XmlElement& root, StringArray& errors)
{
    for (auto* c : root.getChildWithTagNameIterator ("Channel"))
    {
        auto& ch = c->getStringAttribute ("id") == "fx" ? fxCh : mainCh;
        ch.gainDb    = (float) c->getDoubleAttribute ("gainDb", ch.gainDb);
        ch.pan       = (float) c->getDoubleAttribute ("pan", 0.0);
        ch.toOutput  = c->getBoolAttribute ("toOutput", true);
        ch.toMonitor = c->getBoolAttribute ("toMonitor", false);

        std::vector<std::unique_ptr<PluginSlot>> newSlots;
        for (auto* e : c->getChildWithTagNameIterator ("Slot"))
        {
            PluginDescription desc;
            if (auto* pd = e->getChildByName ("PLUGIN"))
                desc.loadFromXml (*pd);

            // ファイルの場所が変わっていても、スキャン済みリストから探す
            if (! File (desc.fileOrIdentifier).exists())
                for (auto& t : knownPlugins.getTypes())
                    if (t.isDuplicateOf (desc)) { desc = t; break; }

            String err;
            auto slot = createSlot (desc, err);
            if (slot == nullptr) { errors.add (desc.name + "："_j + err); continue; }

            slot->enabled = e->getBoolAttribute ("enabled", true);
            if (auto* st = e->getChildByName ("State"))
            {
                MemoryBlock mb;
                if (mb.fromBase64Encoding (st->getAllSubText()) && mb.getSize() > 0)
                    slot->instance->setStateInformation (mb.getData(), (int) mb.getSize());
            }
            newSlots.push_back (std::move (slot));
        }

        {
            const SpinLock::ScopedLockType sl (chainLock);
            std::swap (ch.slots, newSlots);
        }
        newSlots.clear();   // 古いプラグインはロック外で破棄
    }
    notifyChanged();
}

StringArray AudioEngine::getPresetNames() const
{
    StringArray names;
    for (auto& f : getPresetFolder().findChildFiles (File::findFiles, false, "*.xml"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool AudioEngine::savePreset (const String& name)
{
    auto safe = File::createLegalFileName (name.trim());
    if (safe.isEmpty()) return false;
    auto xml = createStateXml();
    xml->setAttribute ("name", name);
    if (! xml->writeTo (getPresetFolder().getChildFile (safe + ".xml")))
        return false;
    currentPresetName = safe;
    saveSettings();
    return true;
}

bool AudioEngine::loadPreset (const File& file, StringArray& errors)
{
    auto xml = XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("AotoVSTRouterPreset"))
    {
        errors.add ("プリセットファイルを読めませんでした"_j);
        return false;
    }
    restoreStateXml (*xml, errors);
    currentPresetName = file.getFileNameWithoutExtension();
    saveSettings();
    return true;
}

void AudioEngine::saveSettings()
{
    auto* props = appProperties.getUserSettings();
    if (deviceManager.getCurrentAudioDevice() != nullptr && pendingDeviceXml == nullptr)
        if (auto dev = deviceManager.createStateXml())
            props->setValue ("audioDevice", dev.get());
    if (auto kp = knownPlugins.createXml())
        props->setValue ("knownPlugins", kp.get());

    props->setValue ("inputChannel", inputChannel.load());
    props->setValue ("inputStereo", inputStereo.load());
    props->setValue ("outputPair", outputPair.load());
    props->setValue ("monitorPair", monitorPair.load());
    props->setValue ("monoOutput", monoOutput.load());
    props->setValue ("monitorGainDb", (double) monitorGainDb.load());
    props->setValue ("presetName", currentPresetName);

    if (auto s = createStateXml())
        props->setValue ("session", s.get());

    props->saveIfNeeded();
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart (AudioIODevice* dev)
{
    currentSampleRate = dev->getCurrentSampleRate();
    currentBlockSize  = jmax (32, dev->getCurrentBufferSizeSamples());

    const SpinLock::ScopedLockType sl (chainLock);
    for (auto* ch : { &mainCh, &fxCh })
    {
        ch->buffer.setSize (2, currentBlockSize, false, true, false);
        for (auto* sm : { &ch->smGainL, &ch->smGainR })
            sm->reset (currentSampleRate, 0.02);
        for (auto& s : ch->slots)
            s->prepare (currentSampleRate, currentBlockSize);
    }
    prepared = true;
}

void AudioEngine::audioDeviceStopped()
{
    const SpinLock::ScopedLockType sl (chainLock);
    prepared = false;
    for (auto* ch : { &mainCh, &fxCh })
        for (auto& s : ch->slots)
            if (s->instance) s->instance->releaseResources();
}

void AudioEngine::processChannel (ChannelStrip& ch, int n) noexcept
{
    for (auto& s : ch.slots)
        s->process (ch.buffer, n);
}

static void applyGainPan (ChannelStrip& ch, AudioBuffer<float>& b, int n)
{
    const float g = dbToGain (ch.gainDb.load (std::memory_order_relaxed));
    const float p = ch.pan.load (std::memory_order_relaxed);
    // バランス式パン（センターで 0dB）
    ch.smGainL.setTargetValue (g * (p > 0 ? 1.0f - p : 1.0f));
    ch.smGainR.setTargetValue (g * (p < 0 ? 1.0f + p : 1.0f));
    ch.smGainL.applyGain (b.getWritePointer (0), n);
    ch.smGainR.applyGain (b.getWritePointer (1), n);
}

static void storePeak (std::atomic<float>& a, float v)
{
    if (v > a.load (std::memory_order_relaxed)) a.store (v, std::memory_order_relaxed);
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* in, int numIn,
                                                    float* const* out, int numOut, int numSamples,
                                                    const AudioIODeviceCallbackContext&)
{
    ScopedNoDenormals noDenormals;

    for (int i = 0; i < numOut; ++i)
        if (out[i] != nullptr) FloatVectorOperations::clear (out[i], numSamples);

    const SpinLock::ScopedLockType sl (chainLock);
    if (! prepared) return;

    const int inCh  = inputChannel.load (std::memory_order_relaxed);
    const bool inSt = inputStereo.load (std::memory_order_relaxed);
    const int oL = outputPair.load (std::memory_order_relaxed);
    const int mL = monitorPair.load (std::memory_order_relaxed);
    const bool mono = monoOutput.load (std::memory_order_relaxed);
    const float monGain = dbToGain (monitorGainDb.load (std::memory_order_relaxed));

    auto getIn = [&] (int idx) -> const float* { return isPositiveAndBelow (idx, numIn) ? in[idx] : nullptr; };
    auto getOut = [&] (int idx) -> float* { return isPositiveAndBelow (idx, numOut) ? out[idx] : nullptr; };

    for (int offset = 0; offset < numSamples; offset += currentBlockSize)
    {
        const int n = jmin (currentBlockSize, numSamples - offset);
        auto& mb = mainCh.buffer;
        auto& fb = fxCh.buffer;
        if (n > mb.getNumSamples()) break;

        // ---- 入力 → Main ----
        const float* srcL = getIn (inCh);
        const float* srcR = inSt ? getIn (inCh + 1) : srcL;
        if (srcL != nullptr) FloatVectorOperations::copy (mb.getWritePointer (0), srcL + offset, n);
        else                 FloatVectorOperations::clear (mb.getWritePointer (0), n);
        if (srcR != nullptr) FloatVectorOperations::copy (mb.getWritePointer (1), srcR + offset, n);
        else                 FloatVectorOperations::clear (mb.getWritePointer (1), n);

        // ---- Main の VST チェーン ----
        processChannel (mainCh, n);

        // ---- FX へセンド（Main の VST 後・フェーダー前）→ FX の VST チェーン ----
        fb.copyFrom (0, 0, mb, 0, 0, n);
        fb.copyFrom (1, 0, mb, 1, 0, n);
        processChannel (fxCh, n);

        applyGainPan (mainCh, mb, n);
        applyGainPan (fxCh, fb, n);

        storePeak (mainCh.peakL, mb.getMagnitude (0, 0, n));
        storePeak (mainCh.peakR, mb.getMagnitude (1, 0, n));
        storePeak (fxCh.peakL, fb.getMagnitude (0, 0, n));
        storePeak (fxCh.peakR, fb.getMagnitude (1, 0, n));

        // ---- 配信出力 ----
        float* outL = getOut (oL);
        float* outR = getOut (oL + 1);
        float pkL = 0, pkR = 0;
        if (outL != nullptr)
        {
            for (auto* ch : { &mainCh, &fxCh })
            {
                if (! ch->toOutput.load (std::memory_order_relaxed)) continue;
                auto& b = ch->buffer;
                if (mono)
                {
                    FloatVectorOperations::addWithMultiply (outL + offset, b.getReadPointer (0), 0.5f, n);
                    FloatVectorOperations::addWithMultiply (outL + offset, b.getReadPointer (1), 0.5f, n);
                }
                else
                {
                    FloatVectorOperations::add (outL + offset, b.getReadPointer (0), n);
                    if (outR != nullptr) FloatVectorOperations::add (outR + offset, b.getReadPointer (1), n);
                }
            }
            if (mono && outR != nullptr)
                FloatVectorOperations::copy (outR + offset, outL + offset, n);

            pkL = FloatVectorOperations::findMaximum (outL + offset, n);
            pkL = jmax (pkL, -FloatVectorOperations::findMinimum (outL + offset, n));
            if (outR != nullptr)
                pkR = jmax (FloatVectorOperations::findMaximum (outR + offset, n),
                            -FloatVectorOperations::findMinimum (outR + offset, n));
            else
                pkR = pkL;
        }
        storePeak (outPeakL, pkL);
        storePeak (outPeakR, pkR);

        // ---- モニター（ヘッドホンボタンが ON のチャンネルだけ）----
        float* monL = getOut (mL);
        float* monR = getOut (mL + 1);
        if (mL >= 0 && monL != nullptr && monGain > 0)
        {
            for (auto* ch : { &mainCh, &fxCh })
            {
                if (! ch->toMonitor.load (std::memory_order_relaxed)) continue;
                auto& b = ch->buffer;
                FloatVectorOperations::addWithMultiply (monL + offset, b.getReadPointer (0), monGain, n);
                if (monR != nullptr)
                    FloatVectorOperations::addWithMultiply (monR + offset, b.getReadPointer (1), monGain, n);
            }
        }
    }
}
