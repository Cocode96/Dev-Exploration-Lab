#include "BonePaletteBenchmark.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace DirectX;

namespace
{
    constexpr uint32_t BONE_COUNT_PER_CHARACTER = 200;
    constexpr uint32_t WARM_UP_FRAME_COUNT = 30;
    constexpr uint32_t MEASURE_FRAME_COUNT = 120;
    constexpr uint32_t REPEAT_COUNT = 7;

    uint32_t Get_WorkerCount()
    {
        const uint32_t iHardwareThreadCount = std::thread::hardware_concurrency();
        return iHardwareThreadCount > 1 ? iHardwareThreadCount - 1 : 1;
    }

    void Update_BoneRange(BONE_PALETTE_DATA& Data, uint32_t iCharacterIndex, uint32_t iBeginBone, uint32_t iEndBone)
    {
        const size_t iCharacterOffset = static_cast<size_t>(iCharacterIndex) * Data.iBoneCount;

        for (uint32_t iBone = iBeginBone; iBone < iEndBone; ++iBone)
        {
            const size_t iIndex = iCharacterOffset + iBone;
            const XMMATRIX Offset = XMLoadFloat4x4(&Data.BoneOffsets[iIndex]);
            const XMMATRIX Combined = XMLoadFloat4x4(&Data.CombinedTransforms[iIndex]);
            XMStoreFloat4x4(&Data.PaletteMatrices[iIndex], XMMatrixTranspose(Offset * Combined));
        }
    }

    template<typename Function>
    double Measure_AverageUS(Function&& Execute, uint32_t iFrameCount)
    {
        const auto StartTime = std::chrono::steady_clock::now();

        for (uint32_t i = 0; i < iFrameCount; ++i)
            Execute();

        const auto EndTime = std::chrono::steady_clock::now();
        const double fTotalUS = std::chrono::duration<double, std::micro>(EndTime - StartTime).count();
        return fTotalUS / static_cast<double>(iFrameCount);
    }

    struct BONE_BENCHMARK_SAMPLE
    {
        uint32_t iCharacterCount = 0;
        uint32_t iRepeatIndex = 0;
        double fSingleUS = 0.0;
        double fPerBoneUS = 0.0;
        double fPerCharacterUS = 0.0;
    };

    struct BONE_BENCHMARK_RESULT
    {
        uint32_t iCharacterCount = 0;
        double fSingleUS = 0.0;
        double fSingleP95US = 0.0;
        double fPerBoneUS = 0.0;
        double fPerBoneP95US = 0.0;
        double fPerCharacterUS = 0.0;
        double fPerCharacterP95US = 0.0;
    };

    bool Has_SamePalette(const BONE_PALETTE_DATA& Left, const BONE_PALETTE_DATA& Right)
    {
        if (Left.PaletteMatrices.size() != Right.PaletteMatrices.size())
            return false;

        constexpr float EPSILON = 1e-5f;

        for (size_t i = 0; i < Left.PaletteMatrices.size(); ++i)
        {
            const float* pLeft = &Left.PaletteMatrices[i]._11;
            const float* pRight = &Right.PaletteMatrices[i]._11;

            for (size_t iElement = 0; iElement < 16; ++iElement)
            {
                if (std::abs(pLeft[iElement] - pRight[iElement]) > EPSILON)
                    return false;
            }
        }

        return true;
    }
}

CBonePaletteWorkerPool::CBonePaletteWorkerPool(uint32_t iWorkerCount)
{
    iWorkerCount = std::max(1u, iWorkerCount);

    for (uint32_t i = 0; i < iWorkerCount; ++i)
        m_Workers.emplace_back(&CBonePaletteWorkerPool::Worker_Main, this, i);
}

CBonePaletteWorkerPool::~CBonePaletteWorkerPool()
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

void CBonePaletteWorkerPool::Update_PerBone(BONE_PALETTE_DATA& Data)
{
    for (uint32_t iCharacter = 0; iCharacter < Data.iCharacterCount; ++iCharacter)
        Dispatch(Data, TASK_MODE::BONE_RANGE, iCharacter, Data.iBoneCount);
}

void CBonePaletteWorkerPool::Update_PerCharacter(BONE_PALETTE_DATA& Data)
{
    Dispatch(Data, TASK_MODE::CHARACTER_RANGE, 0, Data.iCharacterCount);
}

