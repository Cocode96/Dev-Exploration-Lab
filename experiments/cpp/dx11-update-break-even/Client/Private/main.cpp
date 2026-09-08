#include "UpdateBenchmark.h"
#include "BonePaletteBenchmark.h"

#include <iostream>

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::filesystem::path ExecutablePath = std::filesystem::absolute(argv[0]).parent_path();
    std::filesystem::path ShaderPath = ExecutablePath / L"ComputeUpdate.hlsl";
    std::filesystem::path ReportPath = ExecutablePath.parent_path().parent_path() / L"reports" / L"raw_results.csv";

    CUpdateBenchmark Benchmark;

    if (FAILED(Benchmark.Initialize(ShaderPath)))
    {
        std::cerr << "DX11 Update 벤치마크 초기화에 실패했습니다." << std::endl;
        return 1;
    }

    if (FAILED(Benchmark.Run(ReportPath)))
    {
        std::cerr << "DX11 Update 벤치마크 실행에 실패했습니다." << std::endl;
        return 1;
    }

    CBonePaletteBenchmark BoneBenchmark;

    if (FAILED(BoneBenchmark.Run(ReportPath.parent_path())))
    {
        std::cerr << "Bone Palette 벤치마크 실행에 실패했습니다." << std::endl;
        return 1;
    }

    return 0;
}
