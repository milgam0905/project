#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Dependencies/glew.h"
#include <cstdint>

// Render-thread-only instrumentation. Counts API submissions, not GPU driver batches.
namespace RenderStats
{
    void BeginFrame();
    void EndFrame();
    void DrawCall(std::uint64_t indices = 0);
    void SourcePrimitive();
    void SetPass(const char* name);

    // Only for explicitly flushed immediate draws (the three fullscreen postprocess passes).
    // Ordinary geometry must use RenderQueue; DrawCall counts backend submissions.
    void BeginPrimitive(GLenum mode);
} // namespace RenderStats
