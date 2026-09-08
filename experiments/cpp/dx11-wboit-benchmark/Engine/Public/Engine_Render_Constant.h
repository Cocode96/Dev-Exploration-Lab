#pragma once

#include "Engine_Render_Struct.h"
#include <d3d11.h>
#include <array>

namespace Engine
{
using namespace std;

inline constexpr array device_feature_levels{D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

inline constexpr array<EffectVertex, 6> effect_quad_vertices{
    EffectVertex{{-1.0f, -1.0f}, {0.0f, 1.0f}}, EffectVertex{{-1.0f, 1.0f}, {0.0f, 0.0f}},
    EffectVertex{{1.0f, 1.0f}, {1.0f, 0.0f}}, EffectVertex{{-1.0f, -1.0f}, {0.0f, 1.0f}},
    EffectVertex{{1.0f, 1.0f}, {1.0f, 0.0f}}, EffectVertex{{1.0f, -1.0f}, {1.0f, 1.0f}}
};

inline constexpr array<UINT, 2> effect_buffer_offsets{0, 0};

inline constexpr float oit_clear_color[4]{};

inline constexpr float scene_clear_color[]{0.02f, 0.05f, 0.12f, 1.0f};

inline constexpr float ui_clear_color[]{0.035f, 0.045f, 0.06f, 1.0f};
}
