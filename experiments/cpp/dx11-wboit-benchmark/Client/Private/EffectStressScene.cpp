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

void EffectStressScene::set_pattern(ParticleStressPattern pattern)
{
    if (m_pattern == pattern)
        return;
    m_pattern = pattern;
    rebuild(instance_count());
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

    if (m_pattern == ParticleStressPattern::SortingFailure)
    {
        const std::uint32_t crossing_count = (std::min)(count, 48u);
        for (std::uint32_t index = 0; index < crossing_count; ++index)
        {
            const auto color = palette[index % palette.size()];
            const float ratio = static_cast<float>(index) /
                static_cast<float>((std::max)(crossing_count, 1u));
            const float yaw = ratio * DirectX::XM_PI;
            const float height = 2.3f + static_cast<float>(index % 4) * 0.48f;
            const float center_offset = static_cast<float>(static_cast<int>(index % 3) - 1) * 0.025f;
            m_instances.push_back({
                {center_offset, height, 8.0f + center_offset, 0.0f},
                {8.4f, 0.52f},
                {color.x, color.y, color.z, 0.34f},
                {index % 2 == 0 ? yaw : -yaw, ratio * 0.65f}
            });
        }

        std::mt19937 random(0xFA11EDu + count);
        std::uniform_real_distribution<float> horizontal(-7.5f, 7.5f);
        std::uniform_real_distribution<float> height(0.8f, 6.4f);
        std::uniform_real_distribution<float> jitter(-0.18f, 0.18f);
        std::uniform_real_distribution<float> size(0.75f, 1.85f);
        std::uniform_real_distribution<float> angle(0.0f, DirectX::XM_2PI);
        while (m_instances.size() < count)
        {
            const std::size_t index = m_instances.size();
            const bool first_sheet = index % 2 == 0;
            const float x = horizontal(random);
            const float y = height(random);
            const float sheet_depth = first_sheet
                ? 8.0f + x * 0.26f
                : 8.0f - x * 0.26f;
            const auto color = first_sheet ? palette[0] : palette[1];
            const float particle_size = size(random);
            m_instances.push_back({
                {x, y, sheet_depth + jitter(random), 1.0f},
                {particle_size, particle_size},
                {color.x, color.y, color.z, 0.30f + static_cast<float>(index % 3) * 0.06f},
                {0.0f, angle(random)}
            });
        }
        return;
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
