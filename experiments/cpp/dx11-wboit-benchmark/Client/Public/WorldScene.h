#pragma once

#include "Scene.h"

#include <memory>

namespace Client
{
using namespace std;

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
    virtual span<const Engine::EffectInstance> effect_instances() const = 0;

private:
    unique_ptr<SkyBox> m_sky_box;
    unique_ptr<Terrain> m_terrain;
};
}
