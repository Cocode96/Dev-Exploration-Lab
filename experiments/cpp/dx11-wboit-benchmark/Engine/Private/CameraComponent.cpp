#include "CameraComponent.h"

namespace Engine
{
using namespace DirectX;

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

XMVECTOR CameraComponent::forward(const TransformComponent& transform) const noexcept
{
    const auto& rotation = transform.rotation();
    return XMVector3Normalize(XMVector3TransformNormal(
        XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
        XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, 0.0f)));
}

XMVECTOR CameraComponent::right(const TransformComponent& transform) const noexcept
{
    return XMVector3Normalize(XMVector3Cross(
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), forward(transform)));
}

XMVECTOR CameraComponent::up(const TransformComponent& transform) const noexcept
{
    return XMVector3Normalize(XMVector3Cross(
        forward(transform), right(transform)));
}

XMMATRIX CameraComponent::view_matrix(const TransformComponent& transform) const noexcept
{
    return XMMatrixLookToLH(
        XMLoadFloat3(&transform.position()), forward(transform),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
}

XMMATRIX CameraComponent::projection_matrix() const noexcept
{
    return XMMatrixPerspectiveFovLH(
        m_field_of_view, m_aspect_ratio, m_near_plane, m_far_plane);
}
}
