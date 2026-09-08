#pragma once

#include "Engine_Benchmark_Struct.h"
#include "Engine_Enum.h"
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace Engine
{
using namespace std;

inline Statistics summarize(vector<double> values)
{
    Statistics output{};
    if (values.empty())
        return output;
    output.mean = accumulate(values.begin(), values.end(), 0.0) / values.size();
    double variance{};
    for (double value : values)
    {
        const double delta = value - output.mean;
        variance += delta * delta;
    }
    output.standard_deviation = sqrt(variance / values.size());
    sort(values.begin(), values.end());
    output.median = values[values.size() / 2];
    const size_t p95_index = static_cast<size_t>(
        ceil(static_cast<double>(values.size()) * 0.95) - 1.0);
    output.p95 = values[(min)(p95_index, values.size() - 1)];
    return output;
}

inline const char* scene_name(Engine::SceneType type)
{
    return type == Engine::SceneType::Validation ? "validation" : "effect-stress";
}

inline const char* mode_name(Engine::TransparencyMode mode)
{
    switch (mode)
    {
    case Engine::TransparencyMode::UnsortedAlpha: return "unsorted-alpha";
    case Engine::TransparencyMode::ZSortedAlpha: return "z-sorted-alpha";
    case Engine::TransparencyMode::Wboit: return "wboit";
    }
    return "unknown";
}
}
