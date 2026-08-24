#include "UpdateBenchmark.h"

#include <d3dcompiler.h>
#include <dxgi.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>

using namespace DirectX;

namespace
{
    constexpr float TIME_DELTA = 1.f / 60.f;
    constexpr uint32_t WARM_UP_FRAME_COUNT = 30;
    constexpr uint32_t MEASURE_FRAME_COUNT = 120;
    constexpr uint32_t REPEAT_COUNT = 7;
    constexpr uint32_t COMPUTE_THREAD_COUNT = 256;
    constexpr uint32_t MIN_UPDATE_COUNT_PER_WORKER = 2048;

    uint32_t Get_WorkerCount()
    {
        uint32_t iHardwareThreadCount = std::thread::hardware_concurrency();
        return iHardwareThreadCount > 1 ? iHardwareThreadCount - 1 : 1;
    }

    struct FRAME_DESC
    {
        float fTimeDelta = 0.f;
        uint32_t iUpdateCount = 0;
        float vPadding[2] = {};
    };

    struct BENCHMARK_SAMPLE
    {
        uint32_t iUpdateCount = 0;
        uint32_t iRepeatIndex = 0;
        double fCPUSingleUS = 0.0;
        double fCPUWorkersUS = 0.0;
        double fGPUSubmitUS = 0.0;
        double fGPUKernelUS = 0.0;
        double fGPUWallUS = 0.0;
    };

    template<typename Function>
    double Measure_AverageUS(Function&& Execute, uint32_t iFrameCount)
    {
        auto StartTime = std::chrono::steady_clock::now();

        for (uint32_t i = 0; i < iFrameCount; ++i)
            Execute();

        auto EndTime = std::chrono::steady_clock::now();
        double fTotalUS = std::chrono::duration<double, std::micro>(EndTime - StartTime).count();
        return fTotalUS / static_cast<double>(iFrameCount);
    }
}

CUpdateWorkerPool::CUpdateWorkerPool(uint32_t iWorkerCount)
{
    iWorkerCount = std::max(1u, iWorkerCount);

    for (uint32_t i = 0; i < iWorkerCount; ++i)
        m_Workers.emplace_back(&CUpdateWorkerPool::Worker_Main, this, i);
}

CUpdateWorkerPool::~CUpdateWorkerPool()
{
    {
        std::lock_guard Lock{ m_Mutex };
        m_bStop = true;
        ++m_iWorkGeneration;
    }

    m_WorkCondition.notify_all();

    for (std::thread& Worker : m_Workers)
        Worker.join();
}

void CUpdateWorkerPool::Update(std::span<EFFECT_UPDATE_DATA> Datas, float fTimeDelta)
{
    {
        std::lock_guard Lock{ m_Mutex };
        m_UpdateDatas = Datas;
        m_fTimeDelta = fTimeDelta;
        m_iCompleteWorkerCount = 0;
        m_iActiveWorkerCount = std::clamp(
            static_cast<uint32_t>((Datas.size() + MIN_UPDATE_COUNT_PER_WORKER - 1) / MIN_UPDATE_COUNT_PER_WORKER),
            1u, static_cast<uint32_t>(m_Workers.size()));
        ++m_iWorkGeneration;
    }

    m_WorkCondition.notify_all();

    std::unique_lock Lock{ m_Mutex };
    m_CompleteCondition.wait(Lock, [this]() { return m_iCompleteWorkerCount == m_iActiveWorkerCount; });
}

void CUpdateWorkerPool::Worker_Main(uint32_t iWorkerIndex)
{
    uint64_t iLastGeneration = 0;

    while (true)
    {
        std::span<EFFECT_UPDATE_DATA> UpdateDatas;
        float fTimeDelta = 0.f;

        {
            std::unique_lock Lock{ m_Mutex };
            m_WorkCondition.wait(Lock, [this, iLastGeneration]() { return m_bStop || m_iWorkGeneration != iLastGeneration; });

            if (m_bStop)
                return;

            iLastGeneration = m_iWorkGeneration;

            // 작은 Update에 모든 스레드를 깨우지 않고 최소 청크 크기만큼 작업을 묶는다.
            if (iWorkerIndex >= m_iActiveWorkerCount)
                continue;

            size_t iBeginIndex = (m_UpdateDatas.size() * iWorkerIndex) / m_iActiveWorkerCount;
            size_t iEndIndex = (m_UpdateDatas.size() * (iWorkerIndex + 1)) / m_iActiveWorkerCount;
            UpdateDatas = m_UpdateDatas.subspan(iBeginIndex, iEndIndex - iBeginIndex);
            fTimeDelta = m_fTimeDelta;
        }

        CUpdateBenchmark::Update_Range(UpdateDatas, fTimeDelta);

        {
            std::lock_guard Lock{ m_Mutex };
            ++m_iCompleteWorkerCount;

            if (m_iCompleteWorkerCount == m_iActiveWorkerCount)
                m_CompleteCondition.notify_one();
        }
    }
}

