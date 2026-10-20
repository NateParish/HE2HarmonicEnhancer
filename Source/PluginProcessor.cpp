#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace IDs
{
    static constexpr auto power    = "power";
    static constexpr auto mode     = "mode";
    static constexpr auto oversampling = "oversampling";
    static constexpr auto amount   = "amount";
    static constexpr auto warmth   = "warmth";
    static constexpr auto presence = "presence";
    static constexpr auto edge     = "edge";
    static constexpr auto air      = "air";
    static constexpr auto mix      = "mix";
    static constexpr auto output   = "output";
}

namespace StateProps
{
    static constexpr auto currentPresetName     = "uiCurrentPresetName";
    static constexpr auto currentPresetIsUser   = "uiCurrentPresetIsUser";
    static constexpr auto currentPresetModified = "uiCurrentPresetModified";
    static constexpr auto savedWindowWidth      = "uiSavedWindowWidth";
    static constexpr auto savedWindowHeight     = "uiSavedWindowHeight";
    static constexpr auto savedWindowSizeSet    = "uiSavedWindowSizeSet";
}

namespace
{
    inline float dbToGainSafe(float db)
    {
        return juce::Decibels::decibelsToGain(db);
    }


    // Keep the original per-sample, per-channel envelope follower unchanged.
    inline float smoothTowards(float current, float target, float attack, float release)
    {
        return target > current
            ? current + (target - current) * attack
            : current + (target - current) * release;
    }

    inline float clamp01(float x)
    {
        return juce::jlimit(0.0f, 1.0f, x);
    }

    // The Focus asymmetric exciter is NOT zero at x=0. When its drive or gain
    // changes, this input-independent offset changes immediately. Passing that
    // step into an 18 Hz DC blocker produces the audible LF thump/rumble.
    // Calculate this once per HOST sample (per band), not per 2x/4x/8x sample.
    inline float asymmetricZeroInput(float drive) noexcept
    {
        return std::tanh(0.12f * drive) + 0.65f * std::tanh(-0.07f * drive);
    }

    class FullStateUndoAction final : public juce::UndoableAction
    {
    public:
        FullStateUndoAction(HE2AudioProcessor& processorToUse,
                            juce::ValueTree beforeStateToUse,
                            juce::ValueTree afterStateToUse)
            : processor(processorToUse),
              beforeState(beforeStateToUse.createCopy()),
              afterState(afterStateToUse.createCopy())
        {
        }

        bool perform() override
        {
            if (firstPerform)
            {
                firstPerform = false;
                if (processor.currentStateMatches(afterState))
                    return true;
            }

            return processor.applyUndoState(afterState);
        }

        bool undo() override
        {
            return processor.applyUndoState(beforeState);
        }

        int getSizeInUnits() override { return 1; }

    private:
        HE2AudioProcessor& processor;
        juce::ValueTree beforeState;
        juce::ValueTree afterState;
        bool firstPerform = true;
    };
}

HE2AudioProcessor::HE2AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
                    #if ! JucePlugin_IsMidiEffect
                     #if ! JucePlugin_IsSynth
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     #endif
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                    #endif
                      ),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
    for (const auto* id : { IDs::power, IDs::mode, IDs::amount, IDs::warmth, IDs::presence, IDs::edge, IDs::air, IDs::mix, IDs::output })
        apvts.addParameterListener(id, this);

    raw.power        = apvts.getRawParameterValue(IDs::power);
    raw.mode         = apvts.getRawParameterValue(IDs::mode);
    raw.oversampling = apvts.getRawParameterValue(IDs::oversampling);
    raw.amount       = apvts.getRawParameterValue(IDs::amount);
    raw.warmth       = apvts.getRawParameterValue(IDs::warmth);
    raw.presence     = apvts.getRawParameterValue(IDs::presence);
    raw.edge         = apvts.getRawParameterValue(IDs::edge);
    raw.air          = apvts.getRawParameterValue(IDs::air);
    raw.mix          = apvts.getRawParameterValue(IDs::mix);
    raw.output       = apvts.getRawParameterValue(IDs::output);
    jassert(raw.power && raw.mode && raw.oversampling && raw.amount && raw.warmth
            && raw.presence && raw.edge && raw.air && raw.mix && raw.output);

    appData.productName = appInfo.productName;
    appData.version = appInfo.version;
    appData.sku = appInfo.sku;
    appData.companyName = appInfo.companyName;
    appData.companyNameShort = appInfo.companyNameShort;
    appData.buildType = appInfo.buildType;
    appData.loginWindowWidth = appInfo.loginWindowWidth;
    appData.loginWindowHeight = appInfo.loginWindowHeight;
    appData.appHeightPctOfWindow = appInfo.appHeightPctOfWindow;
    appData.ratio = appInfo.ratio;
    appData.menuBarHeightPct = appInfo.menuBarHeightPct;
    appData.minWindowWidth = appInfo.minWindowWidth;
    appData.maxWindowWidth = appInfo.maxWindowWidth;

    authLock.setAppData(appData);
    authLock.registerAppData();
    presetManager.loadAppInfo(appData);
}

HE2AudioProcessor::~HE2AudioProcessor()
{
    for (const auto* id : { IDs::power, IDs::mode, IDs::amount, IDs::warmth, IDs::presence, IDs::edge, IDs::air, IDs::mix, IDs::output })
        apvts.removeParameterListener(id, this);
}

const juce::String HE2AudioProcessor::getName() const { return JucePlugin_Name; }

bool HE2AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool HE2AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool HE2AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double HE2AudioProcessor::getTailLengthSeconds() const { return 0.1; }

int HE2AudioProcessor::getNumPrograms() { return 1; }
int HE2AudioProcessor::getCurrentProgram() { return 0; }
void HE2AudioProcessor::setCurrentProgram(int) {}
const juce::String HE2AudioProcessor::getProgramName(int) { return {}; }
void HE2AudioProcessor::changeProgramName(int, const juce::String&) {}

float HE2AudioProcessor::getFloatParam(const juce::String& id) const
{
    if (auto* p = apvts.getRawParameterValue(id))
        return p->load();

    jassertfalse;
    return 0.0f;
}

bool HE2AudioProcessor::getBoolParam(const juce::String& id) const
{
    return getFloatParam(id) >= 0.5f;
}

juce::String HE2AudioProcessor::getCurrentPresetName() const
{
    const juce::ScopedLock sl(presetStateLock);
    return currentPresetName;
}

juce::String HE2AudioProcessor::getCurrentPresetDisplayName() const
{
    juce::String nameCopy;
    const bool modified = currentPresetModified.load();

    {
        const juce::ScopedLock sl(presetStateLock);
        nameCopy = currentPresetName;
    }

    if (nameCopy.isEmpty())
        nameCopy = "Presets";

    if (modified)
        nameCopy << " *";

    return nameCopy;
}

