#pragma once

#include "WorldScene.h"

#include <vector>

namespace Client
{
using namespace std;

class ValidationScene final : public WorldScene
{
public:
    bool initialize(Engine::ApplicationContext& context) override;
    void update(Engine::ApplicationContext& context, float delta_time) override;
    wstring_view name() const override { return L"Crossing Geometry Validation"; }
    void set_instance_count(uint32_t count) override;
    uint32_t instance_count() const override { return static_cast<uint32_t>(m_instances.size()); }

private:
    span<const Engine::EffectInstance> effect_instances() const override { return m_instances; }
    void rebuild(uint32_t count);
    vector<Engine::EffectInstance> m_instances;
};
}