void CBonePaletteWorkerPool::Dispatch(
    BONE_PALETTE_DATA& Data, TASK_MODE eMode, uint32_t iCharacterIndex, uint32_t iItemCount)
{
    {
        std::lock_guard Lock{ m_Mutex };
        m_pData = &Data;
        m_eMode = eMode;
        m_iCharacterIndex = iCharacterIndex;
        m_iItemCount = iItemCount;
        m_iActiveWorkerCount = std::clamp(iItemCount, 1u, static_cast<uint32_t>(m_Workers.size()));
        m_iCompleteWorkerCount = 0;
        ++m_iWorkGeneration;
    }

    m_WorkCondition.notify_all();

    std::unique_lock Lock{ m_Mutex };
    m_CompleteCondition.wait(Lock, [this]() { return m_iCompleteWorkerCount == m_iActiveWorkerCount; });
}

void CBonePaletteWorkerPool::Worker_Main(uint32_t iWorkerIndex)
{
    uint64_t iLastGeneration = 0;

    while (true)
    {
        BONE_PALETTE_DATA* pData = nullptr;
        TASK_MODE eMode = TASK_MODE::BONE_RANGE;
        uint32_t iCharacterIndex = 0;
        uint32_t iBegin = 0;
        uint32_t iEnd = 0;

        {
            std::unique_lock Lock{ m_Mutex };
            m_WorkCondition.wait(Lock, [this, iLastGeneration]() {
                return m_bStop || m_iWorkGeneration != iLastGeneration;
                });

            if (m_bStop)
                return;

            iLastGeneration = m_iWorkGeneration;

            if (iWorkerIndex >= m_iActiveWorkerCount)
                continue;

            pData = m_pData;
            eMode = m_eMode;
            iCharacterIndex = m_iCharacterIndex;
            iBegin = m_iItemCount * iWorkerIndex / m_iActiveWorkerCount;
            iEnd = m_iItemCount * (iWorkerIndex + 1) / m_iActiveWorkerCount;
        }

        if (eMode == TASK_MODE::BONE_RANGE)
        {
            Update_BoneRange(*pData, iCharacterIndex, iBegin, iEnd);
        }
        else
        {
            for (uint32_t iCharacter = iBegin; iCharacter < iEnd; ++iCharacter)
                Update_BoneRange(*pData, iCharacter, 0, pData->iBoneCount);
        }

        {
            std::lock_guard Lock{ m_Mutex };
            ++m_iCompleteWorkerCount;

            if (m_iCompleteWorkerCount == m_iActiveWorkerCount)
                m_CompleteCondition.notify_one();
        }
    }
}

CBonePaletteBenchmark::CBonePaletteBenchmark()
    : m_WorkerPool{ Get_WorkerCount() }
{
}

