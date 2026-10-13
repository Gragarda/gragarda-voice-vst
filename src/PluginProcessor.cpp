// Gragarda Voice en VST3 : enveloppe JUCE autour du coeur DSP (core/dsp).
// Interface : bouton de calibration + editeur generique (4 macros, destination, debruitage, moteur, gain, avant/apres).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
#include "Calibration.h"
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

// Langues proposees (nom natif) ; "" = automatique (langue du systeme)
const std::pair<const char*, const char*> kLanguages[] = {
    {"ar-SA", "العربية"}, {"bg-BG", "Български"}, {"ca-ES", "Català"}, {"cs-CZ", "Čeština"}, {"da-DK", "Dansk"},
    {"de-DE", "Deutsch"}, {"el-GR", "Ελληνικά"}, {"en-US", "English"}, {"es-ES", "Español"}, {"fi-FI", "Suomi"},
    {"fr-FR", "Français"}, {"he-IL", "עברית"}, {"hi-IN", "हिन्दी"}, {"hr-HR", "Hrvatski"}, {"hu-HU", "Magyar"},
    {"id-ID", "Bahasa Indonesia"}, {"it-IT", "Italiano"}, {"ja-JP", "日本語"}, {"ko-KR", "한국어"},
    {"nb-NO", "Norsk bokmål"}, {"nl-NL", "Nederlands"}, {"pl-PL", "Polski"}, {"pt-BR", "Português (Brasil)"},
    {"pt-PT", "Português (Portugal)"}, {"ro-RO", "Română"}, {"ru-RU", "Русский"}, {"sk-SK", "Slovenčina"},
    {"sv-SE", "Svenska"}, {"th-TH", "ไทย"}, {"tr-TR", "Türkçe"}, {"uk-UA", "Українська"}, {"vi-VN", "Tiếng Việt"},
    {"zh-CN", "简体中文"}, {"zh-TW", "繁體中文"}};

// Choix de langue : reglage utilisateur global (tous projets, tous hotes), pas un parametre de l'hote
std::unique_ptr<juce::PropertiesFile> userSettings()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Gragarda Voice";
    o.filenameSuffix = ".settings";
    o.folderName = "Gragarda Voice";
    o.osxLibrarySubFolder = "Application Support";
    return std::make_unique<juce::PropertiesFile>(o);
}

// Traductions : memes .ini que le filtre OBS (locale/, embarques). Langue choisie, sinon systeme, sinon en-US.
class Strings {
public:
    explicit Strings(const juce::String& chosen)
    {
        load("en-US");
        if (chosen.isNotEmpty() && load(chosen))
            return;
        const auto lang = juce::SystemStats::getUserLanguage(), region = juce::SystemStats::getUserRegion();
        if (!load(lang + "-" + region) && lang != "en")
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
                if (juce::String(BinaryData::originalFilenames[i]).startsWith(lang + "-")) {
                    load(juce::String(BinaryData::originalFilenames[i]).upToFirstOccurrenceOf(".", false, false));
                    break;
                }
    }
    // Les textes OBS sont en HTML Qt : rendu en texte brut
    juce::String operator()(const char* key) const
    {
        auto s = map_[key].replace("<br>", "\n").replace("</p>", "\n").replace("</h3>", "\n\n").replace("</tr>", "\n");
        juce::String out;
        bool tag = false;
        for (auto c : s) {
            if (c == '<') tag = true;
            else if (c == '>') tag = false;
            else if (!tag) out << juce::String::charToString(c);
        }
        return out.trim();
    }

private:
    juce::StringPairArray map_;
    bool load(const juce::String& locale)
    {
        int size = 0;
        const char* data = nullptr;
        for (int i = 0; i < BinaryData::namedResourceListSize && !data; ++i)
            if (juce::String(BinaryData::originalFilenames[i]) == locale + ".ini")
                data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], size);
        if (!data)
            return false;
        for (auto line : juce::StringArray::fromLines(juce::String::fromUTF8(data, size)))
            if (line.contains("=\""))
                map_.set(line.upToFirstOccurrenceOf("=", false, false),
                         line.fromFirstOccurrenceOf("=\"", false, false).dropLastCharacters(1));
        return true;
    }
};
} // namespace

