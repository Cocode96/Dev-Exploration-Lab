#pragma once

#include <Windows.h>

enum class GraphicsBackend : int
{
    DirectX11 = 11,
    DirectX12 = 12,
};

enum class RendererDllLayout : int
{
    Combined = 0,
    Separate = 1,
};

struct ExperimentControls
{
    struct TimingStatistics
    {
        double lastTotalMs{};
        double lastModuleMs{};
        double lastRendererMs{};
        double averageTotalMs{};
        double minimumTotalMs{};
        double maximumTotalMs{};
        double accumulatedTotalMs{};
        unsigned samples{};
    };

    GraphicsBackend activeBackend{GraphicsBackend::DirectX12};
    GraphicsBackend requestedBackend{GraphicsBackend::DirectX12};
    RendererDllLayout activeDllLayout{RendererDllLayout::Combined};
    RendererDllLayout requestedDllLayout{RendererDllLayout::Combined};
    unsigned textureObjectCount{1};
    unsigned appliedTextureObjectCount{1};
    TimingStatistics combinedTiming{};
    TimingStatistics separateTiming{};
};

struct IRealRenderer
{
    virtual bool Initialize(HWND window, unsigned width, unsigned height, const wchar_t *textureRoot,
                            ExperimentControls *controls) = 0;
    virtual bool SetTextureObjectCount(unsigned count) = 0;
    virtual void RenderFrame() = 0;
    virtual LRESULT HandleWindowMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) = 0;
    virtual void WaitForIdle() = 0;
    virtual const wchar_t *Name() const = 0;
    virtual double LastResourceBuildMilliseconds() const = 0;
    virtual double LastFrameCpuMilliseconds() const = 0;

  protected:
    ~IRealRenderer() = default;
};

using ProbeBackendFn = bool (*)(GraphicsBackend backend);
using CreateRealRendererFn = IRealRenderer *(*)(GraphicsBackend backend);
using DestroyRealRendererFn = void (*)(IRealRenderer *renderer);

#define REAL_RENDERER_EXPORT extern "C" __declspec(dllexport)
