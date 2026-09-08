#pragma once
#include <filesystem>
#include <string_view>
namespace Lab
{
inline std::filesystem::path path_from_utf8(std::string_view input)
{
    // ImGui UTF-8 경로를 C++20 filesystem 경로로 전달한다.
    std::u8string converted;
    converted.reserve(input.size());
    for (unsigned char byte : input)
        converted.push_back(static_cast<char8_t>(byte));
    return std::filesystem::path(converted);
}
} // namespace Lab
