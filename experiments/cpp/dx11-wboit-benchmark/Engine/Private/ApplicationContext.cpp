#include "ApplicationContext.h"

#include "BenchmarkManager.h"
#include "CameraManager.h"
#include "DebugUiManager.h"
#include "InputManager.h"
#include "Renderer.h"
#include "SceneManager.h"

#include <stdexcept>

namespace Engine
{
ApplicationContext::ApplicationContext() = default;
ApplicationContext::~ApplicationContext() = default;

bool ApplicationContext::initialize(HWND window, std::uint32_t width, std::uint32_t height,
    const std::filesystem::path& shader_path, const std::filesystem::path& output_directory)
{
    m_input_manager = std::make_unique<InputManager>();
    m_camera_manager = std::make_unique<CameraManager>();
    m_scene_manager = std::make_unique<SceneManager>();
    m_benchmark_manager = std::make_unique<BenchmarkManager>();
    m_renderer = std::make_unique<Renderer>();
    m_debug_ui_manager = std::make_unique<DebugUiManager>();
    m_benchmark_manager->initialize(output_directory);
    if (!m_camera_manager->initialize(static_cast<float>(width) / static_cast<float>(height))
        || !m_renderer->initialize(*this, window, width, height, shader_path))
        return false;
    return m_debug_ui_manager->initialize(
        window, m_renderer->device(), m_renderer->device_context());
}

CameraManager& ApplicationContext::camera_manager()
{
    if (!m_camera_manager) throw std::logic_error("Camera manager is not initialized.");
    return *m_camera_manager;
}

const CameraManager& ApplicationContext::camera_manager() const
{
    if (!m_camera_manager) throw std::logic_error("Camera manager is not initialized.");
    return *m_camera_manager;
}

Renderer& ApplicationContext::renderer()
{
    if (!m_renderer) throw std::logic_error("Renderer is not initialized.");
    return *m_renderer;
}

InputManager& ApplicationContext::input()
{
    if (!m_input_manager) throw std::logic_error("Input manager is not initialized.");
    return *m_input_manager;
}

SceneManager& ApplicationContext::scene_manager()
{
    if (!m_scene_manager) throw std::logic_error("scene manager is not initialized.");
    return *m_scene_manager;
}

BenchmarkManager& ApplicationContext::benchmark_manager()
{
    if (!m_benchmark_manager) throw std::logic_error("Benchmark manager is not initialized.");
    return *m_benchmark_manager;
}

DebugUiManager& ApplicationContext::debug_ui_manager()
{
    if (!m_debug_ui_manager) throw std::logic_error("Debug UI manager is not initialized.");
    return *m_debug_ui_manager;
}
}
