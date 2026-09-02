#include "CameraComponent.h"

namespace Engine
{
void CameraComponent::configure(float field_of_view_radians, float aspect_ratio,
    float near_plane, float far_plane)
{
    m_field_of_view = field_of_view_radians;
    m_aspect_ratio = aspect_ratio;
    m_near_plane = near_plane;
    m_far_plane = far_plane;
}

void CameraComponent::set_aspect_ratio(float aspect_ratio) noexcept
{
    if (aspect_ratio > 0.0f)
        m_aspect_ratio = aspect_ratio;
}

DirectX::XMVECTOR CameraComponent::forward(const TransformComponent& transform) const noexcept
{
    const auto& rotation = transform.rotation();
    return DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(
        DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
        DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, 0.0f)));
}

DirectX::XMVECTOR CameraComponent::right(const TransformComponent& transform) const noexcept
{
    return DirectX::XMVector3Normalize(DirectX::XMVector3Cross(
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), forward(transform)));
}

DirectX::XMVECTOR CameraComponent::up(const TransformComponent& transform) const noexcept
{
    return DirectX::XMVector3Normalize(DirectX::XMVector3Cross(
        forward(transform), right(transform)));
}

DirectX::XMMATRIX CameraComponent::view_matrix(const TransformComponent& transform) const noexcept
{
    return DirectX::XMMatrixLookToLH(
        DirectX::XMLoadFloat3(&transform.position()), forward(transform),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
}

DirectX::XMMATRIX CameraComponent::projection_matrix() const noexcept
{
    return DirectX::XMMatrixPerspectiveFovLH(
        m_field_of_view, m_aspect_ratio, m_near_plane, m_far_plane);
}
}
