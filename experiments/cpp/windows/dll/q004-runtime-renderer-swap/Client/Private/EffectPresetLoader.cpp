#include "EffectPresetLoader.h"
#include "Effect_Function.h"
#include <json.hpp>
#include <fstream>
#include <cmath>
#include <stdexcept>
namespace Lab
{
namespace
{
using nlohmann::json;
json read(const filesystem::path& path)
{
    if (filesystem::file_size(path) > 4 * 1024 * 1024)
        throw runtime_error("JSON exceeds 4 MB");
    ifstream file(path);
    return json::parse(file);
}
float number(const json& j, const char* key, float fallback)
{
    const float value = j.value(key, fallback);
    if (!isfinite(value) || abs(value) > 100000)
        throw runtime_error(string("Invalid value: ") + key);
    return value;
}
template <class T> void array_value(const json& j, const char* key, T& out)
{
    if (!j.contains(key))
        return;
    const auto& value = j.at(key);
    constexpr size_t count = sizeof(T) / sizeof(float);
    if (!value.is_array() || value.size() != count)
        throw runtime_error(string("Invalid array: ") + key);
    float data[count]{};
    for (size_t i = 0; i < count; ++i)
    {
        data[i] = value[i].get<float>();
        if (!isfinite(data[i]) || abs(data[i]) > 100000)
            throw runtime_error("Non-finite vector");
    }
    memcpy(&out, data, sizeof(out));
}
void parse(const json& root, float delay, vector<EffectPreset>& output)
{
    // Mabinogi Point/Quad JSON의 이동 필드를 CPU 풀 입자로 변환한다.
    if (!root.contains("PointDesc") && !root.contains("QuadDesc"))
        throw runtime_error(
            "Only PointDesc / QuadDesc presets are supported; Mesh/Trail require game resources");
    bool quad = root.contains("QuadDesc");
    const auto& d = root.at(quad ? "QuadDesc" : "PointDesc");
    EffectPreset p;
    p.name = root.value("GameObjectName", string("Imported"));
    p.delay = delay;
    p.count = root.value("InstanceCount", 32u);
    if (p.count < 1 || p.count > 16384)
        throw runtime_error("InstanceCount must be 1..16384");
    p.life_min = number(d, "LifetimeMin", 1);
    p.life_max = number(d, "LifetimeMax", 2);
    if (p.life_min <= 0 || p.life_max <= 0)
        throw runtime_error("Lifetime must be positive");
    p.speed_min = number(d, "SpeedMin", 1);
    p.speed_max = number(d, "SpeedMax", 1);
    array_value(d, "OriginMin", p.origin_min);
    array_value(d, "OriginMax", p.origin_max);
    array_value(d, "VelocityMin", p.velocity_min);
    array_value(d, "VelocityMax", p.velocity_max);
    array_value(d, quad ? "AccelMin" : "AccelerationMin", p.acceleration_min);
    array_value(d, quad ? "AccelMax" : "AccelerationMax", p.acceleration_max);
    array_value(d, quad ? "ColorMin" : "StartColor", p.start_color);
    array_value(d, quad ? "ColorMax" : "EndColor", p.end_color);
    array_value(d, quad ? "SizeMin" : "StartSize", p.start_size);
    array_value(d, quad ? "SizeMax" : "EndSize", p.end_size);
    output.push_back(p);
}
} // namespace
bool EffectPresetLoader::load(const filesystem::path& path, vector<EffectPreset>& output, string& message)
{
    try
    {
        vector<EffectPreset> candidate;
        const auto root = read(path);
        if (root.contains("SubEffects"))
        {
            const auto& children = root.at("SubEffects");
            if (!children.is_array() || children.empty() || children.size() > 256)
                throw runtime_error("SubEffects must contain 1..256 entries");
            for (const auto& child : children)
            {
                const string type = child.at("Type").get<string>();
                if (type != "Point" && type != "Quad")
                    throw runtime_error("Unsupported SubEffect Type: " + type);
                const auto relative = path_from_utf8(child.at("FileName").get<string>());
                if (relative.is_absolute() || relative.has_parent_path())
                    throw runtime_error("SubEffect file must be in the preset folder");
                const float delay = number(child, "Delay", 0);
                if (delay < 0)
                    throw runtime_error("Delay must not be negative");
                parse(read(path.parent_path() / relative), delay, candidate);
            }
        }
        else
            parse(root, 0, candidate);
        output = move(candidate);
        message =
            "Loaded Point/Quad motion. Game textures, sprites, shader passes and bloom are not imported.";
        return true;
    }
    catch (const exception& e)
    {
        message = e.what();
        return false;
    }
}
} // namespace Lab
