#pragma once
#include <DirectXMath.h>
namespace Lab
{
using namespace DirectX;
enum class CameraMode
{
    Fixed,
    Orbit,
    Path
};
enum class TestMode
{
    CurrentScene,
    EffectSweep,
    PoolChurn,
    DllSwitch
};
struct SceneVertex
{
    XMFLOAT4 position;
    XMFLOAT4 color;
};
struct SceneFrame
{
    const SceneVertex* vertices{};
    unsigned count{}, opaque_count{};
};
struct BenchmarkOptions
{
    TestMode test{TestMode::DllSwitch};
    CameraMode camera{CameraMode::Orbit};
    int instances{256}, capacity{4096}, warmup{2}, frames{12}, repeats{1}, seed{2026};
    float speed{1};
};
struct BenchmarkRow
{
    int layout{}, backend{}, count{}, repeat{};
    unsigned samples{}, active{}, failures{};
    double cpu_mean{}, cpu_p95{}, cpu_max{}, gpu_mean{}, switch_mean{};
    double idle_mean{}, release_mean{}, module_mean{}, create_mean{}, first_frame_mean{};
};
struct SwitchMetrics
{
    double idle{}, release{}, module{}, create{}, first_frame{};
};
struct LabControls
{
    bool korean{true}, start_requested{}, cancel_requested{}, running{}, result_ready{};
    bool preconditioning{};
    bool import_requested{}, reset_requested{};
    BenchmarkOptions options{};
    SceneFrame scene{};
    unsigned active{}, failures{}, phase{}, phase_count{};
    float simulation_time{};
    double gpu_ms{}, scene_cpu_ms{};
    SwitchMetrics last_switch{};
    char preset_path[1024]{"Presets/Lab.json"};
    char status[1024]{};
    BenchmarkRow results[128]{};
    unsigned result_count{};
};
} // namespace Lab
