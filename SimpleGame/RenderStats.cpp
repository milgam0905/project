#include "stdafx.h"
#include "RenderStats.h"
#include "Profiler.h"
#include <string>

namespace
{
    bool active = false;
    std::uint64_t draws = 0, primitives = 0;
    std::string drawKey = "render.other.draw_calls";
    std::string sourceKey = "render.other.source_primitives";
} // namespace

void RenderStats::BeginFrame()
{
    draws = primitives = 0;
    active = true;
    Profiler::BeginFrame();
    SetPass("render.other");
}

void RenderStats::SetPass(const char* name)
{
    drawKey = std::string(name) + ".draw_calls";
    sourceKey = std::string(name) + ".source_primitives";
}

void RenderStats::DrawCall(std::uint64_t indices)
{
    if (active)
    {
        ++draws;
    }
    Profiler::Count("render.draw_calls");
    Profiler::Count(drawKey.c_str());
    Profiler::Count("render.submitted_indices", indices);
}

void RenderStats::SourcePrimitive()
{
    Profiler::Count(sourceKey.c_str());
    if (active)
    {
        ++primitives;
    }
}

void RenderStats::BeginPrimitive(GLenum mode)
{
    SourcePrimitive();
    DrawCall();
    glBegin(mode);
}

void RenderStats::EndFrame()
{
    if (active)
    {
        active = false;
        Profiler::Gauge("render.last_draw_calls", static_cast<double>(draws));
        Profiler::Gauge("render.last_source_primitives", static_cast<double>(primitives));
        Profiler::EndFrame(draws, primitives);
    }
}
