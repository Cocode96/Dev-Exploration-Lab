#include "SceneGpuDx11.h"
#include "Lab_Render_Constant.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <chrono>
namespace Lab
{
bool SceneGpuDx11::initialize(ID3D11Device* d)
{
    D3D11_TEXTURE2D_DESC t{};
    t.Width = scene_width;
    t.Height = scene_height;
    t.MipLevels = t.ArraySize = 1;
    t.SampleDesc.Count = 1;
    t.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    t.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(d->CreateTexture2D(&t, nullptr, &m_color)) ||
        FAILED(d->CreateRenderTargetView(m_color.Get(), nullptr, &m_rtv)) ||
        FAILED(d->CreateShaderResourceView(m_color.Get(), nullptr, &m_srv)))
        return false;
    t.Format = DXGI_FORMAT_D32_FLOAT;
    t.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(d->CreateTexture2D(&t, nullptr, &m_depth)) ||
        FAILED(d->CreateDepthStencilView(m_depth.Get(), nullptr, &m_dsv)))
        return false;
    D3D11_BUFFER_DESC b{};
    b.ByteWidth = vertex_capacity * sizeof(SceneVertex);
    b.Usage = D3D11_USAGE_DYNAMIC;
    b.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    b.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(d->CreateBuffer(&b, nullptr, &m_vertices)))
        return false;
    ComPtr<ID3DBlob> vs, ps, errors;
    if (FAILED(D3DCompile(scene_shader, sizeof(scene_shader), nullptr, nullptr, nullptr, "VS", "vs_5_0",
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs, &errors)) ||
        FAILED(D3DCompile(scene_shader, sizeof(scene_shader), nullptr, nullptr, nullptr, "PS", "ps_5_0",
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps, &errors)))
        return false;
    D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    if (FAILED(d->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &m_vs)) ||
        FAILED(d->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &m_ps)) ||
        FAILED(d->CreateInputLayout(elements, 2, vs->GetBufferPointer(), vs->GetBufferSize(), &m_layout)))
        return false;
    D3D11_RASTERIZER_DESC r{};
    r.FillMode = D3D11_FILL_SOLID;
    r.CullMode = D3D11_CULL_NONE;
    r.DepthClipEnable = true;
    if (FAILED(d->CreateRasterizerState(&r, &m_raster)))
        return false;
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = true;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(d->CreateDepthStencilState(&depth, &m_opaque_depth)))
        return false;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    if (FAILED(d->CreateDepthStencilState(&depth, &m_alpha_depth)))
        return false;
    D3D11_BLEND_DESC blend{};
    auto& rt = blend.RenderTarget[0];
    rt.BlendEnable = true;
    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = 15;
    if (FAILED(d->CreateBlendState(&blend, &m_blend)))
        return false;
    D3D11_QUERY_DESC q{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
    if (FAILED(d->CreateQuery(&q, &m_disjoint)))
        return false;
    q.Query = D3D11_QUERY_TIMESTAMP;
    return SUCCEEDED(d->CreateQuery(&q, &m_start)) && SUCCEEDED(d->CreateQuery(&q, &m_end));
}
double SceneGpuDx11::render(ID3D11DeviceContext* c, const SceneFrame& frame, bool wait_for_result)
{
    if (!frame.vertices || frame.count > vertex_capacity)
        return -1;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(c->Map(m_vertices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return -1;
    memcpy(mapped.pData, frame.vertices, frame.count * sizeof(SceneVertex));
    c->Unmap(m_vertices.Get(), 0);
    ID3D11ShaderResourceView* null_view = nullptr;
    c->PSSetShaderResources(0, 1, &null_view);
    c->Begin(m_disjoint.Get());
    c->End(m_start.Get());
    c->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());
    const float background[] = {0.025f, 0.04f, 0.06f, 1};
    c->ClearRenderTargetView(m_rtv.Get(), background);
    c->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1, 0);
    D3D11_VIEWPORT viewport{0, 0, float(scene_width), float(scene_height), 0, 1};
    c->RSSetViewports(1, &viewport);
    c->RSSetState(m_raster.Get());
    unsigned stride = sizeof(SceneVertex), offset = 0;
    c->IASetVertexBuffers(0, 1, m_vertices.GetAddressOf(), &stride, &offset);
    c->IASetInputLayout(m_layout.Get());
    c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    c->VSSetShader(m_vs.Get(), nullptr, 0);
    c->PSSetShader(m_ps.Get(), nullptr, 0);
    c->OMSetBlendState(nullptr, nullptr, ~0u);
    c->OMSetDepthStencilState(m_opaque_depth.Get(), 0);
    c->Draw(frame.opaque_count, 0);
    c->OMSetBlendState(m_blend.Get(), nullptr, ~0u);
    c->OMSetDepthStencilState(m_alpha_depth.Get(), 0);
    c->Draw(frame.count - frame.opaque_count, frame.opaque_count);
    c->End(m_end.Get());
    c->End(m_disjoint.Get());
    c->Flush();
    if (!wait_for_result)
        return -1;
    // 장면 GPU 시간만 측정한다. 쿼리 대기는 CPU 프레임 시간에 포함된다.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
    UINT64 start{}, end{};
    while (c->GetData(m_disjoint.Get(), &disjoint, sizeof(disjoint), 0) != S_OK)
    {
        if (std::chrono::steady_clock::now() > deadline)
            return -1;
        Sleep(0);
    }
    if (disjoint.Disjoint || !disjoint.Frequency ||
        c->GetData(m_start.Get(), &start, sizeof(start), 0) != S_OK ||
        c->GetData(m_end.Get(), &end, sizeof(end), 0) != S_OK)
        return -1;
    return double(end - start) * 1000.0 / double(disjoint.Frequency);
}
} // namespace Lab
