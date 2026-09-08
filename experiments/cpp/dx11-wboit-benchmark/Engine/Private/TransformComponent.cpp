#include "TransformComponent.h"

namespace Engine
{
using namespace DirectX;

XMMATRIX TransformComponent::world_matrix() const noexcept
{
    return XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z)
        * XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z)
        * XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
}
}
