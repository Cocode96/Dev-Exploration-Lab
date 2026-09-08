#pragma once

#include "Scene.h"

#include <array>
#include <memory>

namespace Engine
{
using namespace std;

class ApplicationContext;

class SceneManager final
{
public:
    bool register_scene(SceneType type, unique_ptr<IScene> scene, ApplicationContext& context);
    bool change_scene(SceneType type);

    IScene& active_scene();
    const IScene& active_scene() const;
    SceneType active_scene_type() const noexcept { return m_active_type; }

private:
    static constexpr size_t to_index(SceneType type)
    {
        return static_cast<size_t>(type);
    }

    array<unique_ptr<IScene>, 2> m_scenes;
    SceneType m_active_type{SceneType::Validation};
};
}
