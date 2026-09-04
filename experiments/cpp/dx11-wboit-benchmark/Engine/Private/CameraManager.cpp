#include "CameraManager.h"

#include "FreeCamera.h"

#include <stdexcept>

namespace Engine
{
CameraManager::CameraManager() = default;
CameraManager::~CameraManager() = default;

bool CameraManager::initialize(float aspect_ratio)
{
    m_active_camera = std::make_unique<FreeCamera>();
    m_active_camera->initialize(aspect_ratio);
    return true;
}

void CameraManager::update(const InputManager& input, float delta_time)
{
    active_camera().update(input, delta_time);
}

FreeCamera& CameraManager::active_camera()
{
    if (!m_active_camera) throw std::logic_error("Active camera is not initialized.");
    return *m_active_camera;
}

const FreeCamera& CameraManager::active_camera() const
{
    if (!m_active_camera) throw std::logic_error("Active camera is not initialized.");
    return *m_active_camera;
}
}
