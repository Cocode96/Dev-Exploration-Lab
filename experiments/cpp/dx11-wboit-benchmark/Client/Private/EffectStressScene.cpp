#include "EffectStressScene.h"
#include "Client_Effect_Constant.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <random>

namespace Client
{
using namespace std;
using namespace DirectX;

bool EffectStressScene::initialize(Engine::ApplicationContext&)
{
    if (!initialize_world())
        return false;
    rebuild(256);
    return true;
}

void EffectStressScene::update(Engine::ApplicationContext&, float) {}

void EffectStressScene::set_instance_count(uint32_t count)
{
    count = (max)(64u, (min)(count, 16384u));
    if (count != m_instances.size())
    {
        rebuild(count);
        m_rest_instances.clear();
    }
}

void EffectStressScene::set_pattern(ParticleStressPattern pattern)
{
    if (m_pattern == pattern)
        return;
    m_pattern = pattern;
    m_rest_instances.clear();
    rebuild(instance_count());
}

void EffectStressScene::set_benchmark_motion(float time, uint32_t seed, float speed)
{
    if (m_seed != seed)
    {
        m_seed = seed;
        rebuild(instance_count());
        m_rest_instances.clear();
    }
    if (m_rest_instances.size() != m_instances.size())
        m_rest_instances = m_instances;
    // 벤치마크 전용 이동: 원본 위치에 주기 함수를 더해 누적 오차를 막는다.
    for (size_t i = 0; i < m_instances.size(); ++i)
    {
        m_instances[i] = m_rest_instances[i];
        const float phase = float((i * 1664525u + seed) % 1024) * 0.006135923f;
        auto& p = m_instances[i].position_and_billboard;
        p.x += (sin(time * speed + phase) - sin(phase)) * 1.5f;
        p.y += (cos(time * speed * 0.7f + phase) - cos(phase)) * 0.5f;
    }
}

void EffectStressScene::rebuild(uint32_t count)
{

    m_instances.clear();
    m_instances.reserve(count);

    if (m_pattern == ParticleStressPattern::SortingFailure)
    {
        const uint32_t crossing_count = (min)(count, 48u);
        for (uint32_t index = 0; index < crossing_count; ++index)
        {
            const auto color = effect_palette[index % effect_palette.size()];
            const float ratio = static_cast<float>(index) / static_cast<float>((max)(crossing_count, 1u));
            const float yaw = ratio * XM_PI;
            const float height = 2.3f + static_cast<float>(index % 4) * 0.48f;
            const float center_offset = static_cast<float>(static_cast<int>(index % 3) - 1) * 0.025f;
            m_instances.push_back({{center_offset, height, 8.0f + center_offset, 0.0f},
                                   {8.4f, 0.52f},
                                   {color.x, color.y, color.z, 0.34f},
                                   {index % 2 == 0 ? yaw : -yaw, ratio * 0.65f}});
        }

        mt19937 random(m_seed + count);
        uniform_real_distribution<float> horizontal(-7.5f, 7.5f);
        uniform_real_distribution<float> height(0.8f, 6.4f);
        uniform_real_distribution<float> jitter(-0.18f, 0.18f);
        uniform_real_distribution<float> size(0.75f, 1.85f);
        uniform_real_distribution<float> angle(0.0f, XM_2PI);
        while (m_instances.size() < count)
        {
            const size_t index = m_instances.size();
            const bool first_sheet = index % 2 == 0;
            const float x = horizontal(random);
            const float y = height(random);
            const float sheet_depth = first_sheet ? 8.0f + x * 0.26f : 8.0f - x * 0.26f;
            const auto color = first_sheet ? effect_palette[0] : effect_palette[1];
            const float particle_size = size(random);
            m_instances.push_back({{x, y, sheet_depth + jitter(random), 1.0f},
                                   {particle_size, particle_size},
                                   {color.x, color.y, color.z, 0.30f + static_cast<float>(index % 3) * 0.06f},
                                   {0.0f, angle(random)}});
        }
        return;
    }

    mt19937 random(m_seed + count);
    uniform_real_distribution<float> horizontal(-10.0f, 10.0f);
    uniform_real_distribution<float> height(0.45f, 7.5f);
    uniform_real_distribution<float> depth(2.0f, 20.0f);
    uniform_real_distribution<float> size(0.30f, 1.25f);
    uniform_real_distribution<float> angle(0.0f, XM_2PI);
    uniform_real_distribution<float> alpha(0.18f, 0.58f);
    while (m_instances.size() < count)
    {
        const size_t index = m_instances.size();
        const auto color = effect_palette[index % effect_palette.size()];
        const float width = size(random);
        m_instances.push_back({{horizontal(random), height(random), depth(random), 1.0f},
                               {width, width},
                               {color.x, color.y, color.z, alpha(random)},
                               {angle(random), 0.0f}});
    }
}
} // namespace Client