bool HE2AudioProcessor::isCurrentPresetUser() const
{
    const juce::ScopedLock sl(presetStateLock);
    return currentPresetIsUser;
}

bool HE2AudioProcessor::canUndo() const
{
    return !pendingUndoReset.load(std::memory_order_acquire)
        && !hostStateRestoreInProgress.load(std::memory_order_acquire)
        && undoManager.canUndo(); // message thread only
}
bool HE2AudioProcessor::canRedo() const
{
    return !pendingUndoReset.load(std::memory_order_acquire)
        && !hostStateRestoreInProgress.load(std::memory_order_acquire)
        && undoManager.canRedo(); // message thread only
}

void HE2AudioProcessor::servicePendingUndoReset()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (hostStateRestoreInProgress.load(std::memory_order_acquire))
        return;
    if (pendingUndoReset.exchange(false, std::memory_order_acq_rel))
    {
        undoTransactionBeforeState = {};
        undoTransactionActive = false;
        undoManager.clearUndoHistory();
    }
}

void HE2AudioProcessor::serviceHostStateChanges()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    servicePendingUndoReset();
}

void HE2AudioProcessor::performUndo()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    servicePendingUndoReset();
    if (!canUndo() || undoRedoInProgress.exchange(true))
        return;

    cancelUndoTransaction();
    suppressPresetDirtyFlag.store(true);
    undoManager.undo();
    suppressPresetDirtyFlag.store(false);
    undoRedoInProgress.store(false);
}

void HE2AudioProcessor::performRedo()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    servicePendingUndoReset();
    if (!canRedo() || undoRedoInProgress.exchange(true))
        return;

    cancelUndoTransaction();
    suppressPresetDirtyFlag.store(true);
    undoManager.redo();
    suppressPresetDirtyFlag.store(false);
    undoRedoInProgress.store(false);
}

bool HE2AudioProcessor::isCurrentPresetModified() const
{
    return currentPresetModified.load();
}



void HE2AudioProcessor::setCurrentPresetInfo(const juce::String& name, bool isUserPreset, bool isModified)
{
    {
        const juce::ScopedLock sl(presetStateLock);
        currentPresetName = name.isNotEmpty() ? name : "Presets";
        currentPresetIsUser = isUserPreset;
    }

    currentPresetModified.store(isModified);
}

void HE2AudioProcessor::clearCurrentPresetState()
{
    setCurrentPresetInfo("Presets", false, false);
}

void HE2AudioProcessor::beginUndoTransaction(const juce::String& actionName)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    servicePendingUndoReset();
    if (hostStateRestoreInProgress.load(std::memory_order_acquire)
        || pendingUndoReset.load(std::memory_order_acquire)
        || undoRedoInProgress.load() || undoTransactionActive)
        return;

    undoManager.beginNewTransaction(actionName);
    undoTransactionBeforeState = createFullState().createCopy();
    undoTransactionActive = true;
}

void HE2AudioProcessor::endUndoTransaction()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    servicePendingUndoReset();
    if (!hostStateRestoreInProgress.load(std::memory_order_acquire))
        commitUndoTransactionIfNeeded();
}

void HE2AudioProcessor::cancelUndoTransaction()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    undoTransactionBeforeState = {};
    undoTransactionActive = false;
}

void HE2AudioProcessor::commitUndoTransactionIfNeeded()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (pendingUndoReset.load(std::memory_order_acquire)
        || hostStateRestoreInProgress.load(std::memory_order_acquire))
        return;
    if (! undoTransactionActive)
        return;

    const auto beforeState = undoTransactionBeforeState.createCopy();
    undoTransactionBeforeState = {};
    undoTransactionActive = false;

    if (! beforeState.isValid())
        return;

    const auto afterState = createFullState().createCopy();
    if (beforeState.isEquivalentTo(afterState))
        return;

    undoManager.perform(new FullStateUndoAction(*this, beforeState, afterState));
}

bool HE2AudioProcessor::applyUndoState(const juce::ValueTree& state)
{
    if (! state.isValid())
        return false;

    const auto previousSuppress = suppressPresetDirtyFlag.exchange(true);
    restoreFullState(state, true);
    suppressPresetDirtyFlag.store(previousSuppress);
    return true;
}

bool HE2AudioProcessor::currentStateMatches(const juce::ValueTree& state)
{
    return state.isValid() && createFullState().isEquivalentTo(state);
}

void HE2AudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused(parameterID, newValue);

    if (suppressPresetDirtyFlag.load())
        return;

    currentPresetModified.store(true);
}

void HE2AudioProcessor::updateFilters(DSPState& state, double sampleRate)
{
    auto makeAndAssign = [] (auto& filterArray, auto coeffs)
    {
        for (auto& f : filterArray)
            f.coefficients = coeffs;
    };

    makeAndAssign(state.warmthLP,   Coefficients::makeLowPass (sampleRate, 260.0f));
    makeAndAssign(state.presenceHP, Coefficients::makeHighPass(sampleRate, 900.0f));
    makeAndAssign(state.presenceLP, Coefficients::makeLowPass (sampleRate, 3000.0f));
    makeAndAssign(state.edgeHP,     Coefficients::makeHighPass(sampleRate, 2300.0f));
    makeAndAssign(state.edgeLP,     Coefficients::makeLowPass (sampleRate, 6500.0f));
    makeAndAssign(state.airHP,      Coefficients::makeHighPass(sampleRate, 7000.0f));
    makeAndAssign(state.dcBlock,    Coefficients::makeHighPass(sampleRate, 18.0f));
}

void HE2AudioProcessor::resetDSP(DSPState& state)
{
    for (auto* f : {
            &state.warmthLP[0], &state.warmthLP[1],
            &state.presenceHP[0], &state.presenceHP[1],
            &state.presenceLP[0], &state.presenceLP[1],
            &state.edgeHP[0], &state.edgeHP[1],
            &state.edgeLP[0], &state.edgeLP[1],
            &state.airHP[0], &state.airHP[1],
            &state.dcBlock[0], &state.dcBlock[1] })
        f->reset();

    state.warmthEnv = { 0.0f, 0.0f };
    state.presenceEnv = { 0.0f, 0.0f };
    state.edgeEnv = { 0.0f, 0.0f };
    state.airEnv = { 0.0f, 0.0f };

}

