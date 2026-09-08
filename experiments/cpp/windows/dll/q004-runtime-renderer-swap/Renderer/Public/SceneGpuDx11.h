#pragma once
#include "Lab_Struct.h"
#include <d3d11.h>
#include <wrl/client.h>
namespace Lab
{
using Microsoft::WRL::ComPtr;
class SceneGpuDx11 final
{
  public:
    bool initialize(ID3D11Device* device);
    double render(ID3D11DeviceContext* context, const SceneFrame& frame, bool wait_for_result = true);
    ID3D11ShaderResourceView* view() const { return m_srv.Get(); }

  private:
    ComPtr<ID3D11Texture2D> m_color, m_depth;
    ComPtr<ID3D11RenderTargetView> m_rtv;
    ComPtr<ID3D11DepthStencilView> m_dsv;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    ComPtr<ID3D11Buffer> m_vertices;
    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_ps;
    ComPtr<ID3D11InputLayout> m_layout;
    ComPtr<ID3D11RasterizerState> m_raster;
    ComPtr<ID3D11DepthStencilState> m_opaque_depth, m_alpha_depth;
    ComPtr<ID3D11BlendState> m_blend;
    ComPtr<ID3D11Query> m_disjoint, m_start, m_end;
};
} // namespace Lab