CUpdateBenchmark::CUpdateBenchmark()
    : m_WorkerPool{ Get_WorkerCount() }
{
}

HRESULT CUpdateBenchmark::Initialize(const std::filesystem::path& ShaderPath)
{
    UINT iCreateFlags = 0;

#ifdef _DEBUG
    iCreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL FeatureLevel = {};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, iCreateFlags,
        nullptr, 0, D3D11_SDK_VERSION, m_pDevice.GetAddressOf(), &FeatureLevel, m_pContext.GetAddressOf());

    if (FAILED(hr) || FeatureLevel < D3D_FEATURE_LEVEL_11_0)
        return E_FAIL;

    ComPtr<IDXGIDevice> pDXGIDevice;
    ComPtr<IDXGIAdapter> pAdapter;
    DXGI_ADAPTER_DESC AdapterDesc = {};

    if (SUCCEEDED(m_pDevice.As(&pDXGIDevice)) &&
        SUCCEEDED(pDXGIDevice->GetAdapter(pAdapter.GetAddressOf())) &&
        SUCCEEDED(pAdapter->GetDesc(&AdapterDesc)))
    {
        std::wcout << L"GPU: " << AdapterDesc.Description << std::endl;
    }

    if (FAILED(Ready_ComputeShader(ShaderPath)))
        return E_FAIL;

    if (FAILED(Ready_TimestampQueries()))
        return E_FAIL;

    D3D11_BUFFER_DESC FrameBufferDesc = {};
    FrameBufferDesc.ByteWidth = sizeof(FRAME_DESC);
    FrameBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    FrameBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    FrameBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return m_pDevice->CreateBuffer(&FrameBufferDesc, nullptr, m_pFrameBuffer.GetAddressOf());
}

