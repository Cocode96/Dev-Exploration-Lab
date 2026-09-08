#pragma once

#include "Engine_Struct.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Engine
{
using namespace std;

class ApplicationContext;

class BenchmarkManager final
{
public:
    struct ResultSummary
    {
        SceneType scene{};
        TransparencyMode mode{};
        uint32_t instance_count{};
        double gpu_total_mean_ms{};
        double gpu_total_p95_ms{};
        double cpu_sort_mean_ms{};
        uint32_t total_draw_calls{};
        uint32_t transparency_draw_calls{};
    };

    void initialize(filesystem::path output_directory);
    void start(ApplicationContext& context);
    void prepare_frame(ApplicationContext& context);
    void record_frame(ApplicationContext& context, const FrameMetrics& metrics);
    vector<ResultSummary> result_summaries() const;

    bool is_running() const noexcept { return m_running; }
    bool has_results() const noexcept { return !m_results.empty(); }
    wstring status_text() const;

private:
    struct Phase
    {
        SceneType scene{};
        TransparencyMode mode{};
        uint32_t instance_count{};
    };

    struct PhaseResult
    {
        Phase phase{};
        vector<FrameMetrics> samples;
    };

    void complete_current_phase(ApplicationContext& context);
    void write_results() const;

    filesystem::path m_output_directory;
    vector<Phase> m_phases;
    vector<PhaseResult> m_results;
    size_t m_phase_index{};
    uint32_t m_warmup_remaining{};
    uint32_t m_samples_remaining{};
    bool m_running{};
    SceneType m_previous_scene{SceneType::Validation};
    RenderSettings m_previous_settings{};
    uint32_t m_previous_instance_count{};

};
}
