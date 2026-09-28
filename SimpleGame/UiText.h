#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Dependencies/glew.h"
#include <Windows.h>
#include "Renderer.h"
#include "RenderQueue.h"
#include "Profiler.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Bounded glyph atlas. Changing HP/XP strings reuses glyphs instead of creating textures.
class UiText
{
    static constexpr int Cell = 32;

    struct Glyph
    {
        int slot, advance;
        std::uint64_t lastUse;
    };

    HDC dc = nullptr;
    HFONT font = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ oldFont = nullptr, oldBitmap = nullptr;
    void* pixels = nullptr;
    GLuint atlas = 0;
    int side = 0, columns = 0, capacity = 0;
    std::uint64_t clock = 0;
    std::map<wchar_t, Glyph> glyphs;

    const Glyph* GetGlyph(wchar_t character)
    {
        auto found = glyphs.find(character);
        if (found != glyphs.end())
        {
            found->second.lastUse = ++clock;
            Profiler::Count("text.glyph_cache_hits");
            return &found->second;
        }

        Profiler::Scope profile("text.glyph_rasterize_upload");
        Profiler::Count("text.glyph_cache_misses");
        SIZE size = {};
        if (!GetTextExtentPoint32W(dc, &character, 1, &size))
        {
            Profiler::Count("text.rasterize_failures");
            return nullptr;
        }

        int slot = static_cast<int>(glyphs.size());
        if (slot == capacity)
        {
            // Queued vertices may still reference the reused slot.
            RenderQueue::Flush();
            auto oldest = std::min_element(glyphs.begin(),
                                           glyphs.end(),
                                           [](const std::pair<const wchar_t, Glyph>& a,
                                              const std::pair<const wchar_t, Glyph>& b)
                                           {
                                               return a.second.lastUse < b.second.lastUse;
                                           });
            slot = oldest->second.slot;
            glyphs.erase(oldest);
            Profiler::Count("text.glyph_evictions");
        }

        PatBlt(dc, 0, 0, Cell, Cell, BLACKNESS);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        TextOutW(dc, 2, 2, &character, 1);
        GdiFlush();

        std::array<unsigned char, Cell * Cell * 4> rgba;
        const auto* source = static_cast<const unsigned char*>(pixels);
        for (int i = 0; i < Cell * Cell; ++i)
        {
            rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = source[i * 4];
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        (slot % columns) * Cell,
                        (slot / columns) * Cell,
                        Cell,
                        Cell,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        rgba.data());
        Profiler::Count("upload.texture_bytes", rgba.size());
        Profiler::Gauge("text.glyph_resident", static_cast<double>(glyphs.size() + 1));
        return &glyphs.emplace(character, Glyph{slot, size.cx, ++clock}).first->second;
    }

  public:
    UiText()
    {
        Profiler::Scope profile("text.atlas_initialize");
        dc = CreateCompatibleDC(nullptr);
        font = CreateFontW(-21,
                           0,
                           0,
                           0,
                           FW_NORMAL,
                           FALSE,
                           FALSE,
                           FALSE,
                           DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS,
                           ANTIALIASED_QUALITY,
                           DEFAULT_PITCH,
                           L"Malgun Gothic");
        if (!dc || !font)
        {
            Profiler::Count("text.initialize_failures");
            return;
        }
        oldFont = SelectObject(dc, font);
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = Cell;
        info.bmiHeader.biHeight = -Cell;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap || !pixels)
        {
            Profiler::Count("text.initialize_failures");
            return;
        }
        oldBitmap = SelectObject(dc, bitmap);
        GLint maximum = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
        side = std::min(2048, maximum);
        columns = side / Cell;
        capacity = columns * columns;
        if (!capacity)
        {
            Profiler::Count("text.initialize_failures");
            return;
        }
        std::vector<unsigned char> transparent(static_cast<size_t>(side) * side * 4, 0);
        glGenTextures(1, &atlas);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA8,
                     side,
                     side,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     transparent.data());
        Profiler::Count("upload.texture_bytes", transparent.size());
        Profiler::Gauge("text.atlas_bytes", static_cast<double>(transparent.size()));
    }

    UiText(const UiText&) = delete;
    UiText& operator=(const UiText&) = delete;

    ~UiText()
    {
        RenderQueue::Flush();
        glDeleteTextures(1, &atlas);
        if (dc && oldBitmap)
        {
            SelectObject(dc, oldBitmap);
        }
        if (bitmap)
        {
            DeleteObject(bitmap);
        }
        if (dc && oldFont)
        {
            SelectObject(dc, oldFont);
        }
        if (font)
        {
            DeleteObject(font);
        }
        if (dc)
        {
            DeleteDC(dc);
        }
    }

    void Draw(float x, float y, const std::wstring& text, RenderColor color, float scale = 1)
    {
        Profiler::Scope profile("text.submit");
        if (!atlas || text.empty())
        {
            return;
        }
        const float origin = x;
        for (size_t i = 0; i < text.size(); ++i)
        {
            wchar_t character = text[i];
            if (character == L'\n')
            {
                x = origin;
                y += 30 * scale;
                continue;
            }
            // Current UI is BMP Korean/Latin. Consume unsupported UTF-16 pairs once.
            if (character >= 0xD800 && character <= 0xDBFF)
            {
                if (i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF)
                {
                    ++i;
                }
                character = 0xFFFD;
            }
            const Glyph* glyph = GetGlyph(character);
            if (!glyph)
            {
                x += 21 * scale;
                continue;
            }
            const float u = static_cast<float>((glyph->slot % columns) * Cell) / side;
            const float v = static_cast<float>((glyph->slot / columns) * Cell) / side;
            const float extent = static_cast<float>(Cell) / side;
            RenderQueue::EnableTexture(true);
            RenderQueue::Texture(atlas);
            RenderQueue::Color(color.r, color.g, color.b, color.a);
            RenderQueue::Begin(GL_QUADS);
            RenderQueue::TexCoord(u, v);
            RenderQueue::Vertex(x, y);
            RenderQueue::TexCoord(u + extent, v);
            RenderQueue::Vertex(x + Cell * scale, y);
            RenderQueue::TexCoord(u + extent, v + extent);
            RenderQueue::Vertex(x + Cell * scale, y + Cell * scale);
            RenderQueue::TexCoord(u, v + extent);
            RenderQueue::Vertex(x, y + Cell * scale);
            RenderQueue::End();
            x += glyph->advance * scale;
        }
        RenderQueue::EnableTexture(false);
    }
};
