#pragma once

#include "CameraComponent.h"

namespace Engine
{
class InputManager;

class FreeCameraControllerComponent final
{
public:
    void update(TransformComponent& transform, const CameraComponent& camera,
        const InputManager& input, float delta_time) const;

private:
    float m_move_speed{8.0f};
    float m_look_speed{0.0035f};
};
}
