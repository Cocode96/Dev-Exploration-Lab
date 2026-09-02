#include "Renderer.h"

#include "ApplicationContext.h"
#include "CameraManager.h"
#include "DebugUiManager.h"
#include "FreeCamera.h"

#include <d3dcompiler.h>
#include <dxgi1_6.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <thread>

using Microsoft::WRL::ComPtr;

namespace
{
struct EffectVertex
{
    DirectX::XMFLOAT2 position;
    DirectX::XMFLOAT2 uv;
};

struct alignas(16) FrameConstants
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 view_projection;
    DirectX::XMFLOAT4 camera_position;
    DirectX::XMFLOAT4 camera_right;
    DirectX::XMFLOAT4 camera_up;
    DirectX::XMFLOAT4 camera_forward;
    DirectX::XMFLOAT4 sky_zenith;
    DirectX::XMFLOAT4 sky_horizon;
    DirectX::XMFLOAT2 resolution;
    int alpha_mode;
    int depth_mode;
    float p_alpha;
    float k_alpha;
    float k_depth;
    float padding;
};

static_assert(sizeof(FrameConstants) % 16 == 0);

bool succeeded(HRESULT result)
{
    return SUCCEEDED(result);
}

ComPtr<ID3DBlob> compile_shader(const std::filesystem::path& path, const char* entry_point,
    const char* target)
{
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    if (FAILED(D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry_point, target, flags, 0, &bytecode, &errors)))
    {
        if (errors)
            OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
        return nullptr;
    }
    return bytecode;
}

void write_bmp(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height,
    const std::vector<std::uint8_t>& rgba)
{
    std::filesystem::create_directories(path.parent_path());
    BITMAPFILEHEADER file_header{};
    BITMAPINFOHEADER info_header{};
    file_header.bfType = 0x4D42;
    file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    file_header.bfSize = file_header.bfOffBits + width * height * 4;
    info_header.biSize = sizeof(BITMAPINFOHEADER);
    info_header.biWidth = static_cast<LONG>(width);
    info_header.biHeight = -static_cast<LONG>(height);
    info_header.biPlanes = 1;
    info_header.biBitCount = 32;
    info_header.biCompression = BI_RGB;
    info_header.biSizeImage = width * height * 4;

    std::vector<std::uint8_t> bgra(rgba.size());
    for (std::size_t index = 0; index < rgba.size(); index += 4)
    {
        bgra[index] = rgba[index + 2];
        bgra[index + 1] = rgba[index + 1];
        bgra[index + 2] = rgba[index];
        bgra[index + 3] = rgba[index + 3];
    }

    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
    stream.write(reinterpret_cast<const char*>(&info_header), sizeof(info_header));
    stream.write(reinterpret_cast<const char*>(bgra.data()), static_cast<std::streamsize>(bgra.size()));
}
}

