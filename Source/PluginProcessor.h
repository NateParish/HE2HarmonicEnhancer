#pragma once

#include <JuceHeader.h>
#include "AppInfo.h"
#include "../../AppLibrary/Source/AppData.h"
#include "../../AppLibrary/Source/PresettableProcessor.h"
#include "../../AppLibrary/Source/PresetManager.h"
#include "../../AppLibrary/Source/Library/LoginScreen/Authentication.h"
#include "../../AppLibrary/Source/Library/SystemPerformance.h"
#include <array>
#include <memory>
#include <vector>

class HE2AudioProcessor : public juce::AudioProcessor,
                          public PresettableProcessor,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    HE2AudioProcessor();
    ~HE2AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #if ! JucePlugin_IsMidiEffect
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
   #endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::StringArray getFactoryPresetNames() const override;
    juce::StringArray getUserPresetNames() const override;

    void applyFactoryPreset(const juce::String& presetName) override;
    bool saveUserPreset(const juce::String& presetName) override;
    bool loadUserPreset(const juce::String& presetName) override;
    bool deleteUserPreset(const juce::String& presetName) override;
    bool renameUserPreset(const juce::String& oldName, const juce::String& newName) override;

    float getParamValue(const juce::String& paramID) const;
    void setParamValue(const juce::String& paramID, float value);

    // Lightweight, read-only UI activity for the four central band LEDs.
    // Written once per audio block, read by the GUI via relaxed atomic loads.
    float getBandActivity(int band) const noexcept
    {
        return band >= 0 && band < 4
            ? bandActivity[(size_t) band].load(std::memory_order_relaxed)
            : 0.0f;
    }

    bool isLocked()   { return authLock.isLocked(); }
    bool isUnlocked() { return authLock.isUnlocked(); }
    AuthenticationLock* getAuthLock() { return &authLock; }
    PerformanceMonitor* getPerformanceMonitor() { return &performanceMonitor; }

    bool getPowerState() const { return getFloatParam("power") >= 0.5f; }
    int  getModeIndex() const  { return (int) std::round(getFloatParam("mode")); }

    float getCurWindowWidth() const { return curWindowWidth.load(); }
    float getCurWindowHeight() const { return curWindowHeight.load(); }
    void setCurWindowSize(float width, float height)
    {
        curWindowWidth.store(width);
        curWindowHeight.store(height);
        windowSizeSet.store(true);
    }
    bool hasCurWindowSizeBeenSet() const { return windowSizeSet.load(); }
    void clearCurWindowSize() { windowSizeSet.store(false); }

    juce::String getCurrentPresetName() const override;
    juce::String getCurrentPresetDisplayName() const override;
    bool isCurrentPresetUser() const override;
    bool canUndo() const override;
    bool canRedo() const override;
    void performUndo() override;
    void performRedo() override;
    bool isCurrentPresetModified() const;
    void clearCurrentPresetState();

    void beginUndoTransaction(const juce::String& actionName);
    void endUndoTransaction();
    void cancelUndoTransaction();
    // Called only from the GUI/message thread. Safe to call every timer tick.
    void serviceHostStateChanges();
    bool applyUndoState(const juce::ValueTree& state);
    bool currentStateMatches(const juce::ValueTree& state);

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;

    float getFloatParam(const juce::String& id) const;
    bool  getBoolParam(const juce::String& id) const;

    // Cache APVTS atomic handles once during construction; never construct
    // parameter-id Strings or query the APVTS parameter map on the audio thread.
    struct ParameterHandles
    {
        std::atomic<float>* power = nullptr;
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* oversampling = nullptr;
        std::atomic<float>* amount = nullptr;
        std::atomic<float>* warmth = nullptr;
        std::atomic<float>* presence = nullptr;
        std::atomic<float>* edge = nullptr;
        std::atomic<float>* air = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* output = nullptr;
    } raw;

    // Parameter automation and factory-preset updates are smoothed at HOST rate
    // exactly once per host sample, and reused for both oversampling paths.
    enum SmoothedParam : size_t
    {
        sAmount, sWarmth, sPresence, sEdge, sAir, sMix, sOutputGain, numSmoothedParams
    };
    using LinearSmoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    std::array<LinearSmoother, numSmoothedParams> parameterSmoothers;
    std::array<std::vector<float>, numSmoothedParams> smoothedHostValues;
    void updateParameterTargets() noexcept;

    // UndoManager and its transactions belong exclusively to the message thread.
    // Some hosts restore plugin state on another thread: they only set atomic
    // reset flags, which are later consumed on the message thread.
    void servicePendingUndoReset();
    std::atomic<bool> pendingUndoReset { false };
    std::atomic<bool> hostStateRestoreInProgress { false };

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    struct DSPState
    {
        std::array<Filter, 2> warmthLP, presenceHP, presenceLP;
        std::array<Filter, 2> edgeHP, edgeLP, airHP, dcBlock;
        std::array<float, 2> warmthEnv { 0.0f, 0.0f };
        std::array<float, 2> presenceEnv { 0.0f, 0.0f };
        std::array<float, 2> edgeEnv { 0.0f, 0.0f };
        std::array<float, 2> airEnv { 0.0f, 0.0f };

        // 1x constants preserved; higher oversampling rates use the same
        // attack/release TIMES rather than running the envelopes 2/4/8x faster.
        std::array<float, 4> envelopeAttack {};
        std::array<float, 4> envelopeRelease {};
    };

    void resetDSP(DSPState& state);
    void updateFilters(DSPState& state, double sampleRate);
    float delayForFixedLatency(float input, int channel,
                               int delaySamples, int pathIndex) noexcept;
    void renderOversamplingPath(const juce::dsp::AudioBlock<float>& inputBlock,
                                juce::AudioBuffer<float>& scratch,
                                int index, int channels, int samples);

    float processSampleChannel(float inputSample, int channel,
                               float amount, float warmth, float presence, float edge, float air,
                               float mix, float modeBlend,
                               float focusZeroPresence, float focusZeroEdge,
                               float focusZeroAir, DSPState& state);

    float symmetricExciter(float x, float drive) const;
    float asymmetricExciter(float x, float drive) const;

    void setCurrentPresetInfo(const juce::String& name, bool isUserPreset, bool isModified);
    void commitUndoTransactionIfNeeded();
    juce::ValueTree createFullState();
    void restoreFullState(const juce::ValueTree& state, bool restorePresetMetadata);

    // One pre-initialised DSP path per oversampling quality. No allocation or
    // biquad redesign is needed when the user changes quality while playing.
    std::array<DSPState, 4> dspStates;
    // UI only: published by the audio thread after each completed block.
    // These are NOT read by DSP and cannot alter HE2's processing or sound.
    std::array<std::atomic<float>, 4> bandActivity {};
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    int maxOversamplingBlockSize = 1;
    int activeOversamplingIndex = -1;
    std::array<int, 4> modeLatencies { 0, 0, 0, 0 };
    int fixedLatencySamples = 0;
    // Paths 0..3 are the four quality modes. Path 4 is the always-current
    // bypass history. Each quality has its own latency compensation history.
    std::array<std::array<std::vector<float>, 2>, 5> compensationBuffers;
    std::array<std::array<size_t, 2>, 5> compensationWrite {};

    int transitionTargetIndex = -1;
    int transitionProgressSamples = 0;
    int transitionWarmupSamples = 1;
    int transitionFadeSamples = 1;
    std::array<juce::AudioBuffer<float>, 2> pathScratch;
    std::vector<float> modeBlendValues;

    // Cached Focus zero-input offsets (Presence/Edge/Air), including static
    // harmonic gain but not the per-path envelope modulation. These are
    // calculated once per host sample and shared by all quality paths.
    std::array<std::vector<float>, 3> focusZeroContributions;
    // Host-sample POWER crossfade, applied after decimation/DC removal.
    std::vector<float> bypassBlendValues;
    int powerWarmupSamples = 1;
    int powerWarmupRemaining = 0;

    // Host-rate DC removal for the ENHANCEMENT ONLY, applied after decimation
    // and after the click-free quality crossfade. Double precision is deliberate:
    // 18-Hz IIR DC rejection inside an oversampled float path can leave a small
    // numerical residual, particularly with Focus's asymmetric waveshaper.
    // The original (latency-aligned) input is added back unchanged, including
    // any genuine bass/subsonic content and any DC already present at input.
    std::array<double, 2> enhancementDcEstimate { 0.0, 0.0 };
    double enhancementDcPole = 0.0;
    float removeEnhancementDC(float processed, float delayedDry, int channel) noexcept;

    double currentSampleRate = 44100.0;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassBlendSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> modeSmoother;

    std::atomic<bool> windowSizeSet { false };
    std::atomic<float> curWindowWidth { 900.0f };
    std::atomic<float> curWindowHeight { 700.0f };

    PresetManager presetManager;
    AuthenticationLock authLock;
    PerformanceMonitor performanceMonitor;
    juce::UndoManager undoManager;

    mutable juce::CriticalSection presetStateLock;
    juce::String currentPresetName { "Presets" };
    bool currentPresetIsUser = false;
    std::atomic<bool> currentPresetModified { false };
    std::atomic<bool> suppressPresetDirtyFlag { false };
    std::atomic<bool> undoRedoInProgress { false };

    bool undoTransactionActive = false;
    juce::ValueTree undoTransactionBeforeState;

    AppData appData;
    AppInfo appInfo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HE2AudioProcessor)
};
