#include "EffectStressScene.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <random>

namespace Client
{
bool EffectStressScene::initialize(Engine::ApplicationContext&)
{
    if (!initialize_world())
        return false;
    rebuild(256);
    return true;
}

void EffectStressScene::update(Engine::ApplicationContext&, float)
{
}

void EffectStressScene::set_instance_count(std::uint32_t count)
{
    count = (std::max)(64u, (std::min)(count, 16384u));
    if (count != m_instances.size())
        rebuild(count);
}

void EffectStressScene::rebuild(std::uint32_t count)
{
    static constexpr std::array<DirectX::XMFLOAT3, 8> palette{
        DirectX::XMFLOAT3{0.95f, 0.18f, 0.23f}, DirectX::XMFLOAT3{0.12f, 0.65f, 1.00f},
        DirectX::XMFLOAT3{0.95f, 0.75f, 0.10f}, DirectX::XMFLOAT3{0.55f, 0.20f, 0.95f},
        DirectX::XMFLOAT3{0.12f, 0.90f, 0.55f}, DirectX::XMFLOAT3{1.00f, 0.35f, 0.70f},
        DirectX::XMFLOAT3{0.95f, 0.50f, 0.12f}, DirectX::XMFLOAT3{0.35f, 0.90f, 0.95f}
    };

    m_instances.clear();
    m_instances.reserve(count);
    const std::uint32_t crossing_count = (std::min)(count, 16u);
    for (std::uint32_t index = 0; index < crossing_count; ++index)
    {
        const auto color = palette[index % palette.size()];
        const float angle = static_cast<float>(index) * DirectX::XM_PI /
            static_cast<float>(crossing_count);
        m_instances.push_back({
            {0.0f, 3.0f, 8.0f, 0.0f}, {7.0f, 0.44f},
            {color.x, color.y, color.z, 0.42f},
            {index % 2 == 0 ? angle : -angle, angle * 0.35f}
        });
    }

    std::mt19937 random(0xC0C0DEu + count);
    std::uniform_real_distribution<float> horizontal(-10.0f, 10.0f);
    std::uniform_real_distribution<float> height(0.45f, 7.5f);
    std::uniform_real_distribution<float> depth(2.0f, 20.0f);
    std::uniform_real_distribution<float> size(0.30f, 1.25f);
    std::uniform_real_distribution<float> angle(0.0f, DirectX::XM_2PI);
    std::uniform_real_distribution<float> alpha(0.18f, 0.58f);
    while (m_instances.size() < count)
    {
        const std::size_t index = m_instances.size();
        const auto color = palette[index % palette.size()];
        const float width = size(random);
        m_instances.push_back({
            {horizontal(random), height(random), depth(random), 1.0f}, {width, width},
            {color.x, color.y, color.z, alpha(random)},
            {angle(random), 0.0f}
        });
    }
}
}
