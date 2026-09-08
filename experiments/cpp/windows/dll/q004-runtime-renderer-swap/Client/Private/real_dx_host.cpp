#include "real_dx_api.h"
#include "EffectManager.h"
#include "Effect_Function.h"
#include "BenchmarkRunner.h"
#include <filesystem>

#include <Windows.h>

#include <memory>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <vector>

using namespace std;
namespace
{
HMODULE renderer_module{};
ProbeBackendFn probe_backend{};
CreateRealRendererFn create_renderer{};
DestroyRealRendererFn destroy_renderer{};
unique_ptr<IRealRenderer, DestroyRealRendererFn> renderer{nullptr, nullptr};
HWND main_window{};
ExperimentControls controls{};
wstring loaded_module_name;

Lab::EffectManager effects;
Lab::BenchmarkRunner benchmark_runner;
bool command_benchmark{};
int exit_code{};
string initialization_errors;

unsigned ReadUnsignedOption(const wchar_t* command_line, const wchar_t* option, unsigned fallback)
{
    const wchar_t* found = wcsstr(command_line, option);
    if (found == nullptr)
        return fallback;

    found += wcslen(option);
    const unsigned long value = wcstoul(found, nullptr, 10);

    return value == 0 ? fallback : static_cast<unsigned>(value);
}

const wchar_t* ModuleName(RendererDllLayout layout, GraphicsBackend backend)
{
    if (layout == RendererDllLayout::Combined)
        return L"RealDxCombinedRenderer.dll";
    return backend == GraphicsBackend::DirectX12 ? L"RealDx12Renderer.dll" : L"RealDx11Renderer.dll";
}

bool LoadRendererModule(RendererDllLayout layout, GraphicsBackend backend)
{
    const wchar_t* requested = ModuleName(layout, backend);
    if (renderer_module && loaded_module_name == requested)
        return true;

    renderer.reset();
    if (renderer_module)
        FreeLibrary(renderer_module);
    renderer_module = LoadLibraryW(requested);
    if (!renderer_module)
    {
        initialization_errors += "LoadLibraryW 오류 코드: " + to_string(GetLastError()) + "\n";
        return false;
    }

    probe_backend = reinterpret_cast<ProbeBackendFn>(GetProcAddress(renderer_module, "ProbeBackend"));
    create_renderer =
        reinterpret_cast<CreateRealRendererFn>(GetProcAddress(renderer_module, "CreateRealRenderer"));
    destroy_renderer =
        reinterpret_cast<DestroyRealRendererFn>(GetProcAddress(renderer_module, "DestroyRealRenderer"));
    if (!probe_backend || !create_renderer || !destroy_renderer)
    {
        FreeLibrary(renderer_module);
        renderer_module = nullptr;
        loaded_module_name.clear();
        return false;
    }
    renderer = decltype(renderer){nullptr, destroy_renderer};
    loaded_module_name = requested;
    return true;
}

bool SwitchRenderer(RendererDllLayout layout, GraphicsBackend backend)
{
    using Clock = chrono::steady_clock;
    auto elapsed = [](auto a, auto b) { return chrono::duration<double, milli>(b - a).count(); };
    auto& stage = controls.lab.last_switch;
    stage = {};
    auto t = Clock::now();
    // Host는 프레임 사이에만 교체한다. GPU 완료 후 DLL 소유 객체를 파괴한다.
    if (renderer && !renderer->WaitForIdle())
    {
        initialization_errors = "GPU idle timeout";
        return false;
    }
    auto end = Clock::now();
    stage.idle = elapsed(t, end);
    t = end;
    renderer.reset();
    end = Clock::now();
    stage.release = elapsed(t, end);
    t = end;
    if (!LoadRendererModule(layout, backend))
    {
        initialization_errors = "Module unavailable";
        return false;
    }
    end = Clock::now();
    stage.module = elapsed(t, end);
    t = end;
    auto* candidate = create_renderer(backend);
    if (!candidate)
        return false;
    if (!candidate->Initialize(main_window, 1280, 720, L"Texture", &controls))
    {
        destroy_renderer(candidate);
        initialization_errors = "Renderer initialization failed";
        return false;
    }
    renderer = decltype(renderer){candidate, destroy_renderer};
    end = Clock::now();
    stage.create = elapsed(t, end);
    controls.activeBackend = controls.requestedBackend = backend;
    controls.activeDllLayout = controls.requestedDllLayout = layout;
    controls.appliedTextureObjectCount = controls.textureObjectCount;
    auto& timing = layout == RendererDllLayout::Combined ? controls.combinedTiming : controls.separateTiming;
    timing.lastTotalMs = stage.idle + stage.release + stage.module + stage.create;
    timing.lastModuleMs = stage.module;
    timing.lastRendererMs = stage.create;
    timing.accumulatedTotalMs += timing.lastTotalMs;
    ++timing.samples;
    timing.averageTotalMs = timing.accumulatedTotalMs / timing.samples;
    timing.minimumTotalMs =
        timing.samples == 1 ? timing.lastTotalMs : min(timing.minimumTotalMs, timing.lastTotalMs);
    timing.maximumTotalMs = max(timing.maximumTotalMs, timing.lastTotalMs);
    wstring title = L"Renderer Lab | ";
    title += renderer->Name();
    title += layout == RendererDllLayout::Combined ? L" | Combined" : L" | Separate";
    SetWindowTextW(main_window, title.c_str());
    return true;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_KEYDOWN && wparam == VK_ESCAPE)
    {
        DestroyWindow(window);
        return 0;
    }
    if (renderer && renderer->HandleWindowMessage(window, message, wparam, lparam))
        return 1;
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int show_command)
{
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    filesystem::current_path(filesystem::path(executable).parent_path());
    command_benchmark = wcsstr(command_line, L"--benchmark") != nullptr;
    controls.lab.options.frames = int(ReadUnsignedOption(command_line, L"--iterations=", 12));
    controls.lab.options.warmup = int(ReadUnsignedOption(command_line, L"--warmup=", 2));
    controls.lab.options.instances = int(ReadUnsignedOption(command_line, L"--effects=", 256));
    controls.textureObjectCount = min(ReadUnsignedOption(command_line, L"--textures=", 10), 1000u);
    if (const auto* option = wcsstr(command_line, L"--test="))
        controls.lab.options.test = Lab::TestMode(clamp(int(wcstol(option + 7, nullptr, 10)), 0, 3));
    if (const auto* option = wcsstr(command_line, L"--camera="))
        controls.lab.options.camera = Lab::CameraMode(clamp(int(wcstol(option + 9, nullptr, 10)), 0, 2));
    string load_message;
    effects.load("Presets/Lab.json", load_message);
    effects.reset(controls.lab.options);
    effects.update(1.0f / 60.0f, controls.lab.options);
    controls.lab.scene = effects.scene(controls.lab.options.camera, controls.lab.options.speed);
    WNDCLASSW window_class{};
    window_class.hInstance = instance;
    window_class.lpfnWndProc = WindowProc;
    window_class.lpszClassName = L"RealDxSwitchWindow";
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&window_class);

    main_window =
        CreateWindowExW(0, window_class.lpszClassName, L"Runtime Renderer DLL Switch", WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, instance, nullptr);
    if (!main_window)
        return 3;

    ShowWindow(main_window, show_command);
    if (!SwitchRenderer(RendererDllLayout::Combined, GraphicsBackend::DirectX12) &&
        !SwitchRenderer(RendererDllLayout::Combined, GraphicsBackend::DirectX11))
    {
        ofstream("renderer_benchmark_error.txt") << initialization_errors;
        return 4;
    }
    if (command_benchmark)
        benchmark_runner.start(controls);
    MSG message{};
    auto previous = chrono::steady_clock::now();
    while (message.message != WM_QUIT)
    {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            continue;
        }
        if (!renderer)
            break;
        auto& lab = controls.lab;
        if (lab.import_requested)
        {
            string status;
            const bool loaded = effects.load(Lab::path_from_utf8(lab.preset_path), status);
            strncpy_s(lab.status, status.c_str(), _TRUNCATE);
            if (loaded)
                effects.reset(lab.options);
            lab.import_requested = false;
        }
        if (lab.reset_requested)
        {
            effects.reset(lab.options);
            lab.reset_requested = false;
        }
        if (lab.start_requested)
            benchmark_runner.start(controls);
        if (benchmark_runner.prepare(controls))
            effects.reset(lab.options);
        if (command_benchmark && !lab.running)
            break;
        const auto layout = controls.requestedDllLayout;
        const auto backend = controls.requestedBackend;
        // 전환 테스트는 매 표본마다 반대 백엔드를 먼저 준비한다. 이 준비 비용은 표본에서 제외한다.
        if (lab.running && lab.options.test == Lab::TestMode::DllSwitch)
        {
            const auto other = backend == GraphicsBackend::DirectX11 ? GraphicsBackend::DirectX12
                                                                     : GraphicsBackend::DirectX11;
            if (!SwitchRenderer(layout, other))
            {
                benchmark_runner.finish(controls, initialization_errors.c_str());
                exit_code = 6;
                break;
            }
            // 교체 전에 GPU 작업을 제출하고, 완료 대기는 다음 교체 단계에서 측정한다.
            lab.preconditioning = true;
            renderer->RenderFrame();
            lab.preconditioning = false;
        }
        auto frame_start = chrono::steady_clock::now();
        bool switched = false;
        if (layout != controls.activeDllLayout || backend != controls.activeBackend)
        {
            if (!SwitchRenderer(layout, backend))
            {
                benchmark_runner.finish(controls, initialization_errors.c_str());
                exit_code = 6;
                break;
            }
            switched = true;
        }
        else
            lab.last_switch = {};
        if (controls.textureObjectCount != controls.appliedTextureObjectCount)
        {
            if (!renderer->SetTextureObjectCount(controls.textureObjectCount))
            {
                exit_code = 5;
                break;
            }
            controls.appliedTextureObjectCount = controls.textureObjectCount;
        }
        const auto scene_start = chrono::steady_clock::now();
        const float dt =
            lab.running ? 1.0f / 60.0f : min(0.05f, chrono::duration<float>(scene_start - previous).count());
        previous = scene_start;
        effects.update(dt, lab.options);
        lab.scene = effects.scene(lab.options.camera, lab.options.speed);
        lab.active = effects.active_count();
        lab.failures = effects.failures();
        lab.simulation_time = effects.time();
        lab.scene_cpu_ms = chrono::duration<double, milli>(chrono::steady_clock::now() - scene_start).count();
        const auto render_start = chrono::steady_clock::now();
        renderer->RenderFrame();
        // 첫 복구 프레임은 Present 뒤 GPU 완료까지 포함한다.
        if (switched)
        {
            if (!renderer->WaitForIdle())
            {
                exit_code = 7;
                break;
            }
            lab.last_switch.first_frame =
                chrono::duration<double, milli>(chrono::steady_clock::now() - render_start).count();
        }
        benchmark_runner.record(
            controls, chrono::duration<double, milli>(chrono::steady_clock::now() - frame_start).count());
        if (command_benchmark && !lab.running)
            break;
    }
    if (renderer)
        renderer->WaitForIdle();
    renderer.reset();
    if (renderer_module)
        FreeLibrary(renderer_module);
    return exit_code;
}
