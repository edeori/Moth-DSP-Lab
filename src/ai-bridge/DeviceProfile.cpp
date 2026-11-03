#include "DeviceProfile.h"
#include <set>
namespace lab
{
bool genericDevice(const juce::var &s)
{
    return s["deviceSetup"]["type"].toString().isNotEmpty();
}
juce::var panelRecognitionSchema()
{
    return juce::JSON::parse(
        R"({"type":"object","additionalProperties":false,"required":["summary","controlsText","uncertainties"],"properties":{"summary":{"type":"string"},"controlsText":{"type":"string"},"uncertainties":{"type":"string"}}})");
}
juce::var parseControlInventory(const juce::String &text)
{
    juce::Array<juce::var> controls;
    std::set<juce::String> names;
    for (auto line : juce::StringArray::fromLines(text))
    {
        line = line.trim();
        if (line.isEmpty())
            continue;
        auto name = line.upToFirstOccurrenceOf("|", false, false).trim();
        require(name.isNotEmpty() && name.length() <= 100 && names.insert(name.toLowerCase()).second,
                "Each control needs a unique label (channel prefix allowed), at most 100 characters");
        require(line.length() <= 500, "Control description too long");
        auto c = object();
        set(c, "name", name);
        set(c, "description", line);
        controls.add(c);
    }
    require(!controls.isEmpty() && controls.size() <= 64, "Enter 1–64 controls, one per line");
    return juce::var(controls);
}
juce::File importPanelPhoto(const juce::File &project, const juce::File &source)
{
    require(source.existsAsFile() && source.getSize() <= 20000000,
            "Photo must be a PNG/JPEG file under 20 MB");
    auto image = juce::ImageFileFormat::loadFrom(source);
    require(image.isValid(), "Cannot decode photo; use JPEG or PNG");
    // A normalized owned copy removes EXIF/location and bounds upload/memory size.
    if (image.getWidth() > 2400 || image.getHeight() > 2400)
    {
        double scale = 2400.0 / juce::jmax(image.getWidth(), image.getHeight());
        image = image.rescaled(juce::jmax(1, int(image.getWidth() * scale)),
                               juce::jmax(1, int(image.getHeight() * scale)));
    }
    auto dir = project.getChildFile("assets/panels");
    require(dir.createDirectory().wasOk(), "Cannot create panel photo directory");
    auto dest = dir.getChildFile(juce::Uuid().toString() + ".png");
    auto output = dest.createOutputStream();
    require(output && juce::PNGImageFormat().writeImageToStream(image, *output), "Cannot save photo");
    output.reset();
    require(dest.getSize() <= 20000000, "Normalized photo too large");
    return dest;
}
juce::var devicePlannerContext(const juce::var &original, const juce::var &device)
{
    require(juce::StringArray{"pedal", "head", "outboard"}.contains(device["type"].toString()),
            "Select pedal, head or outboard");
    auto c = original.clone();
    auto controls = parseControlInventory(device["controlsText"].toString());
    require(bool(device["controlsConfirmed"]), "Confirm the visible control inventory first");
    require(bool(device["routeConfirmed"]) && device["routeNotes"].toString().trim().length() >= 10,
            "Confirm the device return route and level/load compatibility first");
    set(c, "deviceSetup", device.clone());
    set(c, "confirmedControls", controls);
    set(c, "capabilities",
        "Generic hardware characterization, NO DarkBass or other plugin. Only guarded_noise_2.6s_-36dBFS. "
        "Use mode=2 for every step (generic acquisition, not a channel or distortion setting), "
        "parameters=[]; describe actual channels/switches in settings. "
        "A fresh low-gain baseline capture through the same physical chain is the prerequisite; it is NOT "
        "pedal bypass and NOT absolute voltage calibration. "
        "The first baseline must pass clipping/noise/correlation/repeat-drift checks. Never infer volts, "
        "impedance, load rating, switch semantics or hidden controls from an image. "
        "For an amp head keep a compatible rated load connected to its speaker output; only the loadbox's "
        "documented line/DI output may reach the interface, never speaker level. "
        "Do not change interface/reamp/loadbox settings between captures. Test one confirmed control at a "
        "time and restore reference settings; "
        "do not change power/standby/impedance/output routing in active steps. Unknown switch meanings must "
        "be deferred. All settings need human confirmation. "
        "Use concise, non-repeating Hungarian operator text. Settings: physical states only; reason: one "
        "goal; expected: one observable outcome. "
        "State low-level noise limitations; cannot establish full high-level nonlinear behavior, THD or "
        "absolute transfer. Hungarian instructions.");
    set(c, "coverageRequirements",
        "Include each confirmed control exactly once in coverage. Planned means varied states and actual "
        "step IDs; otherwise deferred with reason. "
        "Return calibration as Hungarian text: exact baseline knob/channel settings, return route checks, "
        "manual load/level compatibility checks, "
        "then run the app's low-level reference check. Never claim a photo has calibrated hardware. Unknown "
        "ratings must be checked in the equipment manual by the operator. "
        "Use only the confirmed inventory and route, not previous pedal defaults.");
    return c;
}
void validateDevicePlan(const juce::var &p, const juce::var &device)
{
    auto controls = parseControlInventory(device["controlsText"].toString());
    require(p["calibration"].toString().trim().length() >= 20 && p["calibration"].toString().length() <= 8000,
            "AI: missing baseline/calibration procedure");
    auto coverage = p["coverage"];
    require(coverage.isArray() && coverage.size() == controls.size(),
            "AI: incomplete device control coverage");
    for (const auto &control : *controls.getArray())
    {
        int n = 0;
        for (const auto &item : *coverage.getArray())
            if (item["control"] == control["name"])
                ++n;
        require(n == 1, "AI: missing or duplicate confirmed control");
    }
    for (const auto &s : *p["steps"].getArray())
        require(int(s["mode"]) == 2 && s["parameters"].size() == 0,
                "AI: generic hardware cannot set pedal plugin parameters");
}
} // namespace lab
