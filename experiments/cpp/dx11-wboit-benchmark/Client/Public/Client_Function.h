#pragma once

#include "Engine_Enum.h"
#include "Engine_DebugUi_Enum.h"
#include "imgui.h"
#include <Windows.h>
#include <string>
#include <string_view>

namespace Client
{
using namespace std;

inline const char* ui_text(Engine::DebugUiLanguage language, const char* english, const char* korean)
{
    return language == Engine::DebugUiLanguage::Korean ? korean : english;
}

inline string to_utf8(wstring_view text)
{
    if (text.empty())
        return {};
    const int required_size = WideCharToMultiByte(CP_UTF8, 0, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (required_size <= 0)
        return {};
    string output(static_cast<size_t>(required_size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        output.data(), required_size, nullptr, nullptr);
    return output;
}

inline void section_title(const char* title, const char* subtitle)
{
    ImGui::TextColored({0.95f, 0.97f, 0.99f, 1.0f}, "%s", title);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", subtitle);
    ImGui::Separator();
}

inline bool method_card(
    const char* id,
    const char* title,
    const char* description,
    bool selected,
    const ImVec4& accent,
    Engine::DebugUiLanguage language)
{
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button,
        selected ? accent : ImVec4{0.14f, 0.20f, 0.26f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        selected ? accent : ImVec4{0.19f, 0.29f, 0.38f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_Text,
        selected ? ImVec4{0.04f, 0.06f, 0.08f, 1.0f} : ImVec4{0.92f, 0.94f, 0.96f, 1.0f});
    const bool pressed = ImGui::Button(title, {ImGui::GetContentRegionAvail().x, 0.0f});
    ImGui::PopStyleColor(3);
    ImGui::TextDisabled("%s", selected
        ? ui_text(language, "ACTIVE", "사용 중")
        : ui_text(language, "CLICK TO SELECT", "클릭하여 선택"));
    ImGui::SameLine();
    ImGui::TextWrapped("%s", description);
    ImGui::Spacing();
    ImGui::PopID();
    return pressed;
}

inline const char* method_name(Engine::TransparencyMode mode, Engine::DebugUiLanguage language)
{
    switch (mode)
    {
    case Engine::TransparencyMode::UnsortedAlpha:
        return ui_text(language, "Unsorted Alpha", "미정렬 알파 블렌딩");
    case Engine::TransparencyMode::ZSortedAlpha:
        return ui_text(language, "CPU Z-Sorted Alpha", "CPU Z 정렬 알파 블렌딩");
    case Engine::TransparencyMode::Wboit:
        return ui_text(language, "Weighted Blended OIT", "가중 혼합 OIT");
    }
    return ui_text(language, "Unknown", "알 수 없음");
}

inline const char* scene_name(Engine::SceneType scene, Engine::DebugUiLanguage language)
{
    return scene == Engine::SceneType::Validation
        ? ui_text(language, "Crossing Geometry Validation", "교차 지오메트리 검증")
        : ui_text(language, "Particle Sorting Stress", "파티클 정렬 스트레스");
}

inline const wchar_t* method_file_name(Engine::TransparencyMode mode)
{
    switch (mode)
    {
    case Engine::TransparencyMode::UnsortedAlpha: return L"unsorted_alpha";
    case Engine::TransparencyMode::ZSortedAlpha: return L"z_sorted_alpha";
    case Engine::TransparencyMode::Wboit: return L"wboit";
    }
    return L"unknown";
}
}
