#include "LabDebugPanel.h"
#include <imgui_internal.h>
#include <algorithm>
namespace Lab
{
void LabDebugPanel::render(ExperimentControls& c, ImTextureID scene)
{
    auto& lab = c.lab;
    auto& o = lab.options;
    auto text = [&](const char* en, const char* ko) { return lab.korean ? ko : en; };
    const auto dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    if (auto node = ImGui::DockBuilderGetNode(dock); node && !node->IsSplitNode())
    {
        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->WorkSize);
        auto center = dock;
        auto right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.34f, nullptr, &center);
        auto bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.26f, nullptr, &center);
        ImGui::DockBuilderDockWindow("Scene###LabScene", center);
        ImGui::DockBuilderDockWindow("Controls###LabControls", right);
        ImGui::DockBuilderDockWindow("Performance###LabPerformance", bottom);
        ImGui::DockBuilderFinish(dock);
    }
    ImGui::Begin(text("Controls###LabControls", "실험 설정###LabControls"));
    if (ImGui::Button(lab.korean ? "English" : "한국어"))
        lab.korean = !lab.korean;
    ImGui::BeginDisabled(lab.running);
    bool separate = c.requestedDllLayout == RendererDllLayout::Separate;
    if (ImGui::Checkbox(text("Separate DLLs", "분리 DLL"), &separate))
        c.requestedDllLayout = separate ? RendererDllLayout::Separate : RendererDllLayout::Combined;
    bool dx12 = c.requestedBackend == GraphicsBackend::DirectX12;
    if (ImGui::Checkbox("DirectX 12", &dx12))
        c.requestedBackend = dx12 ? GraphicsBackend::DirectX12 : GraphicsBackend::DirectX11;
    int textures = int(c.textureObjectCount);
    if (ImGui::SliderInt(text("GPU texture resources", "GPU 텍스처 리소스"), &textures, 1, 1000))
        c.textureObjectCount = unsigned(textures);
    ImGui::SliderInt(text("Effects", "이펙트 수"), &o.instances, 1, 16384);
    int camera = int(o.camera);
    if (ImGui::Combo(text("Camera", "카메라"), &camera,
                     text("Fixed\0Orbit\0Path\0", "고정\0공전\0경로 이동\0")))
        o.camera = CameraMode(camera);
    ImGui::SliderFloat(text("Motion speed", "이동 속도"), &o.speed, 0.1f, 3);
    ImGui::InputText(text("Preset JSON", "프리셋 JSON"), lab.preset_path, sizeof(lab.preset_path));
    if (ImGui::Button(text("Load JSON", "JSON 불러오기")))
        lab.import_requested = true;
    ImGui::SameLine();
    if (ImGui::Button(text("Reset effects", "이펙트 초기화")))
        lab.reset_requested = true;
    ImGui::TextWrapped(
        "%s",
        text("Point/Quad motion only. Game textures, sprites, Mesh and Trail are not emulated.",
             "Point/Quad 이동 설정 지원. 게임 텍스처, 스프라이트, Mesh/Trail은 별도 이식이 필요합니다."));
    if (ImGui::Button(text("Benchmark settings", "벤치마크 설정"), {-1, 0}))
        m_open_settings = true;
    ImGui::EndDisabled();
    if (lab.running)
    {
        ImGui::Text("%s %u / %u", text("Phase", "단계"), lab.phase + 1, lab.phase_count);
        if (ImGui::Button(text("Cancel", "취소")))
            lab.cancel_requested = true;
    }
    else if (lab.result_count && ImGui::Button(text("Show last results", "마지막 결과 보기")))
        lab.result_ready = true;
    ImGui::TextWrapped("%s", lab.status);
    ImGui::End();
    ImGui::Begin(text("Scene###LabScene", "장면###LabScene"));
    auto available = ImGui::GetContentRegionAvail();
    float width = std::max(1.0f, std::min(available.x, available.y * 16.0f / 9.0f));
    ImGui::Image(scene, {width, width * 9.0f / 16.0f});
    ImGui::End();
    ImGui::Begin(text("Performance###LabPerformance", "실시간 성능###LabPerformance"));
    ImGui::Text("%s: %u / %d   %s: %u", text("Pool active", "풀 활성"), lab.active, o.capacity,
                text("Rejected spawns", "생성 거절"), lab.failures);
    ImGui::Text("%s: %.3f ms", text("CPU scene update + assembly", "CPU 장면 갱신 + 정점 구성"),
                lab.scene_cpu_ms);
    ImGui::Text("%s: %.3f ms", text("GPU scene (UI excluded)", "GPU 장면 (UI 제외)"), lab.gpu_ms);
    ImGui::Text("%s: %.3f ms / %.3f ms", text("Last switch: combined / separate", "최근 전환: 통합 / 분리"),
                c.combinedTiming.lastTotalMs, c.separateTiming.lastTotalMs);
    ImGui::TextWrapped("%s", text("Frame CPU results include Present and GPU query/fence waits; GPU results "
                                  "cover only the fixed 960x540 scene.",
                                  "프레임 CPU 결과는 Present와 GPU 쿼리/Fence 대기를 포함합니다. GPU 결과는 "
                                  "고정 960x540 장면만 측정합니다."));
    ImGui::End();
    if (m_open_settings)
    {
        ImGui::OpenPopup("Settings###LabSetup");
        m_open_settings = false;
    }
    if (ImGui::BeginPopupModal(text("Settings###LabSetup", "벤치마크 설정###LabSetup"), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        int test = int(o.test), cam = int(o.camera);
        if (ImGui::Combo(text("Test", "테스트"), &test,
                         text("Current scene\0Effect count sweep\0Pool churn\0DLL/backend switch\0",
                              "현재 장면\0이펙트 수 단계 증가\0풀 생성/회수 반복\0DLL/백엔드 교체\0")))
            o.test = TestMode(test);
        if (ImGui::Combo(text("Camera path", "카메라 경로"), &cam,
                         text("Fixed\0Orbit\0Path\0", "고정\0공전\0경로 이동\0")))
            o.camera = CameraMode(cam);
        ImGui::SliderInt(text("Maximum effects", "최대 이펙트"), &o.instances, 1, 16384);
        ImGui::SliderInt(text("Pool capacity", "풀 용량"), &o.capacity, 1, 16384);
        ImGui::SliderInt(text("Warmup frames", "워밍업 프레임"), &o.warmup, 1, 600);
        ImGui::SliderInt(text("Measured frames", "측정 프레임"), &o.frames, 1, 1800);
        ImGui::SliderInt(text("Repeats", "반복 횟수"), &o.repeats, 1, 5);
        ImGui::InputInt(text("Random seed", "랜덤 시드"), &o.seed);
        ImGui::TextWrapped(
            "%s", text("1 simulation frame = 1/60 second. Each phase restarts the seed, camera and pool. "
                       "Current scene tests the active backend; other tests compare all four combinations.",
                       "시뮬레이션 1프레임 = 1/60초. 각 단계는 시드, 카메라, 풀을 초기화합니다. 현재 장면 외 "
                       "테스트는 통합/분리와 DX11/DX12 네 조합을 비교합니다."));
        if (ImGui::Button(text("Start", "시작")))
        {
            lab.start_requested = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(text("Close", "닫기")))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (lab.result_ready)
    {
        ImGui::OpenPopup("Results###LabResults");
        lab.result_ready = false;
    }
    ImGui::SetNextWindowSize({1000, 460}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(text("Results###LabResults", "벤치마크 결과###LabResults"), nullptr))
    {
        ImGui::TextWrapped("%s", lab.status);
        if (ImGui::BeginTable("ResultsTable", 9,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                              {-1, 300}))
        {
            for (const char* label :
                 {text("Layout", "DLL"), "DX", text("Count", "개수"), text("Run", "반복"), "CPU avg ms",
                  "CPU p95 ms", "CPU max ms", "GPU avg ms", text("Pool rejects", "풀 거절")})
                ImGui::TableSetupColumn(label);
            ImGui::TableHeadersRow();
            for (unsigned i = 0; i < lab.result_count; ++i)
            {
                const auto& r = lab.results[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(r.layout ? "Separate" : "Combined");
                ImGui::TableNextColumn();
                ImGui::Text("%d", r.backend);
                ImGui::TableNextColumn();
                ImGui::Text("%d", r.count);
                ImGui::TableNextColumn();
                ImGui::Text("%d", r.repeat + 1);
                for (double v : {r.cpu_mean, r.cpu_p95, r.cpu_max, r.gpu_mean})
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", v);
                }
                ImGui::TableNextColumn();
                ImGui::Text("%u", r.failures);
            }
            ImGui::EndTable();
        }
        if (ImGui::CollapsingHeader(text("Switch stage breakdown (ms)", "교체 단계별 시간 (ms)"),
                                    ImGuiTreeNodeFlags_DefaultOpen) &&
            ImGui::BeginTable("SwitchStages", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            for (const char* label :
                 {text("Layout", "DLL"), "DX", text("GPU idle", "GPU 완료 대기"),
                  text("Release", "기존 객체 해제"), text("Module", "DLL 교체"), text("Create", "재생성"),
                  text("First frame", "첫 프레임"), text("Total", "합계")})
                ImGui::TableSetupColumn(label);
            ImGui::TableHeadersRow();
            for (unsigned i = 0; i < lab.result_count; ++i)
            {
                const auto& r = lab.results[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(r.layout ? "Separate" : "Combined");
                ImGui::TableNextColumn();
                ImGui::Text("%d", r.backend);
                for (double v : {r.idle_mean, r.release_mean, r.module_mean, r.create_mean,
                                 r.first_frame_mean, r.switch_mean})
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", v);
                }
            }
            ImGui::EndTable();
        }
        if (ImGui::Button(text("Close", "닫기")))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
} // namespace Lab
