#include "Terrain.h"

namespace Client
{
using namespace DirectX;

Terrain::Terrain()
{
    m_mesh.build_procedural_terrain(30.0f, 64);
}

Engine::MeshRenderView Terrain::render_view() const noexcept
{
    Engine::MeshRenderView view{m_mesh.vertices(), m_mesh.indices()};
    XMStoreFloat4x4(&view.world, XMMatrixTranspose(m_transform.world_matrix()));
    return view;
}
}
