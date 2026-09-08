#pragma once

#include <Windows.h>
#include "Engine_DebugUi_Struct.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace Engine
{
using namespace std;

class DebugUiManager final
{
public:
    DebugUiManager() = default;
    ~DebugUiManager();

    DebugUiManager(const DebugUiManager&) = delete;
    DebugUiManager& operator=(const DebugUiManager&) = delete;

    bool initialize(HWND window, ID3D11Device* device, ID3D11DeviceContext* device_context);
    void shutdown();

    void begin_frame();
    void render_workspace();
    void render_scene_view(ID3D11ShaderResourceView* scene_texture,
        uint32_t texture_width, uint32_t texture_height);
    void render_log_panel();
    void end_frame();
    void render_draw_data();
    void set_next_window_default_dock(DebugUiDockRegion region);

    bool handle_window_message(HWND window, UINT message, WPARAM w_param, LPARAM l_param);
    bool wants_keyboard_input() const;
    bool wants_mouse_input() const;
    bool scene_view_hovered() const noexcept { return m_scene_view_hovered; }
    uint32_t requested_scene_width() const noexcept { return m_requested_scene_width; }
    uint32_t requested_scene_height() const noexcept { return m_requested_scene_height; }

    void log(DebugLogLevel level, string message);
    void clear_log();
    void toggle_language();

    bool is_initialized() const noexcept { return m_initialized; }
    DebugUiLanguage language() const noexcept { return m_language; }

private:
    void apply_debug_hell_theme();
    void build_default_layout();


    HWND m_window{};
    bool m_initialized{};
    bool m_frame_active{};
    bool m_layout_initialized{};
    bool m_scroll_log_to_bottom{};
    bool m_scene_view_hovered{};
    uint32_t m_scene_dock_id{};
    uint32_t m_controls_dock_id{};
    uint32_t m_performance_dock_id{};
    uint32_t m_log_dock_id{};
    uint32_t m_requested_scene_width{1280};
    uint32_t m_requested_scene_height{720};
    chrono::steady_clock::time_point m_start_time{};
    deque<DebugLogEntry> m_log_entries;
    DebugUiLanguage m_language{DebugUiLanguage::English};
};
}
