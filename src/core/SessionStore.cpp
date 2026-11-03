#include "SessionStore.h"
#include <cmath>
#include <ctime>
#include <limits>
namespace lab
{
juce::String utcNow()
{
    auto now = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char text[32]{};
    std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return text;
}
void RenderSettings::validate() const
{
    require(std::isfinite(sampleRate) && sampleRate >= 8000 && sampleRate <= 192000,
            juce::String::fromUTF8("Sample rate must be 8000–192000 Hz"));
    require(blockSize >= 16 && blockSize <= 8192, juce::String::fromUTF8("Block size must be 16–8192"));
    require(channels == 1 || channels == 2, "Only mono/stereo is supported");
    require(std::isfinite(tailSeconds) && tailSeconds >= 0 && tailSeconds <= 30,
            juce::String::fromUTF8("Tail must be 0–30 seconds"));
}
juce::var RenderSettings::toJson() const
{
    auto v = object();
    set(v, "sampleRate", sampleRate);
    set(v, "blockSize", blockSize);
    set(v, "channels", channels);
    set(v, "tailSeconds", tailSeconds);
    return v;
}
RenderSettings RenderSettings::fromJson(const juce::var &v)
{
    RenderSettings s;
    s.sampleRate = v["sampleRate"];
    s.blockSize = v["blockSize"];
    s.channels = v["channels"];
    s.tailSeconds = v["tailSeconds"];
    s.validate();
    return s;
}
juce::var TargetSpec::toJson() const
{
    auto v = object();
    set(v, "plugin", plugin.getFullPathName());
    set(v, "descriptionXml", descriptionXml);
    set(v, "state", encodeState(state));
    return v;
}
TargetSpec TargetSpec::fromJson(const juce::var &v)
{
    TargetSpec t;
    t.plugin = juce::File(v["plugin"].toString());
    t.descriptionXml = v["descriptionXml"].toString();
    require(decodeState(t.state, v["state"].toString()), "Invalid plugin state encoding");
    return t;
}
AudioData readWav(const juce::File &file)
{
    juce::WavAudioFormat format;
    auto stream = file.createInputStream();
    require(stream != nullptr, "Cannot open WAV: " + file.getFullPathName());
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(stream.release(), true));
    require(reader != nullptr, "Invalid WAV file");
    require(reader->numChannels >= 1 && reader->numChannels <= 2, "WAV must be mono/stereo");
    require(reader->lengthInSamples > 0 && reader->lengthInSamples <= reader->sampleRate * 600 &&
                reader->lengthInSamples < std::numeric_limits<int>::max(),
            "WAV must be nonempty and at most 10 minutes at 192 kHz");
    AudioData data;
    data.sampleRate = reader->sampleRate;
    data.samples.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    require(reader->read(&data.samples, 0, data.samples.getNumSamples(), 0, true, true), "WAV read failed");
    for (int c = 0; c < data.samples.getNumChannels(); ++c)
        for (int i = 0; i < data.samples.getNumSamples(); ++i)
            require(std::isfinite(data.samples.getSample(c, i)), "WAV contains NaN/Infinity");
    return data;
}
void writeWav(const juce::File &file, const AudioData &data)
{
    require(file.getParentDirectory().createDirectory().wasOk(), "Cannot create output folder");
    auto stream = file.createOutputStream();
    require(stream != nullptr, "Cannot create WAV");
    stream->setPosition(0);
    stream->truncate();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> output = std::move(stream);
    auto writer = format.createWriterFor(
        output, juce::AudioFormatWriterOptions{}
                    .withSampleRate(data.sampleRate)
                    .withNumChannels(data.samples.getNumChannels())
                    .withBitsPerSample(32)
                    .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    require(writer != nullptr, "Cannot create WAV writer");
    require(writer->writeFromAudioSampleBuffer(data.samples, 0, data.samples.getNumSamples()),
            "WAV write failed");
}
juce::var readJson(const juce::File &file)
{
    require(file.existsAsFile(), "Missing JSON: " + file.getFullPathName());
    juce::var v;
    auto result = juce::JSON::parse(file.loadFileAsString(), v);
    require(result.wasOk() && v.isObject(), "Invalid JSON: " + file.getFullPathName());
    return v;
}
void writeJson(const juce::File &file, const juce::var &v)
{
    require(file.replaceWithText(juce::JSON::toString(v)), "Cannot write JSON: " + file.getFullPathName());
}
SessionStore::SessionStore(const juce::File &f) : root(f)
{
    require(root.createDirectory().wasOk(), "Cannot create project directory");
    auto project = root.getChildFile("project.json");
    if (!project.existsAsFile())
    {
        auto v = object();
        set(v, "schemaVersion", 1);
        set(v, "name", root.getFileName());
        writeJson(project, v);
    }
}
juce::String SessionStore::runPath(const juce::var &workflow, const juce::String &id)
{
    auto saved = workflow["storageRun"].toString();
    if (saved.isNotEmpty())
    {
        require(saved.startsWith("measurements/") && !saved.contains("..") && !saved.contains("\\"),
                "Invalid measurement storage path");
        return saved;
    }
    auto scenario = workflow["scenario"];
    auto title = scenario["title"].toString();
    if (title.isEmpty())
        title = "Manual measurements";
    auto profileId =
        scenario.isObject()
            ? juce::SHA256(juce::JSON::toString(scenario).toUTF8()).toHexString().substring(0, 10)
            : id;
    auto name = workflow["name"].toString();
    if (name.isEmpty())
        name = "Measurement run";
    return "measurements/" + juce::File::createLegalFileName(title).substring(0, 80) + "--" + profileId +
           "/" + juce::File::createLegalFileName(name).substring(0, 80) + "--" + id;
}
juce::File SessionStore::runFolder(const juce::File &project, const juce::String &workflowId)
{
    require(workflowId.isNotEmpty() &&
                workflowId.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"),
            "Invalid workflow identifier");
    auto workflow = readJson(project.getChildFile("workflows").getChildFile(workflowId + ".json"));
    return project.getChildFile(runPath(workflow, workflowId));
}
juce::Array<juce::File> SessionStore::sessions(const juce::File &project)
{
    auto folders = project.getChildFile("sessions").findChildFiles(juce::File::findDirectories, false);
    for (const auto &manifest :
         project.getChildFile("measurements").findChildFiles(juce::File::findFiles, true, "manifest.json"))
    {
        auto folder = manifest.getParentDirectory();
        // Export snapshots must never be mistaken for newly committed recordings.
        if (folder.getParentDirectory().getFileName() == "results")
            folders.add(folder);
    }
    return folders;
}
juce::File SessionStore::projectForSession(const juce::File &session)
{
    auto parent = session.getParentDirectory();
    while (parent != parent.getParentDirectory())
    {
        if (parent.getChildFile("project.json").existsAsFile())
            return parent;
        parent = parent.getParentDirectory();
    }
    // Compatibility with old standalone session fixtures/projects without a marker.
    return session.getParentDirectory().getParentDirectory();
}
juce::File SessionStore::beginSession(const juce::String &workflowId)
{
    auto base = workflowId.isEmpty() ? root.getChildFile("sessions")
                                     : runFolder(root, workflowId).getChildFile("results");
    auto f = base.getChildFile(".pending-" + juce::Uuid().toString());
    require(f.createDirectory().wasOk(), "Cannot create pending session");
    return f;
}
juce::File SessionStore::commit(const juce::File &pending)
{
    auto manifest = readJson(pending.getChildFile("manifest.json"));
    require(manifest["status"].toString() == "complete", "Session is not complete");
    auto dest = pending.getSiblingFile(utcNow().removeCharacters(":-") + "-" + juce::Uuid().toString());
    require(pending.moveFileTo(dest), "Cannot publish session");
    return dest;
}
juce::var SessionStore::load(const juce::File &folder)
{
    auto v = readJson(folder.getChildFile("manifest.json"));
    require(static_cast<int>(v["schemaVersion"]) == 1 && v["status"].toString() == "complete",
            "Unsupported/incomplete session");
    return v;
}
} // namespace lab
