// Gragarda Voice en VST3 : enveloppe JUCE autour du coeur DSP (core/dsp).
// Interface : bouton de calibration + controles traduits (4 macros, destination, debruitage, moteur, gain, avant/apres).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
#include "Calibration.h"
#include "VoiceChain.h"

namespace {
// Options avancees : un interrupteur par etage (parametres de l'hote : sauvegardes avec le projet)
struct Stage { const char *id, *name, *label; };
constexpr Stage kStages[] = {
    {"st_hpf", "High-pass on", "StageHpf"},
    {"st_eq", "EQ on", "StageEq"},                    {"st_compressor", "Compressor on", "StageCompressor"},
    {"st_deesser", "De-esser on", "StageDeesser"},    {"st_leveler", "Leveler on", "StageLeveler"},
    {"st_limiter", "Limiter on", "StageLimiter"}};

constexpr float kModeRt60[] = {0.0f, 0.0f, 0.4f, 0.7f, 1.0f}; // Auto, Coupe, Leger, Moyen, Fort

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
    for (const auto& s : kStages)
        l.add(std::make_unique<AudioParameterBool>(ParameterID{s.id, 1}, s.name, true));
    // De-reverb : Auto = RT60 pose par la calibration, sinon force a la main (kModeRt60)
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"dereverb_mode", 1}, "De-reverb",
        StringArray{"Auto (calibration)", "Off", "Light", "Medium", "Strong"}, 0));
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
        st.setProperty("rt60", rt60_.load(), nullptr);
        if (auto xml = st.createXml())
            copyXmlToBinary(*xml, dest);
    }
    void setStateInformation(const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary(data, size)) {
            auto st = juce::ValueTree::fromXml(*xml);
            for (int i = 0; i < 3; ++i)
                corr_[i] = (float)st.getProperty(kCorrIds[i], 0.0f);
            rt60_ = (float)st.getProperty("rt60", 0.0f);
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
        rt60_ = r.dereverbRt60;
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
    std::atomic<float> rt60_{0.0f}; // de-reverb pose par la calibration, s ; 0 = hors service
    std::atomic<bool> corrDirty_{true};
    struct Snapshot {
        float clean = -1, warmth = -1, clarity = -1, punch = -1, gain = 0;
        int dest = -1, engine = -1, stages = -1, drMode = -1; // stages : un bit par interrupteur
        bool macrosDiffer(const Snapshot& o) const
        {
            return clean != o.clean || warmth != o.warmth || clarity != o.clarity || punch != o.punch || dest != o.dest
                || drMode != o.drMode;
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
        s.drMode = juce::jlimit(0, 4, (int)raw("dereverb_mode"));
        s.stages = s.drMode != 1; // bit 0 : de-reverb
        for (int i = 0; i < (int)std::size(kStages); ++i)
            s.stages |= (raw(kStages[i].id) > 0.5f) << (i + 1);
        if (force || s.stages != last_.stages) { // setStages recalcule les filtres : seulement si ca change
            VoiceChain::Stages on;
            bool* b[] = {&on.dereverb, &on.hpf, &on.eq, &on.compressor, &on.deesser, &on.leveler, &on.limiter};
            for (int i = 0; i <= (int)std::size(kStages); ++i)
                *b[i] = (s.stages >> i) & 1;
            chain_.setStages(on);
        }
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
            m.rt60 = s.drMode == 0 ? rt60_.load() : kModeRt60[s.drMode];
            chain_.setMacros(m);
        }
        if (force || s.gain != last_.gain)
            chain_.setGainDb(s.gain);
        if (force || s.engine != last_.engine)
            chain_.setEngine(s.engine == 1 && chain_.maxAvailable() ? VoiceChain::Max : VoiceChain::Fast);
        // latence = moteur + de-reverb s'il tourne ; compensee par l'hote, signalee seulement si elle change
        if (chain_.latencySamples() != getLatencySamples())
            setLatencySamples(chain_.latencySamples());
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
        auto line = [this](Light l, const char* name, const juce::String& msg) {
            const juce::String dot = l == Light::Green ? juce::CharPointer_UTF8("\xf0\x9f\x9f\xa2")
                                   : l == Light::Orange ? juce::CharPointer_UTF8("\xf0\x9f\x9f\xa0")
                                                        : juce::CharPointer_UTF8("\xf0\x9f\x94\xb4");
            return dot + "  " + T(name) + " : " + msg + "\n";
        };
        auto light = [&](Light l, const char* name, const char* g, const char* o, const char* rd) {
            return line(l, name, T(l == Light::Green ? g : l == Light::Orange ? o : rd));
        };
        juce::String s = T("CalibDone") + "\n\n";
        s << light(r.noise, "LightNoise", "NoiseG", "NoiseO", "NoiseR")
          << light(r.level, "LightLevel", "LevelG", "LevelO", "LevelR")
          << light(r.sibilance, "LightSib", "SibG", "SibO", "SibR")
          << light(r.clipping, "LightClip", "ClipG", "ClipO", "ClipR")
          << line(r.reverb, "LightReverb", r.rt60 <= 0 ? T("ReverbNA")
                                          : T(r.dereverbRt60 > 0 ? "ReverbOn" : "ReverbOff")
                                                .replace("%1", juce::String(r.rt60, 2)))
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

// Options avancees : une case par etage, branchee sur les parametres st_*
class AdvancedPanel : public juce::Component {
public:
    AdvancedPanel(juce::AudioProcessorValueTreeState& st, const Strings& T, std::function<void()> onClose)
    {
        intro_.setText(T("AdvancedIntro"), juce::dontSendNotification);
        addAndMakeVisible(intro_);
        drLabel_.setText(T("StageDereverb"), juce::dontSendNotification);
        addAndMakeVisible(drLabel_);
        // items crees avant l'attachment (ComboBox ignore un texte vide)
        dr_.addItemList({T("DereverbAuto"), T("DereverbOff"), T("DereverbLight"), T("DereverbMedium"), T("DereverbStrong")}, 1);
        drAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(st, "dereverb_mode", dr_);
        addAndMakeVisible(dr_);
        for (int i = 0; i < (int)std::size(kStages); ++i) {
            boxes_[i].setButtonText(T(kStages[i].label));
            att_[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(st, kStages[i].id, boxes_[i]);
            addAndMakeVisible(boxes_[i]);
        }
        close_.setButtonText(T("AdvancedClose"));
        close_.onClick = std::move(onClose);
        addAndMakeVisible(close_);
    }
    void paint(juce::Graphics& g) override { g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId)); }
    void resized() override
    {
        auto r = getLocalBounds().reduced(12);
        close_.setBounds(r.removeFromBottom(32).removeFromRight(140));
        intro_.setBounds(r.removeFromTop(48));
        auto row = r.removeFromTop(30).reduced(0, 2);
        drLabel_.setBounds(row.removeFromLeft(200));
        dr_.setBounds(row);
        for (auto& b : boxes_)
            b.setBounds(r.removeFromTop(30));
    }

private:
    juce::Label intro_, drLabel_;
    juce::ComboBox dr_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> drAtt_;
    juce::ToggleButton boxes_[std::size(kStages)];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att_[std::size(kStages)];
    juce::TextButton close_;
};

class GragardaVoiceEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GragardaVoiceEditor(GragardaVoiceProcessor& p)
        : AudioProcessorEditor(p), p_(p), settings_(userSettings()), T(settings_->getValue("language"))
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
            advancedPanel_.reset();
            retranslate();
        };
        // Controles maison plutot que l'editeur generique : les noms de parametres (hote, automation) restent
        // en anglais et stables, les libelles affiches suivent la langue choisie
        auto& st = p.state;
        for (int i = 0; i < 5; ++i) {
            sliders_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 22);
            sAtt_[i] = std::make_unique<APVTS::SliderAttachment>(st, kSliders[i], sliders_[i]);
        }
        sliders_[4].setTextValueSuffix(" dB");
        // items crees avant les attachments (ComboBox ignore un texte vide) ; retranslate() les renomme
        dest_.addItemList({T("DestStream"), T("DestPodcast"), T("DestVoiceOver")}, 1);
        engine_.addItemList({T("EngineFast"), T("EngineMax")}, 1);
        destAtt_ = std::make_unique<APVTS::ComboBoxAttachment>(st, "dest", dest_);
        engineAtt_ = std::make_unique<APVTS::ComboBoxAttachment>(st, "engine", engine_);
        denoiseAtt_ = std::make_unique<APVTS::ButtonAttachment>(st, "denoise", denoise_);
        bypass_.setClickingTogglesState(true);
        bypass_.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkorange);
        addAndMakeVisible(bypass_);
        bypassAtt_ = std::make_unique<APVTS::ButtonAttachment>(st, "bypass", bypass_);
        for (int i = 0; i < kRows; ++i) {
            labels_[i].setMinimumHorizontalScale(0.7f);
            addAndMakeVisible(labels_[i]);
            addAndMakeVisible(*rows()[i]);
        }
        calibrate_.onClick = [this] { openCalibration(); };
        advanced_.onClick = [this] { openAdvanced(); };
        addAndMakeVisible(advanced_);
        kofi_.setURL(juce::URL("https://ko-fi.com/gragarda"));
        retranslate();
        addAndMakeVisible(language_);
        addAndMakeVisible(calibrate_);
        addAndMakeVisible(latency_);
        addAndMakeVisible(kofi_);
        setResizable(true, false);
        setSize(640, 40 + kRows * kRowH + 12 + 32);
        startTimerHz(4);
        timerCallback();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop(40).reduced(6);
        calibrate_.setBounds(top.removeFromLeft(240));
        top.removeFromLeft(10);
        bypass_.setBounds(top.removeFromRight(80));
        top.removeFromRight(10);
        language_.setBounds(top.removeFromRight(170));
        top.removeFromRight(10);
        latency_.setBounds(top);
        auto bottom = r.removeFromBottom(32);
        advanced_.setBounds(bottom.removeFromRight(220).reduced(6, 3));
        kofi_.setBounds(bottom);
        r.reduce(12, 6);
        for (int i = 0; i < kRows; ++i) {
            auto row = r.removeFromTop(kRowH).reduced(0, 3);
            labels_[i].setBounds(row.removeFromLeft(170));
            rows()[i]->setBounds(row);
        }
        if (panel_)
            panel_->setBounds(getLocalBounds());
        if (advancedPanel_)
            advancedPanel_->setBounds(getLocalBounds());
    }

