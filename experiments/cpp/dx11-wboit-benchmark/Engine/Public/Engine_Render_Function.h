#pragma once

#include <Windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <filesystem>
#include <cstdint>
#include <vector>
#include <fstream>

namespace Engine
{
using namespace std;
using Microsoft::WRL::ComPtr;

inline bool succeeded(HRESULT result)
{
    return SUCCEEDED(result);
}

inline ComPtr<ID3DBlob> compile_shader(const filesystem::path& path, const char* entry_point,
    const char* target)
{
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    if (FAILED(D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry_point, target, flags, 0, &bytecode, &errors)))
    {
        if (errors)
            OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
        return nullptr;
    }
    return bytecode;
}

inline void write_bmp(const filesystem::path& path, uint32_t width, uint32_t height,
    const vector<uint8_t>& rgba)
{
    filesystem::create_directories(path.parent_path());
    BITMAPFILEHEADER file_header{};
    BITMAPINFOHEADER info_header{};
    file_header.bfType = 0x4D42;
    file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    file_header.bfSize = file_header.bfOffBits + width * height * 4;
    info_header.biSize = sizeof(BITMAPINFOHEADER);
    info_header.biWidth = static_cast<LONG>(width);
    info_header.biHeight = -static_cast<LONG>(height);
    info_header.biPlanes = 1;
    info_header.biBitCount = 32;
    info_header.biCompression = BI_RGB;
    info_header.biSizeImage = width * height * 4;

    vector<uint8_t> bgra(rgba.size());
    for (size_t index = 0; index < rgba.size(); index += 4)
    {
        bgra[index] = rgba[index + 2];
        bgra[index + 1] = rgba[index + 1];
        bgra[index + 2] = rgba[index];
        bgra[index + 3] = rgba[index + 3];
    }

    ofstream stream(path, ios::binary);
    stream.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
    stream.write(reinterpret_cast<const char*>(&info_header), sizeof(info_header));
    stream.write(reinterpret_cast<const char*>(bgra.data()), static_cast<streamsize>(bgra.size()));
}
}
