#pragma once
#include "Renderer.h"
#include <cmath>
#include <array>

// Shared continuous shoreline for rendering, tree placement, animal AI and collision.
namespace WorldGeometry
{
    constexpr float HalfWidth = 28.f;
    constexpr float HalfHeight = 26.f; // 56 x 52 world units: exactly four times old area.
    constexpr float Pi = 3.14159265f;

    inline RenderPoint Project(RenderPoint p)
    {
        return {(p.x - p.y) * 42.f, (p.x + p.y) * 21.f};
    }

    inline float LakeRadius(float angle)
    {
        return 1.f + .075f * std::sin(angle * 3.f + .5f) + .045f * std::cos(angle * 5.f);
    }

    inline RenderPoint LakeEdge(float angle, float expansion = 0)
    {
        float r = LakeRadius(angle) + expansion;
        return {11.f + 4.4f * r * std::cos(angle), -6.f + 3.8f * r * std::sin(angle)};
    }

    inline bool Water(RenderPoint p, float margin = 0)
    {
        float x = (p.x - 11.f) / 4.4f, y = (p.y + 6.f) / 3.8f;
        return std::sqrt(x * x + y * y) < LakeRadius(std::atan2(y, x)) + margin;
    }

    inline RenderPoint Bezier(RenderPoint a, RenderPoint b, RenderPoint c, RenderPoint d, float t)
    {
        float u = 1.f - t;
        return {u * u * u * a.x + 3 * u * u * t * b.x + 3 * u * t * t * c.x + t * t * t * d.x,
                u * u * u * a.y + 3 * u * u * t * b.y + 3 * u * t * t * c.y + t * t * t * d.y};
    }

    struct Trail
    {
        RenderPoint a, b, c, d;
        float width;
    };

    inline const std::array<Trail, 6>& Trails()
    {
        static const std::array<Trail, 6> trails = {
            {{{0, 9}, {2, 4}, {-1, 1}, {0, -2}, .95f},
             {{0, 1}, {-4, -.4f}, {-4, -6.5f}, {-8, -5}, .65f},
             {{0, 1}, {5, 1}, {1, -7.8f}, {6, -7}, .70f},
             {{-8, -5}, {-14, -4}, {-11, -17}, {-20, -20}, .55f},
             {{0, 8}, {7, 14}, {14, 7}, {23, 18}, .8f},
             {{0, 1}, {6, 4}, {10, 3}, {12, 9}, .7f}}};
        return trails;
    }

    inline bool OnTrail(RenderPoint p)
    {
        for (const Trail& trail : Trails())
        {
            for (int i = 0; i <= 32; ++i)
            {
                RenderPoint q = Bezier(trail.a, trail.b, trail.c, trail.d, i / 32.f);
                float dx = p.x - q.x, dy = p.y - q.y, clearance = trail.width + .85f;
                if (dx * dx + dy * dy < clearance * clearance)
                {
                    return true;
                }
            }
        }
        return false;
    }
} // namespace WorldGeometry
