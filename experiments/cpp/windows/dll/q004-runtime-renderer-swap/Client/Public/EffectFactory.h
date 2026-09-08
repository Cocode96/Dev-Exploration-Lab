#pragma once
#include "Effect_Struct.h"
#include <random>
namespace Lab
{
class EffectFactory final
{
  public:
    static EffectParticle create(const EffectPreset& preset, unsigned index, mt19937& random);
};
} // namespace Lab
