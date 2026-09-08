#pragma once
#include "Lab_Struct.h"
#include <d3d12.h>
#include <wrl/client.h>
namespace Lab
{
using Microsoft::WRL::ComPtr;
class SceneGpuDx12 final
{
  public:
    bool initialize(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srv);
    bool render(ID3D12GraphicsCommandList* list, const SceneFrame& frame);
    double milliseconds(ID3D12CommandQueue* queue);

  private:
    ComPtr<ID3D12Resource> m_color, m_depth, m_vertices, m_readback;
    ComPtr<ID3D12DescriptorHeap> m_rtv, m_dsv;
    ComPtr<ID3D12RootSignature> m_root;
    ComPtr<ID3D12PipelineState> m_opaque, m_alpha;
    ComPtr<ID3D12QueryHeap> m_queries;
};
} // namespace Lab