HRESULT CUpdateBenchmark::Run(const std::filesystem::path& ReportPath)
{
    const std::vector<uint32_t> UpdateCounts = { 64, 128, 200, 256, 512, 1024, 4096, 16384, 65536, 262144 };
    std::vector<BENCHMARK_RESULT> Results;
    std::vector<BENCHMARK_SAMPLE> Samples;

    std::cout << "DX11 Update Break-even Experiment" << std::endl;
    std::cout << "Worker count: " << Get_WorkerCount() << std::endl;

    for (uint32_t iUpdateCount : UpdateCounts)
    {
        std::vector<double> CPUSingleValues;
        std::vector<double> CPUWorkerValues;
        std::vector<double> GPUSubmitValues;
        std::vector<double> GPUKernelValues;
        std::vector<double> GPUWallValues;

        auto SingleDatas = Create_UpdateDatas(iUpdateCount);
        auto WorkerDatas = SingleDatas;

        if (FAILED(Ready_GPUBuffer(SingleDatas)))
            return E_FAIL;

        for (uint32_t i = 0; i < WARM_UP_FRAME_COUNT; ++i)
        {
            Update_Range(SingleDatas, TIME_DELTA);
            m_WorkerPool.Update(WorkerDatas, TIME_DELTA);
        }

        double fDummySubmit = 0.0;
        double fDummyKernel = 0.0;
        double fDummyWall = 0.0;
        if (FAILED(Measure_GPU(iUpdateCount, WARM_UP_FRAME_COUNT, fDummySubmit, fDummyKernel, fDummyWall)))
            return E_FAIL;

        for (uint32_t iRepeat = 0; iRepeat < REPEAT_COUNT; ++iRepeat)
        {
            double fCPUSingleUS = Measure_AverageUS([&]() { Update_Range(SingleDatas, TIME_DELTA); }, MEASURE_FRAME_COUNT);
            double fCPUWorkersUS = Measure_AverageUS([&]() { m_WorkerPool.Update(WorkerDatas, TIME_DELTA); }, MEASURE_FRAME_COUNT);

            double fGPUSubmitUS = 0.0;
            double fGPUKernelUS = 0.0;
            double fGPUWallUS = 0.0;

            if (FAILED(Measure_GPU(iUpdateCount, MEASURE_FRAME_COUNT, fGPUSubmitUS, fGPUKernelUS, fGPUWallUS)))
                return E_FAIL;

            CPUSingleValues.push_back(fCPUSingleUS);
            CPUWorkerValues.push_back(fCPUWorkersUS);
            GPUSubmitValues.push_back(fGPUSubmitUS);
            GPUKernelValues.push_back(fGPUKernelUS);
            GPUWallValues.push_back(fGPUWallUS);

            Samples.push_back({ iUpdateCount, iRepeat, fCPUSingleUS, fCPUWorkersUS,
                fGPUSubmitUS, fGPUKernelUS, fGPUWallUS });
        }

        BENCHMARK_RESULT Result;
        Result.iUpdateCount = iUpdateCount;
        Result.fCPUSingleUS = Get_Median(CPUSingleValues);
        Result.fCPUWorkersUS = Get_Median(CPUWorkerValues);
        Result.fGPUSubmitUS = Get_Median(GPUSubmitValues);
        Result.fGPUKernelUS = Get_Median(GPUKernelValues);
        Result.fGPUWallUS = Get_Median(GPUWallValues);
        Results.push_back(Result);

        // CPU 계산이 최적화로 제거되지 않았는지 마지막 데이터에서 값을 확인한다.
        double fChecksum = Get_Checksum(SingleDatas) + Get_Checksum(WorkerDatas);

        std::cout << std::setw(7) << iUpdateCount
            << " | single " << std::fixed << std::setprecision(3) << Result.fCPUSingleUS << " us"
            << " | workers " << Result.fCPUWorkersUS << " us"
            << " | gpu kernel " << Result.fGPUKernelUS << " us"
            << " | gpu wall " << Result.fGPUWallUS << " us"
            << " | checksum " << fChecksum << std::endl;
    }

    std::filesystem::create_directories(ReportPath.parent_path());
    std::ofstream RawReport{ ReportPath };

    if (!RawReport)
        return E_FAIL;

    RawReport << "update_count,repeat_index,cpu_single_us,cpu_workers_us,gpu_submit_us,gpu_kernel_us,gpu_wall_us\n";
    RawReport << std::fixed << std::setprecision(6);

    for (const BENCHMARK_SAMPLE& Sample : Samples)
    {
        RawReport << Sample.iUpdateCount << ',' << Sample.iRepeatIndex << ','
            << Sample.fCPUSingleUS << ',' << Sample.fCPUWorkersUS << ','
            << Sample.fGPUSubmitUS << ',' << Sample.fGPUKernelUS << ',' << Sample.fGPUWallUS << '\n';
    }

    std::filesystem::path SummaryPath = ReportPath.parent_path() / L"summary_results.csv";
    std::ofstream SummaryReport{ SummaryPath };

    if (!SummaryReport)
        return E_FAIL;

    SummaryReport << "update_count,cpu_single_us,cpu_workers_us,gpu_submit_us,gpu_kernel_us,gpu_wall_us\n";
    SummaryReport << std::fixed << std::setprecision(6);

    for (const BENCHMARK_RESULT& Result : Results)
    {
        SummaryReport << Result.iUpdateCount << ','
            << Result.fCPUSingleUS << ','
            << Result.fCPUWorkersUS << ','
            << Result.fGPUSubmitUS << ','
            << Result.fGPUKernelUS << ','
            << Result.fGPUWallUS << '\n';
    }

    std::cout << "Raw report: " << ReportPath.string() << std::endl;
    std::cout << "Summary report: " << SummaryPath.string() << std::endl;
    return S_OK;
}

void CUpdateBenchmark::Update_Range(std::span<EFFECT_UPDATE_DATA> Datas, float fTimeDelta)
{
    const XMVECTOR Gravity = XMVectorSet(0.f, -9.8f, 0.f, 0.f);

    for (EFFECT_UPDATE_DATA& Data : Datas)
    {
        XMVECTOR Velocity = XMLoadFloat4(&Data.vVelocity);
        XMVECTOR Position = XMLoadFloat4(&Data.vPosition);
        XMVECTOR AngularVelocity = XMLoadFloat4(&Data.vAngularVelocity);
        XMVECTOR Rotation = XMLoadFloat4(&Data.vRotation);

        Velocity = XMVectorMultiplyAdd(Gravity, XMVectorReplicate(fTimeDelta), Velocity);
        Position = XMVectorMultiplyAdd(Velocity, XMVectorReplicate(fTimeDelta), Position);
        Rotation = XMVectorMultiplyAdd(AngularVelocity, XMVectorReplicate(fTimeDelta), Rotation);

        XMStoreFloat4(&Data.vVelocity, Velocity);
        XMStoreFloat4(&Data.vPosition, Position);
        XMStoreFloat4(&Data.vRotation, Rotation);
        Data.vScaleLife.w += fTimeDelta;

        // 실제 이펙트 Update와 비슷하게 S * R * T 순으로 World 행렬을 갱신한다.
        XMMATRIX matScale = XMMatrixScaling(Data.vScaleLife.x, Data.vScaleLife.y, Data.vScaleLife.z);
        XMMATRIX matRotation = XMMatrixRotationZ(Data.vRotation.z);
        XMMATRIX matTranslation = XMMatrixTranslation(Data.vPosition.x, Data.vPosition.y, Data.vPosition.z);
        XMStoreFloat4x4(&Data.matWorld, matScale * matRotation * matTranslation);
    }
}

