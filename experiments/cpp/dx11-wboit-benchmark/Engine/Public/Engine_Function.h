#pragma once

#include "Engine_Enum.h"
#include <string_view>

namespace Engine
{
using namespace std;

inline wstring_view to_string(TransparencyMode mode)
{
    switch (mode)
    {
    case TransparencyMode::UnsortedAlpha: return L"Unsorted Alpha";
    case TransparencyMode::ZSortedAlpha: return L"Object-center Z Sort";
    case TransparencyMode::Wboit: return L"Weighted Blended OIT";
    }
    return L"Unknown";
}
}
