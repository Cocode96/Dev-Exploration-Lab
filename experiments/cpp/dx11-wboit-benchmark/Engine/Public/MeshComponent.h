#pragma once

#include "EngineTypes.h"

#include <vector>

namespace Engine
{
class MeshComponent final
{
public:
    void build_procedural_terrain(float half_extent, std::uint32_t subdivisions);

    std::span<const MeshVertex> vertices() const noexcept { return m_vertices; }
    std::span<const std::uint32_t> indices() const noexcept { return m_indices; }

private:
    std::vector<MeshVertex> m_vertices;
    std::vector<std::uint32_t> m_indices;
};
}
