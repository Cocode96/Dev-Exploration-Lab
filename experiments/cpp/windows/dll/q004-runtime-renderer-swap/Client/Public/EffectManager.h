#pragma once
#include "EffectPool.h"
#include "EffectPresetLoader.h"
#include "Lab_Struct.h"
namespace Lab
{
class EffectManager final
{
  public:
    bool load(const filesystem::path& file, string& message);
    void reset(const BenchmarkOptions& options);
    void update(float dt, const BenchmarkOptions& options);
    SceneFrame scene(CameraMode camera, float speed);
    unsigned active_count() const { return m_pool.active_count(); }
    unsigned failures() const { return m_pool.failures(); }
    float time() const { return m_time; }

  private:
    vector<EffectPreset> m_presets{EffectPreset{}};
    EffectPool m_pool;
    mt19937 m_random{2026};
    vector<SceneVertex> m_vertices;
    float m_time{};
    unsigned m_next_preset{};
};
} // namespace Lab
