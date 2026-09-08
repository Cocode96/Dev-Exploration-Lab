#pragma once

#include <cmath>

namespace Engine
{
using namespace std;

inline float terrain_height(float x, float z)
{
    return 0.18f * sin(x * 0.22f) * cos(z * 0.19f);
}
}
