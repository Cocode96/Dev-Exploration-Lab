#include "WorldScene.h"

#include "SkyBox.h"
#include "Terrain.h"

namespace Client
{
using namespace std;

WorldScene::WorldScene() = default;
WorldScene::~WorldScene() = default;

bool WorldScene::initialize_world()
{
    m_sky_box = make_unique<SkyBox>();
    m_terrain = make_unique<Terrain>();
    return true;
}

Engine::SceneRenderView WorldScene::render_view() const
{
    return {m_sky_box->render_data(), m_terrain->render_view(), effect_instances()};
}
}
