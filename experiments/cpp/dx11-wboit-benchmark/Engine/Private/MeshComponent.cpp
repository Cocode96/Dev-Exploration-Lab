#include "MeshComponent.h"

#include <cmath>

namespace
{
float terrain_height(float x, float z)
{
    return 0.18f * std::sin(x * 0.22f) * std::cos(z * 0.19f);
}
}

namespace Engine
{
void MeshComponent::build_procedural_terrain(float half_extent, std::uint32_t subdivisions)
{
    const std::uint32_t row_size = subdivisions + 1;
    m_vertices.clear();
    m_indices.clear();
    m_vertices.reserve(static_cast<std::size_t>(row_size) * row_size);
    m_indices.reserve(static_cast<std::size_t>(subdivisions) * subdivisions * 6);

    for (std::uint32_t z_index = 0; z_index <= subdivisions; ++z_index)
    {
        for (std::uint32_t x_index = 0; x_index <= subdivisions; ++x_index)
        {
            const float x = -half_extent + 2.0f * half_extent * x_index / subdivisions;
            const float z = -half_extent + 2.0f * half_extent * z_index / subdivisions;
            const float height = terrain_height(x, z);
            const float left = terrain_height(x - 0.1f, z);
            const float right = terrain_height(x + 0.1f, z);
            const float back = terrain_height(x, z - 0.1f);
            const float front = terrain_height(x, z + 0.1f);
            DirectX::XMFLOAT3 normal{};
            DirectX::XMStoreFloat3(&normal, DirectX::XMVector3Normalize(
                DirectX::XMVectorSet(left - right, 0.2f, back - front, 0.0f)));
            const bool alternate = ((x_index / 4) + (z_index / 4)) % 2 == 0;
            const DirectX::XMFLOAT4 color = alternate
                ? DirectX::XMFLOAT4{0.19f, 0.36f, 0.18f, 1.0f}
                : DirectX::XMFLOAT4{0.14f, 0.29f, 0.15f, 1.0f};
            m_vertices.push_back({{x, height, z}, normal, color});
        }
    }

    for (std::uint32_t z = 0; z < subdivisions; ++z)
    {
        for (std::uint32_t x = 0; x < subdivisions; ++x)
        {
            const std::uint32_t top_left = z * row_size + x;
            const std::uint32_t top_right = top_left + 1;
            const std::uint32_t bottom_left = top_left + row_size;
            const std::uint32_t bottom_right = bottom_left + 1;
            m_indices.insert(m_indices.end(),
                {top_left, bottom_left, top_right, top_right, bottom_left, bottom_right});
        }
    }
}
}
