#include "BenchmarkDebugPanel.h"
#include "Client_Constant.h"
#include "Client_Function.h"
#include "Engine_DebugUi_Constant.h"

#include "ApplicationContext.h"
#include "BenchmarkManager.h"
#include "DebugUiManager.h"
#include "EffectStressScene.h"
#include "Engine_Struct.h"
#include "Renderer.h"
#include "SceneManager.h"

#include "imgui.h"

#include <Windows.h>

#include <array>
#include <cstdint>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace Client
{
using namespace std;

void BenchmarkDebugPanel::render(
    Engine::ApplicationContext& context,
    const filesystem::path& output_directory)
{
    const bool benchmark_running = context.benchmark_manager().is_running();
    if (benchmark_running && !m_previous_benchmark_running)
        context.debug_ui_manager().log(Engine::DebugLogLevel::Info,
            "Full benchmark started. UI cost is excluded from GPU timestamps.");
    else if (!benchmark_running && m_previous_benchmark_running)
    {
        m_open_benchmark_result = true;
        context.debug_ui_manager().log(Engine::DebugLogLevel::Success,
            "Full benchmark completed. CSV written to reports/local/windowed.");
    }
    m_previous_benchmark_running = benchmark_running;

    if (m_auto_flip_order && !benchmark_running)
    {
        m_auto_flip_elapsed += ImGui::GetIO().DeltaTime;
        if (m_auto_flip_elapsed >= 0.75f)
        {
            context.render_settings().reverse_submission_order =
                !context.render_settings().reverse_submission_order;
            m_auto_flip_elapsed = 0.0f;
        }
    }

    render_experiment_controls(context, output_directory);
    render_performance(context);
    render_benchmark_result(context);
}

void BenchmarkDebugPanel::render_experiment_controls(
    Engine::ApplicationContext& context,
    const filesystem::path& output_directory)
{
    auto& scene_manager = context.scene_manager();
    auto& benchmark = context.benchmark_manager();
    auto& settings = context.render_settings();
    auto& debug_ui = context.debug_ui_manager();
    const auto language = debug_ui.language();
    const bool locked = benchmark.is_running();

    debug_ui.set_next_window_default_dock(Engine::DebugUiDockRegion::Controls);
    ImGui::Begin(language == Engine::DebugUiLanguage::Korean
        ? "실험 설정###Experiment Controls"
        : "Experiment Controls###Experiment Controls");
    ImGui::TextColored({0.33f, 0.78f, 0.71f, 1.0f}, "DX11 Transparency Lab");
    ImGui::SameLine(ImGui::GetWindowWidth() - 88.0f);
    if (ImGui::SmallButton(language == Engine::DebugUiLanguage::English ? "한국어" : "English"))
        debug_ui.toggle_language();
    ImGui::TextWrapped("%s", ui_text(language,
        "Click through the methods and watch both the image and measured cost.",
        "각 기법을 클릭하며 화면 변화와 측정 비용을 함께 확인하세요."));
    if (ImGui::Button(m_camera_input_enabled
        ? ui_text(language, "Camera Input: Enabled", "카메라 입력: 활성")
        : ui_text(language, "Camera Input: Locked", "카메라 입력: 잠금"),
        {-1.0f, 0.0f}))
    {
        m_camera_input_enabled = !m_camera_input_enabled;
        debug_ui.log(Engine::DebugLogLevel::Info,
            m_camera_input_enabled ? "Camera input enabled." : "Camera input locked.");
    }
    ImGui::TextDisabled("%s", ui_text(language,
        "Camera input is also blocked automatically while ImGui captures input.",
        "ImGui가 입력을 사용하는 동안에는 카메라 입력도 자동 차단됩니다."));
    ImGui::Spacing();

    ImGui::BeginDisabled(locked);
    section_title(ui_text(language, "1. Scene", "1. 장면"),
        ui_text(language, "choose the failure case", "실패 사례 선택"));
    const float scene_button_width = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
    if (ImGui::Button(ui_text(language, "Crossing Geometry", "교차 지오메트리"),
        {scene_button_width, 0.0f}))
    {
        scene_manager.change_scene(Engine::SceneType::Validation);
        debug_ui.log(Engine::DebugLogLevel::Info, "Scene changed: Crossing Geometry Validation.");
    }
    ImGui::SameLine();
    if (ImGui::Button(ui_text(language, "Particle Stress", "파티클 스트레스"),
        {scene_button_width, 0.0f}))
    {
        scene_manager.change_scene(Engine::SceneType::EffectStress);
        debug_ui.log(Engine::DebugLogLevel::Info, "Scene changed: Particle Sorting Stress.");
    }
    ImGui::TextDisabled("%s: %s", ui_text(language, "Active", "현재 장면"),
        scene_name(scene_manager.active_scene_type(), language));

    if (scene_manager.active_scene_type() == Engine::SceneType::EffectStress)
    {
        auto* stress_scene = dynamic_cast<EffectStressScene*>(&scene_manager.active_scene());
        if (stress_scene != nullptr)
        {
            bool sorting_failure = stress_scene->pattern() == ParticleStressPattern::SortingFailure;
            if (ImGui::RadioButton(ui_text(language,
                "Forced sorting failure", "강제 정렬 실패"), sorting_failure))
            {
                stress_scene->set_pattern(ParticleStressPattern::SortingFailure);
                debug_ui.log(Engine::DebugLogLevel::Warning,
                    "Particle preset: overlapping sheets with near-equal centers.");
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(ui_text(language,
                "Natural volume", "자연스러운 볼륨"), !sorting_failure))
            {
                stress_scene->set_pattern(ParticleStressPattern::NaturalVolume);
                debug_ui.log(Engine::DebugLogLevel::Info,
                    "Particle preset: deterministic natural volume.");
            }
            ImGui::TextWrapped("%s", ui_text(language,
                "Forced mode creates intersecting ribbons and two overlapping particle sheets. A single object-center sort cannot define a correct order for every pixel.",
                "강제 모드는 서로 관통하는 리본과 겹친 두 파티클 층을 만듭니다. 하나의 오브젝트 중심점 정렬로는 모든 픽셀의 올바른 순서를 정할 수 없습니다."));
        }
    }

    ImGui::Spacing();
    section_title(ui_text(language, "2. Transparency Method", "2. 투명도 처리 방식"),
        ui_text(language, "full names, no mystery modes", "숫자 대신 전체 이름 표시"));
    if (method_card("UnsortedAlpha", method_name(Engine::TransparencyMode::UnsortedAlpha, language),
        ui_text(language, "Lowest CPU work, but the image depends on submission order.",
            "CPU 작업은 가장 적지만 화면 결과가 제출 순서에 따라 달라집니다."),
        settings.transparency_mode == Engine::TransparencyMode::UnsortedAlpha,
        {0.96f, 0.56f, 0.25f, 1.0f}, language))
    {
        settings.transparency_mode = Engine::TransparencyMode::UnsortedAlpha;
        debug_ui.log(Engine::DebugLogLevel::Info, "Method selected: Unsorted Alpha.");
    }
    if (method_card("ZSortedAlpha", method_name(Engine::TransparencyMode::ZSortedAlpha, language),
        ui_text(language, "Back-to-front object-center sort. Adds CPU cost and still fails on intersections.",
            "오브젝트 중심을 뒤에서 앞으로 정렬합니다. CPU 비용이 생기며 교차 지오메트리에는 여전히 실패합니다."),
        settings.transparency_mode == Engine::TransparencyMode::ZSortedAlpha,
        {0.38f, 0.68f, 0.96f, 1.0f}, language))
    {
        settings.transparency_mode = Engine::TransparencyMode::ZSortedAlpha;
        debug_ui.log(Engine::DebugLogLevel::Info, "Method selected: CPU Z-Sorted Alpha.");
    }
    if (method_card("Wboit", method_name(Engine::TransparencyMode::Wboit, language),
        ui_text(language, "No CPU sort. Extra accumulation targets and resolve pass increase GPU cost.",
            "CPU 정렬은 없습니다. 추가 누적 렌더 타깃과 합성 패스로 GPU 비용이 증가합니다."),
        settings.transparency_mode == Engine::TransparencyMode::Wboit,
        {0.33f, 0.78f, 0.71f, 1.0f}, language))
    {
        settings.transparency_mode = Engine::TransparencyMode::Wboit;
        debug_ui.log(Engine::DebugLogLevel::Info, "Method selected: Weighted Blended OIT.");
    }

    section_title(ui_text(language, "3. Stress Controls", "3. 스트레스 설정"),
        ui_text(language, "make order errors visible", "정렬 오류를 눈에 띄게 만들기"));
    const span<const uint32_t> counts =
        scene_manager.active_scene_type() == Engine::SceneType::Validation
        ? span<const uint32_t>(validation_counts)
        : span<const uint32_t>(stress_counts);
    int selected_count = 0;
    for (size_t index = 0; index < counts.size(); ++index)
    {
        if (counts[index] == scene_manager.active_scene().instance_count())
            selected_count = static_cast<int>(index);
    }
    const char* count_labels_validation[] = {"2", "4", "8", "16", "32"};
    const char* count_labels_stress[] = {"64", "256", "1024", "4096"};
    const char* const* count_labels = scene_manager.active_scene_type() == Engine::SceneType::Validation
        ? count_labels_validation : count_labels_stress;
    if (ImGui::Combo(ui_text(language, "Instances", "인스턴스 수"), &selected_count, count_labels,
        static_cast<int>(counts.size())))
    {
        scene_manager.active_scene().set_instance_count(counts[static_cast<size_t>(selected_count)]);
        ostringstream message;
        message << "Instance count changed: " << counts[static_cast<size_t>(selected_count)] << '.';
        debug_ui.log(Engine::DebugLogLevel::Info, message.str());
    }

    bool reverse_order = settings.reverse_submission_order;
    if (ImGui::Checkbox(ui_text(language,
        "Reverse submission order", "제출 순서 뒤집기"), &reverse_order))
    {
        settings.reverse_submission_order = reverse_order;
        debug_ui.log(Engine::DebugLogLevel::Warning,
            reverse_order ? "Submission order reversed." : "Submission order restored to forward.");
    }
    if (ImGui::Checkbox(ui_text(language,
        "Auto-flip order every 0.75 s", "0.75초마다 제출 순서 자동 반전"), &m_auto_flip_order))
    {
        m_auto_flip_elapsed = 0.0f;
        debug_ui.log(Engine::DebugLogLevel::Info,
            m_auto_flip_order ? "Automatic order flip enabled." : "Automatic order flip disabled.");
    }
    ImGui::TextWrapped("%s", ui_text(language,
        "Quick check: enable Forced sorting failure and Auto-flip. Unsorted Alpha should change visibly. Z-Sorted Alpha improves many particles but cannot solve intersecting geometry. WBOIT should remain nearly order-stable.",
        "빠른 확인: 강제 정렬 실패와 자동 반전을 켜세요. 미정렬 알파는 화면이 눈에 띄게 변합니다. Z 정렬은 일반 파티클을 개선하지만 교차 지오메트리를 해결하지 못합니다. WBOIT는 제출 순서가 바뀌어도 거의 안정적으로 유지됩니다."));
    ImGui::EndDisabled();

    ImGui::Spacing();
    section_title(ui_text(language, "4. Evidence", "4. 증거 만들기"),
        ui_text(language, "capture or measure", "화면 캡처 또는 전체 측정"));
    if (locked)
    {
        ImGui::TextColored({0.96f, 0.72f, 0.20f, 1.0f}, "%s",
            to_utf8(benchmark.status_text()).c_str());
    }
    else if (ImGui::Button(ui_text(language,
        "Run Full Benchmark", "전체 벤치마크 실행"), {-1.0f, 0.0f}))
    {
        m_auto_flip_order = false;
        benchmark.start(context);
    }

    if (!locked && ImGui::Button(ui_text(language,
        "Capture Current Frame", "현재 프레임 캡처"), {-1.0f, 0.0f}))
    {
        const auto& active_scene = scene_manager.active_scene();
        wostringstream file_name;
        file_name << (scene_manager.active_scene_type() == Engine::SceneType::Validation
            ? L"validation_" : L"stress_")
                  << active_scene.instance_count() << L'_'
                  << method_file_name(settings.transparency_mode)
                  << (settings.reverse_submission_order ? L"_reverse" : L"_forward")
                  << L"_scene.bmp";
        wstring safe_name = file_name.str();
        for (wchar_t& character : safe_name)
        {
            if (character == L' ')
                character = L'_';
        }
        context.renderer().request_capture(output_directory / safe_name);
        debug_ui.log(Engine::DebugLogLevel::Success,
            "Scene-only frame capture requested. Runtime UI is excluded.");
    }
    ImGui::End();
}

void BenchmarkDebugPanel::render_performance(Engine::ApplicationContext& context)
{
    const auto& metrics = context.renderer().last_metrics();
    const auto& scene = context.scene_manager().active_scene();
    const auto mode = context.render_settings().transparency_mode;
    const auto language = context.debug_ui_manager().language();
    const double measured_cpu_ms = metrics.cpu_submit_ms + metrics.cpu_sort_ms;

    context.debug_ui_manager().set_next_window_default_dock(Engine::DebugUiDockRegion::Performance);
    ImGui::Begin(language == Engine::DebugUiLanguage::Korean
        ? "실시간 성능###Live Performance"
        : "Live Performance###Live Performance");
    section_title(ui_text(language, "Live Performance", "실시간 성능"),
        ui_text(language, "previous completed frame", "직전 완료 프레임"));
    ImGui::Text("%s: %s", ui_text(language, "Adapter", "그래픽 어댑터"),
        to_utf8(context.renderer().adapter_name()).c_str());
    ImGui::Text("%s: %s", ui_text(language, "Scene", "장면"),
        scene_name(context.scene_manager().active_scene_type(), language));
    ImGui::Text("%s: %s", ui_text(language, "Method", "처리 방식"),
        method_name(mode, language));
    ImGui::Text("%s: %u", ui_text(language, "Instances", "인스턴스 수"), scene.instance_count());
    ImGui::Text("%s: %u x %u", ui_text(language, "Scene View", "장면 화면"),
        context.renderer().scene_width(), context.renderer().scene_height());
    ImGui::Separator();

    if (ImGui::BeginTable("PerformanceTable", 2,
        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        const auto row = [](const char* label, double value)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.4f ms", value);
        };
        row(ui_text(language, "CPU draw submission", "CPU 드로우 제출"), metrics.cpu_submit_ms);
        row(ui_text(language, "CPU Z sort", "CPU Z 정렬"), metrics.cpu_sort_ms);
        row(ui_text(language, "CPU measured sum", "CPU 측정 합계"), measured_cpu_ms);
        row(ui_text(language, "GPU transparency", "GPU 투명도 패스"), metrics.gpu_transparency_ms);
        row(ui_text(language, "GPU WBOIT resolve", "GPU WBOIT 합성"), metrics.gpu_resolve_ms);
        row(ui_text(language, "GPU measured total", "GPU 측정 합계"), metrics.gpu_total_ms);
        ImGui::EndTable();
    }
    ImGui::Text(ui_text(language,
        "Draw calls: %u total, %u transparent",
        "드로우 콜: 전체 %u, 투명도 %u"),
        metrics.total_draw_calls, metrics.transparency_draw_calls);
    ImGui::Spacing();

    if (mode == Engine::TransparencyMode::Wboit)
    {
        ImGui::TextColored({0.33f, 0.78f, 0.71f, 1.0f}, "%s",
            ui_text(language, "CPU/GPU tradeoff", "CPU/GPU 트레이드오프"));
        ImGui::TextWrapped("%s", ui_text(language,
            "CPU object sorting is removed. GPU cost rises because transparency writes two accumulation targets and runs a fullscreen resolve pass.",
            "CPU 오브젝트 정렬은 사라집니다. 투명도를 두 누적 렌더 타깃에 기록하고 전체 화면 합성 패스를 실행하므로 GPU 비용은 증가합니다."));
    }
    else if (mode == Engine::TransparencyMode::ZSortedAlpha)
    {
        ImGui::TextColored({0.38f, 0.68f, 0.96f, 1.0f}, "%s",
            ui_text(language, "CPU/GPU tradeoff", "CPU/GPU 트레이드오프"));
        ImGui::TextWrapped("%s", ui_text(language,
            "The GPU path is simple alpha blending, but the CPU performs a stable object-center sort every frame.",
            "GPU 경로는 단순한 알파 블렌딩이지만 CPU가 매 프레임 안정적인 오브젝트 중심점 정렬을 수행합니다."));
    }
    else
    {
        ImGui::TextColored({0.96f, 0.56f, 0.25f, 1.0f}, "%s",
            ui_text(language, "CPU/GPU tradeoff", "CPU/GPU 트레이드오프"));
        ImGui::TextWrapped("%s", ui_text(language,
            "This is the cheapest baseline, but transparent color depends directly on submission order.",
            "가장 저렴한 기준 경로지만 투명 색상이 제출 순서에 직접 의존합니다."));
    }

    ImGui::Separator();
    ImGui::TextDisabled("%s", ui_text(language, "Measurement boundary", "측정 범위"));
    ImGui::TextWrapped("%s", ui_text(language,
        "GPU timestamps cover sky, terrain and transparency. ImGui and Present are excluded so the debug tool does not contaminate the benchmark result.",
        "GPU 타임스탬프는 하늘, 지형과 투명도 렌더링을 포함합니다. 디버그 도구가 결과를 오염시키지 않도록 ImGui와 Present는 측정에서 제외합니다."));
    ImGui::End();
}

void BenchmarkDebugPanel::render_benchmark_result(Engine::ApplicationContext& context)
{
    const auto language = context.debug_ui_manager().language();
    if (m_open_benchmark_result)
    {
        ImGui::OpenPopup(ui_text(language, "Benchmark Result", "벤치마크 결과"));
        m_open_benchmark_result = false;
    }

    ImGui::SetNextWindowSize({680.0f, 0.0f}, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(ui_text(language, "Benchmark Result", "벤치마크 결과"),
        nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextColored({0.33f, 0.78f, 0.71f, 1.0f}, "%s",
        ui_text(language, "Full benchmark completed", "전체 벤치마크 완료"));
    ImGui::TextWrapped("%s", ui_text(language,
        "Comparison sample: Particle Stress, 1024 instances. GPU timestamps exclude ImGui and Present.",
        "비교 샘플: 파티클 스트레스, 1024개 인스턴스. GPU 타임스탬프에는 ImGui와 Present가 포함되지 않습니다."));
    ImGui::Separator();

    const auto summaries = context.benchmark_manager().result_summaries();
    if (ImGui::BeginTable("BenchmarkResultTable", 5,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(ui_text(language, "Method", "처리 방식"));
        ImGui::TableSetupColumn(ui_text(language, "GPU total (ms)", "GPU 전체 (ms)"));
        ImGui::TableSetupColumn(ui_text(language, "GPU p95 (ms)", "GPU p95 (ms)"));
        ImGui::TableSetupColumn(ui_text(language, "CPU Z sort (ms)", "CPU Z 정렬 (ms)"));
        ImGui::TableSetupColumn(ui_text(language, "Draw calls", "드로우 콜"));
        ImGui::TableHeadersRow();

        for (const auto& summary : summaries)
        {
            if (summary.scene != Engine::SceneType::EffectStress || summary.instance_count != 1024)
                continue;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(method_name(summary.mode, language));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.4f", summary.gpu_total_mean_ms);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.4f", summary.gpu_total_p95_ms);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.4f", summary.cpu_sort_mean_ms);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%u / %u", summary.total_draw_calls, summary.transparency_draw_calls);
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("%s", ui_text(language,
        "Read the CSV for every scene and instance count.",
        "전체 장면과 인스턴스 수별 결과는 CSV에서 확인하세요."));
    if (ImGui::Button(ui_text(language, "Close", "닫기")))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
}
