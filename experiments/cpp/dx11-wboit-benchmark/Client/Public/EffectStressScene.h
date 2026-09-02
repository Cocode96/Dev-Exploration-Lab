#pragma once

#include "WorldScene.h"

#include <vector>

namespace Client
{
enum class ParticleStressPattern
{
    NaturalVolume,
    SortingFailure
};

class EffectStressScene final : public WorldScene
{
public:
    bool initialize(Engine::ApplicationContext& context) override;
    void update(Engine::ApplicationContext& context, float delta_time) override;
    std::wstring_view name() const override { return L"Particle and Billboard Stress"; }
    void set_instance_count(std::uint32_t count) override;
    std::uint32_t instance_count() const override { return static_cast<std::uint32_t>(m_instances.size()); }
    void set_pattern(ParticleStressPattern pattern);
    ParticleStressPattern pattern() const noexcept { return m_pattern; }

private:
    std::span<const Engine::EffectInstance> effect_instances() const override { return m_instances; }
    void rebuild(std::uint32_t count);
    std::vector<Engine::EffectInstance> m_instances;
    ParticleStressPattern m_pattern{ParticleStressPattern::SortingFailure};
};
}
