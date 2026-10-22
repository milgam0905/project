#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include "ResourceSprites.h"
#include "RenderQueue.h"
#include "Profiler.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace
{
    using Microsoft::WRL::ComPtr;

    void Check(HRESULT result)
    {
        if (FAILED(result))
        {
            throw std::runtime_error("ARCANA PNG decode failed; check assets/arcana_lowpoly.");
        }
    }

    struct Apartment
    {
        HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        Apartment()
        {
            if (result != RPC_E_CHANGED_MODE)
            {
                Check(result);
            }
        }

        ~Apartment()
        {
            if (SUCCEEDED(result))
            {
                CoUninitialize();
            }
        }
    };

    GLuint Texture(IWICImagingFactory* factory, const std::filesystem::path& file, bool repeat)
    {
        Profiler::Scope scope("resource.png_load_upload");
        ComPtr<IWICBitmapDecoder> decoder;
        Check(factory->CreateDecoderFromFilename(
            file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder));
        ComPtr<IWICBitmapFrameDecode> frame;
        Check(decoder->GetFrame(0, &frame));
        ComPtr<IWICFormatConverter> converter;
        Check(factory->CreateFormatConverter(&converter));
        Check(converter->Initialize(frame.Get(),
                                    GUID_WICPixelFormat32bppRGBA,
                                    WICBitmapDitherTypeNone,
                                    nullptr,
                                    0,
                                    WICBitmapPaletteTypeCustom));
        UINT width = 0, height = 0;
        Check(converter->GetSize(&width, &height));
        if (!width || !height || width > 2048 || height > 2048 ||
            (!repeat && (width != 2048 || height != 2048)))
        {
            throw std::runtime_error("ARCANA sprite page dimensions are invalid.");
        }
        std::vector<unsigned char> rgba(static_cast<size_t>(width) * height * 4);
        Check(
            converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(rgba.size()), rgba.data()));
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        Profiler::Count("upload.texture_bytes", rgba.size());
        return texture;
    }
} // namespace

ResourceSprites::~ResourceSprites()
{
    RenderQueue::Flush();
    for (auto texture : pages)
    {
        glDeleteTextures(1, &texture);
    }
    glDeleteTextures(3, ground);
}

void ResourceSprites::Load()
{
    Profiler::Scope scope("resource.load");
    RenderQueue::Flush();
    wchar_t executable[32768]{};
    const DWORD count = GetModuleFileNameW(nullptr, executable, 32768);
    if (!count || count >= 32768)
    {
        throw std::runtime_error("Cannot resolve ARCANA resource directory.");
    }
    const auto root =
        std::filesystem::path(executable).parent_path() / L"assets" / L"arcana_lowpoly";
    std::ifstream input(root / L"sprites.txt");
    std::string header;
    int pageCount = 0, entryCount = 0;
    if (!(input >> header >> pageCount >> entryCount) || header != "ARCANA_SPRITES_3" ||
        pageCount < 1 || pageCount > 8 || entryCount < 1200 || entryCount > 4096)
    {
        throw std::runtime_error(
            "ARCANA sprite cache missing/invalid. Run tools/bake_arcana_resources.py, then build.");
    }
    for (int i = 0; i < entryCount; ++i)
    {
        std::string key;
        Sprite sprite{};
        if (!(input >> key >> sprite.page >> sprite.x >> sprite.y >> sprite.width >>
              sprite.height >> sprite.anchorX >> sprite.anchorY >> sprite.scale) ||
            sprite.page < 0 || sprite.page >= pageCount || sprite.x < 0 || sprite.y < 0 ||
            sprite.width < 1 || sprite.height < 1 || sprite.x + sprite.width > 2048 ||
            sprite.y + sprite.height > 2048 || !std::isfinite(sprite.scale) || sprite.scale <= 0 ||
            !std::isfinite(sprite.anchorX) || !std::isfinite(sprite.anchorY) ||
            !sprites.emplace(key, sprite).second)
        {
            throw std::runtime_error("Invalid ARCANA sprite entry.");
        }
    }
    Apartment apartment;
    ComPtr<IWICImagingFactory> factory;
    Check(CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
    for (int i = 0; i < pageCount; ++i)
    {
        pages.push_back(
            Texture(factory.Get(), root / (L"page_" + std::to_wstring(i) + L".png"), false));
    }
    const wchar_t* names[] = {L"grass.png", L"dirt.png", L"stone.png"};
    for (int i = 0; i < 3; ++i)
    {
        ground[i] = Texture(factory.Get(), root / names[i], true);
    }
    Profiler::Gauge("resource.atlas_gpu_bytes",
                    static_cast<double>(pages.size()) * 2048 * 2048 * 4);
    Profiler::Gauge("resource.sprite_count", static_cast<double>(sprites.size()));
    std::cout << "[resource] ARCANA lowpoly sprite cache loaded: " << sprites.size()
              << " sprites\n";
    RenderQueue::ResetState();
}

unsigned ResourceSprites::Ground(int material) const
{
    return ground[std::clamp(material, 0, 2)];
}

void ResourceSprites::Draw(const std::string& name,
                           RenderPoint foot,
                           float pixelsPerMeter,
                           float opacity,
                           bool burned) const
{
    const auto found = sprites.find(name);
    if (found == sprites.end())
    {
        Profiler::Count("resource.missing_sprite");
        return;
    }
    const auto& s = found->second;
    const float scale = pixelsPerMeter / s.scale;
    const float x = foot.x - s.anchorX * scale, y = foot.y - s.anchorY * scale;
    RenderQueue::EnableTexture(true);
    RenderQueue::Texture(pages[s.page]);
    RenderQueue::Color(burned ? .22f : 1.f, burned ? .18f : 1.f, burned ? .16f : 1.f, opacity);
    RenderQueue::Begin(GL_QUADS);
    RenderQueue::TexCoord(s.x / 2048.f, s.y / 2048.f);
    RenderQueue::Vertex(x, y);
    RenderQueue::TexCoord((s.x + s.width) / 2048.f, s.y / 2048.f);
    RenderQueue::Vertex(x + s.width * scale, y);
    RenderQueue::TexCoord((s.x + s.width) / 2048.f, (s.y + s.height) / 2048.f);
    RenderQueue::Vertex(x + s.width * scale, y + s.height * scale);
    RenderQueue::TexCoord(s.x / 2048.f, (s.y + s.height) / 2048.f);
    RenderQueue::Vertex(x, y + s.height * scale);
    RenderQueue::End();
    Profiler::Count("resource.sprite_submissions");
}

void ResourceSprites::Animate(
    const std::string& model, const char* clip, int direction, int frame, RenderPoint foot) const
{
    Draw(model + "." + clip + "." + std::to_string(direction % 8) + "." + std::to_string(frame % 6),
         foot,
         38.f);
}
