#include "FreeCameraControllerComponent.h"

#include "InputManager.h"

#include <algorithm>

namespace Engine
{
void FreeCameraControllerComponent::update(TransformComponent& transform,
    const CameraComponent& camera, const InputManager& input, float delta_time) const
{
    auto rotation = transform.rotation();
    if (input.is_mouse_look_active())
    {
        const auto delta = input.mouse_delta();
        rotation.y += static_cast<float>(delta.x) * m_look_speed;
        rotation.x = (std::clamp)(rotation.x + static_cast<float>(delta.y) * m_look_speed,
            DirectX::XMConvertToRadians(-85.0f), DirectX::XMConvertToRadians(85.0f));
        transform.set_rotation(rotation);
    }

    DirectX::XMVECTOR movement = DirectX::XMVectorZero();
    if (input.is_down('W')) movement = DirectX::XMVectorAdd(movement, camera.forward(transform));
    if (input.is_down('S')) movement = DirectX::XMVectorSubtract(movement, camera.forward(transform));
    if (input.is_down('D')) movement = DirectX::XMVectorAdd(movement, camera.right(transform));
    if (input.is_down('A')) movement = DirectX::XMVectorSubtract(movement, camera.right(transform));
    if (input.is_down('E')) movement = DirectX::XMVectorAdd(movement, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    if (input.is_down('Q')) movement = DirectX::XMVectorSubtract(movement, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(movement)) > 0.0f)
    {
        movement = DirectX::XMVectorScale(DirectX::XMVector3Normalize(movement), m_move_speed * delta_time);
        DirectX::XMFLOAT3 position{};
        DirectX::XMStoreFloat3(&position,
            DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&transform.position()), movement));
        transform.set_position(position);
    }
}
}
