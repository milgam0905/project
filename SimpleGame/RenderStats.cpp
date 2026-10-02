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
    std::uint64_t passSources = 0, passDraws = 0, passIndices = 0;

    void SubmitCounters()
    {
        Profiler::Count(sourceKey.c_str(), passSources);
        Profiler::Count(drawKey.c_str(), passDraws);
        Profiler::Count("render.draw_calls", passDraws);
        Profiler::Count("render.submitted_indices", passIndices);
        passSources = passDraws = passIndices = 0;
    }
} // namespace

void RenderStats::BeginFrame()
{
    SubmitCounters();
    draws = primitives = 0;
    active = true;
    Profiler::BeginFrame();
    SetPass("render.other");
}

void RenderStats::SetPass(const char* name)
{
    SubmitCounters();
    drawKey = std::string(name) + ".draw_calls";
    sourceKey = std::string(name) + ".source_primitives";
}

void RenderStats::DrawCall(std::uint64_t indices)
{
    if (active)
    {
        ++draws;
    }
    ++passDraws;
    passIndices += indices;
}

void RenderStats::SourcePrimitive()
{
    ++passSources;
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
        SubmitCounters();
        Profiler::Gauge("render.last_draw_calls", static_cast<double>(draws));
        Profiler::Gauge("render.last_source_primitives", static_cast<double>(primitives));
        Profiler::EndFrame(draws, primitives);
    }
}