class GragardaVoiceProcessor : public juce::AudioProcessor {
public:
    GragardaVoiceProcessor()
        : AudioProcessor(BusesProperties()
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          state(*this, nullptr, "GragardaVoice", layout())
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
    juce::AudioProcessorEditor* createEditor() override;

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
        capture.prepare((size_t)(16 * sampleRate));
        sampleRate_ = sampleRate;
        syncParams(true);
    }
    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        syncParams(false);
        capture.push(buffer.getArrayOfReadPointers(), buffer.getNumChannels(), (size_t)buffer.getNumSamples());
        chain_.process(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), (size_t)buffer.getNumSamples());
    }

    void getStateInformation(juce::MemoryBlock& dest) override
    {
        auto st = state.copyState();
        for (int i = 0; i < 3; ++i)
            st.setProperty(kCorrIds[i], corr_[i].load(), nullptr);
        if (auto xml = st.createXml())
            copyXmlToBinary(*xml, dest);
    }
    void setStateInformation(const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary(data, size)) {
            auto st = juce::ValueTree::fromXml(*xml);
            for (int i = 0; i < 3; ++i)
                corr_[i] = (float)st.getProperty(kCorrIds[i], 0.0f);
            corrDirty_ = true;
            state.replaceState(st);
        }
    }

    // Thread UI (calibration) : reglages proposes -> parametres de l'hote + correction micro cachee
    void applyCalibration(const CalibrationReport& r)
    {
        auto set = [this](const char* id, float v) {
            auto* p = state.getParameter(id);
            p->setValueNotifyingHost(p->convertTo0to1(v));
        };
        set("gain", r.gainDb);
        set("clean", (float)r.clean);
        set("warmth", (float)r.warmth);
        set("clarity", (float)r.clarity);
        set("punch", (float)r.punch);
        corr_[0] = r.corrLow;
        corr_[1] = r.corrPresence;
        corr_[2] = r.corrAir;
        corrDirty_ = true;
    }

    double currentSampleRate() const { return sampleRate_; }
    float latencyMs() const { return 1000.0f * (float)getLatencySamples() / (float)sampleRate_; }

    juce::AudioProcessorValueTreeState state;
    Capture capture; // signal brut, avant la chaine (thread audio -> assistant)

private:
    static constexpr const char* kCorrIds[3] = {"corr_low", "corr_presence", "corr_air"};
    VoiceChain chain_;
    double sampleRate_ = 48000.0;
    std::atomic<float> corr_[3] = {0.0f, 0.0f, 0.0f}; // correction micro posee par la calibration, dB
    std::atomic<bool> corrDirty_{true};
    struct Snapshot {
        float clean = -1, warmth = -1, clarity = -1, punch = -1, gain = 0;
        int dest = -1, engine = -1;
        bool macrosDiffer(const Snapshot& o) const
        {
            return clean != o.clean || warmth != o.warmth || clarity != o.clarity || punch != o.punch || dest != o.dest;
        }
    } last_;

    float raw(const char* id) const { return state.getRawParameterValue(id)->load(); }

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
        if (force || s.macrosDiffer(last_) || corrDirty_.exchange(false)) {
            VoiceChain::Macros m;
            m.clean = s.clean;
            m.warmth = s.warmth;
            m.clarity = s.clarity;
            m.punch = s.punch;
            m.dest = (VoiceChain::Destination)s.dest;
            m.corrLow = corr_[0];
            m.corrPresence = corr_[1];
            m.corrAir = corr_[2];
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

// Assistant de calibration (meme parcours que le filtre OBS) : 5 s de silence, 15 s de lecture, voyants, Appliquer.
class CalibrationPanel : public juce::Component, private juce::Timer {
public:
    CalibrationPanel(GragardaVoiceProcessor& p, const Strings& t, std::function<void()> onClose)
        : p_(p), T(t), onClose_(std::move(onClose))
    {
        text_.setMultiLine(true);
        text_.setReadOnly(true);
        text_.setCaretVisible(false);
        text_.setFont(juce::FontOptions(16.0f));
        addAndMakeVisible(text_);
        addChildComponent(bar_);
        addAndMakeVisible(go_);
        addAndMakeVisible(cancel_);
        intro();
    }
    ~CalibrationPanel() override { p_.capture.stop(); }

    void paint(juce::Graphics& g) override { g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId)); }
    void resized() override
    {
        auto r = getLocalBounds().reduced(12);
        auto buttons = r.removeFromBottom(32);
        cancel_.setBounds(buttons.removeFromRight(140));
        buttons.removeFromRight(8);
        go_.setBounds(buttons.removeFromRight(140));
        r.removeFromBottom(8);
        bar_.setBounds(r.removeFromBottom(20));
        r.removeFromBottom(8);
        text_.setBounds(r);
    }

