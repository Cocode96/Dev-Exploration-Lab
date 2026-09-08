#include "EffectFactory.h"
#include <algorithm>
namespace Lab
{
EffectParticle EffectFactory::create(const EffectPreset& p, unsigned index, mt19937& random)
{
    // 풀 재활성화 시 같은 시드로 초기값을 재현한다. 뒤집힌 Min/Max도 정규화한다.
    auto sample = [&](float a, float b) {
        return uniform_real_distribution<float>(min(a, b), max(a, b))(random);
    };
    EffectParticle result;
    result.position = {sample(p.origin_min.x, p.origin_max.x), sample(p.origin_min.y, p.origin_max.y),
                       sample(p.origin_min.z, p.origin_max.z)};
    result.velocity = {sample(p.velocity_min.x, p.velocity_max.x), sample(p.velocity_min.y, p.velocity_max.y),
                       sample(p.velocity_min.z, p.velocity_max.z)};
    const float speed = sample(p.speed_min, p.speed_max);
    auto v = XMLoadFloat3(&result.velocity);
    if (XMVectorGetX(XMVector3LengthSq(v)) > 0.000001f)
        XMStoreFloat3(&result.velocity, XMVector3Normalize(v) * speed);
    result.acceleration = {sample(p.acceleration_min.x, p.acceleration_max.x),
                           sample(p.acceleration_min.y, p.acceleration_max.y),
                           sample(p.acceleration_min.z, p.acceleration_max.z)};
    result.lifetime = sample(p.life_min, p.life_max);
    result.age = -p.delay;
    result.color = p.start_color;
    result.size = p.start_size;
    result.preset = index;
    result.active = true;
    return result;
}
} // namespace Lab
