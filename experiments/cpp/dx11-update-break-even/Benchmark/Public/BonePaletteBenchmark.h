#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <DirectXMath.h>
#include <Windows.h>

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

struct BONE_PALETTE_DATA
{
    uint32_t iCharacterCount = 0;
    uint32_t iBoneCount = 0;
    std::vector<DirectX::XMFLOAT4X4> BoneOffsets;
    std::vector<DirectX::XMFLOAT4X4> CombinedTransforms;
    std::vector<DirectX::XMFLOAT4X4> PaletteMatrices;
};

class CBonePaletteWorkerPool final
{
public:
    explicit CBonePaletteWorkerPool(uint32_t iWorkerCount);
    ~CBonePaletteWorkerPool();

    CBonePaletteWorkerPool(const CBonePaletteWorkerPool&) = delete;
    CBonePaletteWorkerPool& operator=(const CBonePaletteWorkerPool&) = delete;

public:
    void Update_PerBone(BONE_PALETTE_DATA& Data);
    void Update_PerCharacter(BONE_PALETTE_DATA& Data);

private:
    enum class TASK_MODE
    {
        BONE_RANGE,
        CHARACTER_RANGE
    };

    void Dispatch(BONE_PALETTE_DATA& Data, TASK_MODE eMode, uint32_t iCharacterIndex, uint32_t iItemCount);
    void Worker_Main(uint32_t iWorkerIndex);

private:
    std::vector<std::thread> m_Workers;
    std::mutex m_Mutex;
    std::condition_variable m_WorkCondition;
    std::condition_variable m_CompleteCondition;
    BONE_PALETTE_DATA* m_pData = nullptr;
    TASK_MODE m_eMode = TASK_MODE::BONE_RANGE;
    uint32_t m_iCharacterIndex = 0;
    uint32_t m_iItemCount = 0;
    uint32_t m_iActiveWorkerCount = 1;
    uint32_t m_iCompleteWorkerCount = 0;
    uint64_t m_iWorkGeneration = 0;
    bool m_bStop = false;
};

class CBonePaletteBenchmark final
{
public:
    CBonePaletteBenchmark();

public:
    HRESULT Run(const std::filesystem::path& ReportDirectory);

private:
    static BONE_PALETTE_DATA Create_Data(uint32_t iCharacterCount, uint32_t iBoneCount);
    static void Update_AllSingle(BONE_PALETTE_DATA& Data);
    static double Get_Median(std::vector<double> Values);
    static double Get_Percentile(std::vector<double> Values, double fPercentile);
    static double Get_Checksum(const BONE_PALETTE_DATA& Data);

private:
    CBonePaletteWorkerPool m_WorkerPool;
};
