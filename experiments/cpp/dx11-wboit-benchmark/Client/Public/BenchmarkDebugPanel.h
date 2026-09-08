#pragma once

#include <filesystem>

namespace Engine
{
class ApplicationContext;
}

namespace Client
{
using namespace std;

class BenchmarkDebugPanel final
{
public:
    void render(Engine::ApplicationContext& context,
        const filesystem::path& output_directory);
    bool camera_input_enabled() const noexcept { return m_camera_input_enabled; }

private:
    void render_experiment_controls(Engine::ApplicationContext& context,
        const filesystem::path& output_directory);
    void render_performance(Engine::ApplicationContext& context);
    void render_benchmark_result(Engine::ApplicationContext& context);

    bool m_auto_flip_order{};
    bool m_camera_input_enabled{true};
    float m_auto_flip_elapsed{};
    bool m_previous_benchmark_running{};
    bool m_open_benchmark_result{};
};
}
