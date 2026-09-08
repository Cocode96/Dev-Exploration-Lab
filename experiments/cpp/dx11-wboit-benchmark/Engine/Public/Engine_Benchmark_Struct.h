#pragma once

namespace Engine
{
struct BenchmarkSettings
{
    int test{1};  // 0: 현재 장면, 1: 개수 단계 증가
    int camera{}; // 0: 고정, 1: 공전, 2: 경로 이동
    int warmup{30}, frames{180}, repeats{1}, maximum_instances{1024}, seed{2026};
    float motion_speed{1};
    bool compare_methods{true}, animate_effects{true}, reverse_order{};
};
struct Statistics
{
    double mean{};
    double median{};
    double p95{};
    double standard_deviation{};
};
} // namespace Engine
