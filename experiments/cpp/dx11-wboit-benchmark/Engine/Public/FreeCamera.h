#pragma once

#include "CameraComponent.h"
#include "FreeCameraControllerComponent.h"
#include "GameObject.h"

namespace Engine
{
class InputManager;

class FreeCamera final : public GameObject
{
  public:
    void initialize(float aspect_ratio);
    void update(const InputManager& input, float delta_time);
    void set_aspect_ratio(float aspect_ratio) noexcept;

    const TransformComponent& transform() const noexcept { return m_transform; }
    TransformComponent& transform() noexcept { return m_transform; }
    const CameraComponent& camera() const noexcept { return m_camera; }

  private:
    TransformComponent m_transform;
    CameraComponent m_camera;
    FreeCameraControllerComponent m_controller;
};
} // namespace Engine