void HE2AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax(1.0, sampleRate);
    maxOversamplingBlockSize = juce::jmax(1, samplesPerBlock);
    const int channels = juce::jlimit(1, 2, getTotalNumInputChannels());

    for (int index = 0; index < 4; ++index)
    {
        const double internalRate = currentSampleRate * (1 << index);
        auto& state = dspStates[(size_t) index];
        updateFilters(state, internalRate);
        resetDSP(state);
        const double factor = (double) (1 << index);
        constexpr std::array<double, 4> attacks  { 0.20, 0.22, 0.25, 0.25 };
        constexpr std::array<double, 4> releases { 0.05, 0.06, 0.07, 0.08 };
        for (size_t band = 0; band < 4; ++band)
        {
            state.envelopeAttack[band] = (float) (1.0 - std::pow(1.0 - attacks[band], 1.0 / factor));
            state.envelopeRelease[band] = (float) (1.0 - std::pow(1.0 - releases[band], 1.0 / factor));
        }
    }

    modeLatencies = { 0, 0, 0, 0 };
    for (int index = 1; index < 4; ++index)
    {
        auto& os = oversamplers[(size_t) index - 1];
        os = std::make_unique<juce::dsp::Oversampling<float>>(
            (size_t) channels, (size_t) index,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true, true);
        os->initProcessing((size_t) maxOversamplingBlockSize);
        os->reset();
        modeLatencies[(size_t) index] = juce::roundToInt(os->getLatencyInSamples());
    }

    // Every path returns audio at this one fixed host-reported latency. Delay
    // compensation belongs to EACH path, not to a single shared delay line,
    // because both paths run simultaneously during a quality change.
    fixedLatencySamples = *std::max_element(modeLatencies.begin(), modeLatencies.end());
    for (auto& path : compensationBuffers)
        for (auto& channelDelay : path)
            channelDelay.assign((size_t) fixedLatencySamples + 1, 0.0f);
    for (auto& path : compensationWrite)
        path = { 0, 0 };

    setLatencySamples(fixedLatencySamples);
    activeOversamplingIndex = -1;
    transitionTargetIndex = -1;
    transitionProgressSamples = 0;
    for (auto& level : bandActivity)
        level.store(0.0f, std::memory_order_relaxed);

    // The newly chosen engine is warmed up BEFORE it becomes audible. This
    // allows the oversampling filters, band-pass filters, 18-Hz DC blocker and
    // the envelope followers to settle even in the asymmetric Focus mode.
    // A 100 ms C2-smooth fade provides less LF modulation on quality changes.
    transitionWarmupSamples = juce::jmax(1, juce::roundToInt(currentSampleRate * 0.100));
    transitionFadeSamples = juce::jmax(1, juce::roundToInt(currentSampleRate * 0.100));

    for (auto& scratch : pathScratch)
        scratch.setSize(channels, maxOversamplingBlockSize, false, false, true);
    modeBlendValues.resize((size_t) maxOversamplingBlockSize);
    for (auto& v : focusZeroContributions)
        v.resize((size_t) maxOversamplingBlockSize);
    bypassBlendValues.resize((size_t) maxOversamplingBlockSize);
    for (auto& hostValues : smoothedHostValues)
        hostValues.resize((size_t) maxOversamplingBlockSize);
    for (size_t i = 0; i < numSmoothedParams; ++i)
        parameterSmoothers[i].reset(currentSampleRate,
                                    i == sOutputGain ? 0.025 : 0.045);
    // 45 ms for harmonic levels/mix (less LF modulation), 25 ms for Output.
    const std::array<float, numSmoothedParams> initialValues {
        clamp01(raw.amount->load() / 100.0f),
        clamp01(raw.warmth->load() / 100.0f),
        clamp01(raw.presence->load() / 100.0f),
        clamp01(raw.edge->load() / 100.0f),
        clamp01(raw.air->load() / 100.0f),
        clamp01(raw.mix->load() / 100.0f),
        dbToGainSafe(raw.output->load())
    };
    for (size_t i = 0; i < numSmoothedParams; ++i)
        parameterSmoothers[i].setCurrentAndTargetValue(initialValues[i]);

    // The POWER button is a true, host-rate dry/processed crossfade. In Focus
    // mode, switching straight from bypass into the asymmetric waveshaper can
    // create an audible step. Warm the entire DSP/decimator/DC path first, then
    // use a sufficiently gentle 75 ms fade. These values are HOST samples, not
    // oversampled samples, so behavior is identical at 1x/2x/4x/8x.
    powerWarmupSamples = juce::jmax(1, juce::roundToInt(currentSampleRate * 0.080));
    powerWarmupRemaining = 0;

    // Second, host-rate DC safeguard. 5 Hz affects the enhancer contribution
    // only, not the incoming dry signal. Working in double avoids coefficient
    // precision issues that become more apparent at 4x and 8x in Focus mode.
    enhancementDcPole = std::exp(-2.0 * juce::MathConstants<double>::pi * 5.0
                                 / currentSampleRate);
    enhancementDcEstimate.fill(0.0);

    bypassBlendSmoother.reset(currentSampleRate, 0.075);
    // Start on the latency-aligned dry signal even if the saved project has
    // Power enabled. The DSP is warmed before the processed audio fades in.
    bypassBlendSmoother.setCurrentAndTargetValue(0.0f);
    // Advance once per HOST sample, in both channels, regardless of quality.
    modeSmoother.reset(currentSampleRate, 0.15);
    modeSmoother.setCurrentAndTargetValue(getModeIndex() == 1 ? 1.0f : 0.0f);
}

void HE2AudioProcessor::updateParameterTargets() noexcept
{
    const std::array<float, numSmoothedParams> targets {
        clamp01(raw.amount->load(std::memory_order_relaxed) / 100.0f),
        clamp01(raw.warmth->load(std::memory_order_relaxed) / 100.0f),
        clamp01(raw.presence->load(std::memory_order_relaxed) / 100.0f),
        clamp01(raw.edge->load(std::memory_order_relaxed) / 100.0f),
        clamp01(raw.air->load(std::memory_order_relaxed) / 100.0f),
        clamp01(raw.mix->load(std::memory_order_relaxed) / 100.0f),
        dbToGainSafe(raw.output->load(std::memory_order_relaxed))
    };
    for (size_t i = 0; i < numSmoothedParams; ++i)
        parameterSmoothers[i].setTargetValue(targets[i]);
}

float HE2AudioProcessor::delayForFixedLatency(float input, int channel,
                                               int delaySamples, int pathIndex) noexcept
{
    auto& delay = compensationBuffers[(size_t) pathIndex][(size_t) channel];
    if (delay.empty())
        return input;

    const size_t size = delay.size();
    const size_t write = compensationWrite[(size_t) pathIndex][(size_t) channel];
    delay[write] = input;
    size_t read = write + size - (size_t) delaySamples;
    if (read >= size) read -= size;
    const float output = delay[read];
    compensationWrite[(size_t) pathIndex][(size_t) channel] = (write + 1 == size) ? 0 : write + 1;
    return output;
}

