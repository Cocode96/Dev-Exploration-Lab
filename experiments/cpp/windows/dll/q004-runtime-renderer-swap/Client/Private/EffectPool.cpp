#include "EffectPool.h"
#include <algorithm>
namespace Lab
{
void EffectPool::reset(unsigned capacity)
{
    m_particles.assign(capacity, {});
    m_cursor = 0;
    m_failures = 0;
}
bool EffectPool::emit(const EffectPreset& preset, unsigned index, mt19937& random)
{
    // Emit에서 빈 슬롯만 재사용한다. 풀 고갈 시 재생 중인 이펙트를 덮지 않는다.
    for (size_t i = 0; i < m_particles.size(); ++i)
    {
        size_t slot = (m_cursor + i) % m_particles.size();
        if (!m_particles[slot].active)
        {
            m_particles[slot] = EffectFactory::create(preset, index, random);
            m_cursor = (slot + 1) % m_particles.size();
            return true;
        }
    }
    ++m_failures;
    return false;
}
unsigned EffectPool::active_count() const
{
    return static_cast<unsigned>(
        count_if(m_particles.begin(), m_particles.end(), [](const auto& p) { return p.active; }));
}
void EffectPool::update(float dt, const vector<EffectPreset>& presets)
{
    for (auto& p : m_particles)
    {
        if (!p.active)
            continue;
        const float previous_age = p.age;
        p.age += dt;
        if (p.age >= p.lifetime)
        {
            p.active = false;
            continue;
        }
        if (p.age < 0)
            continue;
        // 지연 생성 프레임은 실제 활성 시간만 적분한다.
        const float step = previous_age < 0 ? p.age : dt;
        XMStoreFloat3(&p.position, XMLoadFloat3(&p.position) + XMLoadFloat3(&p.velocity) * step +
                                       XMLoadFloat3(&p.acceleration) * (0.5f * step * step));
        XMStoreFloat3(&p.velocity, XMLoadFloat3(&p.velocity) + XMLoadFloat3(&p.acceleration) * step);
        const auto& preset = presets.at(p.preset);
        const float ratio = p.age / p.lifetime;
        XMStoreFloat4(&p.color, XMVectorLerp(XMLoadFloat4(&preset.start_color),
                                             XMLoadFloat4(&preset.end_color), ratio));
        XMStoreFloat2(&p.size,
                      XMVectorLerp(XMLoadFloat2(&preset.start_size), XMLoadFloat2(&preset.end_size), ratio));
    }
}
} // namespace Lab
