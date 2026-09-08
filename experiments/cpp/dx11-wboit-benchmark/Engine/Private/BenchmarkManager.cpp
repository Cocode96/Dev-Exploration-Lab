#include "BenchmarkManager.h"
#include "Engine_Benchmark_Constant.h"
#include "Engine_Benchmark_Function.h"

#include "ApplicationContext.h"
#include "SceneManager.h"
#include "CameraManager.h"
#include "FreeCamera.h"

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
    m_settings.warmup = clamp(m_settings.warmup, 1, 600);
    m_settings.frames = clamp(m_settings.frames, 1, 1800);
    m_settings.repeats = clamp(m_settings.repeats, 1, 5);
    m_settings.maximum_instances = clamp(m_settings.maximum_instances, 64, 16384);
    m_active_settings = m_settings;
    m_error.clear();
    m_previous_camera = context.camera_manager().active_camera().transform();
    vector<TransparencyMode> modes{m_previous_settings.transparency_mode};
    if (m_active_settings.compare_methods)
        modes = {TransparencyMode::UnsortedAlpha, TransparencyMode::ZSortedAlpha, TransparencyMode::Wboit};
    for (int repeat = 0; repeat < m_active_settings.repeats; ++repeat)
        for (auto mode : modes)
        {
            if (m_active_settings.test == 0)
                m_phases.push_back({m_previous_scene, mode, m_previous_instance_count, unsigned(repeat)});
            else
            {
                vector<unsigned> counts{64u, unsigned(max(64, m_active_settings.maximum_instances / 4)),
                                        unsigned(m_active_settings.maximum_instances)};
                sort(counts.begin(), counts.end());
                counts.erase(unique(counts.begin(), counts.end()), counts.end());
                for (auto count : counts)
                    m_phases.push_back({SceneType::EffectStress, mode, count, unsigned(repeat)});
            }
        }
    m_phase_index = 0;
    m_warmup_remaining = unsigned(m_active_settings.warmup);
    m_samples_remaining = unsigned(m_active_settings.frames);
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
    context.render_settings().reverse_submission_order = m_active_settings.reverse_order;
    const unsigned elapsed = unsigned(m_active_settings.warmup + m_active_settings.frames) -
                             m_warmup_remaining - m_samples_remaining;
    const float time = float(elapsed) / 60.0f;
    // 자동 비교에서는 프레임 기준 시간으로 카메라와 이펙트 궤적을 반복한다.
    auto& transform = context.camera_manager().active_camera().transform();
    transform = m_previous_camera;
    if (m_active_settings.camera != 0)
    {
        const float angle = time * 0.35f * m_active_settings.motion_speed;
        DirectX::XMFLOAT3 eye{sin(angle) * 24, 7, 8 - cos(angle) * 24};
        if (m_active_settings.camera == 2)
            eye = {sin(angle) * 15, 6 + sin(angle * 0.5f) * 2, -14 + cos(angle) * 3};
        transform.set_position(eye);
        const float dx = -eye.x, dy = 2.5f - eye.y, dz = 8 - eye.z;
        transform.set_rotation({-atan2(dy, sqrt(dx * dx + dz * dz)), atan2(dx, dz), 0});
    }
    context.scene_manager().active_scene().set_benchmark_motion(m_active_settings.animate_effects ? time : 0,
                                                                unsigned(m_active_settings.seed),
                                                                m_active_settings.motion_speed);
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
        m_warmup_remaining = unsigned(m_active_settings.warmup);
        m_samples_remaining = unsigned(m_active_settings.frames);
        return;
    }

    try
    {
        write_results();
    }
    catch (const exception& e)
    {
        m_error = e.what();
    }
    restore(context);
}

void BenchmarkManager::cancel(ApplicationContext& context)
{
    if (!m_running)
        return;
    m_error = "Cancelled / 취소";
    try
    {
        write_results();
    }
    catch (const exception& e)
    {
        m_error = e.what();
    }
    restore(context);
}

void BenchmarkManager::restore(ApplicationContext& context)
{
    m_running = false;
    context.camera_manager().active_camera().transform() = m_previous_camera;
    context.scene_manager().active_scene().set_benchmark_motion(0, 0xC0C0DEu, 1);
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
        text << L" samples " << (m_active_settings.frames - m_samples_remaining) << L'/'
             << m_active_settings.frames;
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
                             result.samples.empty() ? 0u : result.samples.front().transparency_draw_calls,
                             total.empty() ? 0 : *max_element(total.begin(), total.end()),
                             result.phase.repeat});
    }
    return summaries;
}

void BenchmarkManager::write_results() const
{
    filesystem::create_directories(m_output_directory);
    ofstream output(m_output_directory / "windowed_benchmark.csv");
    output.exceptions(ios::badbit | ios::failbit);
    output << "scene,mode,instances,gpu_total_mean_ms,gpu_total_median_ms,gpu_total_p95_ms,"
              "gpu_total_stddev_ms,gpu_transparency_mean_ms,gpu_resolve_mean_ms,"
              "cpu_submit_mean_ms,cpu_sort_mean_ms,total_draw_calls,transparency_draw_calls,camera,seed,"
              "motion_speed,warmup,frames,repeat,gpu_max_ms\n";
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
               << result.samples.front().transparency_draw_calls << ',' << m_active_settings.camera << ','
               << m_active_settings.seed << ',' << m_active_settings.motion_speed << ','
               << m_active_settings.warmup << ',' << result.samples.size() << ',' << result.phase.repeat
               << ',' << *max_element(total.begin(), total.end()) << '\n';
    }
}
} // namespace Engine