// Reject only the newly generated DC component, not the source signal's DC.
// This runs AFTER the oversampling downsampler, latency compensation and
// quality crossfade, so all four quality choices share the same final response.
// A first-order, double-precision 5-Hz DC servo is intentionally gentle; it
// converges on constant offsets without hard clipping or shifting audio level.
float HE2AudioProcessor::removeEnhancementDC(float processed,
                                              float delayedDry,
                                              int channel) noexcept
{
    const double enhancement = (double) processed - (double) delayedDry;
    auto& dc = enhancementDcEstimate[(size_t) channel];
    dc += (1.0 - enhancementDcPole) * (enhancement - dc);
    return (float) ((double) delayedDry + enhancement - dc);
}

// Render one quality mode into preallocated scratch memory. No allocations or
// state resets occur here; the original HE2 curves and 30% Warmth adjustment
// are still entirely inside processSampleChannel().
void HE2AudioProcessor::renderOversamplingPath(
    const juce::dsp::AudioBlock<float>& inputBlock,
    juce::AudioBuffer<float>& scratch,
    int index, int channels, int samples)
{
    for (int ch = 0; ch < channels; ++ch)
        scratch.copyFrom(ch, 0, inputBlock.getChannelPointer((size_t) ch), samples);

    juce::dsp::AudioBlock<float> fullScratch(scratch);
    auto baseBlock = fullScratch.getSubsetChannelBlock(0, (size_t) channels)
                                .getSubBlock(0, (size_t) samples);
    auto internalBlock = baseBlock;
    if (index > 0)
        internalBlock = oversamplers[(size_t) index - 1]->processSamplesUp(baseBlock);

    auto& state = dspStates[(size_t) index];
    const int factor = 1 << index;
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = internalBlock.getChannelPointer((size_t) ch);
        for (size_t s = 0; s < internalBlock.getNumSamples(); ++s)
        {
            const size_t hostSample = s / (size_t) factor;
            const float dry = data[s];
            const float wet = processSampleChannel(dry, ch,
                smoothedHostValues[sAmount][hostSample],
                smoothedHostValues[sWarmth][hostSample],
                smoothedHostValues[sPresence][hostSample],
                smoothedHostValues[sEdge][hostSample],
                smoothedHostValues[sAir][hostSample],
                smoothedHostValues[sMix][hostSample],
                modeBlendValues[hostSample],
                focusZeroContributions[0][hostSample],
                focusZeroContributions[1][hostSample],
                focusZeroContributions[2][hostSample], state);
            // Always render 100% of the selected HE2 path. Power crossfading
            // happens ONLY after downsampling, fixed latency compensation,
            // and the host-rate enhancement-only DC correction. Smoothing
            // here instead would change the waveshaper/decimator behavior.
            data[s] = wet;
        }
    }

    if (index > 0)
        oversamplers[(size_t) index - 1]->processSamplesDown(baseBlock);

    const int remainingLatency = fixedLatencySamples - modeLatencies[(size_t) index];
    for (int ch = 0; ch < channels; ++ch)
    {
        float* data = scratch.getWritePointer(ch);
        for (int s = 0; s < samples; ++s)
            data[s] = delayForFixedLatency(data[s], ch, remainingLatency, index);
    }
}

void HE2AudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool HE2AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
   #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
   #endif
}
#endif

float HE2AudioProcessor::symmetricExciter(float x, float drive) const
{
    return std::tanh(x * drive) - x * 0.90f;
}

float HE2AudioProcessor::asymmetricExciter(float x, float drive) const
{
    const float a = std::tanh((x + 0.12f) * drive);
    const float b = std::tanh((x - 0.07f) * drive);
    return (a + 0.65f * b) - x * 1.08f;
}

