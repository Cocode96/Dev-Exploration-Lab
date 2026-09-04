#pragma once

#include "GameObject.h"
#include "MeshComponent.h"
#include "TransformComponent.h"

namespace Client
{
class Terrain final : public Engine::GameObject
{
public:
    Terrain();
    Engine::MeshRenderView render_view() const noexcept;

private:
    Engine::TransformComponent m_transform;
    Engine::MeshComponent m_mesh;
};
}
