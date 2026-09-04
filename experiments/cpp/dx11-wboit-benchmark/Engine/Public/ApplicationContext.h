#pragma once

#include "EngineTypes.h"

#include <Windows.h>

#include <filesystem>
#include <memory>

namespace Engine
{
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

    bool initialize(HWND window, std::uint32_t width, std::uint32_t height,
        const std::filesystem::path& shader_path, const std::filesystem::path& output_directory);

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
    std::unique_ptr<InputManager> m_input_manager;
    std::unique_ptr<CameraManager> m_camera_manager;
    std::unique_ptr<SceneManager> m_scene_manager;
    std::unique_ptr<BenchmarkManager> m_benchmark_manager;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<DebugUiManager> m_debug_ui_manager;
    RenderSettings m_render_settings{};
};
}