float HE2AudioProcessor::processSampleChannel(float inputSample, int channel,
                                              float amount, float warmth, float presence, float edge, float air,
                                              float mix, float modeBlend,
                                              float focusZeroPresence, float focusZeroEdge,
                                              float focusZeroAir, DSPState& state)
{
    auto& warmLP = state.warmthLP[(size_t) channel];
    auto& presHP = state.presenceHP[(size_t) channel];
    auto& presLP = state.presenceLP[(size_t) channel];
    auto& edHP   = state.edgeHP[(size_t) channel];
    auto& edLP   = state.edgeLP[(size_t) channel];
    auto& aHP    = state.airHP[(size_t) channel];
    auto& dc     = state.dcBlock[(size_t) channel];

    const float warmBand = warmLP.processSample(inputSample);
    const float presBand = presLP.processSample(presHP.processSample(inputSample));
    const float edgeBand = edLP.processSample(edHP.processSample(inputSample));
    const float airBand  = aHP.processSample(inputSample);

    state.warmthEnv[(size_t) channel]   = smoothTowards(state.warmthEnv[(size_t) channel],   std::abs(warmBand), state.envelopeAttack[0], state.envelopeRelease[0]);
    state.presenceEnv[(size_t) channel] = smoothTowards(state.presenceEnv[(size_t) channel], std::abs(presBand), state.envelopeAttack[1], state.envelopeRelease[1]);
    state.edgeEnv[(size_t) channel]     = smoothTowards(state.edgeEnv[(size_t) channel],     std::abs(edgeBand), state.envelopeAttack[2], state.envelopeRelease[2]);
    state.airEnv[(size_t) channel]      = smoothTowards(state.airEnv[(size_t) channel],      std::abs(airBand),  state.envelopeAttack[3], state.envelopeRelease[3]);

    const float global = amount;
    const float warmAmt = warmth * global;
    const float presAmt = presence * global;
    const float edgeAmt = edge * global;
    const float airAmt  = air * global;

    // In settled modes the opposite shaper contributes nothing, so do not
    // evaluate its expensive tanh calls. Both shapers run ONLY while the
    // host-rate 150 ms mode smoother is genuinely between endpoints.
    auto makeFocusWet = [&]() noexcept -> float
    {
        const float focusWarmEnh =
            symmetricExciter(warmBand, 1.7f + 1.0f * warmAmt) * (0.585f * warmAmt);
    
        const float focusPresDynamic =
            0.7f + 0.8f * clamp01(state.presenceEnv[(size_t) channel] * 4.0f);
        const float focusPresEnh =
            asymmetricExciter(presBand, 2.0f + 1.9f * presAmt) * (0.58f * presAmt * focusPresDynamic);
    
        const float focusEdgeDynamic =
            0.7f + 0.9f * clamp01(state.edgeEnv[(size_t) channel] * 4.5f);
        const float focusEdgeEnh =
            asymmetricExciter(edgeBand, 2.2f + 2.1f * edgeAmt) * (0.62f * edgeAmt * focusEdgeDynamic);
    
        const float focusAirSafety =
            1.0f - 0.45f * clamp01(state.airEnv[(size_t) channel] * 8.0f);
        const float focusAirEnh =
            asymmetricExciter(airBand, 1.8f + 1.7f * airAmt) * (0.42f * airAmt * focusAirSafety);
    
        float focusWet = inputSample + focusWarmEnh + focusPresEnh + focusEdgeEnh + focusAirEnh;
        focusWet = std::tanh(focusWet * 1.06f) * 0.985f;

        // Exact zero-band-input output of the SAME Focus waveshaping formula,
        // using the current dynamic weights. Subtract AFTER final saturation,
        // so the saturation's bias/timbre and the steady-state AC response stay
        // intact. Only the parameter-dependent DC pedestal is cancelled.
        // The existing 18 Hz + host-rate 5 Hz filters still reject real
        // even-harmonic DC produced by a nonzero signal.
        const float focusZeroSum =
              focusZeroPresence * focusPresDynamic
            + focusZeroEdge * focusEdgeDynamic
            + focusZeroAir * focusAirSafety;
        const float focusZeroWet = std::tanh(focusZeroSum * 1.06f) * 0.985f;
        return focusWet - focusZeroWet;
    };
    auto makeSmoothWet = [&]() noexcept -> float
    {
        // Give Smooth a more useful mid-range on the Amount control without
        // changing either endpoint or the original saturation algorithms.
        // This monotonic curve maps 0% -> 0%, 50% -> 67.5%, 100% -> 100%.
        // Focus deliberately continues to use the original linear Amount.
        // Evaluate on the already-smoothed value to avoid knob zipper noise.
        const float smoothAmount = amount + 0.70f * amount * (1.0f - amount);
        const float smoothWarmAmt = warmth * smoothAmount;
        const float smoothPresAmt = presence * smoothAmount;
        const float smoothEdgeAmt = edge * smoothAmount;
        const float smoothAirAmt  = air * smoothAmount;

        // Smooth mode path. The original soft transfer curves, frequency bands,
        // and final full-signal saturation stay unchanged. Increase the level of
        // the generated harmonics modestly to narrow the apparent strength gap
        // with Focus without changing Smooth into another asymmetric exciter.
        // Warmth already has the earlier +30% adjustment, so its extra lift here
        // is deliberately smaller than for Presence / Edge / Air.
        const float smoothWarmDynamic =
            0.75f + 0.6f * clamp01(state.warmthEnv[(size_t) channel] * 4.0f);
        const float smoothWarmEnh =
            symmetricExciter(warmBand, 1.5f + 1.1f * smoothWarmAmt) * (0.830f * smoothWarmAmt * smoothWarmDynamic);
    
        const float smoothPresEnh =
            symmetricExciter(presBand, 1.7f + 1.2f * smoothPresAmt) * (0.54f * smoothPresAmt);
    
        const float smoothEdgeSafety =
            1.0f - 0.35f * clamp01(state.edgeEnv[(size_t) channel] * 7.0f);
        const float smoothEdgeEnh =
            symmetricExciter(edgeBand, 1.5f + 1.0f * smoothEdgeAmt) * (0.42f * smoothEdgeAmt * smoothEdgeSafety);
    
        const float smoothAirSafety =
            1.0f - 0.55f * clamp01(state.airEnv[(size_t) channel] * 10.0f);
        const float smoothAirEnh =
            symmetricExciter(airBand, 1.35f + 0.9f * smoothAirAmt) * (0.315f * smoothAirAmt * smoothAirSafety);
    
        // Single voicing trim for the Smooth harmonic return. Raise/lower this
        // to taste (1.00f = previous HE2 sound, 1.25f = +25% harmonic contribution).
        // This scales only the four band enhancements BEFORE final saturation;
        // Focus, dry signal, Mix, Amount curve and factory presets stay intact.
        constexpr float smoothHarmonicBoost = 1.25f;
        const float smoothHarmonics = smoothWarmEnh + smoothPresEnh
                                    + smoothEdgeEnh + smoothAirEnh;
        float smoothWet = inputSample + smoothHarmonicBoost * smoothHarmonics;
        smoothWet = std::tanh(smoothWet * 1.03f) * 0.992f;
        return smoothWet;
    };

    float wet;
    if (modeBlend <= 0.0f)
        wet = makeSmoothWet();
    else if (modeBlend >= 1.0f)
        wet = makeFocusWet();
    else
    {
        const float smoothWet = makeSmoothWet();
        const float focusWet = makeFocusWet();
        // Smoothstep gives zero slope at each endpoint, avoiding a sudden
        // change in slew rate when switching in either direction.
        const float blend = modeBlend * modeBlend * (3.0f - 2.0f * modeBlend);
        wet = smoothWet + (focusWet - smoothWet) * blend;
    }

    wet = dc.processSample(wet);
    return inputSample + (wet - inputSample) * mix;
}

