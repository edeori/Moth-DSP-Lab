#include <cstdlib>
#include <juce_audio_processors/juce_audio_processors.h>
// Test-only known transfer: gain plus an exactly reported integer delay.
class Fixture final : public juce::AudioProcessor
{
  public:
    Fixture()
        : AudioProcessor(BusesProperties()
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        addParameter(gain =
                         new juce::AudioParameterFloat(juce::ParameterID{"gain", 1}, "Gain", 0.f, 2.f, 1.f));
        addParameter(delay = new juce::AudioParameterInt(juce::ParameterID{"delay", 1}, "Delay", 0, 2048, 0));
        addParameter(terminateOnRender = new juce::AudioParameterBool(juce::ParameterID{"terminate", 1},
                                                                      "Terminate worker", false));
    }
    const juce::String getName() const override
    {
        return "Moth Lab Fixture";
    }
    void prepareToPlay(double, int) override
    {
        latency = delay->get();
        setLatencySamples(latency);
        history.setSize(2, latency + 1);
        reset();
    }
    void releaseResources() override {}
    void reset() override
    {
        history.clear();
        position = 0;
    }
    bool isBusesLayoutSupported(const BusesLayout &l) const override
    {
        return l.getMainInputChannelSet() == l.getMainOutputChannelSet() &&
               (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() ||
                l.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
    }
    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &) override
    {
        if (terminateOnRender->get())
            std::_Exit(99); // Simulate abrupt third-party worker termination.
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            for (int c = 0; c < buffer.getNumChannels(); ++c)
            {
                history.setSample(c, position, buffer.getSample(c, i) * gain->get());
                buffer.setSample(c, i, history.getSample(c, (position + 1) % (latency + 1)));
            }
            position = (position + 1) % (latency + 1);
        }
    }
    bool hasEditor() const override
    {
        return false;
    }
    juce::AudioProcessorEditor *createEditor() override
    {
        return nullptr;
    }
    double getTailLengthSeconds() const override
    {
        return 0;
    }
    bool acceptsMidi() const override
    {
        return false;
    }
    bool producesMidi() const override
    {
        return false;
    }
    int getNumPrograms() override
    {
        return 1;
    }
    int getCurrentProgram() override
    {
        return 0;
    }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override
    {
        return "Default";
    }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &dest) override
    {
        auto *o = new juce::DynamicObject;
        juce::var v(o);
        o->setProperty("gain", gain->get());
        o->setProperty("delay", delay->get());
        o->setProperty("terminateOnRender", terminateOnRender->get());
        auto json = juce::JSON::toString(v);
        dest.replaceAll(json.toRawUTF8(), json.getNumBytesAsUTF8());
    }
    void setStateInformation(const void *data, int size) override
    {
        auto v = juce::JSON::parse(juce::String::fromUTF8(static_cast<const char *>(data), size));
        if (v.isObject())
        {
            *terminateOnRender = static_cast<bool>(v["terminateOnRender"]);
            *gain = static_cast<float>(v["gain"]);
            *delay = static_cast<int>(v["delay"]);
        }
    }

  private:
    juce::AudioParameterFloat *gain = nullptr;
    juce::AudioParameterInt *delay = nullptr;
    juce::AudioParameterBool *terminateOnRender = nullptr;
    juce::AudioBuffer<float> history;
    int latency = 0, position = 0;
};
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new Fixture;
}
