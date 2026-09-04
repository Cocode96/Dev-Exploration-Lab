#pragma once

#include "EngineTypes.h"

#include <span>
#include <string_view>

namespace Engine
{
class ApplicationContext;

class IScene
{
public:
    virtual ~IScene() = default;

    virtual bool initialize(ApplicationContext& context) = 0;
    virtual void update(ApplicationContext& context, float delta_time) = 0;
    virtual std::wstring_view name() const = 0;
    virtual SceneRenderView render_view() const = 0;
    virtual void set_instance_count(std::uint32_t count) = 0;
    virtual std::uint32_t instance_count() const = 0;
};
}
