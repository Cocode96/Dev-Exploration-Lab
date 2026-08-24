#include "real_dx_api.h"

#include <Windows.h>

#include <memory>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <vector>

namespace
{
HMODULE renderer_module{};
ProbeBackendFn probe_backend{};
CreateRealRendererFn create_renderer{};
DestroyRealRendererFn destroy_renderer{};
std::unique_ptr<IRealRenderer, DestroyRealRendererFn> renderer{nullptr, nullptr};
HWND main_window{};
ExperimentControls controls{};
std::wstring loaded_module_name;

struct BenchmarkSample
{
    RendererDllLayout layout{};
    GraphicsBackend backend{};
    double moduleMs{};
    double rendererMs{};
    double totalMs{};
};

struct BenchmarkState
{
    bool enabled{};
    bool captureTiming{true};
    unsigned warmupCount{10};
    unsigned iterationCount{100};
    unsigned warmupRemaining{};
    unsigned samplesInPhase{};
    RendererDllLayout layout{RendererDllLayout::Combined};
    std::vector<BenchmarkSample> samples;
};

BenchmarkState benchmark{};
std::string initialization_errors;

void RecordTiming(RendererDllLayout layout, double module_ms, double renderer_ms)
{
    if (benchmark.enabled && !benchmark.captureTiming)
        return;

    auto &timing = layout == RendererDllLayout::Combined ? controls.combinedTiming : controls.separateTiming;
    timing.lastModuleMs = module_ms;
    timing.lastRendererMs = renderer_ms;
    timing.lastTotalMs = module_ms + renderer_ms;
    timing.accumulatedTotalMs += timing.lastTotalMs;
    ++timing.samples;
    timing.averageTotalMs = timing.accumulatedTotalMs / timing.samples;
    timing.minimumTotalMs =
        timing.samples == 1 ? timing.lastTotalMs : (std::min)(timing.minimumTotalMs, timing.lastTotalMs);
    timing.maximumTotalMs = (std::max)(timing.maximumTotalMs, timing.lastTotalMs);

    if (benchmark.enabled)
    {
        benchmark.samples.push_back(
            {layout, controls.requestedBackend, module_ms, renderer_ms, module_ms + renderer_ms});
    }
}

unsigned ReadUnsignedOption(const wchar_t *command_line, const wchar_t *option, unsigned fallback)
{
    const wchar_t *found = wcsstr(command_line, option);
    if (found == nullptr)
        return fallback;

    found += wcslen(option);
    const unsigned long value = wcstoul(found, nullptr, 10);

    return value == 0 ? fallback : static_cast<unsigned>(value);
}

const wchar_t *ModuleName(RendererDllLayout layout, GraphicsBackend backend)
{
    if (layout == RendererDllLayout::Combined)
        return L"RealDxCombinedRenderer.dll";
    return backend == GraphicsBackend::DirectX12 ? L"RealDx12Renderer.dll" : L"RealDx11Renderer.dll";
}

bool LoadRendererModule(RendererDllLayout layout, GraphicsBackend backend)
{
    const wchar_t *requested = ModuleName(layout, backend);
    if (renderer_module && loaded_module_name == requested)
        return true;

    renderer.reset();
    if (renderer_module)
        FreeLibrary(renderer_module);
    renderer_module = LoadLibraryW(requested);
    if (!renderer_module)
    {
        initialization_errors += "LoadLibraryW 오류 코드: " + std::to_string(GetLastError()) + "\n";
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
    const auto started = std::chrono::steady_clock::now();

    if (!LoadRendererModule(layout, backend))
    {
        initialization_errors += "렌더러 모듈 로드 실패.\n";
        return false;
    }

    if (!probe_backend(backend))
    {
        initialization_errors += backend == GraphicsBackend::DirectX12
            ? "DX12 백엔드 검사 실패.\n"
            : "DX11 백엔드 검사 실패.\n";
        return false;
    }

    const auto module_finished = std::chrono::steady_clock::now();

    renderer.reset();
    auto *candidate = create_renderer(backend);

    if (candidate == nullptr)
    {
        initialization_errors += "렌더러 생성 함수가 nullptr을 반환함.\n";
        return false;
    }

    if (!candidate->Initialize(main_window, 1280, 720, L"Texture", &controls))
    {
        initialization_errors += backend == GraphicsBackend::DirectX12
            ? "DX12 렌더러 초기화 실패.\n"
            : "DX11 렌더러 초기화 실패.\n";
        destroy_renderer(candidate);
        return false;
    }

    renderer = decltype(renderer){candidate, destroy_renderer};
    controls.activeBackend = backend;
    controls.requestedBackend = backend;
    controls.activeDllLayout = layout;
    controls.requestedDllLayout = layout;
    controls.appliedTextureObjectCount = controls.textureObjectCount;
    const auto finished = std::chrono::steady_clock::now();
    RecordTiming(layout, std::chrono::duration<double, std::milli>(module_finished - started).count(),
                 std::chrono::duration<double, std::milli>(finished - module_finished).count());
    std::wstring title = L"Runtime Renderer DLL Switch | ";
    title += renderer->Name();
    title += layout == RendererDllLayout::Combined ? L" | Combined DLL" : L" | Separate DLLs";
    SetWindowTextW(main_window, title.c_str());
    return true;
}

void SaveBenchmarkCsv()
{
    std::ofstream output("renderer_benchmark.csv", std::ios::trunc);
    output << "layout,backend,module_seconds,renderer_texture_seconds,total_seconds\n";
    output << std::fixed << std::setprecision(9);

    for (const BenchmarkSample &sample : benchmark.samples)
    {
        output << (sample.layout == RendererDllLayout::Combined ? "combined" : "separate") << ','
               << (sample.backend == GraphicsBackend::DirectX12 ? "dx12" : "dx11") << ','
               << sample.moduleMs / 1000.0 << ',' << sample.rendererMs / 1000.0 << ','
               << sample.totalMs / 1000.0 << '\n';
    }
}

void RunBenchmarkStep()
{
    benchmark.captureTiming = benchmark.warmupRemaining == 0;

    const GraphicsBackend next_backend = controls.activeBackend == GraphicsBackend::DirectX12
        ? GraphicsBackend::DirectX11
        : GraphicsBackend::DirectX12;

    if (!SwitchRenderer(benchmark.layout, next_backend))
    {
        SaveBenchmarkCsv();
        DestroyWindow(main_window);
        return;
    }

    if (benchmark.warmupRemaining > 0)
    {
        --benchmark.warmupRemaining;
        return;
    }

    ++benchmark.samplesInPhase;

    if (benchmark.samplesInPhase < benchmark.iterationCount)
        return;

    if (benchmark.layout == RendererDllLayout::Combined)
    {
        benchmark.layout = RendererDllLayout::Separate;
        benchmark.warmupRemaining = benchmark.warmupCount;
        benchmark.samplesInPhase = 0;
        return;
    }

    SaveBenchmarkCsv();
    DestroyWindow(main_window);
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
    benchmark.enabled = wcsstr(command_line, L"--benchmark") != nullptr;
    benchmark.iterationCount = ReadUnsignedOption(command_line, L"--iterations=", 100);
    benchmark.warmupCount = ReadUnsignedOption(command_line, L"--warmup=", 10);

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
        if (!benchmark.enabled)
        {
            MessageBoxW(main_window, L"DX12와 DX11 초기화에 실패했습니다.", L"렌더러 오류",
                        MB_ICONERROR);
        }
        else
        {
            std::ofstream error_log("renderer_benchmark_error.txt", std::ios::trunc);
            error_log << "DX12와 DX11 초기화에 실패했습니다.\n";
            error_log << initialization_errors;
        }

        return 4;
    }

    if (benchmark.enabled)
    {
        controls.textureObjectCount = ReadUnsignedOption(command_line, L"--textures=", 1000);
        controls.textureObjectCount = (std::min)(controls.textureObjectCount, 1000u);

        if (!renderer->SetTextureObjectCount(controls.textureObjectCount))
            return 5;

        controls.appliedTextureObjectCount = controls.textureObjectCount;
        controls.combinedTiming = {};
        controls.separateTiming = {};

        benchmark.samples.clear();
        benchmark.layout = RendererDllLayout::Combined;
        benchmark.warmupRemaining = benchmark.warmupCount;
        benchmark.samplesInPhase = 0;
        benchmark.captureTiming = false;
    }

    MSG message{};
    while (message.message != WM_QUIT)
    {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        else if (renderer)
        {
            renderer->RenderFrame();

            if (benchmark.enabled)
            {
                RunBenchmarkStep();
                continue;
            }

            if (controls.requestedBackend != controls.activeBackend ||
                controls.requestedDllLayout != controls.activeDllLayout)
                SwitchRenderer(controls.requestedDllLayout, controls.requestedBackend);
            else if (controls.textureObjectCount != controls.appliedTextureObjectCount)
            {
                if (renderer->SetTextureObjectCount(controls.textureObjectCount))
                    controls.appliedTextureObjectCount = controls.textureObjectCount;
            }
        }
    }

    renderer.reset();
    FreeLibrary(renderer_module);
    return 0;
}
