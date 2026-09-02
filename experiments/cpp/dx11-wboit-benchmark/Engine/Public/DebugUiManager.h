#pragma once

#include <Windows.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace Engine
{
enum class DebugLogLevel
{
    Info,
    Success,
    Warning,
    Error
};

enum class DebugUiLanguage
{
    English,
    Korean
};

enum class DebugUiDockRegion
{
    Controls,
    Performance,
    Log
};

struct DebugLogEntry
{
    DebugLogLevel level{DebugLogLevel::Info};
    double elapsed_seconds{};
    std::string message;
};

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
    void render_log_panel();
    void end_frame();
    void render_draw_data();
    void set_next_window_default_dock(DebugUiDockRegion region);

    bool handle_window_message(HWND window, UINT message, WPARAM w_param, LPARAM l_param);
    bool wants_keyboard_input() const;
    bool wants_mouse_input() const;

    void log(DebugLogLevel level, std::string message);
    void clear_log();
    void toggle_language();

    bool is_initialized() const noexcept { return m_initialized; }
    DebugUiLanguage language() const noexcept { return m_language; }

private:
    void apply_debug_hell_theme();
    void build_default_layout();

    static constexpr std::size_t max_log_entries = 512;

    HWND m_window{};
    bool m_initialized{};
    bool m_frame_active{};
    bool m_layout_initialized{};
    bool m_scroll_log_to_bottom{};
    std::uint32_t m_controls_dock_id{};
    std::uint32_t m_performance_dock_id{};
    std::uint32_t m_log_dock_id{};
    std::chrono::steady_clock::time_point m_start_time{};
    std::deque<DebugLogEntry> m_log_entries;
    DebugUiLanguage m_language{DebugUiLanguage::English};
};
}
