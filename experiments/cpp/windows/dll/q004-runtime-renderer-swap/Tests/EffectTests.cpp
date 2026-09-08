#include "EffectManager.h"
#include "Effect_Function.h"
#include <fstream>
#include <iostream>
#include <cmath>
using namespace Lab;
void require(bool condition, const char* message)
{
    if (!condition)
        throw runtime_error(message);
}
int main(int argc, char** argv)
{
    try
    {
        mt19937 a(42), b(42);
        EffectPreset preset;
        auto p = EffectFactory::create(preset, 0, a), q = EffectFactory::create(preset, 0, b);
        require(p.position.x == q.position.x && p.velocity.z == q.velocity.z, "seed reproducibility");
        EffectPool pool;
        pool.reset(0);
        require(!pool.emit(preset, 0, a), "empty pool");
        pool.reset(1);
        require(pool.emit(preset, 0, a), "first spawn");
        require(!pool.emit(preset, 0, a), "capacity limit");
        require(pool.failures() == 1, "reject count");
        pool.update(10, {preset});
        require(pool.active_count() == 0, "expiry");
        require(pool.emit(preset, 0, a), "reuse");
        vector<EffectPreset> parsed;
        string message;
        require(EffectPresetLoader::load("Presets/Lab.json", parsed, message), "sample JSON");
        if (argc > 1)
            require(EffectPresetLoader::load(path_from_utf8(argv[1]), parsed, message), "Mabinogi JSON");
        const auto previous = parsed.size();
        require(!EffectPresetLoader::load("missing.json", parsed, message) && parsed.size() == previous,
                "transactional load");
        filesystem::create_directories("test-local");
        ofstream("test-local/invalid.json") << R"({"PointDesc":{"LifetimeMin":0}})";
        require(!EffectPresetLoader::load("test-local/invalid.json", parsed, message), "invalid lifetime");
        ofstream("test-local/group.json")
            << R"({"SubEffects":[{"Type":"Point","FileName":"../invalid.json"}]})";
        require(!EffectPresetLoader::load("test-local/group.json", parsed, message), "path boundary");
        ofstream("test-local/unsupported.json") << R"({"SubEffects":[{"Type":"Mesh","FileName":"a.json"}]})";
        require(!EffectPresetLoader::load("test-local/unsupported.json", parsed, message),
                "unsupported type");
        ofstream("test-local/point.json")
            << R"({"InstanceCount":4,"PointDesc":{"LifetimeMin":1,"LifetimeMax":2}})";
        ofstream("test-local/group.json")
            << R"({"SubEffects":[{"Type":"Point","FileName":"point.json","Delay":0.5}]})";
        require(EffectPresetLoader::load("test-local/group.json", parsed, message) && parsed[0].delay == 0.5f,
                "SubEffects delay");
        EffectManager manager;
        BenchmarkOptions options;
        manager.reset(options);
        manager.update(1.0f / 60, options);
        auto frame = manager.scene(CameraMode::Orbit, 1);
        require(frame.opaque_count > 252 && frame.count > frame.opaque_count, "humanoid and particles");
        for (unsigned i = 0; i < frame.count; ++i)
            require(isfinite(frame.vertices[i].position.w), "finite geometry");
        cout << "PASS: seed, empty/capped pool, rejection, expiry/reuse, JSON "
                "transaction/validation/SubEffects, humanoid geometry\n";
        return 0;
    }
    catch (const exception& e)
    {
        cerr << e.what() << '\n';
        return 1;
    }
}
