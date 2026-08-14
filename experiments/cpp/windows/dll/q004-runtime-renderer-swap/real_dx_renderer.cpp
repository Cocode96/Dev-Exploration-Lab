#include "real_dx_api.h"

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace
{
using Clock = std::chrono::steady_clock;

struct ImageData
{
    unsigned width{};
    unsigned height{};
    std::vector<unsigned char> rgba;
};

class TextureLoader
{
  public:
    static bool LoadPpm(const std::filesystem::path &path, ImageData &result)
    {
        std::ifstream input(path);
        std::string magic;
        unsigned maximum{};
        if (!(input >> magic >> result.width >> result.height >> maximum) || magic != "P3" || maximum != 255)
            return false;
        result.rgba.resize(static_cast<size_t>(result.width) * result.height * 4);
        for (size_t i = 0; i < result.rgba.size(); i += 4)
        {
            unsigned r{}, g{}, b{};
            if (!(input >> r >> g >> b))
                return false;
            result.rgba[i] = static_cast<unsigned char>(r);
            result.rgba[i + 1] = static_cast<unsigned char>(g);
            result.rgba[i + 2] = static_cast<unsigned char>(b);
            result.rgba[i + 3] = 255;
        }
        return true;
    }
};

struct InputAssemblerComponent
{
    ImTextureID texture{};
    void DrawAt(ImDrawList *draw_list, const ImVec2 &minimum, const ImVec2 &maximum) const
    {
        draw_list->AddImage(texture, minimum, maximum);
    }
};

D3D12_RESOURCE_BARRIER Transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

class RendererBase : public IRealRenderer
{
  public:
    bool InitializeCommon(HWND window, const wchar_t *textureRoot, ExperimentControls *controls,
                          const wchar_t *file)
    {
        window_ = window;
        controls_ = controls;
        texture_path_ = std::filesystem::path(textureRoot) / file;

        if (!TextureLoader::LoadPpm(texture_path_, source_))
            return false;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();

        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;

        return ImGui_ImplWin32_Init(window);
    }

    LRESULT HandleWindowMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) override
    {
        return ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam);
    }

    double LastResourceBuildMilliseconds() const override
    {
        return resource_ms_;
    }
    double LastFrameCpuMilliseconds() const override
    {
        return frame_ms_;
    }

  protected:
    void DrawExperimentUi()
    {
        const ImGuiID dockspace = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        ImGuiDockNode *dock_node = ImGui::DockBuilderGetNode(dockspace);

        if (dock_node != nullptr && !dock_node->IsSplitNode())
        {
            ImGui::DockBuilderRemoveNode(dockspace);
            ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);
            ImGuiID center = dockspace;
            ImGuiID left{};
            ImGuiID bottom{};
            ImGuiID comparison{};

            left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.30f, nullptr, &center);
            bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, nullptr, &center);
            comparison = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.58f, nullptr, &left);
            ImGui::DockBuilderDockWindow("Experiment Controls", left);
            ImGui::DockBuilderDockWindow("DLL Switch Comparison", comparison);
            ImGui::DockBuilderDockWindow("Texture Objects", center);
            ImGui::DockBuilderDockWindow("Experiment Log", bottom);
            ImGui::DockBuilderFinish(dockspace);
        }

        ImGui::Begin("Experiment Controls");

        bool separate = controls_->requestedDllLayout == RendererDllLayout::Separate;
        if (ImGui::Checkbox("Separate DX DLLs", &separate))
            controls_->requestedDllLayout =
                separate ? RendererDllLayout::Separate : RendererDllLayout::Combined;
        ImGui::Text("DLL layout: %s", controls_->activeDllLayout == RendererDllLayout::Separate
                                          ? "RealDx11Renderer.dll / RealDx12Renderer.dll"
                                          : "RealDxCombinedRenderer.dll");

        bool dx12 = controls_->requestedBackend == GraphicsBackend::DirectX12;
        if (ImGui::Checkbox("DX12 DLL / backend", &dx12))
            controls_->requestedBackend = dx12 ? GraphicsBackend::DirectX12 : GraphicsBackend::DirectX11;
        ImGui::Text("Active: %s",
                    controls_->activeBackend == GraphicsBackend::DirectX12 ? "DirectX 12" : "DirectX 11");

        int count = static_cast<int>(controls_->textureObjectCount);
        if (ImGui::SliderInt("Texture objects", &count, 1, 1000))
            controls_->textureObjectCount = static_cast<unsigned>(count);
        ImGui::SeparatorText("Current renderer");
        ImGui::Text("Texture resource build: %.6f s", resource_ms_ / 1000.0);
        ImGui::Text("Frame CPU: %.6f s  FPS: %.1f", frame_ms_ / 1000.0, ImGui::GetIO().Framerate);
        ImGui::End();

        ImGui::Begin("DLL Switch Comparison");

        auto draw_timing = [](const char *title, const ExperimentControls::TimingStatistics &timing) {
            ImGui::SeparatorText(title);
            ImGui::Text("Samples: %u", timing.samples);
            ImGui::Text("Last total: %.6f s", timing.lastTotalMs / 1000.0);
            ImGui::Text("  DLL module stage: %.6f s", timing.lastModuleMs / 1000.0);
            ImGui::Text("  Renderer + textures: %.6f s", timing.lastRendererMs / 1000.0);
            ImGui::Text("Average: %.6f s", timing.averageTotalMs / 1000.0);
            ImGui::Text("Min / max: %.6f / %.6f s", timing.minimumTotalMs / 1000.0,
                        timing.maximumTotalMs / 1000.0);
        };
        draw_timing("Combined DLL, internal backend switch", controls_->combinedTiming);
        draw_timing("Separate DLLs, unload + load", controls_->separateTiming);

        if (controls_->combinedTiming.samples && controls_->separateTiming.samples)
        {
            const double difference =
                controls_->separateTiming.averageTotalMs - controls_->combinedTiming.averageTotalMs;
            ImGui::SeparatorText("Average difference");
            ImGui::Text("Separate - combined: %+.6f s", difference / 1000.0);
        }
        ImGui::End();

        ImGui::Begin("Texture Objects");
        ImGui::Text("Source: %ls", texture_path_.c_str());
        ImGui::Text("Submitted every frame: %zu textured quads", objects_.size());
        DrawTextureGrid();
        ImGui::End();

        ImGui::Begin("Experiment Log");
        ImGui::Text("[%s] %zu GPU texture resources",
                    controls_->activeBackend == GraphicsBackend::DirectX12 ? "DX12" : "DX11",
                    objects_.size());
        ImGui::Text("[%s] active DLL layout",
                    controls_->activeDllLayout == RendererDllLayout::Separate ? "separate" : "combined");
        ImGui::Text("Last resource rebuild %.6f s", resource_ms_ / 1000.0);
        ImGui::TextWrapped("Compare the two sections only at the same texture count and backend transition. "
                           "Discard the first warm-up sample. Press ESC to exit.");
        ImGui::End();
    }

    void FinishFrameTiming(Clock::time_point begin)
    {
        frame_ms_ = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    }

    void DrawTextureGrid()
    {
        if (objects_.empty())
            return;
        ImVec2 available = ImGui::GetContentRegionAvail();
        available.x = (std::max)(available.x, 64.0f);
        available.y = (std::max)(available.y, 64.0f);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("TextureGridCanvas", available);

        const unsigned count = static_cast<unsigned>(objects_.size());
        const unsigned columns = static_cast<unsigned>(std::ceil(std::sqrt(static_cast<float>(count))));
        const unsigned rows = (count + columns - 1) / columns;
        const float gap = count > 400 ? 0.0f : 1.0f;
        const float cell_width = available.x / columns;
        const float cell_height = available.y / rows;
        ImDrawList *draw_list = ImGui::GetWindowDrawList();

        for (unsigned i = 0; i < count; ++i)
        {
            const unsigned column = i % columns;
            const unsigned row = i / columns;
            const ImVec2 minimum(origin.x + column * cell_width, origin.y + row * cell_height);
            const ImVec2 maximum(origin.x + (column + 1) * cell_width - gap,
                                 origin.y + (row + 1) * cell_height - gap);
            objects_[i].DrawAt(draw_list, minimum, maximum);
        }
    }

    void RenderPlatformWindows()
    {
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    HWND window_{};
    ExperimentControls *controls_{};
    std::filesystem::path texture_path_;
    ImageData source_;
    std::vector<InputAssemblerComponent> objects_;
    double resource_ms_{};
    double frame_ms_{};
};

