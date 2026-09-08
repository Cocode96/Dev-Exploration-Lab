#pragma once

#include "TransformComponent.h"

namespace Engine
{
using namespace DirectX;

class CameraComponent final
{
public:
    void configure(float field_of_view_radians, float aspect_ratio, float near_plane, float far_plane);
    void set_aspect_ratio(float aspect_ratio) noexcept;
    XMMATRIX view_matrix(const TransformComponent& transform) const noexcept;
    XMMATRIX projection_matrix() const noexcept;
    XMVECTOR forward(const TransformComponent& transform) const noexcept;
    XMVECTOR right(const TransformComponent& transform) const noexcept;
    XMVECTOR up(const TransformComponent& transform) const noexcept;

private:
    float m_field_of_view{XMConvertToRadians(60.0f)};
    float m_aspect_ratio{16.0f / 9.0f};
    float m_near_plane{0.1f};
    float m_far_plane{250.0f};
};
}
