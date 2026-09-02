#include "FreeCamera.h"

#include "InputManager.h"

namespace Engine
{
void FreeCamera::initialize(float aspect_ratio)
{
    m_transform.set_position({0.0f, 6.0f, -16.0f});
    m_transform.set_rotation({DirectX::XMConvertToRadians(-7.0f), 0.0f, 0.0f});
    m_camera.configure(DirectX::XMConvertToRadians(60.0f), aspect_ratio, 0.1f, 250.0f);
}

void FreeCamera::update(const InputManager& input, float delta_time)
{
    m_controller.update(m_transform, m_camera, input, delta_time);
}
}
