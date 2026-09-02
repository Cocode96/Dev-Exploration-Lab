#pragma once

#include "EngineTypes.h"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>
#include <vector>

namespace Engine
{
class ApplicationContext;

class Renderer final
{
public:
    Renderer() = default;
    ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool initialize(ApplicationContext& context, HWND window, std::uint32_t width,
        std::uint32_t height, const std::filesystem::path& shader_path);
    bool prepare_scene_view();
    void request_scene_view_size(std::uint32_t width, std::uint32_t height) noexcept;
    FrameMetrics render_frame(const SceneRenderView& scene, const RenderSettings& settings);
    void request_capture(std::filesystem::path output_path);

    const FrameMetrics& last_metrics() const noexcept { return m_last_metrics; }
    const std::wstring& adapter_name() const noexcept { return m_adapter_name; }
    std::uint32_t width() const noexcept { return m_width; }
    std::uint32_t height() const noexcept { return m_height; }
    ID3D11Device* device() const noexcept { return m_device.Get(); }
    ID3D11DeviceContext* device_context() const noexcept { return m_device_context.Get(); }
    ID3D11ShaderResourceView* scene_texture_srv() const noexcept { return m_scene_color_srv.Get(); }
    std::uint32_t scene_width() const noexcept { return m_scene_width; }
    std::uint32_t scene_height() const noexcept { return m_scene_height; }

private:
    bool create_device_and_swap_chain(HWND window);
    bool create_render_targets();
    bool create_scene_render_targets(std::uint32_t width, std::uint32_t height);
    bool create_pipeline(const std::filesystem::path& shader_path);
    bool create_geometry();
    bool create_queries();
    bool upload_terrain(const MeshRenderView& terrain);
    bool upload_instances(std::span<const EffectInstance> instances,
        const RenderSettings& settings, double& sort_milliseconds);
    void update_frame_constants(const SceneRenderView& scene);

    void draw_sky();
    void draw_terrain();
    void draw_alpha();
    void draw_wboit_accumulation();
    void draw_wboit_resolve();
    void bind_effect_pipeline(ID3D11PixelShader* pixel_shader, ID3D11BlendState* blend_state);
    void save_scene_view_bmp(const std::filesystem::path& path);

    ApplicationContext* m_context{};
    std::uint32_t m_width{};
    std::uint32_t m_height{};
    std::uint32_t m_scene_width{};
    std::uint32_t m_scene_height{};
    std::uint32_t m_requested_scene_width{};
    std::uint32_t m_requested_scene_height{};
    std::uint32_t m_active_instance_count{};
    std::uint32_t m_terrain_index_count{};
    const MeshVertex* m_terrain_source{};
    std::wstring m_adapter_name;
    std::filesystem::path m_pending_capture_path;
    FrameMetrics m_last_metrics{};
    D3D11_VIEWPORT m_viewport{};
    std::vector<EffectInstance> m_submission_instances;

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_device_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swap_chain;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_back_buffer;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_back_buffer_rtv;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_scene_color_texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_scene_color_rtv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_scene_color_srv;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depth_texture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depth_dsv;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_capture_staging;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_accum_color_texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_accum_color_rtv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_accum_color_srv;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_accum_weight_texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_accum_weight_rtv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_accum_weight_srv;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_effect_vs;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_world_vs;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_fullscreen_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_alpha_ps;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_wboit_ps;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_sky_ps;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_terrain_ps;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_resolve_ps;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_effect_input_layout;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_world_input_layout;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_alpha_blend;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_additive_blend;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizer_state;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depth_write_state;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depth_read_state;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depth_disabled_state;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constant_buffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_quad_vertex_buffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_instance_buffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_terrain_vertex_buffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_terrain_index_buffer;
    Microsoft::WRL::ComPtr<ID3D11Query> m_disjoint_query;
    Microsoft::WRL::ComPtr<ID3D11Query> m_frame_start_query;
    Microsoft::WRL::ComPtr<ID3D11Query> m_transparency_start_query;
    Microsoft::WRL::ComPtr<ID3D11Query> m_transparency_end_query;
    Microsoft::WRL::ComPtr<ID3D11Query> m_frame_end_query;
};
}
