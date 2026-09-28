#pragma once

#include <chrono>
#include <cstdint>

// Single render/game thread. Keys must be stable ASCII literals, never entity IDs or user text.
namespace Profiler
{
    using Clock = std::chrono::steady_clock;
    void Initialize(bool enabled, const char* mode);
    void Shutdown();
    bool Enabled();
    void Count(const char* key, std::uint64_t amount = 1);
    void Gauge(const char* key, double value);
    void BeginFrame();
    void EndFrame(std::uint64_t drawCalls, std::uint64_t sourcePrimitives);

    class Scope
    {
      public:
        explicit Scope(const char* key);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

      private:
        const char* key;
        Clock::time_point start;
        bool enabled;
    };

    class GpuScope
    {
      public:
        explicit GpuScope(const char* key);
        ~GpuScope();
        GpuScope(const GpuScope&) = delete;
        GpuScope& operator=(const GpuScope&) = delete;

      private:
        int slot = -1;
    };
} // namespace Profiler
