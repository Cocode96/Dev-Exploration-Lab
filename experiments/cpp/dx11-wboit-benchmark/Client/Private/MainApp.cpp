#include "MainApp.h"
#include "Client_Constant.h"
#include "Engine_Function.h"

#include "ApplicationContext.h"
#include "BenchmarkManager.h"
#include "CameraManager.h"
#include "DebugUiManager.h"
#include "EffectStressScene.h"
#include "FreeCamera.h"
#include "InputManager.h"
#include "Renderer.h"
#include "SceneManager.h"
#include "ValidationScene.h"

#include <algorithm>
#include <array>
#include <sstream>

namespace Client
{
using namespace std;

unique_ptr<MainApp> MainApp::create(HINSTANCE instance, int show_command)
{
    unique_ptr<MainApp> application(new MainApp());
    if (!application->initialize(instance, show_command))
        return nullptr;
    return application;
}

MainApp::~MainApp() = default;

bool MainApp::initialize(HINSTANCE instance, int show_command)
{
    if (!create_main_window(instance, show_command))
        return false;

    wchar_t executable_path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable_path, MAX_PATH);
    const filesystem::path executable_directory =
        filesystem::path(executable_path).parent_path();
    const filesystem::path project_directory =
        executable_directory.parent_path().parent_path();
    const auto shader_path = executable_directory / L"Shader_Transparency.hlsl";
    m_output_directory = project_directory / L"reports" / L"local" / L"windowed";

    m_context = make_unique<Engine::ApplicationContext>();
    if (!m_context->initialize(m_window, window_width, window_height, shader_path, m_output_directory))
        return false;
    if (!m_context->scene_manager().register_scene(Engine::SceneType::Validation,
        make_unique<ValidationScene>(), *m_context))
        return false;
    if (!m_context->scene_manager().register_scene(Engine::SceneType::EffectStress,
        make_unique<EffectStressScene>(), *m_context))
        return false;
    m_context->scene_manager().change_scene(Engine::SceneType::Validation);
    m_context->debug_ui_manager().log(Engine::DebugLogLevel::Info,
        "ApplicationContext assembled Input, Camera, Scene, Benchmark, Renderer and Debug UI modules.");
    m_context->debug_ui_manager().log(Engine::DebugLogLevel::Info,
        "Select Particle Stress and Forced sorting failure for the clearest comparison.");
    m_previous_time = chrono::steady_clock::now();
    return true;
}

bool MainApp::create_main_window(HINSTANCE instance, int show_command)
{
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = window_class_name;
    if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    RECT client_rect{0, 0, static_cast<LONG>(window_width), static_cast<LONG>(window_height)};
    AdjustWindowRect(&client_rect, WS_OVERLAPPEDWINDOW, FALSE);
    m_window = CreateWindowExW(0, window_class_name, L"DX11 WBOIT Benchmark",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        client_rect.right - client_rect.left, client_rect.bottom - client_rect.top,
        nullptr, nullptr, instance, this);
    if (!m_window)
        return false;
    ShowWindow(m_window, show_command);
    UpdateWindow(m_window);
    return true;
}

int MainApp::run()
{
    MSG message{};
    while (message.message != WM_QUIT)
    {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            continue;
        }

        const auto now = chrono::steady_clock::now();
        const float delta_time = chrono::duration<float>(now - m_previous_time).count();
        m_previous_time = now;
        update(delta_time);
        render();
    }
    return static_cast<int>(message.wParam);
}

