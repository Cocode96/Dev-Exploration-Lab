#pragma once

#include <DirectXMath.h>

namespace Engine
{
using namespace DirectX;

class TransformComponent final
{
public:
    void set_position(const XMFLOAT3& position) noexcept { m_position = position; }
    void set_rotation(const XMFLOAT3& rotation) noexcept { m_rotation = rotation; }
    void set_scale(const XMFLOAT3& scale) noexcept { m_scale = scale; }

    const XMFLOAT3& position() const noexcept { return m_position; }
    const XMFLOAT3& rotation() const noexcept { return m_rotation; }
    const XMFLOAT3& scale() const noexcept { return m_scale; }
    XMMATRIX world_matrix() const noexcept;

private:
    XMFLOAT3 m_position{};
    XMFLOAT3 m_rotation{};
    XMFLOAT3 m_scale{1.0f, 1.0f, 1.0f};
};
}
