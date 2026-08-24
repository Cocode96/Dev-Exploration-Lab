#include "UpdateBenchmark.h"

#include <iostream>

int wmain(int argc, wchar_t* argv[])
{
    std::filesystem::path ExecutablePath = std::filesystem::absolute(argv[0]).parent_path();
    std::filesystem::path ShaderPath = ExecutablePath / L"ComputeUpdate.hlsl";
    std::filesystem::path ReportPath = ExecutablePath.parent_path().parent_path() / L"reports" / L"raw_results.csv";

    CUpdateBenchmark Benchmark;

    if (FAILED(Benchmark.Initialize(ShaderPath)))
    {
        std::cerr << "Failed to initialize DX11 update benchmark." << std::endl;
        return 1;
    }

    if (FAILED(Benchmark.Run(ReportPath)))
    {
        std::cerr << "Failed to run DX11 update benchmark." << std::endl;
        return 1;
    }

    return 0;
}
