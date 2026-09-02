#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace Engine
{
enum class TransparencyMode
{
    UnsortedAlpha,
    ZSortedAlpha,
    Wboit
};

enum class SceneType
{
    Validation,
    EffectStress
};

struct EffectInstance
{
    DirectX::XMFLOAT4 position_and_billboard{};
    DirectX::XMFLOAT2 size{};
    DirectX::XMFLOAT4 color{};
    DirectX::XMFLOAT2 rotation{};
};

struct MeshVertex
{
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 normal{};
    DirectX::XMFLOAT4 color{};
};

struct MeshRenderView
{
    std::span<const MeshVertex> vertices;
    std::span<const std::uint32_t> indices;
    DirectX::XMFLOAT4X4 world{};
};

struct SkyRenderData
{
    DirectX::XMFLOAT4 zenith_color{0.05f, 0.16f, 0.38f, 1.0f};
    DirectX::XMFLOAT4 horizon_color{0.48f, 0.72f, 0.92f, 1.0f};
};

struct SceneRenderView
{
    SkyRenderData sky;
    MeshRenderView terrain;
    std::span<const EffectInstance> effects;
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
    std::uint32_t total_draw_calls{};
    std::uint32_t transparency_draw_calls{};
};

inline std::wstring_view to_string(TransparencyMode mode)
{
    switch (mode)
    {
    case TransparencyMode::UnsortedAlpha: return L"Unsorted Alpha";
    case TransparencyMode::ZSortedAlpha: return L"Object-center Z Sort";
    case TransparencyMode::Wboit: return L"Weighted Blended OIT";
    }
    return L"Unknown";
}
}

static_assert(sizeof(Engine::EffectInstance) == 48);