private:
    GragardaVoiceProcessor& p_;
    std::unique_ptr<juce::PropertiesFile> settings_;
    Strings T;
    using APVTS = juce::AudioProcessorValueTreeState;
    static constexpr int kRows = 8, kRowH = 32;
    static constexpr const char* kSliders[5] = {"clean", "warmth", "clarity", "punch", "gain"};
    // cle de traduction du libelle de chaque ligne, dans l'ordre de rows()
    static constexpr const char* kLabels[kRows] = {"Clean", "Warmth", "Clarity", "Punch", "Destination",
                                                   "Denoise", "Engine", "Gain"};
    juce::TooltipWindow tips_{this};
    juce::Slider sliders_[5];
    juce::ComboBox dest_, engine_;
    juce::ToggleButton denoise_;
    juce::TextButton bypass_{"Bypass"};
    juce::Label labels_[kRows];
    std::unique_ptr<APVTS::SliderAttachment> sAtt_[5];
    std::unique_ptr<APVTS::ComboBoxAttachment> destAtt_, engineAtt_;
    std::unique_ptr<APVTS::ButtonAttachment> denoiseAtt_, bypassAtt_;
    juce::ComboBox language_;
    juce::TextButton calibrate_;
    juce::Label latency_;
    juce::HyperlinkButton kofi_;
    std::unique_ptr<CalibrationPanel> panel_;
    juce::TextButton advanced_;
    std::unique_ptr<AdvancedPanel> advancedPanel_;

    std::array<juce::Component*, kRows> rows()
    {
        return {&sliders_[0], &sliders_[1], &sliders_[2], &sliders_[3], &dest_, &denoise_, &engine_, &sliders_[4]};
    }

    void retranslate()
    {
        for (int i = 0; i < kRows; ++i)
            labels_[i].setText(T(kLabels[i]), juce::dontSendNotification);
        // bouton court "Bypass", explication traduite en infobulle
        bypass_.setTooltip(T("Bypass"));
        for (int i = 0; i < 4; ++i)
            sliders_[i].setTooltip(T((juce::String(kLabels[i]) + "Tip").toRawUTF8()));
        const char* dests[] = {"DestStream", "DestPodcast", "DestVoiceOver"};
        for (int i = 0; i < 3; ++i)
            dest_.changeItemText(i + 1, T(dests[i]));
        engine_.changeItemText(1, T("EngineFast"));
        engine_.changeItemText(2, T("EngineMax"));
        calibrate_.setButtonText(T("Calibrate"));
        advanced_.setButtonText(T("Advanced"));
        kofi_.setButtonText(T("SupportKofi"));
        timerCallback();
    }

    void timerCallback() override
    {
        latency_.setText(T("LatencyInfo") + " " + juce::String(juce::roundToInt(p_.latencyMs())) + " ms",
                         juce::dontSendNotification);
    }

    void openAdvanced()
    {
        juce::Component::SafePointer<GragardaVoiceEditor> self(this);
        advancedPanel_ = std::make_unique<AdvancedPanel>(p_.state, T, [self] {
            juce::MessageManager::callAsync([self] {
                if (self)
                    self->advancedPanel_.reset();
            });
        });
        addAndMakeVisible(*advancedPanel_);
        resized();
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
