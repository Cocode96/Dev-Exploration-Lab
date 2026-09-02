#include "DebugUiManager.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <algorithm>
#include <utility>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window, UINT message, WPARAM w_param, LPARAM l_param);

namespace
{
constexpr char workspace_window_name[] = "RuntimeDebugWorkspace";
constexpr char dockspace_name[] = "RuntimeDebugDockspace";
constexpr char experiment_window_name[] = "Experiment Controls";
constexpr char performance_window_name[] = "Live Performance";
constexpr char log_window_name[] = "Runtime Log";

ImVec4 log_color(Engine::DebugLogLevel level)
{
    switch (level)
    {
    case Engine::DebugLogLevel::Success: return {0.33f, 0.78f, 0.71f, 1.0f};
    case Engine::DebugLogLevel::Warning: return {0.96f, 0.72f, 0.20f, 1.0f};
    case Engine::DebugLogLevel::Error: return {0.96f, 0.35f, 0.38f, 1.0f};
    case Engine::DebugLogLevel::Info:
    default: return {0.72f, 0.78f, 0.84f, 1.0f};
    }
}

const char* log_label(Engine::DebugLogLevel level)
{
    switch (level)
    {
    case Engine::DebugLogLevel::Success: return "PASS";
    case Engine::DebugLogLevel::Warning: return "WARN";
    case Engine::DebugLogLevel::Error: return "ERROR";
    case Engine::DebugLogLevel::Info:
    default: return "INFO";
    }
}
}

namespace Engine
{
DebugUiManager::~DebugUiManager()
{
    shutdown();
}

bool DebugUiManager::initialize(
    HWND window,
    ID3D11Device* device,
    ID3D11DeviceContext* device_context)
{
    shutdown();
    if (window == nullptr || device == nullptr || device_context == nullptr)
        return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;
    if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f,
        nullptr, io.Fonts->GetGlyphRangesKorean()) == nullptr)
        io.Fonts->AddFontDefault();

    ImGui::StyleColorsDark();
    apply_debug_hell_theme();

    if (!ImGui_ImplWin32_Init(window))
    {
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplDX11_Init(device, device_context))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    m_window = window;
    m_start_time = std::chrono::steady_clock::now();
    m_initialized = true;
    log(DebugLogLevel::Success, "Runtime Debug UI initialized.");
    return true;
}

void DebugUiManager::shutdown()
{
    if (m_initialized)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    m_window = nullptr;
    m_initialized = false;
    m_frame_active = false;
    m_layout_initialized = false;
    m_controls_dock_id = 0;
    m_performance_dock_id = 0;
    m_log_dock_id = 0;
}

void DebugUiManager::begin_frame()
{
    if (!m_initialized || m_frame_active)
        return;
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    m_frame_active = true;
}

void DebugUiManager::render_workspace()
{
    if (!m_frame_active)
        return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin(workspace_window_name, nullptr, flags);
    ImGui::PopStyleVar(3);
    ImGui::DockSpace(ImGui::GetID(dockspace_name), {0.0f, 0.0f},
        ImGuiDockNodeFlags_PassthruCentralNode);
    build_default_layout();
    ImGui::End();
}

void DebugUiManager::render_log_panel()
{
    if (!m_frame_active)
        return;

    set_next_window_default_dock(DebugUiDockRegion::Log);
    const bool korean = m_language == DebugUiLanguage::Korean;
    ImGui::Begin(korean ? "런타임 로그###Runtime Log" : "Runtime Log###Runtime Log");
    ImGui::TextColored({0.95f, 0.97f, 0.99f, 1.0f},
        "%s", korean ? "런타임 로그" : "Runtime Log");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", korean ? "엔진 및 벤치마크 이벤트" : "engine and benchmark events");
    ImGui::SameLine(ImGui::GetWindowWidth() - 72.0f);
    if (ImGui::SmallButton(korean ? "지우기" : "Clear"))
        clear_log();
    ImGui::Separator();

    ImGui::BeginChild("RuntimeLogScroll", {0.0f, 0.0f}, false,
        ImGuiWindowFlags_HorizontalScrollbar);
    for (const DebugLogEntry& entry : m_log_entries)
    {
        ImGui::TextColored(log_color(entry.level), "[%7.2fs] %-5s",
            entry.elapsed_seconds, log_label(entry.level));
        ImGui::SameLine();
        ImGui::TextUnformatted(entry.message.c_str());
    }
    if (m_scroll_log_to_bottom)
    {
        ImGui::SetScrollHereY(1.0f);
        m_scroll_log_to_bottom = false;
    }
    ImGui::EndChild();
    ImGui::End();
}

