#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>

void checkImpactPanel(const juce::File& folder, bool exportFrames);

static void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static juce::TextButton* button(juce::Component& parent, const juce::String& text)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* b = dynamic_cast<juce::TextButton*>(child))
            if (b->getButtonText() == text) return b;
        if (auto* found = button(*child, text)) return found;
    }
    return nullptr;
}

static void click(juce::Component& parent, const juce::String& text)
{
    auto* b = button(parent, text); require(b != nullptr, "Missing control");
    if (b->getClickingTogglesState()) b->setToggleState(!b->getToggleState(), juce::sendNotificationSync);
    else if (b->onClick) b->onClick();
}

static void snapshot(juce::Component& editor, const juce::File& folder, const juce::String& name)
{
    auto file = folder.getChildFile(name + ".png");
    juce::FileOutputStream stream(file); require(stream.openedOk(), "Cannot write screenshot");
    stream.setPosition(0); stream.truncate();
    juce::PNGImageFormat png;
    require(png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f), stream), "Cannot encode screenshot");
}

static juce::Component* findNamed(juce::Component& parent, const juce::String& name)
{
    for (auto* child : parent.getChildren())
    {
        if (child->getName() == name) return child;
        if (auto* match = findNamed(*child, name)) return match;
    }
    return nullptr;
}

