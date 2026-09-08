#pragma once
#include "real_dx_api.h"
#include <imgui.h>
namespace Lab
{
class LabDebugPanel final
{
  public:
    void render(ExperimentControls& controls, ImTextureID scene);

  private:
    bool m_open_settings{};
};
} // namespace Lab
