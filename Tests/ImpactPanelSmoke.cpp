#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>

namespace
{
constexpr int framesPerSecond = 60;
constexpr int sampleRate = 48000;
constexpr int samplesPerFrame = sampleRate / framesPerSecond;
constexpr double frameMs = 1000.0 / framesPerSecond;

void require(bool success, const char* message)
{
    if (!success) throw std::runtime_error(message);
}

void set(BreachResonatorAudioProcessor& processor, const char* id, float value)
{
    auto& parameter = *processor.apvts.getParameter(id);
    parameter.setValueNotifyingHost(parameter.convertTo0to1(value));
}

juce::Image render(MangaSplitViewport& panel)
{
    return panel.createComponentSnapshot(panel.getLocalBounds(), true);
}

int difference(const juce::Image& first, const juce::Image& second, juce::Rectangle<int> area)
{
    const juce::Image::BitmapData a(first, juce::Image::BitmapData::readOnly);
    const juce::Image::BitmapData b(second, juce::Image::BitmapData::readOnly);
    int changed = 0;
    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x)
            if (a.getPixelColour(x, y) != b.getPixelColour(x, y)) ++changed;
    return changed;
}

void save(const juce::Image& image, const juce::File& folder, const juce::String& name)
{
    juce::FileOutputStream stream(folder.getChildFile(name + ".png"));
    require(stream.openedOk(), "Cannot create impact screenshot");
    stream.setPosition(0);
    stream.truncate();
    require(juce::PNGImageFormat().writeImageToStream(image, stream), "Cannot encode impact screenshot");
}

float chord(double seconds)
{
    const auto phase = juce::MathConstants<double>::twoPi * seconds;
    return static_cast<float>((std::sin(phase * 130.81) + std::sin(phase * 164.81) + std::sin(phase * 196.00)) / 3);
}

float kick(double seconds)
{
    if (seconds < 0 || seconds > .018) return 0.0f;
    const double phase = juce::MathConstants<double>::twoPi * seconds;
    return static_cast<float>((.68 * std::sin(phase * 124) + .32 * std::sin(phase * 685)) * std::exp(-seconds / .006));
}

template <typename Signal>
void processFrame(BreachResonatorAudioProcessor& processor, int frame, Signal signal, juce::OutputStream* output = nullptr)
{
    juce::AudioBuffer<float> block(2, samplesPerFrame);
    for (int sample = 0; sample < samplesPerFrame; ++sample)
    {
        const double seconds = static_cast<double>(frame * samplesPerFrame + sample) / sampleRate;
        const auto values = signal(seconds);
        block.setSample(0, sample, values[0]);
        block.setSample(1, sample, values[1]);
    }
    juce::MidiBuffer midi;
    processor.processBlock(block, midi);
    if (output)
        for (int sample = 0; sample < samplesPerFrame; ++sample)
            for (int channel = 0; channel < 2; ++channel)
                require(output->writeFloat(block.getSample(channel, sample)), "Cannot write preview audio");
}

std::array<float, 2> stereo(float value)
{
    return { value, value };
}

void verifyTapSeparation()
{
    BreachResonatorAudioProcessor processor;
    set(processor, "mix", 0);
    set(processor, "oversampling", 2);
    processor.prepareToPlay(sampleRate, samplesPerFrame);
    for (int frame = 0; frame < 15; ++frame)
        processFrame(processor, frame, [](double t) { return std::array<float, 2> { 0, .3f * chord(t) }; });
    auto& signals = processor.telemetry.impact;
    require(signals.output[0].load() < 1.0e-6f && signals.output[1].load() > .045f,
            "4x final output meters did not maintain stereo channel separation");

    breach::impact::Analysis analysis;
    auto texture = [&](float frequency)
    {
        analysis.reset(signals);
        analysis.setRates(sampleRate, sampleRate);
        for (int i = 0; i < sampleRate / 10; ++i)
        {
            const auto wave = std::sin(juce::MathConstants<float>::twoPi * frequency
                * static_cast<float>(i) / static_cast<float>(sampleRate));
            analysis.observeResonance(wave, -wave);
        }
        analysis.publishCore(signals);
        return signals.brightness.load();
    };
    require(texture(180) < .1f && texture(6600) > .9f, "Resonant low/high split does not distinguish debris textures");
}
} // namespace