void MainApp::update(float delta_time)
{
    auto& input = m_context->input();
    auto& benchmark = m_context->benchmark_manager();
    auto& renderer = m_context->renderer();
    if (renderer.prepare_scene_view())
    {
        m_context->camera_manager().active_camera().set_aspect_ratio(
            static_cast<float>(renderer.scene_width()) /
            static_cast<float>(renderer.scene_height()));
    }
    if (!benchmark.is_running())
    {
        if (input.was_pressed(VK_F1))
        {
            m_context->scene_manager().change_scene(Engine::SceneType::Validation);
            m_context->debug_ui_manager().log(Engine::DebugLogLevel::Info,
                "Scene changed by F1: Crossing Geometry Validation.");
        }
        if (input.was_pressed(VK_F2))
        {
            m_context->scene_manager().change_scene(Engine::SceneType::EffectStress);
            m_context->debug_ui_manager().log(Engine::DebugLogLevel::Info,
                "Scene changed by F2: Particle Sorting Stress.");
        }
        if (input.was_pressed('1'))
        {
            m_context->render_settings().transparency_mode = Engine::TransparencyMode::UnsortedAlpha;
            m_context->debug_ui_manager().log(Engine::DebugLogLevel::Info,
                "Method selected by keyboard: Unsorted Alpha.");
        }
        if (input.was_pressed('2'))
        {
            m_context->render_settings().transparency_mode = Engine::TransparencyMode::ZSortedAlpha;
            m_context->debug_ui_manager().log(Engine::DebugLogLevel::Info,
                "Method selected by keyboard: CPU Z-Sorted Alpha.");
        }
        if (input.was_pressed('3'))
        {
            m_context->render_settings().transparency_mode = Engine::TransparencyMode::Wboit;
            m_context->debug_ui_manager().log(Engine::DebugLogLevel::Info,
                "Method selected by keyboard: Weighted Blended OIT.");
        }
        if (input.was_pressed('R'))
        {
            m_context->render_settings().reverse_submission_order =
                !m_context->render_settings().reverse_submission_order;
            m_context->debug_ui_manager().log(Engine::DebugLogLevel::Warning,
                "Submission order toggled by keyboard.");
        }
        if (input.was_pressed(VK_OEM_PLUS) || input.was_pressed(VK_ADD))
            adjust_instance_count(1);
        if (input.was_pressed(VK_OEM_MINUS) || input.was_pressed(VK_SUBTRACT))
            adjust_instance_count(-1);
        if (input.was_pressed('B'))
            benchmark.start(*m_context);
        if (input.was_pressed('P'))
        {
            const auto& scene = m_context->scene_manager().active_scene();
            wostringstream name;
            name << (m_context->scene_manager().active_scene_type() == Engine::SceneType::Validation
                ? L"validation_" : L"stress_")
                 << scene.instance_count() << L"_capture.bmp";
            m_context->renderer().request_capture(m_output_directory / name.str());
            m_context->debug_ui_manager().log(Engine::DebugLogLevel::Success,
                "Frame capture requested by keyboard.");
        }
    }

    auto& debug_ui = m_context->debug_ui_manager();
    debug_ui.begin_frame();
    debug_ui.render_workspace();
    debug_ui.render_scene_view(
        renderer.scene_texture_srv(), renderer.scene_width(), renderer.scene_height());
    renderer.request_scene_view_size(
        debug_ui.requested_scene_width(), debug_ui.requested_scene_height());
    m_debug_panel.render(*m_context, m_output_directory);
    debug_ui.render_log_panel();
    debug_ui.end_frame();

    benchmark.prepare_frame(*m_context);
    m_context->scene_manager().active_scene().update(*m_context, delta_time);
    if (m_debug_panel.camera_input_enabled()
        && (debug_ui.scene_view_hovered() || input.is_mouse_look_active()))
    {
        m_context->camera_manager().update(input, delta_time);
    }
    else
    {
        input.release_active_input();
    }
    input.end_frame();
}

void MainApp::render()
{
    const auto metrics = m_context->renderer().render_frame(
        m_context->scene_manager().active_scene().render_view(), m_context->render_settings());
    m_context->benchmark_manager().record_frame(*m_context, metrics);
}

void MainApp::adjust_instance_count(int direction)
{
    auto& scene = m_context->scene_manager().active_scene();
    const auto choose = [&](const auto& counts)
    {
        auto iterator = lower_bound(counts.begin(), counts.end(), scene.instance_count());
        ptrdiff_t index = iterator == counts.end() ? counts.size() - 1 : iterator - counts.begin();
        index = (max)(ptrdiff_t{0}, (min)(index + direction,
            static_cast<ptrdiff_t>(counts.size() - 1)));
        scene.set_instance_count(counts[static_cast<size_t>(index)]);
    };
    if (m_context->scene_manager().active_scene_type() == Engine::SceneType::Validation)
        choose(validation_counts);
    else
        choose(stress_counts);
    ostringstream message;
    message << "Instance count changed by keyboard: " << scene.instance_count() << '.';
    m_context->debug_ui_manager().log(Engine::DebugLogLevel::Info, message.str());
}

LRESULT MainApp::handle_window_message(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    bool ui_captured_input = false;
    if (m_context)
        ui_captured_input = m_context->debug_ui_manager().handle_window_message(
            window, message, w_param, l_param);
    const bool scene_view_input = m_context
        && (m_context->debug_ui_manager().scene_view_hovered()
            || m_context->input().is_mouse_look_active());
    const bool release_message = message == WM_KEYUP
        || message == WM_SYSKEYUP
        || message == WM_LBUTTONUP
        || message == WM_RBUTTONUP
        || message == WM_MBUTTONUP;
    const bool keyboard_message = message == WM_KEYDOWN
        || message == WM_SYSKEYDOWN
        || message == WM_KEYUP
        || message == WM_SYSKEYUP;
    if (m_context
        && (!ui_captured_input || release_message || keyboard_message || scene_view_input))
        m_context->input().handle_message(window, message, w_param, l_param);
    if (message == WM_KEYDOWN && w_param == VK_ESCAPE)
    {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    if (ui_captured_input)
        return 1;
    return DefWindowProcW(window, message, w_param, l_param);
}

LRESULT CALLBACK MainApp::window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    MainApp* application = reinterpret_cast<MainApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
        application = static_cast<MainApp*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application));
    }
    return application
        ? application->handle_window_message(window, message, w_param, l_param)
        : DefWindowProcW(window, message, w_param, l_param);
}
}
