#include "ValidationScene.h"
#include "Client_Effect_Constant.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>

namespace Client
{
using namespace std;
using namespace DirectX;

bool ValidationScene::initialize(Engine::ApplicationContext&)
{
    if (!initialize_world())
        return false;
    rebuild(16);
    return true;
}

void ValidationScene::update(Engine::ApplicationContext&, float)
{
}

void ValidationScene::set_instance_count(uint32_t count)
{
    count = (max)(2u, (min)(count, 32u));
    if (count != m_instances.size())
        rebuild(count);
}

void ValidationScene::rebuild(uint32_t count)
{
    m_instances.clear();
    m_instances.reserve(count);
    for (uint32_t index = 0; index < count; ++index)
    {
        const auto color = effect_palette[index % effect_palette.size()];
        const float angle = static_cast<float>(index) * XM_PI / static_cast<float>(count);
        m_instances.push_back({
            {0.0f, 3.0f, 8.0f, 0.0f}, {7.5f, 0.48f},
            {color.x, color.y, color.z, 0.42f},
            {index % 2 == 0 ? angle : -angle, angle * 0.35f}
        });
    }
}
}
