#include "MeshComponent.h"
#include "Engine_Mesh_Function.h"

#include <cmath>

namespace Engine
{
using namespace std;
using namespace DirectX;

void MeshComponent::build_procedural_terrain(float half_extent, uint32_t subdivisions)
{
    const uint32_t row_size = subdivisions + 1;
    m_vertices.clear();
    m_indices.clear();
    m_vertices.reserve(static_cast<size_t>(row_size) * row_size);
    m_indices.reserve(static_cast<size_t>(subdivisions) * subdivisions * 6);

    for (uint32_t z_index = 0; z_index <= subdivisions; ++z_index)
    {
        for (uint32_t x_index = 0; x_index <= subdivisions; ++x_index)
        {
            const float x = -half_extent + 2.0f * half_extent * x_index / subdivisions;
            const float z = -half_extent + 2.0f * half_extent * z_index / subdivisions;
            const float height = terrain_height(x, z);
            const float left = terrain_height(x - 0.1f, z);
            const float right = terrain_height(x + 0.1f, z);
            const float back = terrain_height(x, z - 0.1f);
            const float front = terrain_height(x, z + 0.1f);
            XMFLOAT3 normal{};
            XMStoreFloat3(&normal, XMVector3Normalize(
                XMVectorSet(left - right, 0.2f, back - front, 0.0f)));
            const bool alternate = ((x_index / 4) + (z_index / 4)) % 2 == 0;
            const XMFLOAT4 color = alternate
                ? XMFLOAT4{0.19f, 0.36f, 0.18f, 1.0f}
                : XMFLOAT4{0.14f, 0.29f, 0.15f, 1.0f};
            m_vertices.push_back({{x, height, z}, normal, color});
        }
    }

    for (uint32_t z = 0; z < subdivisions; ++z)
    {
        for (uint32_t x = 0; x < subdivisions; ++x)
        {
            const uint32_t top_left = z * row_size + x;
            const uint32_t top_right = top_left + 1;
            const uint32_t bottom_left = top_left + row_size;
            const uint32_t bottom_right = bottom_left + 1;
            m_indices.insert(m_indices.end(),
                {top_left, bottom_left, top_right, top_right, bottom_left, bottom_right});
        }
    }
}
}
