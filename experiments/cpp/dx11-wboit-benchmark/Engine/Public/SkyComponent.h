#pragma once

#include "EngineTypes.h"

namespace Engine
{
class SkyComponent final
{
public:
    void set_colors(const DirectX::XMFLOAT4& zenith, const DirectX::XMFLOAT4& horizon) noexcept
    {
        m_data.zenith_color = zenith;
        m_data.horizon_color = horizon;
    }

    const SkyRenderData& render_data() const noexcept { return m_data; }

private:
    SkyRenderData m_data;
};
}