void HE2AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int channels = juce::jmin(2, juce::jmin(getTotalNumInputChannels(), buffer.getNumChannels()));
    const int samples = buffer.getNumSamples();
    for (int ch = channels; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, samples);
    if (channels < 1 || samples < 1)
        return;

    const int requestedIndex = juce::jlimit(0, 3,
        juce::roundToInt(raw.oversampling->load(std::memory_order_relaxed)));
    const bool enabled = authLock.isUnlocked()
                         && raw.power->load(std::memory_order_relaxed) >= 0.5f;
    updateParameterTargets();

    // In steady bypass, avoid running any nonlinear or oversampling DSP.
    // Keep the dry delay filled so power switching remains sample-aligned.
    // However, while turning OFF we continue processing until the fade reaches
    // zero. While turning ON we warm the selected DSP silently for 80 ms.
    if (! enabled && activeOversamplingIndex < 0)
    {
        for (auto& level : bandActivity)
            level.store(0.0f, std::memory_order_relaxed);
        for (int ch = 0; ch < channels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int s = 0; s < samples; ++s)
                data[s] = delayForFixedLatency(data[s], ch, fixedLatencySamples, 4);
        }
        modeSmoother.setCurrentAndTargetValue(raw.mode->load(std::memory_order_relaxed) >= 0.5f ? 1.0f : 0.0f);
        return;
    }

    if (enabled && activeOversamplingIndex < 0)
    {
        activeOversamplingIndex = requestedIndex;
        resetDSP(dspStates[(size_t) activeOversamplingIndex]);
        if (activeOversamplingIndex > 0)
            oversamplers[(size_t) activeOversamplingIndex - 1]->reset();
        for (int ch = 0; ch < channels; ++ch)
        {
            auto& delay = compensationBuffers[(size_t) activeOversamplingIndex][(size_t) ch];
            std::fill(delay.begin(), delay.end(), 0.0f);
            compensationWrite[(size_t) activeOversamplingIndex][(size_t) ch] = 0;
        }

        // The filters, decimator, fixed-latency FIFO and the DC servo all need
        // history. No processed signal reaches the output during warmup.
        enhancementDcEstimate.fill(0.0);
        powerWarmupRemaining = powerWarmupSamples;
        bypassBlendSmoother.setCurrentAndTargetValue(0.0f);
    }

    if (! enabled)
    {
        // A power-off request is an ordinary fade back to delayed dry. This
        // also handles turning off midway through a warmup or fade-in.
        powerWarmupRemaining = 0;
        bypassBlendSmoother.setTargetValue(0.0f);
    }

    // Never drop the currently audible processing path on a quality change.
    // A second path starts silently, runs for 100 ms, then crossfades for 100 ms.
    // A further click while fading is queued by the actual APVTS parameter and
    // handled after the current transition completes.
    if (transitionTargetIndex < 0 && requestedIndex != activeOversamplingIndex)
    {
        transitionTargetIndex = requestedIndex;
        transitionProgressSamples = 0;
        resetDSP(dspStates[(size_t) transitionTargetIndex]);
        if (transitionTargetIndex > 0)
            oversamplers[(size_t) transitionTargetIndex - 1]->reset();
        for (int ch = 0; ch < channels; ++ch)
        {
            auto& delay = compensationBuffers[(size_t) transitionTargetIndex][(size_t) ch];
            std::fill(delay.begin(), delay.end(), 0.0f);
            compensationWrite[(size_t) transitionTargetIndex][(size_t) ch] = 0;
        }
    }

    // All seven scalar values are ramped once at host rate and shared by
    // both quality paths. Avoid zipper noise without multiplying the smoother
    // advance rate by 2/4/8 or by the number of stereo channels.

    if (enabled && powerWarmupRemaining == 0)
        bypassBlendSmoother.setTargetValue(1.0f);
    modeSmoother.setTargetValue(raw.mode->load(std::memory_order_relaxed) >= 0.5f ? 1.0f : 0.0f);

    juce::dsp::AudioBlock<float> wholeBlock(buffer);
    auto input = wholeBlock.getSubsetChannelBlock(0, (size_t) channels);

    int offset = 0;
    while (offset < samples)
    {
        const int count = juce::jmin(maxOversamplingBlockSize, samples - offset);
        auto segment = input.getSubBlock((size_t) offset, (size_t) count);

        // Do not advance these smoothers twice during parallel rendering, or
        // once per channel. Both quality paths see the same mode modulation.
        for (int s = 0; s < count; ++s)
        {
            const size_t hs = (size_t) s;
            modeBlendValues[hs] = modeSmoother.getNextValue();
            for (size_t p = 0; p < numSmoothedParams; ++p)
                smoothedHostValues[p][hs] = parameterSmoothers[p].getNextValue();

            // Cache the asymmetric zero-input pedestal in the HOST domain.
            // Its per-path envelope-dependent weighting is applied in the
            // waveshaper. This saves 6 tanh() calls per OVERSAMPLED sample.
            if (modeBlendValues[hs] > 0.0f)
            {
                const float amount = smoothedHostValues[sAmount][hs];
                const float pres = amount * smoothedHostValues[sPresence][hs];
                const float edge = amount * smoothedHostValues[sEdge][hs];
                const float air = amount * smoothedHostValues[sAir][hs];
                focusZeroContributions[0][hs] = asymmetricZeroInput(2.0f + 1.9f * pres)
                                                  * (0.58f * pres);
                focusZeroContributions[1][hs] = asymmetricZeroInput(2.2f + 2.1f * edge)
                                                  * (0.62f * edge);
                focusZeroContributions[2][hs] = asymmetricZeroInput(1.8f + 1.7f * air)
                                                  * (0.42f * air);
            }
            if (enabled && powerWarmupRemaining > 0)
            {
                bypassBlendValues[(size_t) s] = 0.0f;
                if (--powerWarmupRemaining == 0)
                    bypassBlendSmoother.setTargetValue(1.0f);
            }
            else
            {
                bypassBlendValues[(size_t) s] = bypassBlendSmoother.getNextValue();
            }
        }

        renderOversamplingPath(segment, pathScratch[0], activeOversamplingIndex,
            channels, count);
        const bool transitioning = transitionTargetIndex >= 0;
        if (transitioning)
            renderOversamplingPath(segment, pathScratch[1], transitionTargetIndex,
                channels, count);

        const int transitionLength = transitionWarmupSamples + transitionFadeSamples;
        for (int ch = 0; ch < channels; ++ch)
        {
            float* dest = buffer.getWritePointer(ch) + offset;
            const float* oldOutput = pathScratch[0].getReadPointer(ch);
            const float* newOutput = transitioning ? pathScratch[1].getReadPointer(ch) : nullptr;

            for (int s = 0; s < count; ++s)
            {
                // Bypass delay is always fed with original, unprocessed audio.
                // Do this before overwriting the incoming host buffer.
                const float delayedDry =
                    delayForFixedLatency(dest[s], ch, fixedLatencySamples, 4);

                float out = oldOutput[s];
                if (transitioning)
                {
                    const int progress = transitionProgressSamples + s;
                    if (progress >= transitionWarmupSamples)
                    {
                        const float t = juce::jmin(1.0f,
                            (float) (progress - transitionWarmupSamples) / (float) transitionFadeSamples);
                        const float fade = t * t * t * (t * (6.0f * t - 15.0f) + 10.0f);
                        out += (newOutput[s] - oldOutput[s]) * fade;
                    }
                }
                // Remove any post-decimation DC from the harmonic contribution
                // alone. This preserves the incoming bass/dry path and works in
                // Focus/Smooth at 1x, 2x, 4x and 8x alike.
                // Crossfade the FINAL processed result against the same
                // latency-aligned dry signal. Including Output trim inside
                // the processed endpoint also avoids a jump when Output is
                // not 0 dB. The DC servo keeps learning during warmup/fades.
                const float enhanced = removeEnhancementDC(out, delayedDry, ch)
                                       * smoothedHostValues[sOutputGain][(size_t) s];
                dest[s] = delayedDry + (enhanced - delayedDry) * bypassBlendValues[(size_t) s];
            }
        }

        if (transitioning)
        {
            transitionProgressSamples += count;
            if (transitionProgressSamples >= transitionLength)
            {
                activeOversamplingIndex = transitionTargetIndex;
                transitionTargetIndex = -1;
                transitionProgressSamples = 0;
            }
        }
        offset += count;
    }

    // Read existing band envelope followers at BLOCK rate. Their amplitudes
    // are shaped by the relevant user amount and mix only for visualization;
    // there are no additional filter passes, allocations, or DSP changes.
    if (activeOversamplingIndex >= 0)
    {
        const auto& state = dspStates[(size_t) activeOversamplingIndex];
        const float amount = parameterSmoothers[sAmount].getCurrentValue();
        const float mix    = parameterSmoothers[sMix].getCurrentValue();
        const std::array<float, 4> bands {
            parameterSmoothers[sWarmth].getCurrentValue(),
            parameterSmoothers[sPresence].getCurrentValue(),
            parameterSmoothers[sEdge].getCurrentValue(),
            parameterSmoothers[sAir].getCurrentValue()
        };
        const std::array<const std::array<float, 2>*, 4> envelopes {
            &state.warmthEnv, &state.presenceEnv, &state.edgeEnv, &state.airEnv
        };
        for (size_t band = 0; band < 4; ++band)
        {
            const float envelope = ((*envelopes[band])[0]
                                  + (channels > 1 ? (*envelopes[band])[1] : 0.0f))
                                 / (float) channels;
            const float drive = envelope * 10.8f * juce::jmax(0.12f, bands[band])
                              * juce::jmax(0.18f, amount * mix);
            const float activity = std::pow(juce::jlimit(0.0f, 1.0f, drive), 0.40f);
            bandActivity[band].store(enabled ? activity : 0.0f,
                                     std::memory_order_relaxed);
        }
    }

    // Once power-off has faded fully to dry, stop running the DSP to save CPU.
    // The next power-on request will reset/warm the chosen path before fading.
    if (! enabled && ! bypassBlendSmoother.isSmoothing()
        && bypassBlendSmoother.getCurrentValue() == 0.0f)
    {
        activeOversamplingIndex = -1;
        transitionTargetIndex = -1;
        transitionProgressSamples = 0;
    }
}

