#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <DirectXMath.h>
#include <Windows.h>
#include <wrl/client.h>
#include <d3d11.h>

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;

struct EFFECT_UPDATE_DATA
{
    DirectX::XMFLOAT4 vPosition;
    DirectX::XMFLOAT4 vVelocity;
    DirectX::XMFLOAT4 vRotation;
    DirectX::XMFLOAT4 vAngularVelocity;
    DirectX::XMFLOAT4 vScaleLife;
    DirectX::XMFLOAT4X4 matWorld;
};

struct BENCHMARK_RESULT
{
    uint32_t iUpdateCount = 0;
    double fCPUSingleUS = 0.0;
    double fCPUWorkersUS = 0.0;
    double fGPUSubmitUS = 0.0;
    double fGPUKernelUS = 0.0;
    double fGPUWallUS = 0.0;
};

class CUpdateWorkerPool final
{
public:
    explicit CUpdateWorkerPool(uint32_t iWorkerCount);
    ~CUpdateWorkerPool();

public:
    void Update(std::span<EFFECT_UPDATE_DATA> Datas, float fTimeDelta);

private:
    void Worker_Main(uint32_t iWorkerIndex);

private:
    std::vector<std::thread> m_Workers;
    std::mutex m_Mutex;
    std::condition_variable m_WorkCondition;
    std::condition_variable m_CompleteCondition;
    std::span<EFFECT_UPDATE_DATA> m_UpdateDatas;
    float m_fTimeDelta = 0.f;
    uint64_t m_iWorkGeneration = 0;
    uint32_t m_iCompleteWorkerCount = 0;
    uint32_t m_iActiveWorkerCount = 1;
    bool m_bStop = false;
};

class CUpdateBenchmark final
{
    friend class CUpdateWorkerPool;

public:
    CUpdateBenchmark();

public:
    HRESULT Initialize(const std::filesystem::path& ShaderPath);
    HRESULT Run(const std::filesystem::path& ReportPath);

private:
    static void Update_Range(std::span<EFFECT_UPDATE_DATA> Datas, float fTimeDelta);
    static std::vector<EFFECT_UPDATE_DATA> Create_UpdateDatas(uint32_t iUpdateCount);
    static double Get_Median(std::vector<double> Values);
    static double Get_Checksum(std::span<const EFFECT_UPDATE_DATA> Datas);

private:
    HRESULT Ready_ComputeShader(const std::filesystem::path& ShaderPath);
    HRESULT Ready_GPUBuffer(std::span<const EFFECT_UPDATE_DATA> Datas);
    HRESULT Ready_TimestampQueries();
    HRESULT Measure_GPU(uint32_t iUpdateCount, uint32_t iFrameCount, double& fSubmitUS, double& fKernelUS, double& fWallUS);
    HRESULT Wait_Query(ID3D11Query* pQuery, void* pData, uint32_t iDataSize);

private:
    ComPtr<ID3D11Device> m_pDevice;
    ComPtr<ID3D11DeviceContext> m_pContext;
    ComPtr<ID3D11ComputeShader> m_pComputeShader;
    ComPtr<ID3D11Buffer> m_pUpdateBuffer;
    ComPtr<ID3D11UnorderedAccessView> m_pUpdateUAV;
    ComPtr<ID3D11Buffer> m_pFrameBuffer;
    ComPtr<ID3D11Query> m_pDisjointQuery;
    ComPtr<ID3D11Query> m_pStartQuery;
    ComPtr<ID3D11Query> m_pEndQuery;
    CUpdateWorkerPool m_WorkerPool;
};
