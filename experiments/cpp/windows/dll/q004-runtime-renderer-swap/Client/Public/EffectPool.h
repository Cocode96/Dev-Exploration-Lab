#pragma once
#include "EffectFactory.h"
namespace Lab
{
class EffectPool final
{
  public:
    void reset(unsigned capacity);
    bool emit(const EffectPreset& preset, unsigned index, mt19937& random);
    void update(float dt, const vector<EffectPreset>& presets);
    unsigned active_count() const;
    unsigned failures() const { return m_failures; }
    const vector<EffectParticle>& particles() const { return m_particles; }

  private:
    vector<EffectParticle> m_particles;
    size_t m_cursor{};
    unsigned m_failures{};
};
} // namespace Lab
