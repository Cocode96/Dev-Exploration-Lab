#include "BenchmarkManager.h"
#include "Engine_Benchmark_Constant.h"
#include "Engine_Benchmark_Function.h"

#include "ApplicationContext.h"
#include "SceneManager.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace Engine
{
using namespace std;

void BenchmarkManager::initialize(filesystem::path output_directory)
{
    m_output_directory = move(output_directory);
}

void BenchmarkManager::start(ApplicationContext& context)
{
    if (m_running)
        return;
    m_previous_scene = context.scene_manager().active_scene_type();
    m_previous_settings = context.render_settings();
    m_previous_instance_count = context.scene_manager().active_scene().instance_count();
    m_phases.clear();
    m_results.clear();
    for (TransparencyMode mode : {TransparencyMode::UnsortedAlpha,
        TransparencyMode::ZSortedAlpha, TransparencyMode::Wboit})
    {
        for (uint32_t count : {2u, 4u, 8u, 16u, 32u})
            m_phases.push_back({SceneType::Validation, mode, count});
        for (uint32_t count : {64u, 256u, 1024u})
            m_phases.push_back({SceneType::EffectStress, mode, count});
    }
    m_phase_index = 0;
    m_warmup_remaining = warmup_frames;
    m_samples_remaining = sample_frames;
    m_running = true;
}

void BenchmarkManager::prepare_frame(ApplicationContext& context)
{
    if (!m_running)
        return;
    const auto& phase = m_phases[m_phase_index];
    context.scene_manager().change_scene(phase.scene);
    context.scene_manager().active_scene().set_instance_count(phase.instance_count);
    context.render_settings().transparency_mode = phase.mode;
    context.render_settings().reverse_submission_order = false;
}

void BenchmarkManager::record_frame(ApplicationContext& context, const FrameMetrics& metrics)
{
    if (!m_running)
        return;
    if (m_warmup_remaining > 0)
    {
        --m_warmup_remaining;
        return;
    }
    if (m_results.size() <= m_phase_index)
        m_results.push_back({m_phases[m_phase_index], {}});
    m_results[m_phase_index].samples.push_back(metrics);
    if (--m_samples_remaining == 0)
        complete_current_phase(context);
}

void BenchmarkManager::complete_current_phase(ApplicationContext& context)
{
    ++m_phase_index;
    if (m_phase_index < m_phases.size())
    {
        m_warmup_remaining = warmup_frames;
        m_samples_remaining = sample_frames;
        return;
    }

    write_results();
    m_running = false;
    context.scene_manager().change_scene(m_previous_scene);
    context.scene_manager().active_scene().set_instance_count(m_previous_instance_count);
    context.render_settings() = m_previous_settings;
}

wstring BenchmarkManager::status_text() const
{
    if (!m_running)
        return L"B: run full benchmark";
    wostringstream text;
    text << L"Benchmark " << (m_phase_index + 1) << L'/' << m_phases.size();
    if (m_warmup_remaining > 0)
        text << L" warmup " << m_warmup_remaining;
    else
        text << L" samples " << (sample_frames - m_samples_remaining) << L'/' << sample_frames;
    return text.str();
}

vector<BenchmarkManager::ResultSummary> BenchmarkManager::result_summaries() const
{
    vector<ResultSummary> summaries;
    summaries.reserve(m_results.size());
    for (const auto& result : m_results)
    {
        vector<double> total;
        vector<double> sort;
        total.reserve(result.samples.size());
        sort.reserve(result.samples.size());
        for (const auto& sample : result.samples)
        {
            total.push_back(sample.gpu_total_ms);
            sort.push_back(sample.cpu_sort_ms);
        }
        const auto total_stats = summarize(total);
        summaries.push_back({result.phase.scene, result.phase.mode, result.phase.instance_count,
            total_stats.mean, total_stats.p95, summarize(sort).mean,
            result.samples.empty() ? 0u : result.samples.front().total_draw_calls,
            result.samples.empty() ? 0u : result.samples.front().transparency_draw_calls});
    }
    return summaries;
}

void BenchmarkManager::write_results() const
{
    filesystem::create_directories(m_output_directory);
    ofstream output(m_output_directory / "windowed_benchmark.csv");
    output << "scene,mode,instances,gpu_total_mean_ms,gpu_total_median_ms,gpu_total_p95_ms,"
              "gpu_total_stddev_ms,gpu_transparency_mean_ms,gpu_resolve_mean_ms,"
              "cpu_submit_mean_ms,cpu_sort_mean_ms,total_draw_calls,transparency_draw_calls\n";
    output << fixed << setprecision(6);
    for (const auto& result : m_results)
    {
        vector<double> total;
        vector<double> transparency;
        vector<double> resolve;
        vector<double> submit;
        vector<double> sort;
        for (const auto& sample : result.samples)
        {
            total.push_back(sample.gpu_total_ms);
            transparency.push_back(sample.gpu_transparency_ms);
            resolve.push_back(sample.gpu_resolve_ms);
            submit.push_back(sample.cpu_submit_ms);
            sort.push_back(sample.cpu_sort_ms);
        }
        const auto total_stats = summarize(total);
        output << scene_name(result.phase.scene) << ',' << mode_name(result.phase.mode) << ','
               << result.phase.instance_count << ',' << total_stats.mean << ',' << total_stats.median << ','
               << total_stats.p95 << ',' << total_stats.standard_deviation << ','
               << summarize(transparency).mean << ',' << summarize(resolve).mean << ','
               << summarize(submit).mean << ',' << summarize(sort).mean << ','
               << result.samples.front().total_draw_calls << ','
               << result.samples.front().transparency_draw_calls << '\n';
    }
}
}