private:
    enum Step { Noise, Speech };
    GragardaVoiceProcessor& p_;
    const Strings& T;
    std::function<void()> onClose_;
    juce::TextEditor text_;
    double progress_ = 0;
    juce::ProgressBar bar_{progress_};
    juce::TextButton go_, cancel_;
    Step step_ = Noise;
    int stalled_ = 0;
    float lastProgress_ = 0;
    std::vector<float> noise_;
    CalibrationReport rep_;

    void buttons(const char* goKey, std::function<void()> go, const char* cancelKey, std::function<void()> cancel)
    {
        go_.setButtonText(T(goKey));
        go_.onClick = std::move(go);
        go_.setEnabled(true);
        cancel_.setButtonText(T(cancelKey));
        cancel_.onClick = std::move(cancel);
    }

    void intro()
    {
        text_.setText(T("CalibIntro"));
        buttons("CalibStart", [this] { record(Noise); }, "CalibCancel", [this] { onClose_(); });
    }

    void record(Step s)
    {
        step_ = s;
        text_.setText(T(s == Noise ? "CalibNoise" : "CalibSpeech"));
        progress_ = 0;
        bar_.setVisible(true);
        go_.setEnabled(false);
        stalled_ = 0;
        lastProgress_ = 0;
        p_.capture.start((size_t)((s == Noise ? 5.0 : 15.0) * p_.currentSampleRate()));
        startTimer(50);
    }

    void timerCallback() override
    {
        const float pr = p_.capture.progress();
        progress_ = pr;
        // Aucun son recu (piste non armee, transport arrete, entree muette) : on previent
        stalled_ = pr > lastProgress_ ? 0 : stalled_ + 1;
        lastProgress_ = pr;
        if (stalled_ == 30)
            text_.setText(text_.getText() + "\n\n" + T("CalibNoAudio"));
        if (!p_.capture.done())
            return;
        stopTimer();
        p_.capture.stop();
        if (step_ == Noise) {
            noise_ = p_.capture.take();
            record(Speech);
        } else {
            rep_ = analyzeCalibration(noise_, p_.capture.take(), p_.currentSampleRate());
            showResult();
        }
    }

    void showResult()
    {
        bar_.setVisible(false);
        const auto& r = rep_;
        if (!r.valid) {
            text_.setText(T("CalibInvalid"));
            buttons("CalibRetry", [this] { record(Noise); }, "CalibCancel", [this] { onClose_(); });
            return;
        }
        auto light = [this](Light l, const char* name, const char* g, const char* o, const char* rd) {
            const juce::String dot = l == Light::Green ? juce::CharPointer_UTF8("\xf0\x9f\x9f\xa2")
                                   : l == Light::Orange ? juce::CharPointer_UTF8("\xf0\x9f\x9f\xa0")
                                                        : juce::CharPointer_UTF8("\xf0\x9f\x94\xb4");
            return dot + "  " + T(name) + " : " + T(l == Light::Green ? g : l == Light::Orange ? o : rd) + "\n";
        };
        juce::String s = T("CalibDone") + "\n\n";
        s << light(r.noise, "LightNoise", "NoiseG", "NoiseO", "NoiseR")
          << light(r.level, "LightLevel", "LevelG", "LevelO", "LevelR")
          << light(r.sibilance, "LightSib", "SibG", "SibO", "SibR")
          << light(r.clipping, "LightClip", "ClipG", "ClipO", "ClipR")
          << "\n" << T("CalibProposal") << "\n"
          << T("Gain") << " : " << juce::String(r.gainDb, 1) << " dB\n"
          << T("Clean") << " : " << r.clean << "\n"
          << T("Warmth") << " : " << r.warmth << "\n"
          << T("Clarity") << " : " << r.clarity << "\n"
          << T("Punch") << " : " << r.punch << "\n"
          << T("MicCorrection") << " : " << juce::String(r.corrLow, 1) << " / " << juce::String(r.corrPresence, 1)
          << " / " << juce::String(r.corrAir, 1) << " dB";
        text_.setText(s);
        buttons("CalibApply", [this] { p_.applyCalibration(rep_); onClose_(); }, "CalibRetry", [this] { record(Noise); });
    }
};

class GragardaVoiceEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GragardaVoiceEditor(GragardaVoiceProcessor& p)
        : AudioProcessorEditor(p), p_(p), settings_(userSettings()), T(settings_->getValue("language")), generic_(p)
    {
        // Langue : l'hote ne la transmet pas, choix manuel (Auto = langue du systeme)
        language_.addItem("Auto", 1);
        for (int i = 0; i < (int)std::size(kLanguages); ++i) {
            language_.addItem(juce::String::fromUTF8(kLanguages[i].second), i + 2);
            if (settings_->getValue("language") == kLanguages[i].first)
                language_.setSelectedId(i + 2, juce::dontSendNotification);
        }
        if (language_.getSelectedId() == 0)
            language_.setSelectedId(1, juce::dontSendNotification);
        language_.onChange = [this] {
            const int id = language_.getSelectedId();
            settings_->setValue("language", id > 1 ? juce::String(kLanguages[id - 2].first) : juce::String());
            settings_->saveIfNeeded();
            T = Strings(settings_->getValue("language"));
            panel_.reset();
            retranslate();
        };
        calibrate_.onClick = [this] { openCalibration(); };
        kofi_.setURL(juce::URL("https://ko-fi.com/gragarda"));
        retranslate();
        addAndMakeVisible(language_);
        addAndMakeVisible(calibrate_);
        addAndMakeVisible(latency_);
        addAndMakeVisible(kofi_);
        addAndMakeVisible(generic_);
        setResizable(true, false);
        setSize(640, 40 + generic_.getHeight() + 32);
        startTimerHz(4);
        timerCallback();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop(40).reduced(6);
        calibrate_.setBounds(top.removeFromLeft(240));
        top.removeFromLeft(10);
        language_.setBounds(top.removeFromRight(170));
        top.removeFromRight(10);
        latency_.setBounds(top);
        kofi_.setBounds(r.removeFromBottom(32));
        generic_.setBounds(r);
        if (panel_)
            panel_->setBounds(getLocalBounds());
    }

private:
    GragardaVoiceProcessor& p_;
    std::unique_ptr<juce::PropertiesFile> settings_;
    Strings T;
    juce::GenericAudioProcessorEditor generic_;
    juce::ComboBox language_;
    juce::TextButton calibrate_;
    juce::Label latency_;
    juce::HyperlinkButton kofi_;
    std::unique_ptr<CalibrationPanel> panel_;

    void retranslate()
    {
        calibrate_.setButtonText(T("Calibrate"));
        kofi_.setButtonText(T("SupportKofi"));
        timerCallback();
    }

    void timerCallback() override
    {
        latency_.setText(T("LatencyInfo") + " " + juce::String(juce::roundToInt(p_.latencyMs())) + " ms",
                         juce::dontSendNotification);
    }

    void openCalibration()
    {
        // Fenetre agrandie le temps de l'assistant (le texte de lecture est long)
        // fermeture differee : le panneau ne se detruit pas pendant son propre clic
        juce::Component::SafePointer<GragardaVoiceEditor> self(this);
        panel_ = std::make_unique<CalibrationPanel>(p_, T, [self] {
            juce::MessageManager::callAsync([self] {
                if (self)
                    self->panel_.reset();
            });
        });
        addAndMakeVisible(*panel_);
        setSize(juce::jmax(getWidth(), 640), juce::jmax(getHeight(), 520));
        resized();
    }
};

juce::AudioProcessorEditor* GragardaVoiceProcessor::createEditor() { return new GragardaVoiceEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GragardaVoiceProcessor(); }