HRESULT CBonePaletteBenchmark::Run(const std::filesystem::path& ReportDirectory)
{
    const std::vector<uint32_t> CharacterCounts = { 1, 2, 4, 8, 16, 32, 64, 128 };
    std::vector<BONE_BENCHMARK_SAMPLE> Samples;
    std::vector<BONE_BENCHMARK_RESULT> Results;

    std::cout << std::endl
        << "FF7 방식 Bone Palette 병렬화 단위 실험" << std::endl
        << "캐릭터당 Bone 수: " << BONE_COUNT_PER_CHARACTER << std::endl
        << "비교: 단일 스레드 / 캐릭터마다 Bone 병렬 / 캐릭터 단위 병렬" << std::endl
        << "측정 범위: offset * combined 후 transpose, 부모-자식 계층 갱신 제외" << std::endl
        << "작업 스레드 수: " << Get_WorkerCount() << std::endl
        << "------------------------------------------------------------" << std::endl;

    for (uint32_t iCharacterCount : CharacterCounts)
    {
        auto SingleData = Create_Data(iCharacterCount, BONE_COUNT_PER_CHARACTER);
        auto PerBoneData = SingleData;
        auto PerCharacterData = SingleData;

        for (uint32_t i = 0; i < WARM_UP_FRAME_COUNT; ++i)
        {
            Update_AllSingle(SingleData);
            m_WorkerPool.Update_PerBone(PerBoneData);
            m_WorkerPool.Update_PerCharacter(PerCharacterData);
        }

        std::vector<double> SingleValues;
        std::vector<double> PerBoneValues;
        std::vector<double> PerCharacterValues;

        for (uint32_t iRepeat = 0; iRepeat < REPEAT_COUNT; ++iRepeat)
        {
            const double fSingleUS = Measure_AverageUS(
                [&]() { Update_AllSingle(SingleData); }, MEASURE_FRAME_COUNT);
            const double fPerBoneUS = Measure_AverageUS(
                [&]() { m_WorkerPool.Update_PerBone(PerBoneData); }, MEASURE_FRAME_COUNT);
            const double fPerCharacterUS = Measure_AverageUS(
                [&]() { m_WorkerPool.Update_PerCharacter(PerCharacterData); }, MEASURE_FRAME_COUNT);

            SingleValues.push_back(fSingleUS);
            PerBoneValues.push_back(fPerBoneUS);
            PerCharacterValues.push_back(fPerCharacterUS);
            Samples.push_back({ iCharacterCount, iRepeat, fSingleUS, fPerBoneUS, fPerCharacterUS });
        }

        if (!Has_SamePalette(SingleData, PerBoneData)
            || !Has_SamePalette(SingleData, PerCharacterData))
        {
            std::cerr << "Bone Palette 결과가 실행 방식에 따라 달라졌습니다." << std::endl;
            return E_FAIL;
        }

        BONE_BENCHMARK_RESULT Result;
        Result.iCharacterCount = iCharacterCount;
        Result.fSingleUS = Get_Median(SingleValues);
        Result.fSingleP95US = Get_Percentile(SingleValues, 0.95);
        Result.fPerBoneUS = Get_Median(PerBoneValues);
        Result.fPerBoneP95US = Get_Percentile(PerBoneValues, 0.95);
        Result.fPerCharacterUS = Get_Median(PerCharacterValues);
        Result.fPerCharacterP95US = Get_Percentile(PerCharacterValues, 0.95);
        Results.push_back(Result);

        const double fChecksum = Get_Checksum(SingleData);
        const char* pFastest = "단일 스레드";
        double fFastestUS = Result.fSingleUS;

        if (Result.fPerBoneUS < fFastestUS)
        {
            pFastest = "Bone 병렬";
            fFastestUS = Result.fPerBoneUS;
        }

        if (Result.fPerCharacterUS < fFastestUS)
            pFastest = "캐릭터 병렬";

        std::cout << '[' << iCharacterCount << "명, "
            << iCharacterCount * BONE_COUNT_PER_CHARACTER << " Bone]" << std::endl
            << "  단일 스레드 : 중앙 " << std::fixed << std::setprecision(3) << Result.fSingleUS
            << " us | p95 " << Result.fSingleP95US << " us" << std::endl
            << "  Bone 병렬   : 중앙 " << Result.fPerBoneUS
            << " us | p95 " << Result.fPerBoneP95US << " us" << std::endl
            << "  캐릭터 병렬 : 중앙 " << Result.fPerCharacterUS
            << " us | p95 " << Result.fPerCharacterP95US << " us" << std::endl
            << "  가장 빠름: " << pFastest << " | 출력 일치: 예 | 결과 검증값: " << fChecksum << std::endl
            << "------------------------------------------------------------" << std::endl;
    }

    std::filesystem::create_directories(ReportDirectory);
    const std::filesystem::path RawPath = ReportDirectory / L"bone_palette_raw_results.csv";
    const std::filesystem::path SummaryPath = ReportDirectory / L"bone_palette_summary_results.csv";
    std::ofstream RawReport{ RawPath };
    std::ofstream SummaryReport{ SummaryPath };

    if (!RawReport || !SummaryReport)
        return E_FAIL;

    RawReport << "character_count,bones_per_character,total_bone_count,repeat_index,"
        "single_us,per_bone_parallel_us,per_character_parallel_us\n";
    RawReport << std::fixed << std::setprecision(6);

    for (const BONE_BENCHMARK_SAMPLE& Sample : Samples)
    {
        RawReport << Sample.iCharacterCount << ',' << BONE_COUNT_PER_CHARACTER << ','
            << Sample.iCharacterCount * BONE_COUNT_PER_CHARACTER << ',' << Sample.iRepeatIndex << ','
            << Sample.fSingleUS << ',' << Sample.fPerBoneUS << ',' << Sample.fPerCharacterUS << '\n';
    }

    SummaryReport << "character_count,bones_per_character,total_bone_count,"
        "single_us,single_p95_us,per_bone_parallel_us,per_bone_parallel_p95_us,"
        "per_bone_speedup_vs_single,per_bone_normalized_time,per_bone_sync_count,"
        "per_character_parallel_us,per_character_parallel_p95_us,"
        "per_character_speedup_vs_single,per_character_normalized_time,per_character_sync_count\n";
    SummaryReport << std::fixed << std::setprecision(6);

    for (const BONE_BENCHMARK_RESULT& Result : Results)
    {
        SummaryReport << Result.iCharacterCount << ',' << BONE_COUNT_PER_CHARACTER << ','
            << Result.iCharacterCount * BONE_COUNT_PER_CHARACTER << ','
            << Result.fSingleUS << ',' << Result.fSingleP95US << ','
            << Result.fPerBoneUS << ',' << Result.fPerBoneP95US << ','
            << (Result.fSingleUS / Result.fPerBoneUS) << ','
            << (Result.fPerBoneUS / Result.fSingleUS) << ',' << Result.iCharacterCount << ','
            << Result.fPerCharacterUS << ',' << Result.fPerCharacterP95US << ','
            << (Result.fSingleUS / Result.fPerCharacterUS) << ','
            << (Result.fPerCharacterUS / Result.fSingleUS) << ",1\n";
    }

    std::cout << "Bone 반복별 원본 결과: " << RawPath.string() << std::endl;
    std::cout << "Bone 중앙값 요약 결과: " << SummaryPath.string() << std::endl;
    return S_OK;
}