void checkImpactPanel(const juce::File& folder, bool exportFrames)
{
    verifyTapSeparation();

    BreachResonatorAudioProcessor processor;
    processor.applyChordPreset(0);
    set(processor, "oversampling", 0);
    set(processor, "decay", .20f);
    set(processor, "density", .92f);
    set(processor, "mix", .66f);
    set(processor, "reverbEnable", 1);
    set(processor, "reverbMix", .67f);
    set(processor, "reverbDecay", 3.6f);
    set(processor, "reverbPreDelay", 38);
    set(processor, "vLevel_1", 0);
    processor.prepareToPlay(sampleRate, samplesPerFrame);

    BreachResonatorAudioProcessorEditor editor(processor);
    MangaSplitViewport* viewport = nullptr;
    juce::TextButton* advanced = nullptr;
    juce::TextButton* invert = nullptr;
    for (auto* child : editor.getChildren())
    {
        if (auto* view = dynamic_cast<MangaSplitViewport*>(child)) viewport = view;
        if (auto* button = dynamic_cast<juce::TextButton*>(child))
        {
            if (button->getButtonText() == "ADVANCED") advanced = button;
            if (button->getButtonText() == "INVERT") invert = button;
        }
    }
    require(viewport && advanced && invert, "Impact viewport or theme controls missing from the editor");
    advanced->setToggleState(false, juce::sendNotificationSync);
    auto& panel = *viewport;
    const auto idle = render(panel);
    save(idle, folder, "impact-idle");
    const auto frames = folder.getChildFile("impact-frames");
    std::unique_ptr<juce::FileOutputStream> audio;
    if (exportFrames)
    {
        require(frames.createDirectory().wasOk(), "Cannot create impact frame directory");
        audio = std::make_unique<juce::FileOutputStream>(frames.getChildFile("audio.f32le"));
        require(audio->openedOk(), "Cannot open preview audio file");
        audio->setPosition(0);
        audio->truncate();
    }

    const auto& telemetry = processor.telemetry.impact;
    const juce::Rectangle<int> inputArea(5, 8, 215, 295);
    const juce::Rectangle<int> onsetArea(48, 27, 214, 91);
    const juce::Rectangle<int> wallArea(340, 44, 258, 237);
    const juce::Rectangle<int> meterArea(607, 36, 62, 221);
    int strikes = 0;
    std::uint32_t observed = 0;
    float excitationOnHold = 0, voiceOnHold = 0, outputAfterHold = 0;
    bool checkedTail = false;
    const int frameCount = 330;
    for (int frame = 0; frame < frameCount; ++frame)
    {
        processFrame(processor, frame, [](double time)
        {
            const float hit = kick(time - .10) + kick(time - .65) + kick(time - .80) + kick(time - .95);
            const float held = time >= 1.40 && time < 2.25 ? .57f * chord(time - 1.40) : 0.0f;
            return std::array<float, 2> { hit + held, hit + .69f * held };
        }, audio.get());
        panel.tickAnimation(frameMs * (frame + 1));
        const auto serial = telemetry.strike.load() >> 8;
        if (serial != observed)
        {
            strikes += static_cast<int>(serial - observed);
            observed = serial;
            std::cout << "Impact onset " << strikes << " at frame " << frame << '\n';
        }

        if (frame == 7)
        {
            const auto image = render(panel);
            require(strikes == 1 && difference(idle, image, onsetArea) > 300, "A single hit did not draw a burst and lettering");
            save(image, folder, "impact-single-hit");
            save(editor.createComponentSnapshot(editor.getLocalBounds(), true), folder, "impact-main");
        }
        if (frame == 58)
        {
            require(strikes == 4, "Three successive drum hits did not produce three new bursts");
            save(render(panel), folder, "impact-repeated-hits");
        }
        if (frame == 119)
        {
            const auto image = render(panel);
            excitationOnHold = telemetry.excitation.load();
            voiceOnHold = telemetry.voices[0].load();
            std::cout << "Held chord: input " << excitationOnHold << ", V01 " << voiceOnHold
                      << ", V02 muted " << telemetry.voices[1].load() << '\n';
            require(strikes == 5 && excitationOnHold > .03f && voiceOnHold > .001f && telemetry.voices[1].load() < 1.0e-6f,
                    "Sustained chord retriggered, or per-voice readings missed their actual contributions");
            require(difference(idle, image, inputArea) > 150 && difference(idle, image, wallArea) > 150,
                    "Sustained chord did not keep the speed lines and cracks visible");
            save(image, folder, "impact-held-chord");
        }
        if (frame >= 166 && frame < 232 && !checkedTail)
        {
            outputAfterHold = std::max(telemetry.output[0].load(), telemetry.output[1].load());
            if (telemetry.voices[0].load() < .0003f && outputAfterHold > .001f)
            {
                const auto image = render(panel);
                if (difference(idle, image, wallArea) < 145 && difference(idle, image, meterArea) > 180)
                {
                    checkedTail = true;
                    std::cout << "Reverb-only frame " << frame << ", output RMS " << outputAfterHold << '\n';
                    save(image, folder, "impact-reverb-tail");
                }
            }
        }
        if (exportFrames)
            save(render(panel), frames, juce::String(frame).paddedLeft('0', 4));
    }
    require(strikes == 5, "False attacks occurred during decay or silence");
    require(checkedTail, "Reverb output did not remain visible after the resonant cracks faded");
    require(telemetry.excitation.load() < excitationOnHold * .001f && telemetry.voices[0].load() < voiceOnHold * .001f,
            "Input or resonance telemetry stayed active after the real sound decayed");

    // Some hosts stop delivering buffers when the transport stops.
    for (int frame = frameCount; frame < frameCount + 50; ++frame)
        panel.tickAnimation(frameMs * (frame + 1));
    require(difference(idle, render(panel), panel.getLocalBounds()) == 0, "Panel did not settle to its exact idle artwork");

    invert->setToggleState(true, juce::sendNotificationSync);
    panel.triggerPreview();
    panel.tickAnimation(frameMs * (frameCount + 51));
    save(render(panel), folder, "impact-inverted");
    require((telemetry.strike.load() >> 8) == observed, "Manual preview changed audio onset telemetry");

    invert->setToggleState(false, juce::sendNotificationSync);
    juce::Image previousMode;
    for (int mode = 0; mode < 5; ++mode)
    {
        set(processor, "gunMode", static_cast<float>(mode));
        const double time = frameMs * (frameCount + 54 + 60 * mode);
        panel.tickAnimation(time);
        panel.triggerPreview();
        panel.tickAnimation(time + frameMs);
        const auto image = render(panel);
        if (previousMode.isValid())
            require(difference(previousMode, image, onsetArea) > 450, "Mode change did not draw a different impact");
        previousMode = image;
    }

    std::cout << "PASS: five impact brushes, single hit, repeated hits, sustained chord, reverb-only tail, idle, stereo and inversion\n";
}
