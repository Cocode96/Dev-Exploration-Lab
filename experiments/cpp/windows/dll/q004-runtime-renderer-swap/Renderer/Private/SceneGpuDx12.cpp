#include "SceneGpuDx12.h"
#include "Lab_Render_Constant.h"
#include <d3dcompiler.h>
namespace Lab
{
namespace
{
D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
    return b;
}
} // namespace
bool SceneGpuDx12::initialize(ID3D12Device* d, D3D12_CPU_DESCRIPTOR_HANDLE srv)
{
    D3D12_DESCRIPTOR_HEAP_DESC h{};
    h.NumDescriptors = 1;
    h.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(d->CreateDescriptorHeap(&h, IID_PPV_ARGS(&m_rtv))))
        return false;
    h.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    if (FAILED(d->CreateDescriptorHeap(&h, IID_PPV_ARGS(&m_dsv))))
        return false;
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC t{};
    t.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    t.Width = scene_width;
    t.Height = scene_height;
    t.DepthOrArraySize = t.MipLevels = 1;
    t.SampleDesc.Count = 1;
    t.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    t.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE clear{};
    clear.Format = t.Format;
    clear.Color[3] = 1;
    if (FAILED(d->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &t,
                                          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear,
                                          IID_PPV_ARGS(&m_color))))
        return false;
    d->CreateRenderTargetView(m_color.Get(), nullptr, m_rtv->GetCPUDescriptorHandleForHeapStart());
    D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = t.Format;
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.Texture2D.MipLevels = 1;
    d->CreateShaderResourceView(m_color.Get(), &sd, srv);
    t.Format = DXGI_FORMAT_D32_FLOAT;
    t.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    clear.Format = t.Format;
    clear.DepthStencil.Depth = 1;
    if (FAILED(d->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &t, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                          &clear, IID_PPV_ARGS(&m_depth))))
        return false;
    d->CreateDepthStencilView(m_depth.Get(), nullptr, m_dsv->GetCPUDescriptorHandleForHeapStart());
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = vertex_capacity * sizeof(SceneVertex);
    buffer.Height = 1;
    buffer.DepthOrArraySize = buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    if (FAILED(d->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &buffer,
                                          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                          IID_PPV_ARGS(&m_vertices))))
        return false;
    buffer.Width = 16;
    hp.Type = D3D12_HEAP_TYPE_READBACK;
    if (FAILED(d->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST,
                                          nullptr, IID_PPV_ARGS(&m_readback))))
        return false;
    D3D12_QUERY_HEAP_DESC query{};
    query.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    query.Count = 2;
    if (FAILED(d->CreateQueryHeap(&query, IID_PPV_ARGS(&m_queries))))
        return false;
    D3D12_ROOT_SIGNATURE_DESC root{};
    root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> signature, errors, vs, ps;
    if (FAILED(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors)) ||
        FAILED(d->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                      IID_PPV_ARGS(&m_root))))
        return false;
    if (FAILED(D3DCompile(scene_shader, sizeof(scene_shader), nullptr, nullptr, nullptr, "VS", "vs_5_0",
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs, &errors)) ||
        FAILED(D3DCompile(scene_shader, sizeof(scene_shader), nullptr, nullptr, nullptr, "PS", "ps_5_0",
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps, &errors)))
        return false;
    D3D12_INPUT_ELEMENT_DESC input[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = m_root.Get();
    p.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    p.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    p.InputLayout = {input, 2};
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    p.RasterizerState.DepthClipEnable = true;
    p.SampleMask = ~0u;
    p.SampleDesc.Count = 1;
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    p.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    p.DepthStencilState.DepthEnable = true;
    p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    p.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    auto& blend = p.BlendState.RenderTarget[0];
    blend.RenderTargetWriteMask = 15;
    blend.SrcBlend = D3D12_BLEND_ONE;
    blend.DestBlend = D3D12_BLEND_ZERO;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    if (FAILED(d->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&m_opaque))))
        return false;
    p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    blend.BlendEnable = true;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    return SUCCEEDED(d->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&m_alpha)));
}
bool SceneGpuDx12::render(ID3D12GraphicsCommandList* list, const SceneFrame& frame)
{
    if (!frame.vertices || frame.count > vertex_capacity)
        return false;
    void* mapped{};
    D3D12_RANGE empty{};
    if (FAILED(m_vertices->Map(0, &empty, &mapped)))
        return false;
    memcpy(mapped, frame.vertices, frame.count * sizeof(SceneVertex));
    m_vertices->Unmap(0, nullptr);
    list->EndQuery(m_queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
    auto barrier = transition(m_color.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                              D3D12_RESOURCE_STATE_RENDER_TARGET);
    list->ResourceBarrier(1, &barrier);
    auto rtv = m_rtv->GetCPUDescriptorHandleForHeapStart(), dsv = m_dsv->GetCPUDescriptorHandleForHeapStart();
    const float background[] = {0.025f, 0.04f, 0.06f, 1};
    list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    list->ClearRenderTargetView(rtv, background, 0, nullptr);
    list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
    D3D12_VIEWPORT vp{0, 0, float(scene_width), float(scene_height), 0, 1};
    D3D12_RECT rect{0, 0, scene_width, scene_height};
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &rect);
    list->SetGraphicsRootSignature(m_root.Get());
    list->SetPipelineState(m_opaque.Get());
    D3D12_VERTEX_BUFFER_VIEW view{m_vertices->GetGPUVirtualAddress(), frame.count * sizeof(SceneVertex),
                                  sizeof(SceneVertex)};
    list->IASetVertexBuffers(0, 1, &view);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(frame.opaque_count, 1, 0, 0);
    list->SetPipelineState(m_alpha.Get());
    list->DrawInstanced(frame.count - frame.opaque_count, 1, frame.opaque_count, 0);
    barrier = transition(m_color.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    list->ResourceBarrier(1, &barrier);
    list->EndQuery(m_queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
    list->ResolveQueryData(m_queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, m_readback.Get(), 0);
    return true;
}
double SceneGpuDx12::milliseconds(ID3D12CommandQueue* queue)
{
    // 호출자는 Fence 완료 후 읽는다. 미완료 GPU 결과를 CPU가 읽지 않게 한다.
    UINT64 frequency{};
    if (FAILED(queue->GetTimestampFrequency(&frequency)) || !frequency)
        return -1;
    UINT64* ticks{};
    D3D12_RANGE range{0, 16};
    if (FAILED(m_readback->Map(0, &range, reinterpret_cast<void**>(&ticks))))
        return -1;
    const double result = double(ticks[1] - ticks[0]) * 1000.0 / double(frequency);
    D3D12_RANGE empty{};
    m_readback->Unmap(0, &empty);
    return result;
}
} // namespace Lab
