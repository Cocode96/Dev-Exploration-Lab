#pragma once

#include "Engine_Struct.h"

#include <vector>

namespace Engine
{
using namespace std;

class MeshComponent final
{
public:
    void build_procedural_terrain(float half_extent, uint32_t subdivisions);

    span<const MeshVertex> vertices() const noexcept { return m_vertices; }
    span<const uint32_t> indices() const noexcept { return m_indices; }

private:
    vector<MeshVertex> m_vertices;
    vector<uint32_t> m_indices;
};
}