static void checkBounds(juce::Component& component)
{
    for (auto* child : component.getChildren())
    {
        if (!child->isVisible()) continue;
        if (!component.getLocalBounds().contains(child->getBounds()))
            throw std::runtime_error("Visible control outside its parent: " + child->getName().toStdString()
                + " " + child->getBounds().toString().toStdString() + " in " + component.getName().toStdString());
        // Slider and list internals may extend beyond their clipped region.
        if (dynamic_cast<juce::Slider*>(child) == nullptr && dynamic_cast<juce::ListBox*>(child) == nullptr)
            checkBounds(*child);
    }
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    try
    {
        const juce::File folder(argc > 1 ? argv[1] : "/tmp/breach-ui-smoke");
        folder.createDirectory();
        BreachResonatorAudioProcessor processor;
        auto set = [&](const char* id, float value)
        {
            auto* p = processor.apvts.getParameter(id); p->setValueNotifyingHost(p->convertTo0to1(value));
        };
        // A saved session with a custom voicing must survive opening and navigating UI.
        set("note_1", 1.0f); set("vDetune_0", -7.0f); set("vDetune_1", 11.0f);
        juce::MemoryBlock before; processor.getStateInformation(before);
        auto editor = std::make_unique<BreachResonatorAudioProcessorEditor>(processor);
        require(editor->getWidth() == 1024 && editor->getHeight() == 765, "Expanded size changed");
        checkBounds(*editor); snapshot(*editor, folder, "01-voices");
        click(*editor, "INVERT"); checkBounds(*editor); snapshot(*editor, folder, "02-inverted");
        click(*editor, "INVERT");
        for (const auto* tab : {"B // RESONATOR", "C // MODULATION", "D // GLOBAL", "E // DYNAMICS", "F // REVERB", "G // EQ", "A // VOICES"})
        {
            click(*editor, tab); checkBounds(*editor);
            snapshot(*editor, folder, juce::String("tab-") + tab[0]);
        }
        click(*editor, "VOICE EDIT >"); checkBounds(*editor); snapshot(*editor, folder, "03-voice-detail");
        click(*editor, "< VOICES");
        for (auto* child : editor->getChildren())
            if (auto* keyboard = dynamic_cast<MangaKeyboard*>(child)) keyboard->onSettings();
        checkBounds(*editor); snapshot(*editor, folder, "04-keyboard-settings");
        click(*editor, "CLOSE"); click(*editor, "TRIGGER");
        click(*editor, "ADVANCED"); require(editor->getHeight() == 562, "Collapsed size changed");
        checkBounds(*editor); snapshot(*editor, folder, "05-collapsed");
        GunSelectorStrip* modeCards = nullptr;
        for (auto* child : editor->getChildren())
            if (auto* strip = dynamic_cast<GunSelectorStrip*>(child)) modeCards = strip;
        require(modeCards != nullptr, "Missing mode cards");
        auto* modeParameter = processor.apvts.getRawParameterValue("gunMode");
        const float originalMode = modeParameter->load();
        const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
        for (int mode = 0; mode < 5; ++mode)
        {
            const auto x = static_cast<float>(modeCards->getWidth()) * (static_cast<float>(mode) + .5f) / 5;
            const juce::Point<float> point(x, 38.0f);
            const auto now = juce::Time::getCurrentTime();
            const juce::MouseEvent event(mouse, point, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
                                        1, 0, 0, 0, 0, modeCards, modeCards, now, point, now, 1, false);
            modeCards->mouseDown(event);
            require(std::abs(modeParameter->load() - static_cast<float>(mode)) < .01f, "Mode card selected the wrong DSP mode");
            snapshot(*modeCards, folder, "mode-cards-" + juce::String(mode + 1));
        }
        click(*editor, "INVERT");
        snapshot(*modeCards, folder, "mode-cards-invert");
        click(*editor, "INVERT");
        set("gunMode", originalMode);
        click(*editor, "ADVANCED");
        click(*editor, "B // RESONATOR");
        click(*editor, "SMEAR: SHORT");require(processor.getSmearDspRevision()==1,"SMEAR legacy selection failed");
        click(*editor, "SMEAR: LEGACY");require(processor.isShortSmearEnabled(),"Short SMEAR selection failed");
        click(*editor, "E // DYNAMICS");
        click(*editor, "OTT: OFF");
        require(processor.apvts.getRawParameterValue("ottEnable")->load() > .5f, "OTT enable failed");
        auto* depth = dynamic_cast<juce::Slider*>(findNamed(*editor, "OTT DEPTH"));
        auto* upward = dynamic_cast<juce::Slider*>(findNamed(*editor, "UPWARD"));
        auto* timeKnob = dynamic_cast<juce::Slider*>(findNamed(*editor, "TIME"));
        auto* split = findNamed(*editor, "LOW SPLIT");
        require(depth && depth->isVisible() && upward && timeKnob && split && !split->isVisible(),
                "OTT controls are not organised into the main and setup views");
        require(upward->getTextFromValue(.5).startsWith("100") && std::abs(upward->getValueFromText("125%") - .625) < .001
                && timeKnob->getTextFromValue(100).endsWithChar('%'), "OTT controls display or parse the wrong units");
        processor.prepareToPlay(48000, 256);
        for (int block = 0; block < 23; ++block)
        {
            juce::AudioBuffer<float> audio(2, 256);
            juce::MidiBuffer midi;
            for (int n = 0; n < 256; ++n)
            {
                const float phase = static_cast<float>(block * 256 + n) * juce::MathConstants<float>::twoPi / 48000;
                const float sample = .31f * std::sin(52 * phase) + .012f * std::sin(490 * phase) + .0025f * std::sin(4100 * phase);
                audio.setSample(0, n, sample); audio.setSample(1, n, sample * .72f);
            }
            processor.processBlock(audio, midi);
        }
        require(processor.telemetry.ottGainDb[0].load() < -.5f && processor.telemetry.ottGainDb[2].load() > .5f,
                "OTT drawer did not receive live bidirectional gain readings");
        checkBounds(*editor);
        snapshot(*editor, folder, "ott-main");
        click(*editor, "SETUP >");
        require(split->isVisible(), "OTT setup did not reveal crossover controls");
        checkBounds(*editor);
        snapshot(*editor, folder, "ott-setup");
        click(*editor, "MODEL: FAUST");
        require(!processor.usesFaustOtt() && upward->getTextFromValue(.5).startsWith("50"), "Classic OTT selection failed");
        click(*editor, "MODEL: CLASSIC");
        require(processor.usesFaustOtt(), "Faust OTT selection failed");
        click(*editor, "< BACK");
        require(!split->isVisible(), "OTT setup did not close");
        click(*editor, "INVERT"); snapshot(*editor, folder, "ott-inverted"); click(*editor, "INVERT");
        processor.releaseResources();
        click(*editor, "OTT: ON");
        click(*editor, "F // REVERB");
        click(*editor, "REVERB: OFF");require(processor.apvts.getRawParameterValue("reverbEnable")->load()>.5f,"Reverb enable failed");
        click(*editor, "REVERB: ON");
        for(auto* child:editor->getChildren())
            if(auto* box=dynamic_cast<juce::ComboBox*>(child))
                if(box->getName()=="REVERB TYPE")
                {
                    box->setSelectedId(4,juce::sendNotificationSync);
                    require(processor.apvts.getRawParameterValue("reverbType")->load()==3,"Reverb type UI failed");
                    for(auto* component:editor->getChildren())
                        if(auto* slider=dynamic_cast<juce::Slider*>(component))
                            if(slider->getName()=="SHIMMER")require(slider->isEnabled(),"Shimmer amount is disabled in Shimmer mode");
                    snapshot(*editor,folder,"06-shimmer-controls");
                    box->setSelectedId(1,juce::sendNotificationSync);
                }
        click(*editor, "G // EQ");
        click(*editor, "EQ OFF");
        require(processor.apvts.getRawParameterValue("outputEqEnable")->load() > .5f, "EQ master control did not enable audio");
        auto* type = dynamic_cast<juce::ComboBox*>(findNamed(*editor, "EQ FILTER TYPE"));
        auto* graph = dynamic_cast<OutputEqGraph*>(findNamed(*editor, "OUTPUT EQ RESPONSE"));
        auto* gain = dynamic_cast<juce::Slider*>(findNamed(*editor, "EQ GAIN (DB)"));
        require(type && graph && gain, "Missing EQ inspector or graph");
        type->setSelectedId(3, juce::sendNotificationSync);
        require(processor.readOutputEqBand(0).shape == breach::eq::Shape::lowCut48 && !gain->isEnabled(), "Steep low cut controls are wrong");
        const auto inputSource = juce::Desktop::getInstance().getMainMouseSource();
        const auto time = juce::Time::getCurrentTime();
        const float x = 36 + static_cast<float>(std::log(8.0) / std::log(2000.0)) * static_cast<float>(graph->getWidth() - 50);
        const juce::Point<float> node(x, 11 + .25f * static_cast<float>(graph->getHeight() - 31));
        auto event = [&](juce::Point<float> position, bool dragged)
        {
            return juce::MouseEvent(inputSource, position, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
                1, 0, 0, 0, 0, graph, graph, time, node, time, 1, dragged);
        };
        graph->mouseDown(event(node, false));
        graph->mouseDrag(event(node.translated(55, -5), true));
        graph->mouseUp(event(node.translated(55, -5), true));
        require(processor.readOutputEqBand(0).hz > 120 && processor.readOutputEqBand(0).q > .75, "Dragging low-cut node failed");
        auto* bandEight = dynamic_cast<juce::TextButton*>(findNamed(*editor, "EQ BAND 8"));
        require(bandEight != nullptr, "Missing eighth EQ band");
        bandEight->onClick();
        click(*editor, "BAND OFF");
        type->setSelectedId(5, juce::sendNotificationSync);
        gain->setValue(-6, juce::sendNotificationSync);
        require(processor.readOutputEqBand(7).enabled && std::abs(processor.readOutputEqBand(7).gain + 6) < .01, "Eighth band inspector edits the wrong parameters");
        snapshot(*editor, folder, "07-eq-active");
        click(*editor, "INVERT"); snapshot(*editor, folder, "08-eq-inverted"); click(*editor, "INVERT");
        processor.setStateInformation(before.getData(), static_cast<int>(before.getSize()));
        click(*editor, "D // GLOBAL");
        click(*editor, "LEVELS: BALANCED");
        require(processor.getBalanceRevision()==1,"Levels UI did not choose legacy");
        click(*editor, "LEVELS: LEGACY");
        require(processor.getBalanceRevision()==2,"Levels UI did not choose balanced");
        click(*editor, "MODAL: RINGS");
        require(processor.getModalDspRevision()==1,"Modal UI did not choose legacy");
        click(*editor, "MODAL: LEGACY");
        require(processor.getModalDspRevision()==2,"Modal UI did not choose Rings");
        click(*editor, "FORMANT: STK");
        require(processor.getFormantDspRevision()==1,"Formant UI did not choose legacy");
        click(*editor, "FORMANT: LEGACY");
        require(processor.getFormantDspRevision()==2,"Formant UI did not choose STK");
        click(*editor, "STRING: RINGS");
        require(processor.getStringDspRevision()==1,"String UI did not choose legacy");
        click(*editor, "STRING: LEGACY");
        require(processor.getStringDspRevision()==2,"String UI did not choose Rings");
        click(*editor, "DRIVE DSP: NEW");
        require(processor.getNonlinearDspRevision()==1,"DRIVE UI did not select legacy");
        click(*editor, "DRIVE DSP: LEGACY");
        require(processor.getNonlinearDspRevision()==2,"DRIVE UI did not select new engine");
        click(*editor, "SPATIAL: NEW");
        require(processor.getSpatialDspRevision() == 1, "UI did not select legacy spatial engine");
        click(*editor, "SPATIAL: LEGACY");
        require(processor.getSpatialDspRevision() == 2, "UI did not select upgraded spatial engine");
        click(*editor, "A // VOICES");
        juce::MemoryBlock after; processor.getStateInformation(after);
        require(before == after, "View-only interactions changed saved audio parameters");
        auto raw = [&](const char* id) { return processor.apvts.getRawParameterValue(id)->load(); };
        auto setKnob = [&](const char* name, double value)
        {
            auto* slider = dynamic_cast<juce::Slider*>(findNamed(*editor, name));
            require(slider != nullptr && slider->isVisible(), "Missing voice or route knob");
            slider->setValue(value, juce::sendNotificationSync);
        };
        const float panBefore = raw("vPan_1");
        setKnob("TUNE SPREAD", 10);
        setKnob("PAN SPREAD", 60);
        require(std::abs(raw("voiceTuneSpread") - 10) < .001 && std::abs(raw("voicePanSpread") - 60) < .001,
                "Voice macros did not reach their own parameters");
        require(std::abs(raw("vDetune_0") + 7) < .01 && std::abs(raw("vDetune_1") - 11) < .01
                && std::abs(raw("vPan_1") - panBefore) < .001, "Macro changed per-voice settings");
        click(*editor, "VOICE EDIT >");
        auto* slot = dynamic_cast<juce::ComboBox*>(findNamed(*editor, "VOICE SLOT"));
        require(slot != nullptr, "Missing voice slot selector");
        slot->setSelectedId(12, juce::sendNotificationSync);
        setKnob("TUNE", -4);
        setKnob("RING TRIM", 1.6);
        require(std::abs(raw("vDetune_11") + 4) < .01 && std::abs(raw("vDecay_11") - 1.6f) < .001
                && std::abs(raw("vDetune_1") - 11) < .01, "Editing V12 changed the wrong voice");
        checkBounds(*editor);
        snapshot(*editor, folder, "09-voice-slot");
        click(*editor, "< VOICES");
        click(*editor, "B // RESONATOR");
        click(*editor, "RING: NEW");
        require(!processor.usesRingTime(), "Legacy ring-time selection failed");
        click(*editor, "RING: LEGACY");
        require(processor.usesRingTime(), "New ring-time selection failed");
        click(*editor, "C // MODULATION");
        auto* source = dynamic_cast<juce::ComboBox*>(findNamed(*editor, "ROUTE SOURCE"));
        auto* destination = dynamic_cast<juce::ComboBox*>(findNamed(*editor, "ROUTE DESTINATION"));
        require(source && destination, "Missing route selectors");
        source->setSelectedId(2, juce::sendNotificationSync);
        destination->setSelectedId(5, juce::sendNotificationSync);
        click(*editor, "ADD");
        require(std::abs(raw("mod_lfo2_tilt") - .25f) < .001, "Route ADD set the wrong parameter");
        setKnob("AMOUNT", -.4);
        click(*editor, "ADD");
        require(std::abs(raw("mod_lfo2_tilt") + .4f) < .001, "Editing or reselecting route lost its amount");
        source->setSelectedId(1, juce::sendNotificationSync);
        destination->setSelectedId(2, juce::sendNotificationSync);
        click(*editor, "ADD");
        snapshot(*editor, folder, "10-active-routes");
        click(*editor, "CLEAR");
        require(std::abs(raw("mod_lfo1_drift")) < .001 && std::abs(raw("mod_lfo2_tilt") + .4f) < .001,
                "Route CLEAR touched the wrong parameter");
        processor.setStateInformation(before.getData(), static_cast<int>(before.getSize()));
        processor.getStateInformation(after); require(before == after, "State roundtrip failed");
        editor.reset();
        checkImpactPanel(folder, argc > 2 && juce::String(argv[2]) == "--impact-frames");
        std::cout << "PASS: view navigation, both themes, five mode cards, control bounds, voice macros, selected slot, route add/edit/clear, ring revision, EQ gestures and inspector, saved-state preservation and roundtrip\n";
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    return 0;
}
