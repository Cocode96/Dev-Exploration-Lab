#pragma once
#include "Effect_Struct.h"
#include <filesystem>
namespace Lab
{
class EffectPresetLoader final
{
  public:
    static bool load(const filesystem::path& file, vector<EffectPreset>& output, string& message);
};
} // namespace Lab
