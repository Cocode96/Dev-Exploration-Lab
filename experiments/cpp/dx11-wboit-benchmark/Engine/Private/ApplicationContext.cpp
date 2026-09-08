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
using namespace std;

ApplicationContext::ApplicationContext() = default;
ApplicationContext::~ApplicationContext() = default;

bool ApplicationContext::initialize(HWND window, uint32_t width, uint32_t height,
    const filesystem::path& shader_path, const filesystem::path& output_directory)
{
    m_input_manager = make_unique<InputManager>();
    m_camera_manager = make_unique<CameraManager>();
    m_scene_manager = make_unique<SceneManager>();
    m_benchmark_manager = make_unique<BenchmarkManager>();
    m_renderer = make_unique<Renderer>();
    m_debug_ui_manager = make_unique<DebugUiManager>();
    m_benchmark_manager->initialize(output_directory);
    if (!m_camera_manager->initialize(static_cast<float>(width) / static_cast<float>(height))
        || !m_renderer->initialize(*this, window, width, height, shader_path))
        return false;
    return m_debug_ui_manager->initialize(
        window, m_renderer->device(), m_renderer->device_context());
}

CameraManager& ApplicationContext::camera_manager()
{
    if (!m_camera_manager) throw logic_error("Camera manager is not initialized.");
    return *m_camera_manager;
}

const CameraManager& ApplicationContext::camera_manager() const
{
    if (!m_camera_manager) throw logic_error("Camera manager is not initialized.");
    return *m_camera_manager;
}

Renderer& ApplicationContext::renderer()
{
    if (!m_renderer) throw logic_error("Renderer is not initialized.");
    return *m_renderer;
}

InputManager& ApplicationContext::input()
{
    if (!m_input_manager) throw logic_error("Input manager is not initialized.");
    return *m_input_manager;
}

SceneManager& ApplicationContext::scene_manager()
{
    if (!m_scene_manager) throw logic_error("scene manager is not initialized.");
    return *m_scene_manager;
}

BenchmarkManager& ApplicationContext::benchmark_manager()
{
    if (!m_benchmark_manager) throw logic_error("Benchmark manager is not initialized.");
    return *m_benchmark_manager;
}

DebugUiManager& ApplicationContext::debug_ui_manager()
{
    if (!m_debug_ui_manager) throw logic_error("Debug UI manager is not initialized.");
    return *m_debug_ui_manager;
}
}
