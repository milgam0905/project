#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "AssetCache.h"
#include <fstream>
#include <iostream>
#include <utility>

namespace
{
    struct Header
    {
        std::uint32_t magic, version, width, height, bytes, checksum;
    };

    std::wstring Directory()
    {
        wchar_t localAppData[32768] = {};
        DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, 32768);
        if (size == 0 || size >= 32768)
        {
            return {};
        }

        std::wstring directory = std::wstring(localAppData) + L"\\SimpleGame";
        CreateDirectoryW(directory.c_str(), nullptr);
        directory += L"\\ModelCache_v1";
        if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            return {};
        }

        return directory + L"\\";
    }

    std::uint32_t Checksum(const unsigned char* data, size_t count)
    {
        std::uint32_t hash = 2166136261u;
        for (size_t i = 0; i < count; ++i)
        {
            hash = (hash ^ data[i]) * 16777619u;
        }

        return hash;
    }
} // namespace

bool AssetCache::Load(const std::wstring& name,
                      int width,
                      int height,
                      std::vector<unsigned char>& rgba)
{
    const std::wstring directory = Directory();
    if (directory.empty())
    {
        return false;
    }

    std::ifstream stream((directory + name + L".rgba").c_str(), std::ios::binary);
    Header header = {};
    if (!stream.read(reinterpret_cast<char*>(&header), sizeof(header)))
    {
        return false;
    }

    const std::uint32_t expected = static_cast<std::uint32_t>(width * height * 4);
    if (header.magic != 0x53474143 || header.version != 1 ||
        header.width != static_cast<std::uint32_t>(width) ||
        header.height != static_cast<std::uint32_t>(height) || header.bytes != expected ||
        expected > 32u * 1024u * 1024u)
    {
        return false;
    }

    std::vector<unsigned char> loaded(expected);
    if (!stream.read(reinterpret_cast<char*>(loaded.data()), expected) ||
        stream.peek() != std::char_traits<char>::eof() ||
        Checksum(loaded.data(), loaded.size()) != header.checksum)
    {
        return false;
    }

    rgba = std::move(loaded);
    std::wcout << L"[cache loaded] " << name << L"\n";
    return true;
}

void AssetCache::Save(const std::wstring& name, int width, int height, const void* rgba)
{
    const std::wstring directory = Directory();
    if (directory.empty())
    {
        std::cerr << "Asset cache unavailable: assets remain usable in memory.\n";
        return;
    }

    const std::wstring path = directory + name + L".rgba";
    const std::wstring temporary = path + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    const std::uint32_t count = static_cast<std::uint32_t>(width * height * 4);
    Header header = {0x53474143,
                     1,
                     static_cast<std::uint32_t>(width),
                     static_cast<std::uint32_t>(height),
                     count,
                     Checksum(static_cast<const unsigned char*>(rgba), count)};

    std::ofstream stream(temporary.c_str(), std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
    stream.write(static_cast<const char*>(rgba), count);
    stream.close();
    if (!stream || !MoveFileExW(temporary.c_str(),
                                path.c_str(),
                                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        // Only this process's exact temporary cache file is removed on failure.
        DeleteFileW(temporary.c_str());
        std::cerr << "Could not persist model cache; next launch may regenerate assets.\n";
    }
    else
    {
        std::wcout << L"[cache generated] " << name << L"\n";
    }
}
