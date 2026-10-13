// Gragarda Voice en VST3 : enveloppe JUCE autour du coeur DSP (core/dsp/VoiceChain).
// Interface : editeur generique (4 macros, destination, debruitage, moteur, gain, avant/apres).
#include <juce_audio_processors/juce_audio_processors.h>
#include "VoiceChain.h"

namespace {
juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    auto pct = [](const char* id, const char* name) {
        return std::make_unique<AudioParameterInt>(ParameterID{id, 1}, name, 0, 100, 50);
    };
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add(pct("clean", "Clean"), pct("warmth", "Warmth"), pct("clarity", "Clarity"), pct("punch", "Punch"));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"dest", 1}, "Destination",
        StringArray{"Stream (-14 LUFS)", "Podcast (-16 LUFS)", "Voice-over (-18 LUFS)"}, 0));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"denoise", 1}, "Noise removal", true));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"engine", 1}, "Engine",
        StringArray{"Fast (RNNoise)", "Max quality (DeepFilterNet)"}, 1));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"gain", 1}, "Input gain",
        NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass", 1}, "Before/after (level matched)", false));
    return l;
}
} // namespace

class GragardaVoiceProcessor : public juce::AudioProcessor {
public:
    GragardaVoiceProcessor()
        : AudioProcessor(BusesProperties()
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          state_(*this, nullptr, "GragardaVoice", layout())
    {
    }

    const juce::String getName() const override { return "Gragarda Voice"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }

    bool isBusesLayoutSupported(const BusesLayout& l) const override
    {
        const auto& out = l.getMainOutputChannelSet();
        return out == l.getMainInputChannelSet()
            && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
    }

    void prepareToPlay(double sampleRate, int) override
    {
        // Modele dans <bundle>.vst3/Contents/Resources ; le binaire est dans Contents/<arch> (MacOS sur macOS)
        const auto model = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                               .getParentDirectory().getParentDirectory()
                               .getChildFile("Resources/DeepFilterNet3_onnx.tar.gz");
        // ponytail: RNNoise et DeepFilterNet sont entraines a 48 kHz ; a 44,1 kHz ils tournent quand meme
        // (debruitage moins bon). Reechantillonner autour du debruiteur si les retours le demandent.
        chain_.prepare(sampleRate, getTotalNumOutputChannels(),
                       model.existsAsFile() ? model.getFullPathName().toRawUTF8() : nullptr);
        syncParams(true);
    }
    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        syncParams(false);
        chain_.process(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), (size_t)buffer.getNumSamples());
    }

    void getStateInformation(juce::MemoryBlock& dest) override
    {
        if (auto xml = state_.copyState().createXml())
            copyXmlToBinary(*xml, dest);
    }
    void setStateInformation(const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary(data, size))
            state_.replaceState(juce::ValueTree::fromXml(*xml));
    }

private:
    juce::AudioProcessorValueTreeState state_;
    VoiceChain chain_;
    struct Snapshot {
        float clean = -1, warmth = -1, clarity = -1, punch = -1, gain = 0;
        int dest = -1, engine = -1;
        bool macrosDiffer(const Snapshot& o) const
        {
            return clean != o.clean || warmth != o.warmth || clarity != o.clarity || punch != o.punch || dest != o.dest;
        }
    } last_;

    float raw(const char* id) const { return state_.getRawParameterValue(id)->load(); }

    // Parametres -> chaine. setMacros seulement si un macro change (recalcul des filtres)
    void syncParams(bool force)
    {
        Snapshot s;
        s.clean = raw("clean") / 100.0f;
        s.warmth = raw("warmth") / 100.0f;
        s.clarity = raw("clarity") / 100.0f;
        s.punch = raw("punch") / 100.0f;
        s.dest = (int)raw("dest");
        s.engine = (int)raw("engine");
        s.gain = raw("gain");
        if (force || s.macrosDiffer(last_)) {
            VoiceChain::Macros m;
            m.clean = s.clean;
            m.warmth = s.warmth;
            m.clarity = s.clarity;
            m.punch = s.punch;
            m.dest = (VoiceChain::Destination)s.dest;
            chain_.setMacros(m);
        }
        if (force || s.gain != last_.gain)
            chain_.setGainDb(s.gain);
        if (force || s.engine != last_.engine) {
            chain_.setEngine(s.engine == 1 && chain_.maxAvailable() ? VoiceChain::Max : VoiceChain::Fast);
            setLatencySamples(chain_.latencySamples()); // latence fixe par moteur, compensee par l'hote
        }
        chain_.setDenoise(raw("denoise") > 0.5f);
        chain_.setBypass(raw("bypass") > 0.5f);
        last_ = s;
    }
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GragardaVoiceProcessor(); }