void DebugUiManager::end_frame()
{
    if (!m_frame_active)
        return;
    ImGui::Render();
    m_frame_active = false;
}

void DebugUiManager::render_draw_data()
{
    if (!m_initialized || m_frame_active)
        return;
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void DebugUiManager::set_next_window_default_dock(DebugUiDockRegion region)
{
    if (!m_frame_active)
        return;

    ImGuiID dock_id{};
    switch (region)
    {
    case DebugUiDockRegion::Controls: dock_id = m_controls_dock_id; break;
    case DebugUiDockRegion::Performance: dock_id = m_performance_dock_id; break;
    case DebugUiDockRegion::Log: dock_id = m_log_dock_id; break;
    }
    if (dock_id != 0)
        ImGui::SetNextWindowDockID(dock_id, ImGuiCond_FirstUseEver);
}

bool DebugUiManager::handle_window_message(
    HWND window,
    UINT message,
    WPARAM w_param,
    LPARAM l_param)
{
    if (!m_initialized || window != m_window)
        return false;
    ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param);

    const ImGuiIO& io = ImGui::GetIO();
    switch (message)
    {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
        return io.WantCaptureKeyboard;
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        return io.WantCaptureMouse;
    default:
        return false;
    }
}

bool DebugUiManager::wants_keyboard_input() const
{
    return m_initialized && ImGui::GetIO().WantCaptureKeyboard;
}

bool DebugUiManager::wants_mouse_input() const
{
    return m_initialized && ImGui::GetIO().WantCaptureMouse;
}

void DebugUiManager::log(DebugLogLevel level, std::string message)
{
    if (m_log_entries.size() >= max_log_entries)
        m_log_entries.pop_front();
    const double elapsed_seconds = m_initialized
        ? std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start_time).count()
        : 0.0;
    m_log_entries.push_back({level, elapsed_seconds, std::move(message)});
    m_scroll_log_to_bottom = true;
}

void DebugUiManager::clear_log()
{
    m_log_entries.clear();
    m_scroll_log_to_bottom = false;
}

void DebugUiManager::toggle_language()
{
    m_language = m_language == DebugUiLanguage::English
        ? DebugUiLanguage::Korean
        : DebugUiLanguage::English;
    log(DebugLogLevel::Info,
        m_language == DebugUiLanguage::Korean
        ? "UI language changed: Korean."
        : "UI language changed: English.");
}