class Dx11Renderer final : public RendererBase
{
  public:
    ~Dx11Renderer()
    {
        if (ImGui::GetCurrentContext())
        {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
    }

    bool Initialize(HWND window, unsigned width, unsigned height, const wchar_t *root,
                    ExperimentControls *controls) override
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 2;
        desc.BufferDesc.Width = width;
        desc.BufferDesc.Height = height;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = window;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        D3D_FEATURE_LEVEL level{};

        if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                                 D3D11_SDK_VERSION, &desc, &swap_chain_, &device_, &level,
                                                 &context_)))
            return false;

        ComPtr<ID3D11Texture2D> back_buffer;

        if (FAILED(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer))) ||
            FAILED(device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &rtv_)))
            return false;

        if (!InitializeCommon(window, root, controls, L"dx11.ppm") ||
            !ImGui_ImplDX11_Init(device_.Get(), context_.Get()))
            return false;

        return SetTextureObjectCount(controls->textureObjectCount);
    }

    bool SetTextureObjectCount(unsigned count) override
    {
        const auto begin = Clock::now();

        textures_.clear();
        views_.clear();
        objects_.clear();
        textures_.reserve(count);
        views_.reserve(count);
        objects_.reserve(count);

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = source_.width;
        desc.Height = source_.height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA data{source_.rgba.data(), source_.width * 4, 0};

        for (unsigned i = 0; i < count; ++i)
        {
            ComPtr<ID3D11Texture2D> texture;
            ComPtr<ID3D11ShaderResourceView> view;

            if (FAILED(device_->CreateTexture2D(&desc, &data, &texture)) ||
                FAILED(device_->CreateShaderResourceView(texture.Get(), nullptr, &view)))
                return false;
            objects_.push_back({reinterpret_cast<ImTextureID>(view.Get())});
            textures_.push_back(std::move(texture));
            views_.push_back(std::move(view));
        }

        resource_ms_ = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();

        return true;
    }

    void RenderFrame() override
    {
        const auto begin = Clock::now();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DrawExperimentUi();
        ImGui::Render();

        constexpr float color[4]{0.025f, 0.035f, 0.055f, 1.0f};

        context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
        context_->ClearRenderTargetView(rtv_.Get(), color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        RenderPlatformWindows();

        swap_chain_->Present(1, 0);

        FinishFrameTiming(begin);
    }
    void WaitForIdle() override
    {
        if (context_)
            context_->Flush();
    }
    const wchar_t *Name() const override
    {
        return L"DirectX 11, blue texture";
    }

  private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swap_chain_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    std::vector<ComPtr<ID3D11Texture2D>> textures_;
    std::vector<ComPtr<ID3D11ShaderResourceView>> views_;
};

