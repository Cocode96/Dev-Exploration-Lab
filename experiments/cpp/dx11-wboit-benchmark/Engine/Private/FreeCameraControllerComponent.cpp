#include "FreeCameraControllerComponent.h"

#include "InputManager.h"

#include <algorithm>

namespace Engine
{
using namespace std;
using namespace DirectX;

void FreeCameraControllerComponent::update(TransformComponent& transform,
    const CameraComponent& camera, const InputManager& input, float delta_time) const
{
    auto rotation = transform.rotation();
    if (input.is_mouse_look_active())
    {
        const auto delta = input.mouse_delta();
        rotation.y += static_cast<float>(delta.x) * m_look_speed;
        rotation.x = (clamp)(rotation.x + static_cast<float>(delta.y) * m_look_speed,
            XMConvertToRadians(-85.0f), XMConvertToRadians(85.0f));
        transform.set_rotation(rotation);
    }

    XMVECTOR movement = XMVectorZero();
    if (input.is_down('W')) movement = XMVectorAdd(movement, camera.forward(transform));
    if (input.is_down('S')) movement = XMVectorSubtract(movement, camera.forward(transform));
    if (input.is_down('D')) movement = XMVectorAdd(movement, camera.right(transform));
    if (input.is_down('A')) movement = XMVectorSubtract(movement, camera.right(transform));
    if (input.is_down('E')) movement = XMVectorAdd(movement, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    if (input.is_down('Q')) movement = XMVectorSubtract(movement, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    if (XMVectorGetX(XMVector3LengthSq(movement)) > 0.0f)
    {
        movement = XMVectorScale(XMVector3Normalize(movement), m_move_speed * delta_time);
        XMFLOAT3 position{};
        XMStoreFloat3(&position,
            XMVectorAdd(XMLoadFloat3(&transform.position()), movement));
        transform.set_position(position);
    }
}
}
