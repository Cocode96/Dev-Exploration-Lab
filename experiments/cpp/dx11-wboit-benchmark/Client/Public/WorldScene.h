#pragma once

#include "Scene.h"

#include <memory>

namespace Client
{
class SkyBox;
class Terrain;

class WorldScene : public Engine::IScene
{
public:
    WorldScene();
    ~WorldScene() override;

    Engine::SceneRenderView render_view() const final;

protected:
    bool initialize_world();
    virtual std::span<const Engine::EffectInstance> effect_instances() const = 0;

private:
    std::unique_ptr<SkyBox> m_sky_box;
    std::unique_ptr<Terrain> m_terrain;
};
}
