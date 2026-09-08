#pragma once

#include "Engine_DebugUi_Enum.h"
#include <string>

namespace Engine
{
using namespace std;

struct DebugLogEntry
{
    DebugLogLevel level{DebugLogLevel::Info};
    double elapsed_seconds{};
    string message;
};
}
