#include "PluginProcessor.h"
#include "PluginEditor.h"

PapaloteAudioProcessor::PapaloteAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    apvts.addParameterListener(AppConstants::OS_FACTOR_ID, this);
    apvts.addParameterListener(AppConstants::TAPE_TYPE_ID, this);
}

PapaloteAudioProcessor::~PapaloteAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
PapaloteAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::NormalisableRange<float> dbRange(AppConstants::DB_MIN, AppConstants::DB_MAX, 0.01f);
    dbRange.setSkewForCentre(AppConstants::DB_SKEW_CENTER);

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        AppConstants::INPUT_TRIM_ID, "Input Trim", dbRange, AppConstants::DB_DEFAULT));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        AppConstants::DRIVE_ID, "Drive",
        AppConstants::DRIVE_MIN, AppConstants::DRIVE_MAX, AppConstants::DRIVE_DEFAULT));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        AppConstants::DRY_WET_ID, "Dry/Wet",
        AppConstants::DRY_WET_MIN, AppConstants::DRY_WET_MAX, AppConstants::DRY_WET_DEFAULT));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        AppConstants::TONE_ID, "Tone",
        AppConstants::TONE_MIN, AppConstants::TONE_MAX, AppConstants::TONE_DEFAULT));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        AppConstants::CLIP_ID, "Out", dbRange, AppConstants::DB_DEFAULT));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        AppConstants::TAPE_TYPE_ID, "Material",
        juce::StringArray { "Ambar", "Obsidiana", "Aguamarina", "Ceniza", "Niebla" },
        AppConstants::TAPE_TYPE_DEFAULT));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        AppConstants::BYPASS_ID, "Bypass", false));

    // Choice index 0 = "x1" (no module); 1..4 map to 2x/4x/8x/16x factors.
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        AppConstants::OS_FACTOR_ID, "Oversampling",
        juce::StringArray { "x1", "2x", "4x", "8x", "16x" },
        AppConstants::OS_FACTOR_DEFAULT_INDEX));

    return { params.begin(), params.end() };
}

void PapaloteAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == AppConstants::OS_FACTOR_ID)
    {
        needsOversamplingRebuild.store(true);
        triggerAsyncUpdate();
    }
    else if (parameterID == AppConstants::TAPE_TYPE_ID)
    {
        applyModeToAll(static_cast<int> (apvts.getRawParameterValue(AppConstants::TAPE_TYPE_ID)->load()));
    }
}

void PapaloteAudioProcessor::applyModeToAll(int type)
{
    for (int i = 0; i < getTotalNumInputChannels(); ++i)
        if (ptrTapeEmulation[i])
        {
            ptrTapeEmulation[i]->setMode(type);
            ptrTapeEmulation[i]->resetPreviousState();
        }
}

void PapaloteAudioProcessor::handleAsyncUpdate()
{
    if (needsOversamplingRebuild.load())
    {
        rebuildOversamplingModule(getTotalNumInputChannels(), currentSamplesPerBlock);
    }
}

void PapaloteAudioProcessor::rebuildOversamplingModule(int numChannels, int samplesPerBlock)
{
    const int factorIndex = static_cast<int> (
        apvts.getRawParameterValue(AppConstants::OS_FACTOR_ID)->load());

    juce::ScopedWriteLock wl(oversamplingLock);

    osToggle = factorIndex > 0;

    if (osToggle)
    {
        oversamplingModule = std::make_unique<juce::dsp::Oversampling<float>>(
            numChannels,
            factorIndex,
            juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR);
        oversamplingModule->reset();
        oversamplingModule->initProcessing(static_cast<size_t> (samplesPerBlock));
    }
    else
    {
        oversamplingModule.reset();
    }

    needsOversamplingRebuild.store(false);
}

void PapaloteAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSamplesPerBlock = samplesPerBlock;

    rebuildOversamplingModule(getTotalNumInputChannels(), samplesPerBlock);

    const int tapeType = static_cast<int> (
        apvts.getRawParameterValue(AppConstants::TAPE_TYPE_ID)->load());

    for (int i = 0; i < getTotalNumInputChannels(); ++i)
    {
        if (!ptrTapeEmulation[i])
            ptrTapeEmulation[i] = std::make_unique<TapeEmulation>();

        ptrTapeEmulation[i]->setSampleRate(sampleRate);
        ptrTapeEmulation[i]->resetPreviousState();
        ptrTapeEmulation[i]->setMode(tapeType);
    }
}

void PapaloteAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PapaloteAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif
    return true;
#endif
}
#endif

void PapaloteAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    if (apvts.getRawParameterValue(AppConstants::BYPASS_ID)->load()
    > AppConstants::BUTTON_THRESHOLD)
        return;

    const int   tapeType       = static_cast<int> (apvts.getRawParameterValue(AppConstants::TAPE_TYPE_ID)->load());
    const float inputTrimGain  = juce::Decibels::decibelsToGain(apvts.getRawParameterValue(AppConstants::INPUT_TRIM_ID)->load());
    const float drive          = apvts.getRawParameterValue(AppConstants::DRIVE_ID)->load();
    const float dryWet         = apvts.getRawParameterValue(AppConstants::DRY_WET_ID)->load();
    const float tone           = apvts.getRawParameterValue(AppConstants::TONE_ID)->load();

    juce::dsp::AudioBlock<float> fullBlock(buffer);
    auto activeBlock = fullBlock.getSubsetChannelBlock(0, (size_t) totalNumInputChannels);

    // processBlock only reads the module; rebuildOversamplingModule() holds the WriteLock.
    juce::ScopedReadLock rl(oversamplingLock);

    if (osToggle && oversamplingModule != nullptr)
    {
        auto upBlock = oversamplingModule->processSamplesUp(activeBlock);

        for (size_t ch = 0; ch < (size_t) totalNumInputChannels; ++ch)
        {
            float* data = upBlock.getChannelPointer(ch);
            ptrTapeEmulation[ch]->processAudio(
                data, data,
                inputTrimGain,
                drive, tapeType, dryWet, tone,
                static_cast<int>(upBlock.getNumSamples()));
        }

        oversamplingModule->processSamplesDown(activeBlock);
    }
    else
    {
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            ptrTapeEmulation[ch]->processAudio(
                data, data,
                inputTrimGain,
                drive, tapeType, dryWet, tone,
                buffer.getNumSamples());
        }
    }

    const float outGain = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue(AppConstants::CLIP_ID)->load());
    buffer.applyGain(0, buffer.getNumSamples(), outGain);
}

bool PapaloteAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* PapaloteAudioProcessor::createEditor()
{
    return new PapaloteAudioProcessorEditor(*this);
}

void PapaloteAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PapaloteAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

const juce::String PapaloteAudioProcessor::getName() const { return JucePlugin_Name; }
bool PapaloteAudioProcessor::acceptsMidi()  const { return false; }
bool PapaloteAudioProcessor::producesMidi() const { return false; }
bool PapaloteAudioProcessor::isMidiEffect() const { return false; }
double PapaloteAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int  PapaloteAudioProcessor::getNumPrograms() { return 1; }
int  PapaloteAudioProcessor::getCurrentProgram() { return 0; }
void PapaloteAudioProcessor::setCurrentProgram(int) {}
const juce::String PapaloteAudioProcessor::getProgramName(int) { return {}; }
void PapaloteAudioProcessor::changeProgramName(int, const juce::String&) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PapaloteAudioProcessor();
}