bool HE2AudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* HE2AudioProcessor::createEditor()
{
    return new HE2AudioProcessorEditor(*this);
}

juce::ValueTree HE2AudioProcessor::createFullState()
{
    auto state = apvts.copyState();

    {
        const juce::ScopedLock sl(presetStateLock);
        state.setProperty(StateProps::currentPresetName, currentPresetName, nullptr);
        state.setProperty(StateProps::currentPresetIsUser, currentPresetIsUser, nullptr);
    }

    state.setProperty(StateProps::currentPresetModified, currentPresetModified.load(), nullptr);
    state.setProperty(StateProps::savedWindowWidth, curWindowWidth.load(), nullptr);
    state.setProperty(StateProps::savedWindowHeight, curWindowHeight.load(), nullptr);
    state.setProperty(StateProps::savedWindowSizeSet, windowSizeSet.load(), nullptr);
    return state;
}

void HE2AudioProcessor::restoreFullState(const juce::ValueTree& state,
                                         bool restorePresetMetadata)
{
    if (! state.isValid())
        return;

    const juce::ScopedLock callbackLock(getCallbackLock());

    const juce::String restoredName =
        state.getProperty(StateProps::currentPresetName, "Presets").toString();
    const bool restoredIsUser =
        (bool) state.getProperty(StateProps::currentPresetIsUser, false);
    const bool restoredModified =
        (bool) state.getProperty(StateProps::currentPresetModified, false);
    const bool restoredWindowSizeSet =
        (bool) state.getProperty(StateProps::savedWindowSizeSet, windowSizeSet.load());
    const float restoredWindowWidth =
        (float) state.getProperty(StateProps::savedWindowWidth, curWindowWidth.load());
    const float restoredWindowHeight =
        (float) state.getProperty(StateProps::savedWindowHeight, curWindowHeight.load());

    suppressPresetDirtyFlag.store(true);
    apvts.replaceState(state);
    suppressPresetDirtyFlag.store(false);

    if (restorePresetMetadata)
        setCurrentPresetInfo(restoredName, restoredIsUser, restoredModified);

    if (restoredWindowSizeSet && restoredWindowWidth > 0.0f && restoredWindowHeight > 0.0f)
        setCurWindowSize(restoredWindowWidth, restoredWindowHeight);
    else
        clearCurWindowSize();
}

void HE2AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = createFullState().createXml())
        copyXmlToBinary(*xml, destData);
}