class Dx12Renderer final : public RendererBase
{
  public:
    ~Dx12Renderer()
    {
        WaitForIdle();
        if (ImGui::GetCurrentContext())
        {
            ImGui_ImplDX12_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
        if (fence_event_)
            CloseHandle(fence_event_);
    }

    bool Initialize(HWND window, unsigned width, unsigned height, const wchar_t *root,
                    ExperimentControls *controls) override
    {
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_))))
            return false;

        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        if (FAILED(device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue_))))
            return false;

        ComPtr<IDXGIFactory4> factory;

        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
            return false;

        DXGI_SWAP_CHAIN_DESC1 swap{};
        swap.BufferCount = frame_count_;
        swap.Width = width;
        swap.Height = height;
        swap.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swap.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> base;

        if (FAILED(factory->CreateSwapChainForHwnd(queue_.Get(), window, &swap, nullptr, nullptr, &base)) ||
            FAILED(base.As(&swap_chain_)))
            return false;

        D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
        rtv_desc.NumDescriptors = frame_count_;
        rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        if (FAILED(device_->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&rtv_heap_))))
            return false;

        rtv_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        auto rtv = rtv_heap_->GetCPUDescriptorHandleForHeapStart();

        for (unsigned i = 0; i < frame_count_; ++i)
        {
            if (FAILED(swap_chain_->GetBuffer(i, IID_PPV_ARGS(&buffers_[i]))))
                return false;
            device_->CreateRenderTargetView(buffers_[i].Get(), nullptr, rtv);
            rtv.ptr += rtv_size_;
            if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                       IID_PPV_ARGS(&allocators_[i]))))
                return false;
        }

        if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators_[0].Get(),
                                              nullptr, IID_PPV_ARGS(&list_))))
            return false;

        list_->Close();

        if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_))))
            return false;

        fence_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);

        if (!fence_event_)
            return false;

        D3D12_DESCRIPTOR_HEAP_DESC srv_desc{};
        srv_desc.NumDescriptors = 1001;
        srv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(device_->CreateDescriptorHeap(&srv_desc, IID_PPV_ARGS(&srv_heap_))))
            return false;

        srv_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        if (!InitializeCommon(window, root, controls, L"dx12.ppm"))
            return false;

        ImGui_ImplDX12_InitInfo imgui_info{};
        imgui_info.Device = device_.Get();
        imgui_info.CommandQueue = queue_.Get();
        imgui_info.NumFramesInFlight = frame_count_;
        imgui_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        imgui_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
        imgui_info.UserData = this;
        imgui_info.SrvDescriptorHeap = srv_heap_.Get();
        imgui_info.SrvDescriptorAllocFn = AllocateImGuiDescriptor;
        imgui_info.SrvDescriptorFreeFn = FreeImGuiDescriptor;

        if (!ImGui_ImplDX12_Init(&imgui_info))
            return false;

        return SetTextureObjectCount(controls->textureObjectCount);
    }

    bool SetTextureObjectCount(unsigned count) override
    {
        WaitForIdle();

        const auto begin = Clock::now();

        textures_.clear();
        objects_.clear();

        textures_.reserve(count);
        objects_.reserve(count);

        allocators_[0]->Reset();
        list_->Reset(allocators_[0].Get(), nullptr);

        std::vector<ComPtr<ID3D12Resource>> uploads;

        for (unsigned i = 0; i < count; ++i)
        {
            // Default heap texture
            D3D12_RESOURCE_DESC td{};
            td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            td.Width = source_.width;
            td.Height = source_.height;
            td.DepthOrArraySize = 1;
            td.MipLevels = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            D3D12_HEAP_PROPERTIES hp{};
            hp.Type = D3D12_HEAP_TYPE_DEFAULT;

            ComPtr<ID3D12Resource> texture;

            if (FAILED(device_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
                                                        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                        IID_PPV_ARGS(&texture))))
                return false;

            // Upload heap and row-pitch aligned pixel copy
            UINT64 bytes{};
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
            device_->GetCopyableFootprints(&td, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);

            D3D12_RESOURCE_DESC bd{};
            bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bd.Width = bytes;
            bd.Height = 1;
            bd.DepthOrArraySize = 1;
            bd.MipLevels = 1;
            bd.SampleDesc.Count = 1;
            bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            hp.Type = D3D12_HEAP_TYPE_UPLOAD;

            ComPtr<ID3D12Resource> upload;

            if (FAILED(device_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                        IID_PPV_ARGS(&upload))))
                return false;

            unsigned char *mapped{};
            upload->Map(0, nullptr, reinterpret_cast<void **>(&mapped));

            for (unsigned y = 0; y < source_.height; ++y)
                std::copy_n(source_.rgba.data() + y * source_.width * 4, source_.width * 4,
                            mapped + footprint.Offset + y * footprint.Footprint.RowPitch);

            upload->Unmap(0, nullptr);

            // GPU copy and resource-state transition
            D3D12_TEXTURE_COPY_LOCATION dst{texture.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
            dst.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION src{upload.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
            src.PlacedFootprint = footprint;

            list_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

            auto barrier = Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            list_->ResourceBarrier(1, &barrier);

            // One SRV descriptor per textured quad
            auto cpu = srv_heap_->GetCPUDescriptorHandleForHeapStart();
            cpu.ptr += static_cast<SIZE_T>(i + 1) * srv_size_;

            auto gpu = srv_heap_->GetGPUDescriptorHandleForHeapStart();
            gpu.ptr += static_cast<UINT64>(i + 1) * srv_size_;

            D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
            sd.Format = td.Format;
            sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            sd.Texture2D.MipLevels = 1;

            device_->CreateShaderResourceView(texture.Get(), &sd, cpu);

            objects_.push_back({static_cast<ImTextureID>(gpu.ptr)});
            textures_.push_back(std::move(texture));
            uploads.push_back(std::move(upload));
        }

        list_->Close();

        ID3D12CommandList *lists[]{list_.Get()};
        queue_->ExecuteCommandLists(1, lists);

        WaitForIdle();

        resource_ms_ = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();

        return true;
    }

    void RenderFrame() override
    {
        const auto begin = Clock::now();

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DrawExperimentUi();
        ImGui::Render();

        const unsigned index = swap_chain_->GetCurrentBackBufferIndex();

        allocators_[index]->Reset();
        list_->Reset(allocators_[index].Get(), nullptr);

        auto barrier = Transition(buffers_[index].Get(), D3D12_RESOURCE_STATE_PRESENT,
                                  D3D12_RESOURCE_STATE_RENDER_TARGET);
        list_->ResourceBarrier(1, &barrier);

        auto rtv = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += static_cast<SIZE_T>(index) * rtv_size_;

        constexpr float color[4]{0.055f, 0.025f, 0.02f, 1.0f};

        list_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        list_->ClearRenderTargetView(rtv, color, 0, nullptr);

        ID3D12DescriptorHeap *heaps[]{srv_heap_.Get()};
        list_->SetDescriptorHeaps(1, heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list_.Get());

        barrier = Transition(buffers_[index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                             D3D12_RESOURCE_STATE_PRESENT);
        list_->ResourceBarrier(1, &barrier);
        list_->Close();

        ID3D12CommandList *lists[]{list_.Get()};
        queue_->ExecuteCommandLists(1, lists);

        RenderPlatformWindows();

        swap_chain_->Present(1, 0);
        WaitForIdle();

        FinishFrameTiming(begin);
    }
    void WaitForIdle() override
    {
        if (!queue_ || !fence_ || !fence_event_)
            return;
        const auto value = ++fence_value_;
        queue_->Signal(fence_.Get(), value);
        if (fence_->GetCompletedValue() < value)
        {
            fence_->SetEventOnCompletion(value, fence_event_);
            WaitForSingleObject(fence_event_, INFINITE);
        }
    }
    const wchar_t *Name() const override
    {
        return L"DirectX 12, orange texture";
    }

  private:
    static void AllocateImGuiDescriptor(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *cpu,
                                        D3D12_GPU_DESCRIPTOR_HANDLE *gpu)
    {
        auto *self = static_cast<Dx12Renderer *>(info->UserData);
        *cpu = self->srv_heap_->GetCPUDescriptorHandleForHeapStart();
        *gpu = self->srv_heap_->GetGPUDescriptorHandleForHeapStart();
    }

    static void FreeImGuiDescriptor(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE,
                                    D3D12_GPU_DESCRIPTOR_HANDLE)
    {
    }

    static constexpr unsigned frame_count_ = 2;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<IDXGISwapChain3> swap_chain_;
    ComPtr<ID3D12DescriptorHeap> rtv_heap_, srv_heap_;
    std::array<ComPtr<ID3D12Resource>, frame_count_> buffers_;
    std::array<ComPtr<ID3D12CommandAllocator>, frame_count_> allocators_;
    ComPtr<ID3D12GraphicsCommandList> list_;
    ComPtr<ID3D12Fence> fence_;
    std::vector<ComPtr<ID3D12Resource>> textures_;
    HANDLE fence_event_{};
    UINT rtv_size_{}, srv_size_{};
    UINT64 fence_value_{};
};
} // namespace

