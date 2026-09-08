#pragma once

#include <DirectXMath.h>
#include <array>

namespace Client
{
using namespace std;
using namespace DirectX;

inline constexpr array<XMFLOAT3, 8> effect_palette{
    XMFLOAT3{0.95f, 0.18f, 0.23f}, XMFLOAT3{0.12f, 0.65f, 1.00f},
    XMFLOAT3{0.95f, 0.75f, 0.10f}, XMFLOAT3{0.55f, 0.20f, 0.95f},
    XMFLOAT3{0.12f, 0.90f, 0.55f}, XMFLOAT3{1.00f, 0.35f, 0.70f},
    XMFLOAT3{0.95f, 0.50f, 0.12f}, XMFLOAT3{0.35f, 0.90f, 0.95f}
};
}
