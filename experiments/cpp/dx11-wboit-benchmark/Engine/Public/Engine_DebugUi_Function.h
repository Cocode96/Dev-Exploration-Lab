#pragma once

#include "Engine_DebugUi_Enum.h"
#include "imgui.h"

namespace Engine
{
inline ImVec4 log_color(Engine::DebugLogLevel level)
{
    switch (level)
    {
    case Engine::DebugLogLevel::Success: return {0.33f, 0.78f, 0.71f, 1.0f};
    case Engine::DebugLogLevel::Warning: return {0.96f, 0.72f, 0.20f, 1.0f};
    case Engine::DebugLogLevel::Error: return {0.96f, 0.35f, 0.38f, 1.0f};
    case Engine::DebugLogLevel::Info:
    default: return {0.72f, 0.78f, 0.84f, 1.0f};
    }
}

inline const char* log_label(Engine::DebugLogLevel level)
{
    switch (level)
    {
    case Engine::DebugLogLevel::Success: return "PASS";
    case Engine::DebugLogLevel::Warning: return "WARN";
    case Engine::DebugLogLevel::Error: return "ERROR";
    case Engine::DebugLogLevel::Info:
    default: return "INFO";
    }
}
}
