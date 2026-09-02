#pragma once

#include "TransformComponent.h"

namespace Engine
{
class CameraComponent final
{
public:
    void configure(float field_of_view_radians, float aspect_ratio, float near_plane, float far_plane);
    DirectX::XMMATRIX view_matrix(const TransformComponent& transform) const noexcept;
    DirectX::XMMATRIX projection_matrix() const noexcept;
    DirectX::XMVECTOR forward(const TransformComponent& transform) const noexcept;
    DirectX::XMVECTOR right(const TransformComponent& transform) const noexcept;
    DirectX::XMVECTOR up(const TransformComponent& transform) const noexcept;

private:
    float m_field_of_view{DirectX::XMConvertToRadians(60.0f)};
    float m_aspect_ratio{16.0f / 9.0f};
    float m_near_plane{0.1f};
    float m_far_plane{250.0f};
};
}
