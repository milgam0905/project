#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Dependencies/glew.h"
#include <Windows.h>
#include "Renderer.h"
#include <map>
#include <string>

class UiText
{
    struct Entry
    {
        GLuint id;
        int w, h;
    };

    HDC dc = nullptr;
    HFONT font = nullptr;
    HGDIOBJ oldFont = nullptr;
    std::map<std::wstring, Entry> entries;

  public:
    UiText()
    {
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
        if (dc && font)
        {
            oldFont = SelectObject(dc, font);
        }
    }

    ~UiText()
    {
        for (const auto& e : entries)
        {
            glDeleteTextures(1, &e.second.id);
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
        if (!dc || !font || text.empty())
        {
            return;
        }

        // Numeric HUD strings can vary indefinitely; keep GPU text cache bounded.
        if (entries.size() >= 192 && entries.find(text) == entries.end())
        {
            for (const auto& entry : entries)
            {
                glDeleteTextures(1, &entry.second.id);
            }
            entries.clear();
        }
        auto it = entries.find(text);
        if (it == entries.end())
        {
            SIZE size = {};
            if (!GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size))
            {
                return;
            }

            const int w = size.cx + 4, h = 30;
            BITMAPINFO info = {};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = w;
            info.bmiHeader.biHeight = -h;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            void* pixels = nullptr;
            HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
            if (!bitmap || !pixels)
            {
                if (bitmap)
                {
                    DeleteObject(bitmap);
                }

                return;
            }

            HGDIOBJ previous = SelectObject(dc, bitmap);
            PatBlt(dc, 0, 0, w, h, BLACKNESS);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 255, 255));
            TextOutW(dc, 2, 2, text.c_str(), static_cast<int>(text.size()));
            GdiFlush();
            auto bytes = static_cast<unsigned char*>(pixels);
            for (int i = 0; i < w * h; ++i)
            {
                const unsigned char coverage = bytes[i * 4];
                bytes[i * 4] = bytes[i * 4 + 1] = bytes[i * 4 + 2] = 255;
                bytes[i * 4 + 3] = coverage;
            }

            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            SelectObject(dc, previous);
            DeleteObject(bitmap);
            it = entries.insert({text, {texture, w, h}}).first;
        }

        const Entry& e = it->second;
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, e.id);
        glColor4f(color.r, color.g, color.b, color.a);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex2f(x, y);
        glTexCoord2f(1, 0);
        glVertex2f(x + e.w * scale, y);
        glTexCoord2f(1, 1);
        glVertex2f(x + e.w * scale, y + e.h * scale);
        glTexCoord2f(0, 1);
        glVertex2f(x, y + e.h * scale);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }
};
