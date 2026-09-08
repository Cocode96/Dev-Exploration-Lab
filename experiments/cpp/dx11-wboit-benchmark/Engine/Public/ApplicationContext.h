#pragma once

#include "Engine_Struct.h"

#include <Windows.h>

#include <filesystem>
#include <memory>

namespace Engine
{
using namespace std;

class BenchmarkManager;
class CameraManager;
class DebugUiManager;
class InputManager;
class Renderer;
class SceneManager;

class ApplicationContext final
{
public:
    ApplicationContext();
    ~ApplicationContext();

    ApplicationContext(const ApplicationContext&) = delete;
    ApplicationContext& operator=(const ApplicationContext&) = delete;

    bool initialize(HWND window, uint32_t width, uint32_t height,
        const filesystem::path& shader_path, const filesystem::path& output_directory);

    Renderer& renderer();
    CameraManager& camera_manager();
    const CameraManager& camera_manager() const;
    InputManager& input();
    SceneManager& scene_manager();
    BenchmarkManager& benchmark_manager();
    DebugUiManager& debug_ui_manager();
    RenderSettings& render_settings() noexcept { return m_render_settings; }
    const RenderSettings& render_settings() const noexcept { return m_render_settings; }

private:
    unique_ptr<InputManager> m_input_manager;
    unique_ptr<CameraManager> m_camera_manager;
    unique_ptr<SceneManager> m_scene_manager;
    unique_ptr<BenchmarkManager> m_benchmark_manager;
    unique_ptr<Renderer> m_renderer;
    unique_ptr<DebugUiManager> m_debug_ui_manager;
    RenderSettings m_render_settings{};
};
}
