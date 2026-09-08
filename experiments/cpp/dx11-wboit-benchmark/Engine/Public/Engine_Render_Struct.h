#pragma once

#include <DirectXMath.h>

namespace Engine
{
using namespace DirectX;

struct EffectVertex
{
    XMFLOAT2 position;
    XMFLOAT2 uv;
};

struct alignas(16) FrameConstants
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 view_projection;
    XMFLOAT4 camera_position;
    XMFLOAT4 camera_right;
    XMFLOAT4 camera_up;
    XMFLOAT4 camera_forward;
    XMFLOAT4 sky_zenith;
    XMFLOAT4 sky_horizon;
    XMFLOAT2 resolution;
    int alpha_mode;
    int depth_mode;
    float p_alpha;
    float k_alpha;
    float k_depth;
    float padding;
};

static_assert(sizeof(FrameConstants) % 16 == 0);
}
