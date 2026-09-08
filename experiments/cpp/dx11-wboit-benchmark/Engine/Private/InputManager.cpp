#include "InputManager.h"

#include <windowsx.h>

namespace Engine
{
using namespace std;

void InputManager::handle_message(HWND window, UINT message, WPARAM key, LPARAM data)
{
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
    {
        if (key < m_current.size()) m_current[key] = true;
    }
    else if (message == WM_KEYUP || message == WM_SYSKEYUP)
    {
        if (key < m_current.size()) m_current[key] = false;
    }
    else if (message == WM_RBUTTONDOWN)
    {
        m_mouse_look_active = true;
        m_has_mouse_position = false;
        SetCapture(window);
    }
    else if (message == WM_RBUTTONUP)
    {
        m_mouse_look_active = false;
        m_has_mouse_position = false;
        ReleaseCapture();
    }
    else if (message == WM_MOUSEMOVE && m_mouse_look_active)
    {
        const POINT position{GET_X_LPARAM(data), GET_Y_LPARAM(data)};
        if (m_has_mouse_position)
        {
            m_mouse_delta.x += position.x - m_last_mouse_position.x;
            m_mouse_delta.y += position.y - m_last_mouse_position.y;
        }
        m_last_mouse_position = position;
        m_has_mouse_position = true;
    }
}

bool InputManager::is_down(int virtual_key) const
{
    if (virtual_key < 0 || static_cast<size_t>(virtual_key) >= m_current.size())
        return false;
    return m_current[virtual_key];
}

bool InputManager::was_pressed(int virtual_key) const
{
    if (virtual_key < 0 || static_cast<size_t>(virtual_key) >= m_current.size())
        return false;
    return m_current[virtual_key] && !m_previous[virtual_key];
}

void InputManager::release_active_input()
{
    m_current.fill(false);
    m_previous.fill(false);
    m_mouse_delta = {};
    m_has_mouse_position = false;
    if (m_mouse_look_active)
        ReleaseCapture();
    m_mouse_look_active = false;
}

void InputManager::end_frame()
{
    m_previous = m_current;
    m_mouse_delta = {};
}
}
