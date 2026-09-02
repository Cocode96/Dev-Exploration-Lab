#pragma once

#include "GameObject.h"
#include "SkyComponent.h"

namespace Client
{
class SkyBox final : public Engine::GameObject
{
public:
    SkyBox();
    const Engine::SkyRenderData& render_data() const noexcept { return m_sky.render_data(); }

private:
    Engine::SkyComponent m_sky;
};
}
