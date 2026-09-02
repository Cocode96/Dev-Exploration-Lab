#include "ValidationScene.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>

namespace Client
{
bool ValidationScene::initialize(Engine::ApplicationContext&)
{
    if (!initialize_world())
        return false;
    rebuild(16);
    return true;
}

void ValidationScene::update(Engine::ApplicationContext&, float)
{
}

void ValidationScene::set_instance_count(std::uint32_t count)
{
    count = (std::max)(2u, (std::min)(count, 32u));
    if (count != m_instances.size())
        rebuild(count);
}

void ValidationScene::rebuild(std::uint32_t count)
{
    static constexpr std::array<DirectX::XMFLOAT3, 8> palette{
        DirectX::XMFLOAT3{0.95f, 0.18f, 0.23f}, DirectX::XMFLOAT3{0.12f, 0.65f, 1.00f},
        DirectX::XMFLOAT3{0.95f, 0.75f, 0.10f}, DirectX::XMFLOAT3{0.55f, 0.20f, 0.95f},
        DirectX::XMFLOAT3{0.12f, 0.90f, 0.55f}, DirectX::XMFLOAT3{1.00f, 0.35f, 0.70f},
        DirectX::XMFLOAT3{0.95f, 0.50f, 0.12f}, DirectX::XMFLOAT3{0.35f, 0.90f, 0.95f}
    };
    m_instances.clear();
    m_instances.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const auto color = palette[index % palette.size()];
        const float angle = static_cast<float>(index) * DirectX::XM_PI / static_cast<float>(count);
        m_instances.push_back({
            {0.0f, 3.0f, 8.0f, 0.0f}, {7.5f, 0.48f},
            {color.x, color.y, color.z, 0.42f},
            {index % 2 == 0 ? angle : -angle, angle * 0.35f}
        });
    }
}
}
