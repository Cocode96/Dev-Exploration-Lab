#pragma once

#include <cstddef>
#include "imgui.h"

namespace Engine
{
using namespace std;

inline constexpr size_t max_log_entries = 512;

inline constexpr ImGuiWindowFlags workspace_window_flags = ImGuiWindowFlags_NoDocking |
    ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoBringToFrontOnFocus |
    ImGuiWindowFlags_NoNavFocus |
    ImGuiWindowFlags_NoBackground;

inline constexpr char workspace_window_name[] = "RuntimeDebugWorkspace";
inline constexpr char dockspace_name[] = "RuntimeDebugDockspace";
inline constexpr char scene_window_name[] = "Scene View";
inline constexpr char experiment_window_name[] = "Experiment Controls";
inline constexpr char performance_window_name[] = "Live Performance";
inline constexpr char log_window_name[] = "Runtime Log";
}
