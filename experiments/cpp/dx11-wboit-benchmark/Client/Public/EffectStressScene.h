#pragma once

#include "WorldScene.h"
#include "Client_Enum.h"

#include <vector>

namespace Client
{
using namespace std;

class EffectStressScene final : public WorldScene
{
  public:
    bool initialize(Engine::ApplicationContext& context) override;
    void update(Engine::ApplicationContext& context, float delta_time) override;
    wstring_view name() const override { return L"Particle and Billboard Stress"; }
    void set_instance_count(uint32_t count) override;
    uint32_t instance_count() const override { return static_cast<uint32_t>(m_instances.size()); }
    void set_pattern(ParticleStressPattern pattern);
    ParticleStressPattern pattern() const noexcept { return m_pattern; }
    void set_benchmark_motion(float time, uint32_t seed, float speed) override;

  private:
    span<const Engine::EffectInstance> effect_instances() const override { return m_instances; }
    void rebuild(uint32_t count);
    vector<Engine::EffectInstance> m_instances;
    vector<Engine::EffectInstance> m_rest_instances;
    uint32_t m_seed{0xC0C0DEu};
    ParticleStressPattern m_pattern{ParticleStressPattern::SortingFailure};
};
} // namespace Client