BONE_PALETTE_DATA CBonePaletteBenchmark::Create_Data(uint32_t iCharacterCount, uint32_t iBoneCount)
{
    BONE_PALETTE_DATA Data;
    Data.iCharacterCount = iCharacterCount;
    Data.iBoneCount = iBoneCount;
    const size_t iTotalBoneCount = static_cast<size_t>(iCharacterCount) * iBoneCount;
    Data.BoneOffsets.resize(iTotalBoneCount);
    Data.CombinedTransforms.resize(iTotalBoneCount);
    Data.PaletteMatrices.resize(iTotalBoneCount);

    for (uint32_t iCharacter = 0; iCharacter < iCharacterCount; ++iCharacter)
    {
        for (uint32_t iBone = 0; iBone < iBoneCount; ++iBone)
        {
            const size_t iIndex = static_cast<size_t>(iCharacter) * iBoneCount + iBone;
            const float fBoneRatio = static_cast<float>(iBone + 1) / static_cast<float>(iBoneCount);
            const float fCharacterOffset = static_cast<float>(iCharacter) * 0.01f;
            XMStoreFloat4x4(&Data.BoneOffsets[iIndex],
                XMMatrixRotationRollPitchYaw(fBoneRatio * 0.03f, fBoneRatio * 0.02f, fBoneRatio * 0.01f)
                * XMMatrixTranslation(0.f, fBoneRatio * 0.1f, 0.f));
            XMStoreFloat4x4(&Data.CombinedTransforms[iIndex],
                XMMatrixScaling(1.f + fBoneRatio * 0.001f, 1.f, 1.f)
                * XMMatrixRotationY(fBoneRatio + fCharacterOffset)
                * XMMatrixTranslation(fCharacterOffset, fBoneRatio, 0.f));
            XMStoreFloat4x4(&Data.PaletteMatrices[iIndex], XMMatrixIdentity());
        }
    }

    return Data;
}

void CBonePaletteBenchmark::Update_AllSingle(BONE_PALETTE_DATA& Data)
{
    for (uint32_t iCharacter = 0; iCharacter < Data.iCharacterCount; ++iCharacter)
        Update_BoneRange(Data, iCharacter, 0, Data.iBoneCount);
}

double CBonePaletteBenchmark::Get_Median(std::vector<double> Values)
{
    return Get_Percentile(std::move(Values), 0.5);
}

double CBonePaletteBenchmark::Get_Percentile(std::vector<double> Values, double fPercentile)
{
    std::sort(Values.begin(), Values.end());
    const size_t iIndex = static_cast<size_t>(
        std::ceil(fPercentile * static_cast<double>(Values.size())) - 1.0);
    return Values[std::min(iIndex, Values.size() - 1)];
}

double CBonePaletteBenchmark::Get_Checksum(const BONE_PALETTE_DATA& Data)
{
    double fChecksum = 0.0;

    for (const XMFLOAT4X4& Matrix : Data.PaletteMatrices)
        fChecksum += Matrix._11 + Matrix._22 + Matrix._33 + Matrix._41 + Matrix._42 + Matrix._43;

    return fChecksum;
}
