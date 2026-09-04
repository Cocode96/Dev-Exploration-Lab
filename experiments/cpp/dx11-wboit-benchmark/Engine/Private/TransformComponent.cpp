#include "TransformComponent.h"

namespace Engine
{
DirectX::XMMATRIX TransformComponent::world_matrix() const noexcept
{
    return DirectX::XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z)
        * DirectX::XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z)
        * DirectX::XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
}
}
