#pragma once

#include "Engine_Struct.h"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>
#include <vector>

namespace Engine
{
using namespace std;
using Microsoft::WRL::ComPtr;

class ApplicationContext;

class Renderer final
{
public:
    Renderer() = default;
    ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool initialize(ApplicationContext& context, HWND window, uint32_t width,
        uint32_t height, const filesystem::path& shader_path);
    bool prepare_scene_view();
    void request_scene_view_size(uint32_t width, uint32_t height) noexcept;
    FrameMetrics render_frame(const SceneRenderView& scene, const RenderSettings& settings);
    void request_capture(filesystem::path output_path);

    const FrameMetrics& last_metrics() const noexcept { return m_last_metrics; }
    const wstring& adapter_name() const noexcept { return m_adapter_name; }
    uint32_t width() const noexcept { return m_width; }
    uint32_t height() const noexcept { return m_height; }
    ID3D11Device* device() const noexcept { return m_device.Get(); }
    ID3D11DeviceContext* device_context() const noexcept { return m_device_context.Get(); }
    ID3D11ShaderResourceView* scene_texture_srv() const noexcept { return m_scene_color_srv.Get(); }
    uint32_t scene_width() const noexcept { return m_scene_width; }
    uint32_t scene_height() const noexcept { return m_scene_height; }

private:
    bool create_device_and_swap_chain(HWND window);
    bool create_render_targets();
    bool create_scene_render_targets(uint32_t width, uint32_t height);
    bool create_pipeline(const filesystem::path& shader_path);
    bool create_geometry();
    bool create_queries();
    bool upload_terrain(const MeshRenderView& terrain);
    bool upload_instances(span<const EffectInstance> instances,
        const RenderSettings& settings, double& sort_milliseconds);
    void update_frame_constants(const SceneRenderView& scene);

    void draw_sky();
    void draw_terrain();
    void draw_alpha();
    void draw_wboit_accumulation();
    void draw_wboit_resolve();
    void bind_effect_pipeline(ID3D11PixelShader* pixel_shader, ID3D11BlendState* blend_state);
    void save_scene_view_bmp(const filesystem::path& path);

    ApplicationContext* m_context{};
    uint32_t m_width{};
    uint32_t m_height{};
    uint32_t m_scene_width{};
    uint32_t m_scene_height{};
    uint32_t m_requested_scene_width{};
    uint32_t m_requested_scene_height{};
    uint32_t m_active_instance_count{};
    uint32_t m_terrain_index_count{};
    const MeshVertex* m_terrain_source{};
    wstring m_adapter_name;
    filesystem::path m_pending_capture_path;
    FrameMetrics m_last_metrics{};
    D3D11_VIEWPORT m_viewport{};
    vector<EffectInstance> m_submission_instances;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_device_context;
    ComPtr<IDXGISwapChain1> m_swap_chain;
    ComPtr<ID3D11Texture2D> m_back_buffer;
    ComPtr<ID3D11RenderTargetView> m_back_buffer_rtv;
    ComPtr<ID3D11Texture2D> m_scene_color_texture;
    ComPtr<ID3D11RenderTargetView> m_scene_color_rtv;
    ComPtr<ID3D11ShaderResourceView> m_scene_color_srv;
    ComPtr<ID3D11Texture2D> m_depth_texture;
    ComPtr<ID3D11DepthStencilView> m_depth_dsv;
    ComPtr<ID3D11Texture2D> m_capture_staging;
    ComPtr<ID3D11Texture2D> m_accum_color_texture;
    ComPtr<ID3D11RenderTargetView> m_accum_color_rtv;
    ComPtr<ID3D11ShaderResourceView> m_accum_color_srv;
    ComPtr<ID3D11Texture2D> m_accum_weight_texture;
    ComPtr<ID3D11RenderTargetView> m_accum_weight_rtv;
    ComPtr<ID3D11ShaderResourceView> m_accum_weight_srv;
    ComPtr<ID3D11VertexShader> m_effect_vs;
    ComPtr<ID3D11VertexShader> m_world_vs;
    ComPtr<ID3D11VertexShader> m_fullscreen_vs;
    ComPtr<ID3D11PixelShader> m_alpha_ps;
    ComPtr<ID3D11PixelShader> m_wboit_ps;
    ComPtr<ID3D11PixelShader> m_sky_ps;
    ComPtr<ID3D11PixelShader> m_terrain_ps;
    ComPtr<ID3D11PixelShader> m_resolve_ps;
    ComPtr<ID3D11InputLayout> m_effect_input_layout;
    ComPtr<ID3D11InputLayout> m_world_input_layout;
    ComPtr<ID3D11BlendState> m_alpha_blend;
    ComPtr<ID3D11BlendState> m_additive_blend;
    ComPtr<ID3D11RasterizerState> m_rasterizer_state;
    ComPtr<ID3D11DepthStencilState> m_depth_write_state;
    ComPtr<ID3D11DepthStencilState> m_depth_read_state;
    ComPtr<ID3D11DepthStencilState> m_depth_disabled_state;
    ComPtr<ID3D11Buffer> m_constant_buffer;
    ComPtr<ID3D11Buffer> m_quad_vertex_buffer;
    ComPtr<ID3D11Buffer> m_instance_buffer;
    ComPtr<ID3D11Buffer> m_terrain_vertex_buffer;
    ComPtr<ID3D11Buffer> m_terrain_index_buffer;
    ComPtr<ID3D11Query> m_disjoint_query;
    ComPtr<ID3D11Query> m_frame_start_query;
    ComPtr<ID3D11Query> m_transparency_start_query;
    ComPtr<ID3D11Query> m_transparency_end_query;
    ComPtr<ID3D11Query> m_frame_end_query;
};
}
