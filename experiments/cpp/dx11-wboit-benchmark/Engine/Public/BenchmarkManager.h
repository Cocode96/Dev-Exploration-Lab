#pragma once

#include "EngineTypes.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Engine
{
class ApplicationContext;

class BenchmarkManager final
{
public:
    void initialize(std::filesystem::path output_directory);
    void start(ApplicationContext& context);
    void prepare_frame(ApplicationContext& context);
    void record_frame(ApplicationContext& context, const FrameMetrics& metrics);

    bool is_running() const noexcept { return m_running; }
    std::wstring status_text() const;

private:
    struct Phase
    {
        SceneType scene{};
        TransparencyMode mode{};
        std::uint32_t instance_count{};
    };

    struct PhaseResult
    {
        Phase phase{};
        std::vector<FrameMetrics> samples;
    };

    void complete_current_phase(ApplicationContext& context);
    void write_results() const;

    std::filesystem::path m_output_directory;
    std::vector<Phase> m_phases;
    std::vector<PhaseResult> m_results;
    std::size_t m_phase_index{};
    std::uint32_t m_warmup_remaining{};
    std::uint32_t m_samples_remaining{};
    bool m_running{};
    SceneType m_previous_scene{SceneType::Validation};
    RenderSettings m_previous_settings{};
    std::uint32_t m_previous_instance_count{};

    static constexpr std::uint32_t warmup_frames = 60;
    static constexpr std::uint32_t sample_frames = 300;
};
}