std::vector<EFFECT_UPDATE_DATA> CUpdateBenchmark::Create_UpdateDatas(uint32_t iUpdateCount)
{
    std::vector<EFFECT_UPDATE_DATA> Datas(iUpdateCount);

    for (uint32_t i = 0; i < iUpdateCount; ++i)
    {
        EFFECT_UPDATE_DATA& Data = Datas[i];
        float fSeed = static_cast<float>(i % 97) * 0.01f;
        Data.vPosition = { fSeed, fSeed * 2.f, fSeed * 3.f, 1.f };
        Data.vVelocity = { 1.f + fSeed, 2.f, -0.5f, 0.f };
        Data.vRotation = { 0.f, 0.f, fSeed, 0.f };
        Data.vAngularVelocity = { 0.f, 0.f, 0.5f + fSeed, 0.f };
        Data.vScaleLife = { 1.f, 1.f, 1.f, 0.f };
        XMStoreFloat4x4(&Data.matWorld, XMMatrixIdentity());
    }

    return Datas;
}

double CUpdateBenchmark::Get_Median(std::vector<double> Values)
{
    std::sort(Values.begin(), Values.end());
    return Values[Values.size() / 2];
}

double CUpdateBenchmark::Get_Checksum(std::span<const EFFECT_UPDATE_DATA> Datas)
{
    double fChecksum = 0.0;

    for (size_t i = 0; i < Datas.size(); i += std::max<size_t>(1, Datas.size() / 16))
        fChecksum += Datas[i].vPosition.x + Datas[i].matWorld._41;

    return fChecksum;
}

HRESULT CUpdateBenchmark::Ready_ComputeShader(const std::filesystem::path& ShaderPath)
{
    ComPtr<ID3DBlob> pShaderBlob;
    ComPtr<ID3DBlob> pErrorBlob;
    UINT iCompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#ifdef _DEBUG
    iCompileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    iCompileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    HRESULT hr = D3DCompileFromFile(ShaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "CS_MAIN", "cs_5_0", iCompileFlags, 0, pShaderBlob.GetAddressOf(), pErrorBlob.GetAddressOf());

    if (FAILED(hr))
    {
        if (pErrorBlob)
            std::cerr << static_cast<const char*>(pErrorBlob->GetBufferPointer()) << std::endl;

        return E_FAIL;
    }

    return m_pDevice->CreateComputeShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(),
        nullptr, m_pComputeShader.GetAddressOf());
}

HRESULT CUpdateBenchmark::Ready_GPUBuffer(std::span<const EFFECT_UPDATE_DATA> Datas)
{
    m_pUpdateUAV.Reset();
    m_pUpdateBuffer.Reset();

    D3D11_BUFFER_DESC BufferDesc = {};
    BufferDesc.ByteWidth = static_cast<UINT>(sizeof(EFFECT_UPDATE_DATA) * Datas.size());
    BufferDesc.Usage = D3D11_USAGE_DEFAULT;
    BufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    BufferDesc.StructureByteStride = sizeof(EFFECT_UPDATE_DATA);

    D3D11_SUBRESOURCE_DATA InitialData = {};
    InitialData.pSysMem = Datas.data();

    if (FAILED(m_pDevice->CreateBuffer(&BufferDesc, &InitialData, m_pUpdateBuffer.GetAddressOf())))
        return E_FAIL;

    D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    UAVDesc.Buffer.NumElements = static_cast<UINT>(Datas.size());

    return m_pDevice->CreateUnorderedAccessView(m_pUpdateBuffer.Get(), &UAVDesc, m_pUpdateUAV.GetAddressOf());
}

