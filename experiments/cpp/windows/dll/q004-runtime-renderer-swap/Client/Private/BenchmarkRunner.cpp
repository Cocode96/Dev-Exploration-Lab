#include "BenchmarkRunner.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
namespace Lab
{
void BenchmarkRunner::start(ExperimentControls& c)
{
    auto& l = c.lab;
    auto& o = l.options;
    o.instances = clamp(o.instances, 1, 16384);
    o.capacity = clamp(o.capacity, 1, 16384);
    o.warmup = clamp(o.warmup, 1, 600);
    o.frames = clamp(o.frames, 1, 1800);
    o.repeats = clamp(o.repeats, 1, 5);
    m_saved = o;
    m_phases.clear();
    m_index = m_frame = 0;
    m_new_phase = true;
    for (int repeat = 0; repeat < o.repeats; ++repeat)
    {
        vector<int> counts{o.instances};
        if (o.test == TestMode::EffectSweep)
            counts = {max(1, o.instances / 16), max(1, o.instances / 4), o.instances};
        sort(counts.begin(), counts.end());
        counts.erase(unique(counts.begin(), counts.end()), counts.end());
        for (int count : counts)
        {
            if (o.test == TestMode::CurrentScene)
                m_phases.push_back({int(c.activeDllLayout), int(c.activeBackend), count, repeat});
            else
                for (int layout : {0, 1})
                    for (int backend : {11, 12})
                        m_phases.push_back({layout, backend, count, repeat});
        }
    }
    l.result_count = 0;
    l.result_ready = false;
    l.running = true;
    l.start_requested = false;
    l.phase_count = unsigned(m_phases.size());
    l.cancel_requested = false;
    strcpy_s(l.status, "Running / 측정 중");
}
bool BenchmarkRunner::prepare(ExperimentControls& c)
{
    auto& l = c.lab;
    if (l.cancel_requested)
    {
        finish(c, "Cancelled: completed phases saved / 취소: 완료 단계만 저장");
        return false;
    }
    if (!l.running)
        return false;
    const auto& phase = m_phases.at(m_index);
    l.phase = m_index;
    c.requestedBackend = GraphicsBackend(phase.backend);
    c.requestedDllLayout = RendererDllLayout(phase.layout);
    l.options.instances = phase.count;
    const bool reset = m_new_phase;
    if (reset)
    {
        m_cpu.clear();
        m_gpu.clear();
        m_switch.clear();
        m_frame = 0;
        m_new_phase = false;
    }
    return reset;
}
void BenchmarkRunner::record(ExperimentControls& c, double cpu_ms)
{
    auto& l = c.lab;
    if (!l.running)
        return;
    if (m_frame++ < unsigned(l.options.warmup))
        return;
    if (l.gpu_ms < 0)
    {
        finish(c, "GPU timestamp failed / GPU 타임스탬프 실패");
        return;
    }
    m_cpu.push_back(cpu_ms);
    m_gpu.push_back(l.gpu_ms);
    m_switch.push_back(l.last_switch);
    if (m_cpu.size() < size_t(l.options.frames))
        return;
    const auto& p = m_phases[m_index];
    auto& row = l.results[l.result_count++];
    row = {};
    row.layout = p.layout;
    row.backend = p.backend;
    row.count = p.count;
    row.repeat = p.repeat;
    row.samples = unsigned(m_cpu.size());
    row.active = l.active;
    row.failures = l.failures;
    auto mean = [](const auto& v) { return accumulate(v.begin(), v.end(), 0.0) / v.size(); };
    row.cpu_mean = mean(m_cpu);
    row.gpu_mean = mean(m_gpu);
    sort(m_cpu.begin(), m_cpu.end());
    row.cpu_max = m_cpu.back();
    // 결과 팝업의 p95: 정렬된 실제 프레임 표본에서 nearest-rank를 사용한다.
    row.cpu_p95 = m_cpu[size_t(ceil(m_cpu.size() * 0.95)) - 1];
    for (const auto& s : m_switch)
    {
        row.idle_mean += s.idle;
        row.release_mean += s.release;
        row.module_mean += s.module;
        row.create_mean += s.create;
        row.first_frame_mean += s.first_frame;
    }
    const double n = double(m_switch.size());
    row.idle_mean /= n;
    row.release_mean /= n;
    row.module_mean /= n;
    row.create_mean /= n;
    row.first_frame_mean /= n;
    row.switch_mean =
        row.idle_mean + row.release_mean + row.module_mean + row.create_mean + row.first_frame_mean;
    if (++m_index >= m_phases.size())
    {
        finish(c, "Completed: reports/renderer_benchmark.csv / 완료: CSV 저장");
        return;
    }
    m_new_phase = true;
}
void BenchmarkRunner::finish(ExperimentControls& c, const char* message)
{
    c.lab.running = false;
    c.lab.options = m_saved;
    c.lab.result_ready = true;
    strcpy_s(c.lab.status, message);
    try
    {
        save(c);
    }
    catch (const exception& e)
    {
        strncpy_s(c.lab.status, e.what(), _TRUNCATE);
    }
}
void BenchmarkRunner::save(const ExperimentControls& c)
{
    filesystem::create_directories("reports");
    ofstream file("reports/renderer_benchmark.csv");
    file.exceptions(ios::badbit | ios::failbit);
    file << "test,camera,seed,speed,capacity,warmup,frames,layout,backend,instances,repeat,samples,cpu_mean_"
            "ms,cpu_p95_ms,cpu_max_ms,gpu_mean_ms,idle_ms,release_ms,module_ms,create_ms,first_frame_ms,"
            "switch_total_ms,active,rejected\n"
         << fixed << setprecision(6);
    for (unsigned i = 0; i < c.lab.result_count; ++i)
    {
        const auto& r = c.lab.results[i];
        file << int(m_saved.test) << ',' << int(m_saved.camera) << ',' << m_saved.seed << ',' << m_saved.speed
             << ',' << m_saved.capacity << ',' << m_saved.warmup << ',' << m_saved.frames << ',' << r.layout
             << ',' << r.backend << ',' << r.count << ',' << r.repeat << ',' << r.samples << ',' << r.cpu_mean
             << ',' << r.cpu_p95 << ',' << r.cpu_max << ',' << r.gpu_mean << ',' << r.idle_mean << ','
             << r.release_mean << ',' << r.module_mean << ',' << r.create_mean << ',' << r.first_frame_mean
             << ',' << r.switch_mean << ',' << r.active << ',' << r.failures << '\n';
    }
}
} // namespace Lab
