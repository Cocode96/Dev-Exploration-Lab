#include "SceneManager.h"

#include "ApplicationContext.h"

#include <stdexcept>

namespace Engine
{
using namespace std;

bool SceneManager::register_scene(SceneType type, unique_ptr<IScene> scene,
    ApplicationContext& context)
{
    if (!scene || !scene->initialize(context))
        return false;
    m_scenes[to_index(type)] = move(scene);
    return true;
}

bool SceneManager::change_scene(SceneType type)
{
    if (!m_scenes[to_index(type)])
        return false;
    m_active_type = type;
    return true;
}

IScene& SceneManager::active_scene()
{
    auto& scene = m_scenes[to_index(m_active_type)];
    if (!scene)
        throw runtime_error("Active scene has not been registered.");
    return *scene;
}

const IScene& SceneManager::active_scene() const
{
    const auto& scene = m_scenes[to_index(m_active_type)];
    if (!scene)
        throw runtime_error("Active scene has not been registered.");
    return *scene;
}
}