HRESULT CUpdateBenchmark::Ready_TimestampQueries()
{
    D3D11_QUERY_DESC QueryDesc = {};
    QueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

    if (FAILED(m_pDevice->CreateQuery(&QueryDesc, m_pDisjointQuery.GetAddressOf())))
        return E_FAIL;

    QueryDesc.Query = D3D11_QUERY_TIMESTAMP;

    if (FAILED(m_pDevice->CreateQuery(&QueryDesc, m_pStartQuery.GetAddressOf())))
        return E_FAIL;

    return m_pDevice->CreateQuery(&QueryDesc, m_pEndQuery.GetAddressOf());
}

HRESULT CUpdateBenchmark::Measure_GPU(uint32_t iUpdateCount, uint32_t iFrameCount, double& fSubmitUS, double& fKernelUS, double& fWallUS)
{
    FRAME_DESC FrameDesc;
    FrameDesc.fTimeDelta = TIME_DELTA;
    FrameDesc.iUpdateCount = iUpdateCount;

    D3D11_MAPPED_SUBRESOURCE MappedResource = {};
    if (FAILED(m_pContext->Map(m_pFrameBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource)))
        return E_FAIL;

    memcpy(MappedResource.pData, &FrameDesc, sizeof(FRAME_DESC));
    m_pContext->Unmap(m_pFrameBuffer.Get(), 0);

    ID3D11Buffer* pFrameBuffer = m_pFrameBuffer.Get();
    ID3D11UnorderedAccessView* pUpdateUAV = m_pUpdateUAV.Get();
    UINT iInitialCount = 0;

    m_pContext->CSSetShader(m_pComputeShader.Get(), nullptr, 0);
    m_pContext->CSSetConstantBuffers(0, 1, &pFrameBuffer);
    m_pContext->CSSetUnorderedAccessViews(0, 1, &pUpdateUAV, &iInitialCount);

    auto WallStart = std::chrono::steady_clock::now();
    m_pContext->Begin(m_pDisjointQuery.Get());
    m_pContext->End(m_pStartQuery.Get());

    auto SubmitStart = std::chrono::steady_clock::now();
    uint32_t iGroupCount = (iUpdateCount + COMPUTE_THREAD_COUNT - 1) / COMPUTE_THREAD_COUNT;

    for (uint32_t i = 0; i < iFrameCount; ++i)
        m_pContext->Dispatch(iGroupCount, 1, 1);

    auto SubmitEnd = std::chrono::steady_clock::now();
    m_pContext->End(m_pEndQuery.Get());
    m_pContext->End(m_pDisjointQuery.Get());

    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointData = {};
    uint64_t iStartTimestamp = 0;
    uint64_t iEndTimestamp = 0;

    if (FAILED(Wait_Query(m_pDisjointQuery.Get(), &DisjointData, sizeof(DisjointData))))
        return E_FAIL;

    if (FAILED(Wait_Query(m_pStartQuery.Get(), &iStartTimestamp, sizeof(iStartTimestamp))))
        return E_FAIL;

    if (FAILED(Wait_Query(m_pEndQuery.Get(), &iEndTimestamp, sizeof(iEndTimestamp))))
        return E_FAIL;

    auto WallEnd = std::chrono::steady_clock::now();

    ID3D11UnorderedAccessView* pNullUAV = nullptr;
    m_pContext->CSSetUnorderedAccessViews(0, 1, &pNullUAV, nullptr);
    m_pContext->CSSetShader(nullptr, nullptr, 0);

    if (DisjointData.Disjoint || DisjointData.Frequency == 0)
        return E_FAIL;

    double fGPUSeconds = static_cast<double>(iEndTimestamp - iStartTimestamp) / static_cast<double>(DisjointData.Frequency);
    fSubmitUS = std::chrono::duration<double, std::micro>(SubmitEnd - SubmitStart).count() / static_cast<double>(iFrameCount);
    fKernelUS = (fGPUSeconds * 1'000'000.0) / static_cast<double>(iFrameCount);
    fWallUS = std::chrono::duration<double, std::micro>(WallEnd - WallStart).count() / static_cast<double>(iFrameCount);

    return S_OK;
}

HRESULT CUpdateBenchmark::Wait_Query(ID3D11Query* pQuery, void* pData, uint32_t iDataSize)
{
    while (true)
    {
        HRESULT hr = m_pContext->GetData(pQuery, pData, iDataSize, 0);

        if (hr == S_OK)
            return S_OK;

        if (FAILED(hr))
            return E_FAIL;

        SwitchToThread();
    }
}
