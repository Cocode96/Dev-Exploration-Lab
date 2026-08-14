cmake_minimum_required(VERSION 3.24)
project(runtime_renderer_swap LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8-docking
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT 00_START_TextureRendererExperiment)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(20_DLL_RealDxCombinedRenderer SHARED real_dx_renderer.cpp)
add_library(21_DLL_RealDx11Renderer SHARED real_dx_renderer.cpp)
add_library(22_DLL_RealDx12Renderer SHARED real_dx_renderer.cpp)
add_executable(00_START_TextureRendererExperiment WIN32 real_dx_host.cpp)

function(configure_real_renderer target output_name)
    target_sources(${target} PRIVATE
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    )
    target_include_directories(${target} PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(${target} PRIVATE d3d11 d3d12 dxgi d3dcompiler)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME ${output_name})
endfunction()

configure_real_renderer(20_DLL_RealDxCombinedRenderer RealDxCombinedRenderer)
configure_real_renderer(21_DLL_RealDx11Renderer RealDx11Renderer)
configure_real_renderer(22_DLL_RealDx12Renderer RealDx12Renderer)
target_compile_definitions(21_DLL_RealDx11Renderer PRIVATE REAL_DX11_ONLY)
target_compile_definitions(22_DLL_RealDx12Renderer PRIVATE REAL_DX12_ONLY)
target_link_libraries(00_START_TextureRendererExperiment PRIVATE user32)

set_target_properties(00_START_TextureRendererExperiment PROPERTIES
    OUTPUT_NAME "real_dx_switch"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:00_START_TextureRendererExperiment>"
)
foreach(target
    00_START_TextureRendererExperiment
    20_DLL_RealDxCombinedRenderer
    21_DLL_RealDx11Renderer
    22_DLL_RealDx12Renderer
)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE)
endforeach()

add_dependencies(00_START_TextureRendererExperiment
    20_DLL_RealDxCombinedRenderer 21_DLL_RealDx11Renderer 22_DLL_RealDx12Renderer)
add_custom_command(TARGET 00_START_TextureRendererExperiment POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:20_DLL_RealDxCombinedRenderer> $<TARGET_FILE_DIR:00_START_TextureRendererExperiment>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:21_DLL_RealDx11Renderer> $<TARGET_FILE_DIR:00_START_TextureRendererExperiment>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:22_DLL_RealDx12Renderer> $<TARGET_FILE_DIR:00_START_TextureRendererExperiment>
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Texture $<TARGET_FILE_DIR:00_START_TextureRendererExperiment>/Texture
)
