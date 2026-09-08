#pragma once

#include <memory>

namespace Engine
{
using namespace std;

class FreeCamera;
class InputManager;

class CameraManager final
{
public:
    CameraManager();
    ~CameraManager();

    bool initialize(float aspect_ratio);
    void update(const InputManager& input, float delta_time);
    FreeCamera& active_camera();
    const FreeCamera& active_camera() const;

private:
    unique_ptr<FreeCamera> m_active_camera;
};
}
