#pragma once

#include <DirectXMath.h>

namespace Engine
{
class TransformComponent final
{
public:
    void set_position(const DirectX::XMFLOAT3& position) noexcept { m_position = position; }
    void set_rotation(const DirectX::XMFLOAT3& rotation) noexcept { m_rotation = rotation; }
    void set_scale(const DirectX::XMFLOAT3& scale) noexcept { m_scale = scale; }

    const DirectX::XMFLOAT3& position() const noexcept { return m_position; }
    const DirectX::XMFLOAT3& rotation() const noexcept { return m_rotation; }
    const DirectX::XMFLOAT3& scale() const noexcept { return m_scale; }
    DirectX::XMMATRIX world_matrix() const noexcept;

private:
    DirectX::XMFLOAT3 m_position{};
    DirectX::XMFLOAT3 m_rotation{};
    DirectX::XMFLOAT3 m_scale{1.0f, 1.0f, 1.0f};
};
}