REAL_RENDERER_EXPORT bool ProbeBackend(GraphicsBackend backend)
{
#if defined(REAL_DX11_ONLY)
    return backend == GraphicsBackend::DirectX11;
#elif defined(REAL_DX12_ONLY)
    return backend == GraphicsBackend::DirectX12 &&
           SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr));
#else
    if (backend == GraphicsBackend::DirectX12)
        return SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr));
    return backend == GraphicsBackend::DirectX11;
#endif
}

REAL_RENDERER_EXPORT IRealRenderer *CreateRealRenderer(GraphicsBackend backend)
{
#if defined(REAL_DX11_ONLY)
    return backend == GraphicsBackend::DirectX11 ? new Dx11Renderer{} : nullptr;
#elif defined(REAL_DX12_ONLY)
    return backend == GraphicsBackend::DirectX12 ? new Dx12Renderer{} : nullptr;
#else
    if (backend == GraphicsBackend::DirectX12)
        return new Dx12Renderer{};
    if (backend == GraphicsBackend::DirectX11)
        return new Dx11Renderer{};
    return nullptr;
#endif
}

REAL_RENDERER_EXPORT void DestroyRealRenderer(IRealRenderer *renderer)
{
    if (auto *dx12 = dynamic_cast<Dx12Renderer *>(renderer))
    {
        delete dx12;
        return;
    }
    delete dynamic_cast<Dx11Renderer *>(renderer);
}
