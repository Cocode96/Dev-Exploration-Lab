#pragma once

#include <Windows.h>

#include <array>

namespace Engine
{
class InputManager final
{
public:
    void handle_message(HWND window, UINT message, WPARAM key, LPARAM data);
    bool is_down(int virtual_key) const;
    bool was_pressed(int virtual_key) const;
    POINT mouse_delta() const noexcept { return m_mouse_delta; }
    bool is_mouse_look_active() const noexcept { return m_mouse_look_active; }
    void end_frame();

private:
    std::array<bool, 256> m_current{};
    std::array<bool, 256> m_previous{};
    POINT m_last_mouse_position{};
    POINT m_mouse_delta{};
    bool m_has_mouse_position{};
    bool m_mouse_look_active{};
};
}