void HE2AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        const auto restored = juce::ValueTree::fromXml(*xmlState);
        if (!restored.isValid())
            return;

        // Host project restoration is allowed to arrive on a non-message
        // thread. Never touch UndoManager or its GUI-owned transaction state
        // here. The next GUI tick/undo operation performs the invalidation.
        hostStateRestoreInProgress.store(true, std::memory_order_release);
        pendingUndoReset.store(true, std::memory_order_release);
        restoreFullState(restored, true);
        hostStateRestoreInProgress.store(false, std::memory_order_release);

        if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
            if (mm->isThisTheMessageThread())
                servicePendingUndoReset();
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout HE2AudioProcessor::createParameterLayout()
{
    using namespace juce;

    auto pid = [] (const char* id, int version = 1) { return ParameterID { id, version }; };

    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back(std::make_unique<AudioParameterBool>(pid("power"), "Power", true));

    params.push_back(std::make_unique<AudioParameterChoice>(pid("mode"), "Mode",
        StringArray { "Smooth", "Focus" }, 0));

    // Index 0 preserves HE2 original 1x signal path and harmonic curves.
    params.push_back(std::make_unique<AudioParameterChoice>(pid("oversampling"), "Oversampling",
        StringArray { "1x", "2x", "4x", "8x" }, 0));

    params.push_back(std::make_unique<AudioParameterFloat>(pid("amount"), "Amount",
        NormalisableRange<float>(0.0f, 100.0f, 0.01f), 55.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(pid("warmth"), "Warmth",
        NormalisableRange<float>(0.0f, 100.0f, 0.01f), 28.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(pid("presence"), "Presence",
        NormalisableRange<float>(0.0f, 100.0f, 0.01f), 34.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(pid("edge"), "Edge",
        NormalisableRange<float>(0.0f, 100.0f, 0.01f), 22.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(pid("air"), "Air",
        NormalisableRange<float>(0.0f, 100.0f, 0.01f), 26.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(pid("mix"), "Mix",
        NormalisableRange<float>(0.0f, 100.0f, 0.01f), 42.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(pid("output"), "Output",
        NormalisableRange<float>(-18.0f, 18.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

float HE2AudioProcessor::getParamValue(const juce::String& paramID) const
{
    return getFloatParam(paramID);
}

void HE2AudioProcessor::setParamValue(const juce::String& paramID, float value)
{
    if (auto* param = apvts.getParameter(paramID))
        param->setValueNotifyingHost(param->convertTo0to1(value));
}

juce::StringArray HE2AudioProcessor::getFactoryPresetNames() const
{
    return {
        "Vocal Lift",
        "Airy Vocal",
        "Mix Polish",
        "Guitar Presence",
        "Bass Focus",
        "Acoustic Shine",
        "Snare Crack",
        "Modern Gloss",
        "Classic Exciter",
        "Master Sparkle"
    };
}

juce::StringArray HE2AudioProcessor::getUserPresetNames() const
{
    return presetManager.getUserPresetNames();
}

bool HE2AudioProcessor::renameUserPreset(const juce::String& oldName, const juce::String& newName)
{
    const bool ok = presetManager.renamePreset(oldName, newName);

    if (ok && isCurrentPresetUser() && getCurrentPresetName().equalsIgnoreCase(oldName))
        setCurrentPresetInfo(newName, true, false);

    return ok;
}

void HE2AudioProcessor::applyFactoryPreset(const juce::String& presetName)
{
    beginUndoTransaction("Factory Preset");
    suppressPresetDirtyFlag.store(true);

    // Updated for the stronger Warmth algorithm and the current HE2 processing.
    // Presets have clearly distinguishable purposes, but avoid pushing every
    // band at once. Output trim and oversampling are user preferences, not
    // part of the sound-design preset; neither is modified below.
    auto setPreset = [this](float mode,
                            float amount,
                            float warmth,
                            float presence,
                            float edge,
                            float air,
                            float mix)
    {
        setParamValue(IDs::power, 1.0f);
        setParamValue(IDs::mode, mode);
        setParamValue(IDs::amount, amount);
        setParamValue(IDs::warmth, warmth);
        setParamValue(IDs::presence, presence);
        setParamValue(IDs::edge, edge);
        setParamValue(IDs::air, air);
        setParamValue(IDs::mix, mix);
    };

    // Order: Mode (0 Smooth, 1 Focus), Amount, Warmth, Presence, Edge, Air, Mix.
    // Each name maps to a deliberate tonal emphasis rather than a general
    // increase in every parameter. These are starting points for final DAW
    // listening/level-matched verification on representative source material.
    if (presetName == "Vocal Lift")
        setPreset(1.0f, 72.0f, 10.0f, 58.0f, 18.0f, 35.0f, 52.0f);
    else if (presetName == "Airy Vocal")
        setPreset(0.0f, 70.0f,  5.0f, 35.0f,  9.0f, 66.0f, 48.0f);
    else if (presetName == "Mix Polish")
        setPreset(0.0f, 62.0f, 18.0f, 30.0f, 12.0f, 32.0f, 48.0f);
    else if (presetName == "Guitar Presence")
        setPreset(1.0f, 75.0f, 18.0f, 55.0f, 46.0f, 14.0f, 52.0f);
    else if (presetName == "Bass Focus")
        setPreset(1.0f, 68.0f, 58.0f, 20.0f,  8.0f,  4.0f, 44.0f);
    else if (presetName == "Acoustic Shine")
        setPreset(0.0f, 65.0f,  8.0f, 36.0f, 19.0f, 53.0f, 46.0f);
    else if (presetName == "Snare Crack")
        setPreset(1.0f, 82.0f,  5.0f, 58.0f, 62.0f, 11.0f, 52.0f);
    else if (presetName == "Modern Gloss")
        setPreset(0.0f, 74.0f, 10.0f, 42.0f, 20.0f, 62.0f, 50.0f);
    else if (presetName == "Classic Exciter")
        setPreset(1.0f, 82.0f, 40.0f, 43.0f, 34.0f, 28.0f, 54.0f);
    else if (presetName == "Master Sparkle")
        setPreset(0.0f, 62.0f,  5.0f, 24.0f,  9.0f, 41.0f, 45.0f);
    else
        setPreset(0.0f, 60.0f, 14.0f, 30.0f, 15.0f, 30.0f, 40.0f);

    suppressPresetDirtyFlag.store(false);
    setCurrentPresetInfo(presetName, false, false);
    endUndoTransaction();
}

bool HE2AudioProcessor::saveUserPreset(const juce::String& presetName)
{
    const bool ok = presetManager.savePreset(presetName, apvts.copyState());

    if (ok)
        setCurrentPresetInfo(presetName, true, false);

    return ok;
}

bool HE2AudioProcessor::loadUserPreset(const juce::String& presetName)
{
    juce::ValueTree state;
    if (! presetManager.loadPreset(presetName, state))
        return false;

    beginUndoTransaction("Load User Preset");

    struct ScopedProcessingSuspend
    {
        explicit ScopedProcessingSuspend(juce::AudioProcessor& p) : processor(p)
        {
            processor.suspendProcessing(true);
        }

        ~ScopedProcessingSuspend()
        {
            processor.suspendProcessing(false);
        }

        juce::AudioProcessor& processor;
    };

    ScopedProcessingSuspend suspend(*this);

    // The output trim and oversampling quality are global/user preferences:
    // never overwrite them while auditioning a user preset. Copy before editing
    // so the original preset on disk remains untouched. Host project/undo state
    // loads still restore ALL parameters through restoreFullState normally.
    auto restoredPreset = state.createCopy();
    const auto liveState = apvts.copyState();
    for (const auto* id : { IDs::output, IDs::oversampling })
    {
        const auto liveParameter = liveState.getChildWithProperty("id", id);
        auto loadedParameter = restoredPreset.getChildWithProperty("id", id);
        if (liveParameter.isValid())
        {
            if (loadedParameter.isValid())
                loadedParameter.copyPropertiesFrom(liveParameter, nullptr);
            else
                restoredPreset.addChild(liveParameter.createCopy(), -1, nullptr);
        }
        if (liveState.hasProperty(id))
            restoredPreset.setProperty(id, liveState.getProperty(id), nullptr);
        else
            restoredPreset.removeProperty(id, nullptr);
    }
    restoreFullState(restoredPreset, false);
    setCurrentPresetInfo(presetName, true, false);
    endUndoTransaction();
    return true;
}

bool HE2AudioProcessor::deleteUserPreset(const juce::String& presetName)
{
    const bool ok = presetManager.deletePreset(presetName);

    if (ok && isCurrentPresetUser() && getCurrentPresetName().equalsIgnoreCase(presetName))
        clearCurrentPresetState();

    return ok;
}

#include "../../AppLibrary/Source/PresetManager.cpp"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HE2AudioProcessor();
}
