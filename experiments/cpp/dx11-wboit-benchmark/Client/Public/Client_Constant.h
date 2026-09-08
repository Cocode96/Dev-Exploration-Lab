#pragma once

#include <cstdint>
#include <array>

namespace Client
{
using namespace std;

inline constexpr uint32_t window_width = 1280;

inline constexpr uint32_t window_height = 720;

inline constexpr wchar_t window_class_name[] = L"WboitBenchmarkWindow";

inline constexpr array validation_counts{2u, 4u, 8u, 16u, 32u};

inline constexpr array stress_counts{64u, 256u, 1024u, 4096u};
}
