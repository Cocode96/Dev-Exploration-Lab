#pragma once

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <memory>

namespace Engine
{
class ApplicationContext;
}

namespace Client
{
class MainApp final
{
public:
    static std::unique_ptr<MainApp> create(HINSTANCE instance, int show_command);
    ~MainApp();

    MainApp(const MainApp&) = delete;
    MainApp& operator=(const MainApp&) = delete;

    int run();

private:
    MainApp() = default;
    bool initialize(HINSTANCE instance, int show_command);
    bool create_main_window(HINSTANCE instance, int show_command);
    void update(float delta_time);
    void render();
    void update_window_title();
    void adjust_instance_count(int direction);
    LRESULT handle_window_message(HWND window, UINT message, WPARAM w_param, LPARAM l_param);
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param);

    HWND m_window{};
    std::unique_ptr<Engine::ApplicationContext> m_context;
    std::filesystem::path m_output_directory;
    std::chrono::steady_clock::time_point m_previous_time{};
    std::chrono::steady_clock::time_point m_last_title_update{};
};
}
