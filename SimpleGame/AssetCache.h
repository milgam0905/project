#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Versioned RGBA model/material cache. No OpenGL handles are serialized.
namespace AssetCache
{
    bool Load(const std::wstring& name, int width, int height, std::vector<unsigned char>& rgba);
    void Save(const std::wstring& name, int width, int height, const void* rgba);
} // namespace AssetCache