void DebugUiManager::apply_debug_hell_theme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = {10.0f, 8.0f};
    style.FramePadding = {7.0f, 4.0f};
    style.CellPadding = {8.0f, 4.0f};
    style.ItemSpacing = {8.0f, 6.0f};
    style.ItemInnerSpacing = {6.0f, 4.0f};
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;
    style.WindowRounding = 9.0f;
    style.ChildRounding = 7.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 7.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 10.0f;
    style.TabRounding = 7.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = {0.92f, 0.94f, 0.96f, 1.0f};
    colors[ImGuiCol_TextDisabled] = {0.47f, 0.52f, 0.58f, 1.0f};
    colors[ImGuiCol_WindowBg] = {0.08f, 0.10f, 0.13f, 0.96f};
    colors[ImGuiCol_ChildBg] = {0.10f, 0.12f, 0.16f, 0.92f};
    colors[ImGuiCol_PopupBg] = {0.08f, 0.10f, 0.14f, 0.98f};
    colors[ImGuiCol_Border] = {0.17f, 0.22f, 0.29f, 1.0f};
    colors[ImGuiCol_BorderShadow] = {0.0f, 0.0f, 0.0f, 0.0f};
    colors[ImGuiCol_FrameBg] = {0.12f, 0.15f, 0.20f, 1.0f};
    colors[ImGuiCol_FrameBgHovered] = {0.16f, 0.20f, 0.27f, 1.0f};
    colors[ImGuiCol_FrameBgActive] = {0.19f, 0.25f, 0.33f, 1.0f};
    colors[ImGuiCol_TitleBg] = {0.10f, 0.13f, 0.17f, 1.0f};
    colors[ImGuiCol_TitleBgActive] = {0.12f, 0.16f, 0.22f, 1.0f};
    colors[ImGuiCol_MenuBarBg] = {0.09f, 0.12f, 0.16f, 1.0f};
    colors[ImGuiCol_ScrollbarBg] = {0.08f, 0.10f, 0.13f, 1.0f};
    colors[ImGuiCol_ScrollbarGrab] = {0.23f, 0.31f, 0.39f, 1.0f};
    colors[ImGuiCol_ScrollbarGrabHovered] = {0.30f, 0.40f, 0.50f, 1.0f};
    colors[ImGuiCol_ScrollbarGrabActive] = {0.36f, 0.49f, 0.60f, 1.0f};
    colors[ImGuiCol_CheckMark] = {0.33f, 0.78f, 0.71f, 1.0f};
    colors[ImGuiCol_SliderGrab] = {0.33f, 0.78f, 0.71f, 1.0f};
    colors[ImGuiCol_SliderGrabActive] = {0.42f, 0.87f, 0.78f, 1.0f};
    colors[ImGuiCol_Button] = {0.14f, 0.20f, 0.26f, 1.0f};
    colors[ImGuiCol_ButtonHovered] = {0.19f, 0.29f, 0.38f, 1.0f};
    colors[ImGuiCol_ButtonActive] = {0.21f, 0.36f, 0.47f, 1.0f};
    colors[ImGuiCol_Header] = {0.14f, 0.20f, 0.26f, 1.0f};
    colors[ImGuiCol_HeaderHovered] = {0.19f, 0.29f, 0.38f, 1.0f};
    colors[ImGuiCol_HeaderActive] = {0.21f, 0.36f, 0.47f, 1.0f};
    colors[ImGuiCol_Separator] = {0.18f, 0.23f, 0.30f, 1.0f};
    colors[ImGuiCol_Tab] = {0.11f, 0.15f, 0.21f, 1.0f};
    colors[ImGuiCol_TabHovered] = {0.18f, 0.28f, 0.37f, 1.0f};
    colors[ImGuiCol_TabActive] = {0.17f, 0.28f, 0.37f, 1.0f};
    colors[ImGuiCol_TableHeaderBg] = {0.11f, 0.15f, 0.20f, 1.0f};
    colors[ImGuiCol_TableRowBgAlt] = {0.10f, 0.13f, 0.18f, 0.55f};
    colors[ImGuiCol_NavHighlight] = {0.35f, 0.76f, 0.70f, 1.0f};
}

void DebugUiManager::build_default_layout()
{
    if (m_layout_initialized)
        return;

    const ImGuiID dockspace_id = ImGui::GetID(dockspace_name);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

    ImGuiID center_id = dockspace_id;
    ImGuiID right_id = ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Right, 0.34f,
        nullptr, &center_id);
    ImGuiID log_id = ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Down, 0.28f,
        nullptr, &center_id);

    ImGui::DockBuilderDockWindow(experiment_window_name, right_id);
    ImGui::DockBuilderDockWindow(performance_window_name, log_id);
    ImGui::DockBuilderDockWindow(log_window_name, log_id);
    ImGui::DockBuilderFinish(dockspace_id);
    m_controls_dock_id = right_id;
    m_performance_dock_id = log_id;
    m_log_dock_id = log_id;
    m_layout_initialized = true;
}
}
