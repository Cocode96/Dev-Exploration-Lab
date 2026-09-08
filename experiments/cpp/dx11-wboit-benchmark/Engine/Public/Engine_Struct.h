#pragma once

#include "Engine_Enum.h"
#include <DirectXMath.h>
#include <cstdint>
#include <span>

namespace Engine
{
using namespace std;
using namespace DirectX;

struct EffectInstance
{
    XMFLOAT4 position_and_billboard{};
    XMFLOAT2 size{};
    XMFLOAT4 color{};
    XMFLOAT2 rotation{};
};

struct MeshVertex
{
    XMFLOAT3 position{};
    XMFLOAT3 normal{};
    XMFLOAT4 color{};
};

struct MeshRenderView
{
    span<const MeshVertex> vertices;
    span<const uint32_t> indices;
    XMFLOAT4X4 world{};
};

struct SkyRenderData
{
    XMFLOAT4 zenith_color{0.05f, 0.16f, 0.38f, 1.0f};
    XMFLOAT4 horizon_color{0.48f, 0.72f, 0.92f, 1.0f};
};

struct SceneRenderView
{
    SkyRenderData sky;
    MeshRenderView terrain;
    span<const EffectInstance> effects;
};

struct RenderSettings
{
    TransparencyMode transparency_mode{TransparencyMode::UnsortedAlpha};
    bool reverse_submission_order{false};
};

struct FrameMetrics
{
    double cpu_submit_ms{};
    double cpu_sort_ms{};
    double gpu_total_ms{};
    double gpu_transparency_ms{};
    double gpu_resolve_ms{};
    uint32_t total_draw_calls{};
    uint32_t transparency_draw_calls{};
};

static_assert(sizeof(EffectInstance) == 48);
}
