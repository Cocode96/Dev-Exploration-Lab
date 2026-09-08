#pragma once
#include <DirectXMath.h>
#include <string>
#include <vector>
namespace Lab
{
using namespace std;
using namespace DirectX;
struct EffectPreset
{
    string name;
    float delay{}, life_min{1}, life_max{2}, speed_min{1}, speed_max{2};
    XMFLOAT3 origin_min{-3, 0, -3}, origin_max{3, 3, 3};
    XMFLOAT3 velocity_min{-1, 0, -1}, velocity_max{1, 1, 1};
    XMFLOAT3 acceleration_min{}, acceleration_max{};
    XMFLOAT4 start_color{0.1f, 0.8f, 1, 0.8f}, end_color{1, 0.2f, 0.1f, 0};
    XMFLOAT2 start_size{0.15f, 0.15f}, end_size{0.03f, 0.03f};
    unsigned count{32};
};
struct EffectParticle
{
    XMFLOAT3 position{}, velocity{}, acceleration{};
    XMFLOAT4 color{};
    XMFLOAT2 size{};
    float age{}, lifetime{};
    unsigned preset{};
    bool active{};
};
} // namespace Lab