namespace Engine
{
bool Renderer::initialize(ApplicationContext& context, HWND window, std::uint32_t width,
    std::uint32_t height, const std::filesystem::path& shader_path)
{
    m_context = &context;
    m_width = width;
    m_height = height;
    m_submission_instances.reserve(16384);
    return create_device_and_swap_chain(window)
        && create_render_targets()
        && create_pipeline(shader_path)
        && create_geometry()
        && create_queries();
}

bool Renderer::create_device_and_swap_chain(HWND window)
{
    ComPtr<IDXGIFactory6> factory;
    if (!succeeded(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
        return false;

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT index = 0;
        factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
        ++index)
    {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
        {
            m_adapter_name = desc.Description;
            break;
        }
        adapter.Reset();
    }
    if (!adapter)
        return false;

    constexpr std::array levels{D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL selected{};
    if (!succeeded(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels.data(), static_cast<UINT>(levels.size()),
        D3D11_SDK_VERSION, &m_device, &selected, &m_device_context)))
        return false;

    DXGI_SWAP_CHAIN_DESC1 swap_desc{};
    swap_desc.Width = m_width;
    swap_desc.Height = m_height;
    swap_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.SampleDesc.Count = 1;
    swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount = 2;
    swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (!succeeded(factory->CreateSwapChainForHwnd(m_device.Get(), window, &swap_desc,
        nullptr, nullptr, &m_swap_chain)))
        return false;
    factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
    return true;
}

bool Renderer::create_render_targets()
{
    if (!succeeded(m_swap_chain->GetBuffer(0, IID_PPV_ARGS(&m_back_buffer)))
        || !succeeded(m_device->CreateRenderTargetView(m_back_buffer.Get(), nullptr, &m_back_buffer_rtv)))
        return false;

    m_requested_scene_width = m_width;
    m_requested_scene_height = m_height;
    return create_scene_render_targets(m_requested_scene_width, m_requested_scene_height);
}

bool Renderer::create_scene_render_targets(std::uint32_t width, std::uint32_t height)
{
    D3D11_TEXTURE2D_DESC scene_desc{};
    scene_desc.Width = width;
    scene_desc.Height = height;
    scene_desc.MipLevels = 1;
    scene_desc.ArraySize = 1;
    scene_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scene_desc.SampleDesc.Count = 1;
    scene_desc.Usage = D3D11_USAGE_DEFAULT;
    scene_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> scene_color_texture;
    ComPtr<ID3D11RenderTargetView> scene_color_rtv;
    ComPtr<ID3D11ShaderResourceView> scene_color_srv;
    if (!succeeded(m_device->CreateTexture2D(&scene_desc, nullptr, &scene_color_texture))
        || !succeeded(m_device->CreateRenderTargetView(
            scene_color_texture.Get(), nullptr, &scene_color_rtv))
        || !succeeded(m_device->CreateShaderResourceView(
            scene_color_texture.Get(), nullptr, &scene_color_srv)))
        return false;

    D3D11_TEXTURE2D_DESC staging_desc = scene_desc;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> capture_staging;
    if (!succeeded(m_device->CreateTexture2D(&staging_desc, nullptr, &capture_staging)))
        return false;

    D3D11_TEXTURE2D_DESC depth_desc{};
    depth_desc.Width = width;
    depth_desc.Height = height;
    depth_desc.MipLevels = 1;
    depth_desc.ArraySize = 1;
    depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_desc.SampleDesc.Count = 1;
    depth_desc.Usage = D3D11_USAGE_DEFAULT;
    depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth_texture;
    ComPtr<ID3D11DepthStencilView> depth_dsv;
    if (!succeeded(m_device->CreateTexture2D(&depth_desc, nullptr, &depth_texture))
        || !succeeded(m_device->CreateDepthStencilView(depth_texture.Get(), nullptr, &depth_dsv)))
        return false;

    D3D11_TEXTURE2D_DESC accum_desc{};
    accum_desc.Width = width;
    accum_desc.Height = height;
    accum_desc.MipLevels = 1;
    accum_desc.ArraySize = 1;
    accum_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    accum_desc.SampleDesc.Count = 1;
    accum_desc.Usage = D3D11_USAGE_DEFAULT;
    accum_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> accum_color_texture;
    ComPtr<ID3D11RenderTargetView> accum_color_rtv;
    ComPtr<ID3D11ShaderResourceView> accum_color_srv;
    if (!succeeded(m_device->CreateTexture2D(&accum_desc, nullptr, &accum_color_texture))
        || !succeeded(m_device->CreateRenderTargetView(
            accum_color_texture.Get(), nullptr, &accum_color_rtv))
        || !succeeded(m_device->CreateShaderResourceView(
            accum_color_texture.Get(), nullptr, &accum_color_srv)))
        return false;

    accum_desc.Format = DXGI_FORMAT_R16_FLOAT;
    ComPtr<ID3D11Texture2D> accum_weight_texture;
    ComPtr<ID3D11RenderTargetView> accum_weight_rtv;
    ComPtr<ID3D11ShaderResourceView> accum_weight_srv;
    if (!succeeded(m_device->CreateTexture2D(&accum_desc, nullptr, &accum_weight_texture))
        || !succeeded(m_device->CreateRenderTargetView(
            accum_weight_texture.Get(), nullptr, &accum_weight_rtv))
        || !succeeded(m_device->CreateShaderResourceView(
            accum_weight_texture.Get(), nullptr, &accum_weight_srv)))
        return false;

    m_scene_color_texture = std::move(scene_color_texture);
    m_scene_color_rtv = std::move(scene_color_rtv);
    m_scene_color_srv = std::move(scene_color_srv);
    m_capture_staging = std::move(capture_staging);
    m_depth_texture = std::move(depth_texture);
    m_depth_dsv = std::move(depth_dsv);
    m_accum_color_texture = std::move(accum_color_texture);
    m_accum_color_rtv = std::move(accum_color_rtv);
    m_accum_color_srv = std::move(accum_color_srv);
    m_accum_weight_texture = std::move(accum_weight_texture);
    m_accum_weight_rtv = std::move(accum_weight_rtv);
    m_accum_weight_srv = std::move(accum_weight_srv);
    m_scene_width = width;
    m_scene_height = height;
    m_viewport = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    return true;
}

bool Renderer::prepare_scene_view()
{
    if (m_requested_scene_width == m_scene_width
        && m_requested_scene_height == m_scene_height)
        return true;
    return create_scene_render_targets(m_requested_scene_width, m_requested_scene_height);
}

void Renderer::request_scene_view_size(std::uint32_t width, std::uint32_t height) noexcept
{
    m_requested_scene_width = (std::clamp)(width, 64u, 4096u);
    m_requested_scene_height = (std::clamp)(height, 64u, 4096u);
}

bool Renderer::create_pipeline(const std::filesystem::path& shader_path)
{
    const auto effect_vs = compile_shader(shader_path, "VSEffect", "vs_5_0");
    const auto world_vs = compile_shader(shader_path, "VSWorld", "vs_5_0");
    const auto fullscreen_vs = compile_shader(shader_path, "VSFullscreen", "vs_5_0");
    const auto alpha_ps = compile_shader(shader_path, "PSAlpha", "ps_5_0");
    const auto wboit_ps = compile_shader(shader_path, "PSWboit", "ps_5_0");
    const auto sky_ps = compile_shader(shader_path, "PSSky", "ps_5_0");
    const auto terrain_ps = compile_shader(shader_path, "PSTerrain", "ps_5_0");
    const auto resolve_ps = compile_shader(shader_path, "PSResolve", "ps_5_0");
    if (!effect_vs || !world_vs || !fullscreen_vs || !alpha_ps || !wboit_ps
        || !sky_ps || !terrain_ps || !resolve_ps)
        return false;

    if (!succeeded(m_device->CreateVertexShader(effect_vs->GetBufferPointer(), effect_vs->GetBufferSize(), nullptr, &m_effect_vs))
        || !succeeded(m_device->CreateVertexShader(world_vs->GetBufferPointer(), world_vs->GetBufferSize(), nullptr, &m_world_vs))
        || !succeeded(m_device->CreateVertexShader(fullscreen_vs->GetBufferPointer(), fullscreen_vs->GetBufferSize(), nullptr, &m_fullscreen_vs))
        || !succeeded(m_device->CreatePixelShader(alpha_ps->GetBufferPointer(), alpha_ps->GetBufferSize(), nullptr, &m_alpha_ps))
        || !succeeded(m_device->CreatePixelShader(wboit_ps->GetBufferPointer(), wboit_ps->GetBufferSize(), nullptr, &m_wboit_ps))
        || !succeeded(m_device->CreatePixelShader(sky_ps->GetBufferPointer(), sky_ps->GetBufferSize(), nullptr, &m_sky_ps))
        || !succeeded(m_device->CreatePixelShader(terrain_ps->GetBufferPointer(), terrain_ps->GetBufferSize(), nullptr, &m_terrain_ps))
        || !succeeded(m_device->CreatePixelShader(resolve_ps->GetBufferPointer(), resolve_ps->GetBufferSize(), nullptr, &m_resolve_ps)))
        return false;

    const std::array<D3D11_INPUT_ELEMENT_DESC, 6> effect_elements{
        D3D11_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        D3D11_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        D3D11_INPUT_ELEMENT_DESC{"INSTANCE_POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        D3D11_INPUT_ELEMENT_DESC{"INSTANCE_SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        D3D11_INPUT_ELEMENT_DESC{"INSTANCE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 24, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        D3D11_INPUT_ELEMENT_DESC{"INSTANCE_ROTATION", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 40, D3D11_INPUT_PER_INSTANCE_DATA, 1}
    };
    if (!succeeded(m_device->CreateInputLayout(effect_elements.data(), static_cast<UINT>(effect_elements.size()),
        effect_vs->GetBufferPointer(), effect_vs->GetBufferSize(), &m_effect_input_layout)))
        return false;

    const std::array<D3D11_INPUT_ELEMENT_DESC, 3> world_elements{
        D3D11_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        D3D11_INPUT_ELEMENT_DESC{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        D3D11_INPUT_ELEMENT_DESC{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    if (!succeeded(m_device->CreateInputLayout(world_elements.data(), static_cast<UINT>(world_elements.size()),
        world_vs->GetBufferPointer(), world_vs->GetBufferSize(), &m_world_input_layout)))
        return false;

    D3D11_BLEND_DESC alpha{};
    auto& alpha_target = alpha.RenderTarget[0];
    alpha_target.BlendEnable = TRUE;
    alpha_target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    alpha_target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    alpha_target.BlendOp = D3D11_BLEND_OP_ADD;
    alpha_target.SrcBlendAlpha = D3D11_BLEND_ONE;
    alpha_target.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    alpha_target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    alpha_target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (!succeeded(m_device->CreateBlendState(&alpha, &m_alpha_blend)))
        return false;

    D3D11_BLEND_DESC additive{};
    additive.IndependentBlendEnable = TRUE;
    for (auto& target : additive.RenderTarget)
    {
        target.BlendEnable = TRUE;
        target.SrcBlend = D3D11_BLEND_ONE;
        target.DestBlend = D3D11_BLEND_ONE;
        target.BlendOp = D3D11_BLEND_OP_ADD;
        target.SrcBlendAlpha = D3D11_BLEND_ONE;
        target.DestBlendAlpha = D3D11_BLEND_ONE;
        target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    if (!succeeded(m_device->CreateBlendState(&additive, &m_additive_blend)))
        return false;

    D3D11_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;
    if (!succeeded(m_device->CreateRasterizerState(&rasterizer, &m_rasterizer_state)))
        return false;

    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = TRUE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (!succeeded(m_device->CreateDepthStencilState(&depth, &m_depth_write_state)))
        return false;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    if (!succeeded(m_device->CreateDepthStencilState(&depth, &m_depth_read_state)))
        return false;
    depth.DepthEnable = FALSE;
    if (!succeeded(m_device->CreateDepthStencilState(&depth, &m_depth_disabled_state)))
        return false;

    D3D11_BUFFER_DESC constant_desc{};
    constant_desc.ByteWidth = sizeof(FrameConstants);
    constant_desc.Usage = D3D11_USAGE_DEFAULT;
    constant_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    return succeeded(m_device->CreateBuffer(&constant_desc, nullptr, &m_constant_buffer));
}

bool Renderer::create_geometry()
{
    constexpr std::array<EffectVertex, 6> vertices{
        EffectVertex{{-1.0f, -1.0f}, {0.0f, 1.0f}}, EffectVertex{{-1.0f, 1.0f}, {0.0f, 0.0f}},
        EffectVertex{{1.0f, 1.0f}, {1.0f, 0.0f}}, EffectVertex{{-1.0f, -1.0f}, {0.0f, 1.0f}},
        EffectVertex{{1.0f, 1.0f}, {1.0f, 0.0f}}, EffectVertex{{1.0f, -1.0f}, {1.0f, 1.0f}}
    };
    D3D11_BUFFER_DESC vertex_desc{};
    vertex_desc.ByteWidth = sizeof(vertices);
    vertex_desc.Usage = D3D11_USAGE_IMMUTABLE;
    vertex_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertex_data{vertices.data()};
    if (!succeeded(m_device->CreateBuffer(&vertex_desc, &vertex_data, &m_quad_vertex_buffer)))
        return false;

    D3D11_BUFFER_DESC instance_desc{};
    instance_desc.ByteWidth = sizeof(EffectInstance) * 16384;
    instance_desc.Usage = D3D11_USAGE_DYNAMIC;
    instance_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    instance_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return succeeded(m_device->CreateBuffer(&instance_desc, nullptr, &m_instance_buffer));
}

bool Renderer::create_queries()
{
    const D3D11_QUERY_DESC disjoint{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
    const D3D11_QUERY_DESC timestamp{D3D11_QUERY_TIMESTAMP, 0};
    return succeeded(m_device->CreateQuery(&disjoint, &m_disjoint_query))
        && succeeded(m_device->CreateQuery(&timestamp, &m_frame_start_query))
        && succeeded(m_device->CreateQuery(&timestamp, &m_transparency_start_query))
        && succeeded(m_device->CreateQuery(&timestamp, &m_transparency_end_query))
        && succeeded(m_device->CreateQuery(&timestamp, &m_frame_end_query));
}

bool Renderer::upload_terrain(const MeshRenderView& terrain)
{
    if (terrain.vertices.empty() || terrain.indices.empty())
        return false;
    if (m_terrain_source == terrain.vertices.data())
        return true;

    D3D11_BUFFER_DESC vertex_desc{};
    vertex_desc.ByteWidth = static_cast<UINT>(terrain.vertices.size_bytes());
    vertex_desc.Usage = D3D11_USAGE_IMMUTABLE;
    vertex_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertex_data{terrain.vertices.data()};

    D3D11_BUFFER_DESC index_desc{};
    index_desc.ByteWidth = static_cast<UINT>(terrain.indices.size_bytes());
    index_desc.Usage = D3D11_USAGE_IMMUTABLE;
    index_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA index_data{terrain.indices.data()};

    ComPtr<ID3D11Buffer> vertex_buffer;
    ComPtr<ID3D11Buffer> index_buffer;
    if (!succeeded(m_device->CreateBuffer(&vertex_desc, &vertex_data, &vertex_buffer))
        || !succeeded(m_device->CreateBuffer(&index_desc, &index_data, &index_buffer)))
        return false;
    m_terrain_vertex_buffer = std::move(vertex_buffer);
    m_terrain_index_buffer = std::move(index_buffer);
    m_terrain_index_count = static_cast<std::uint32_t>(terrain.indices.size());
    m_terrain_source = terrain.vertices.data();
    return true;
}

bool Renderer::upload_instances(std::span<const EffectInstance> instances,
    const RenderSettings& settings, double& sort_milliseconds)
{
    if (instances.empty() || instances.size() > 16384)
        return false;
    m_submission_instances.assign(instances.begin(), instances.end());
    if (settings.reverse_submission_order)
        std::reverse(m_submission_instances.begin(), m_submission_instances.end());
    if (settings.transparency_mode == TransparencyMode::ZSortedAlpha)
    {
        const auto start = std::chrono::steady_clock::now();
        const auto& active_camera = m_context->camera_manager().active_camera();
        const auto camera_position = DirectX::XMLoadFloat3(&active_camera.transform().position());
        const auto camera_forward = active_camera.camera().forward(active_camera.transform());
        std::stable_sort(m_submission_instances.begin(), m_submission_instances.end(),
            [camera_position, camera_forward](const EffectInstance& left, const EffectInstance& right)
            {
                const auto depth = [camera_position, camera_forward](const EffectInstance& instance)
                {
                    const auto position = DirectX::XMLoadFloat4(&instance.position_and_billboard);
                    return DirectX::XMVectorGetX(DirectX::XMVector3Dot(
                        DirectX::XMVectorSubtract(position, camera_position), camera_forward));
                };
                return depth(left) > depth(right);
            });
        sort_milliseconds = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!succeeded(m_device_context->Map(m_instance_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return false;
    std::memcpy(mapped.pData, m_submission_instances.data(),
        m_submission_instances.size() * sizeof(EffectInstance));
    m_device_context->Unmap(m_instance_buffer.Get(), 0);
    m_active_instance_count = static_cast<std::uint32_t>(m_submission_instances.size());
    return true;
}

void Renderer::update_frame_constants(const SceneRenderView& scene)
{
    const auto& active_camera = m_context->camera_manager().active_camera();
    const auto& transform = active_camera.transform();
    const auto& camera = active_camera.camera();
    FrameConstants constants{};
    constants.world = scene.terrain.world;
    DirectX::XMStoreFloat4x4(&constants.view_projection, DirectX::XMMatrixTranspose(
        camera.view_matrix(transform) * camera.projection_matrix()));
    DirectX::XMStoreFloat4(&constants.camera_position,
        DirectX::XMVectorSet(transform.position().x, transform.position().y, transform.position().z, 1.0f));
    DirectX::XMStoreFloat4(&constants.camera_right, camera.right(transform));
    DirectX::XMStoreFloat4(&constants.camera_up, camera.up(transform));
    DirectX::XMStoreFloat4(&constants.camera_forward, camera.forward(transform));
    constants.sky_zenith = scene.sky.zenith_color;
    constants.sky_horizon = scene.sky.horizon_color;
    constants.resolution = {
        static_cast<float>(m_scene_width), static_cast<float>(m_scene_height)};
    constants.alpha_mode = 0;
    constants.depth_mode = 1;
    constants.p_alpha = 1.8f;
    constants.k_alpha = 6.0f;
    constants.k_depth = 2.5f;
    m_device_context->UpdateSubresource(m_constant_buffer.Get(), 0, nullptr, &constants, 0, 0);
}

void Renderer::draw_sky()
{
    ID3D11RenderTargetView* target = m_scene_color_rtv.Get();
    m_device_context->OMSetRenderTargets(1, &target, nullptr);
    m_device_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFFu);
    m_device_context->OMSetDepthStencilState(m_depth_disabled_state.Get(), 0);
    m_device_context->IASetInputLayout(nullptr);
    m_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_device_context->VSSetShader(m_fullscreen_vs.Get(), nullptr, 0);
    m_device_context->PSSetShader(m_sky_ps.Get(), nullptr, 0);
    ID3D11Buffer* constants[]{m_constant_buffer.Get()};
    m_device_context->PSSetConstantBuffers(0, 1, constants);
    m_device_context->Draw(3, 0);
}

void Renderer::draw_terrain()
{
    ID3D11RenderTargetView* target = m_scene_color_rtv.Get();
    m_device_context->OMSetRenderTargets(1, &target, m_depth_dsv.Get());
    m_device_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFFu);
    m_device_context->OMSetDepthStencilState(m_depth_write_state.Get(), 0);
    const UINT stride = sizeof(MeshVertex);
    const UINT offset = 0;
    ID3D11Buffer* vertex_buffer = m_terrain_vertex_buffer.Get();
    m_device_context->IASetInputLayout(m_world_input_layout.Get());
    m_device_context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
    m_device_context->IASetIndexBuffer(m_terrain_index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_device_context->VSSetShader(m_world_vs.Get(), nullptr, 0);
    m_device_context->PSSetShader(m_terrain_ps.Get(), nullptr, 0);
    ID3D11Buffer* constants[]{m_constant_buffer.Get()};
    m_device_context->VSSetConstantBuffers(0, 1, constants);
    m_device_context->PSSetConstantBuffers(0, 1, constants);
    m_device_context->RSSetState(m_rasterizer_state.Get());
    m_device_context->DrawIndexed(m_terrain_index_count, 0, 0);
}

void Renderer::bind_effect_pipeline(ID3D11PixelShader* pixel_shader, ID3D11BlendState* blend_state)
{
    const std::array<ID3D11Buffer*, 2> buffers{m_quad_vertex_buffer.Get(), m_instance_buffer.Get()};
    const std::array<UINT, 2> strides{sizeof(EffectVertex), sizeof(EffectInstance)};
    constexpr std::array<UINT, 2> offsets{0, 0};
    m_device_context->IASetInputLayout(m_effect_input_layout.Get());
    m_device_context->IASetVertexBuffers(0, 2, buffers.data(), strides.data(), offsets.data());
    m_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_device_context->VSSetShader(m_effect_vs.Get(), nullptr, 0);
    m_device_context->PSSetShader(pixel_shader, nullptr, 0);
    ID3D11Buffer* constants[]{m_constant_buffer.Get()};
    m_device_context->VSSetConstantBuffers(0, 1, constants);
    m_device_context->PSSetConstantBuffers(0, 1, constants);
    m_device_context->OMSetBlendState(blend_state, nullptr, 0xFFFFFFFFu);
    m_device_context->OMSetDepthStencilState(m_depth_read_state.Get(), 0);
    m_device_context->RSSetState(m_rasterizer_state.Get());
}

void Renderer::draw_alpha()
{
    ID3D11RenderTargetView* target = m_scene_color_rtv.Get();
    m_device_context->OMSetRenderTargets(1, &target, m_depth_dsv.Get());
    bind_effect_pipeline(m_alpha_ps.Get(), m_alpha_blend.Get());
    m_device_context->DrawInstanced(6, m_active_instance_count, 0, 0);
}

void Renderer::draw_wboit_accumulation()
{
    constexpr float zero[4]{};
    m_device_context->ClearRenderTargetView(m_accum_color_rtv.Get(), zero);
    m_device_context->ClearRenderTargetView(m_accum_weight_rtv.Get(), zero);
    const std::array<ID3D11RenderTargetView*, 2> targets{m_accum_color_rtv.Get(), m_accum_weight_rtv.Get()};
    m_device_context->OMSetRenderTargets(2, targets.data(), m_depth_dsv.Get());
    bind_effect_pipeline(m_wboit_ps.Get(), m_additive_blend.Get());
    m_device_context->DrawInstanced(6, m_active_instance_count, 0, 0);
}

void Renderer::draw_wboit_resolve()
{
    ID3D11RenderTargetView* null_targets[2]{};
    m_device_context->OMSetRenderTargets(2, null_targets, nullptr);
    const std::array<ID3D11ShaderResourceView*, 2> resources{m_accum_color_srv.Get(), m_accum_weight_srv.Get()};
    m_device_context->PSSetShaderResources(0, 2, resources.data());
    ID3D11RenderTargetView* target = m_scene_color_rtv.Get();
    m_device_context->OMSetRenderTargets(1, &target, nullptr);
    m_device_context->OMSetBlendState(m_alpha_blend.Get(), nullptr, 0xFFFFFFFFu);
    m_device_context->OMSetDepthStencilState(m_depth_disabled_state.Get(), 0);
    m_device_context->IASetInputLayout(nullptr);
    m_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_device_context->VSSetShader(m_fullscreen_vs.Get(), nullptr, 0);
    m_device_context->PSSetShader(m_resolve_ps.Get(), nullptr, 0);
    m_device_context->Draw(3, 0);
    const std::array<ID3D11ShaderResourceView*, 2> null_resources{};
    m_device_context->PSSetShaderResources(0, 2, null_resources.data());
}

FrameMetrics Renderer::render_frame(const SceneRenderView& scene, const RenderSettings& settings)
{
    FrameMetrics metrics{};
    if (!upload_terrain(scene.terrain)
        || !upload_instances(scene.effects, settings, metrics.cpu_sort_ms))
        return metrics;
    update_frame_constants(scene);

    m_device_context->Begin(m_disjoint_query.Get());
    m_device_context->End(m_frame_start_query.Get());
    m_device_context->RSSetViewports(1, &m_viewport);
    constexpr float clear_color[]{0.02f, 0.05f, 0.12f, 1.0f};
    m_device_context->ClearRenderTargetView(m_scene_color_rtv.Get(), clear_color);
    m_device_context->ClearDepthStencilView(m_depth_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    const auto cpu_start = std::chrono::steady_clock::now();
    draw_sky();
    draw_terrain();
    m_device_context->End(m_transparency_start_query.Get());
    if (settings.transparency_mode == TransparencyMode::Wboit)
    {
        draw_wboit_accumulation();
        m_device_context->End(m_transparency_end_query.Get());
        draw_wboit_resolve();
        metrics.transparency_draw_calls = 2;
        metrics.total_draw_calls = 4;
    }
    else
    {
        draw_alpha();
        m_device_context->End(m_transparency_end_query.Get());
        metrics.transparency_draw_calls = 1;
        metrics.total_draw_calls = 3;
    }
    metrics.cpu_submit_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - cpu_start).count();
    m_device_context->End(m_frame_end_query.Get());
    m_device_context->End(m_disjoint_query.Get());

    if (!m_pending_capture_path.empty())
    {
        save_scene_view_bmp(m_pending_capture_path);
        m_pending_capture_path.clear();
    }

    ID3D11RenderTargetView* back_buffer_target = m_back_buffer_rtv.Get();
    m_device_context->OMSetRenderTargets(1, &back_buffer_target, nullptr);
    m_device_context->RSSetViewports(1, &m_viewport);
    constexpr float ui_clear_color[]{0.035f, 0.045f, 0.06f, 1.0f};
    m_device_context->ClearRenderTargetView(m_back_buffer_rtv.Get(), ui_clear_color);
    m_context->debug_ui_manager().render_draw_data();
    m_swap_chain->Present(0, 0);
    m_device_context->Flush();

    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
    while (m_device_context->GetData(m_disjoint_query.Get(), &disjoint, sizeof(disjoint), 0) == S_FALSE)
        std::this_thread::yield();
    if (!disjoint.Disjoint && disjoint.Frequency > 0)
    {
        UINT64 frame_start{}, transparency_start{}, transparency_end{}, frame_end{};
        const auto wait = [this](ID3D11Query* query, UINT64& timestamp)
        {
            while (m_device_context->GetData(query, &timestamp, sizeof(timestamp), 0) == S_FALSE)
                std::this_thread::yield();
        };
        wait(m_frame_start_query.Get(), frame_start);
        wait(m_transparency_start_query.Get(), transparency_start);
        wait(m_transparency_end_query.Get(), transparency_end);
        wait(m_frame_end_query.Get(), frame_end);
        const double scale = 1000.0 / static_cast<double>(disjoint.Frequency);
        metrics.gpu_total_ms = static_cast<double>(frame_end - frame_start) * scale;
        metrics.gpu_transparency_ms = static_cast<double>(transparency_end - transparency_start) * scale;
        if (settings.transparency_mode == TransparencyMode::Wboit)
            metrics.gpu_resolve_ms = static_cast<double>(frame_end - transparency_end) * scale;
    }
    m_last_metrics = metrics;
    return metrics;
}

void Renderer::request_capture(std::filesystem::path output_path)
{
    m_pending_capture_path = std::move(output_path);
}

void Renderer::save_scene_view_bmp(const std::filesystem::path& path)
{
    m_device_context->CopyResource(m_capture_staging.Get(), m_scene_color_texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(m_device_context->Map(m_capture_staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        return;
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(m_scene_width) * m_scene_height * 4);
    for (std::uint32_t row = 0; row < m_scene_height; ++row)
    {
        const auto* source = static_cast<const std::uint8_t*>(mapped.pData)
            + static_cast<std::size_t>(row) * mapped.RowPitch;
        auto* destination = pixels.data()
            + static_cast<std::size_t>(row) * m_scene_width * 4;
        std::memcpy(destination, source, static_cast<std::size_t>(m_scene_width) * 4);
    }
    m_device_context->Unmap(m_capture_staging.Get(), 0);
    write_bmp(path, m_scene_width, m_scene_height, pixels);
}
}
