#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Dependencies/glew.h"
#include <Windows.h>
#include "Profiler.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    using Clock = Profiler::Clock;

    struct Metric
    {
        std::uint64_t count = 0;
        double sum = 0, maximum = 0, minimum = std::numeric_limits<double>::max();

        void Add(double value)
        {
            ++count;
            sum += value;
            maximum = std::max(maximum, value);
            minimum = std::min(minimum, value);
        }
    };

    struct Query
    {
        GLuint begin = 0, end = 0;
        bool busy = false, ended = false;
        const char* key = nullptr;
        std::uint64_t frame = 0;
    };

    bool enabled = false, gpuSupported = false;
    std::string mode;
    using Metrics = std::map<std::string, Metric, std::less<>>;
    Metrics cpu, gpu;
    std::map<std::string, std::uint64_t, std::less<>> counts;
    std::map<std::string, double, std::less<>> gauges;
    std::array<Query, 96> queries;
    Clock::time_point sessionStart, windowStart, frameStart, previousFrame, lastSpike;
    bool hasPreviousFrame = false;
    std::uint64_t frameId = 0, windowFrames = 0;
    Metric frameIntervals, frameSubmission, draws, primitives;
    std::vector<double> intervalSamples;
    std::ofstream logFile;
    std::wstring logPath, previousPath;
    std::uint64_t logBytes = 0;
    constexpr std::uint64_t LogLimit = 8 * 1024 * 1024;

    double Ms(Clock::duration duration)
    {
        return std::chrono::duration<double, std::milli>(duration).count();
    }

    std::string Escape(const char* value)
    {
        std::string result;
        if (!value)
        {
            return result;
        }
        for (const unsigned char c : std::string(value))
        {
            if (c == '"' || c == '\\')
            {
                result += '\\';
                result += static_cast<char>(c);
            }
            else if (c >= 32)
            {
                result += static_cast<char>(c);
            }
        }
        return result;
    }

    void Emit(const std::string& json)
    {
        const auto start = Clock::now();
        std::printf("%s\n", json.c_str());
        std::fflush(stdout);
        if (logFile && logBytes + json.size() + 1 > LogLimit)
        {
            logFile.close();
            if (MoveFileExW(logPath.c_str(), previousPath.c_str(), MOVEFILE_REPLACE_EXISTING))
            {
                logFile.open(logPath.c_str(), std::ios::binary | std::ios::trunc);
                logBytes = 0;
            }
        }
        if (logFile.is_open())
        {
            logFile << json << '\n';
            logFile.flush();
            logBytes += json.size() + 1;
        }
        gauges["logging.file_available"] = logFile.is_open() && logFile.good() ? 1 : 0;
        gauges["logging.last_write_cpu_ms"] = Ms(Clock::now() - start);
    }

    void MetricJson(std::ostream& out, const Metric& value)
    {
        out << "{\"samples\":" << value.count << ",\"sum\":" << value.sum
            << ",\"avg\":" << (value.count ? value.sum / value.count : 0)
            << ",\"min\":" << (value.count ? value.minimum : 0) << ",\"max\":" << value.maximum
            << "}";
    }

    void MetricMap(std::ostream& out, const Metrics& values)
    {
        out << "{";
        bool first = true;
        for (const auto& pair : values)
        {
            out << (first ? "" : ",") << '"' << pair.first << "\":";
            MetricJson(out, pair.second);
            first = false;
        }
        out << "}";
    }

    void PollGpu()
    {
        if (!gpuSupported)
        {
            return;
        }
        for (Query& query : queries)
        {
            if (!query.busy || !query.ended)
            {
                continue;
            }
            GLint available = GL_FALSE;
            glGetQueryObjectiv(query.end, GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available)
            {
                continue;
            }
            GLuint64 begin = 0, end = 0;
            glGetQueryObjectui64v(query.begin, GL_QUERY_RESULT, &begin);
            glGetQueryObjectui64v(query.end, GL_QUERY_RESULT, &end);
            if (end >= begin)
            {
                gpu[query.key].Add(static_cast<double>(end - begin) / 1000000.0);
            }
            gauges["gpu.last_result_age_frames"] = static_cast<double>(frameId - query.frame);
            query.busy = false;
            query.ended = false;
        }
    }

    void Report(const char* phase = "runtime")
    {
        const auto now = Clock::now();
        const double seconds = Ms(now - windowStart) / 1000.0;
        if (seconds <= 0)
        {
            return;
        }
        std::sort(intervalSamples.begin(), intervalSamples.end());
        const double p95 =
            intervalSamples.empty()
                ? 0
                : intervalSamples[static_cast<size_t>((intervalSamples.size() - 1) * .95)];
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::fixed << std::setprecision(3)
            << "{\"schema\":\"simplegame.profile.v1\",\"event\":\"profile_window\","
            << "\"phase\":\"" << phase << "\","
            << "\"mode\":\"" << mode << "\",\"elapsed_ms\":" << Ms(now - sessionStart)
            << ",\"window_seconds\":" << seconds << ",\"frame_id\":" << frameId
            << ",\"frames\":" << windowFrames << ",\"fps\":" << windowFrames / seconds
            << ",\"frame_interval_ms\":";
        MetricJson(out, frameIntervals);
        out << ",\"frame_interval_p95_ms\":" << p95 << ",\"render_and_swap_cpu_ms\":";
        MetricJson(out, frameSubmission);
        out << ",\"draw_calls_per_frame\":";
        MetricJson(out, draws);
        out << ",\"source_primitives_per_frame\":";
        MetricJson(out, primitives);
        out << ",\"cpu_ms_inclusive\":";
        MetricMap(out, cpu);
        out << ",\"gpu_supported\":" << (gpuSupported ? "true" : "false")
            << ",\"gpu_ms_delayed_inclusive\":";
        MetricMap(out, gpu);
        out << ",\"counters_window\":{";
        bool first = true;
        for (const auto& pair : counts)
        {
            out << (first ? "" : ",") << '"' << pair.first << "\":" << pair.second;
            first = false;
        }
        out << "},\"gauges_last\":{";
        first = true;
        for (const auto& pair : gauges)
        {
            out << (first ? "" : ",") << '"' << pair.first << "\":" << pair.second;
            first = false;
        }
        out << "}}";
        Emit(out.str());
        cpu.clear();
        gpu.clear();
        counts.clear();
        windowFrames = 0;
        frameIntervals = {};
        frameSubmission = {};
        draws = {};
        primitives = {};
        intervalSamples.clear();
        windowStart = now;
    }
} // namespace

void Profiler::Initialize(bool requested, const char* gameMode)
{
    enabled = requested;
    if (!enabled)
    {
        return;
    }
    mode = gameMode;
    sessionStart = windowStart = lastSpike = Clock::now();
    gpuSupported =
        (GLEW_VERSION_3_3 || GLEW_ARB_timer_query) && glQueryCounter && glGetQueryObjectui64v;
    if (gpuSupported)
    {
        GLint bits = 0;
        glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &bits);
        gpuSupported = bits > 0;
    }
    if (gpuSupported)
    {
        for (Query& query : queries)
        {
            glGenQueries(1, &query.begin);
            glGenQueries(1, &query.end);
        }
    }
    wchar_t directory[32768] = {};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, 32768);
    if (length > 0 && length < 32768)
    {
        std::wstring path = std::wstring(directory) + L"\\SimpleGame";
        CreateDirectoryW(path.c_str(), nullptr);
        path += L"\\Profiles";
        CreateDirectoryW(path.c_str(), nullptr);
        // Per-process files prevent concurrently running instances from clobbering each other.
        path += L"\\profile-" + std::to_wstring(GetCurrentProcessId());
        logPath = path + L"-current.jsonl";
        previousPath = path + L"-previous.jsonl";
        logFile.open(logPath.c_str(), std::ios::binary | std::ios::trunc);
    }
    std::ostringstream out;
#ifdef NDEBUG
    const char* buildMode = "release";
#else
    const char* buildMode = "debug";
#endif
    out << "{\"schema\":\"simplegame.profile.v1\",\"event\":\"session_start\","
        << "\"build_mode\":\"" << buildMode << "\","
        << "\"mode\":\"" << mode << "\",\"pid\":" << GetCurrentProcessId()
        << ",\"gpu_supported\":" << (gpuSupported ? "true" : "false") << ",\"gl_version\":\""
        << Escape(reinterpret_cast<const char*>(glGetString(GL_VERSION))) << "\",\"gl_renderer\":\""
        << Escape(reinterpret_cast<const char*>(glGetString(GL_RENDERER)))
        << "\",\"log_file_available\":" << (logFile ? "true" : "false") << "}";
    Emit(out.str());
}

bool Profiler::Enabled()
{
    return enabled;
}

void Profiler::Count(const char* key, std::uint64_t amount)
{
    if (!enabled)
    {
        return;
    }
    auto found = counts.find(key);
    if (found != counts.end())
    {
        found->second += amount;
    }
    else if (counts.size() < 128)
    {
        counts.emplace(key, amount);
    }
}

void Profiler::Gauge(const char* key, double value)
{
    if (!enabled)
    {
        return;
    }
    auto found = gauges.find(key);
    if (found != gauges.end())
    {
        found->second = value;
    }
    else if (gauges.size() < 128)
    {
        gauges.emplace(key, value);
    }
}

Profiler::Scope::Scope(const char* value) : key(value), enabled(Profiler::Enabled())
{
    if (enabled)
    {
        start = Clock::now();
    }
}

Profiler::Scope::~Scope()
{
    if (!enabled)
    {
        return;
    }
    const double milliseconds = Ms(Clock::now() - start);
    auto found = cpu.find(key);
    if (found != cpu.end())
    {
        found->second.Add(milliseconds);
    }
    else if (cpu.size() < 128)
    {
        cpu.emplace(key, Metric{}).first->second.Add(milliseconds);
    }
}

Profiler::GpuScope::GpuScope(const char* key)
{
    if (!enabled || !gpuSupported)
    {
        return;
    }
    for (size_t i = 0; i < queries.size(); ++i)
    {
        Query& query = queries[i];
        if (!query.busy)
        {
            query.busy = true;
            query.ended = false;
            query.key = key;
            query.frame = frameId;
            glQueryCounter(query.begin, GL_TIMESTAMP);
            slot = static_cast<int>(i);
            return;
        }
    }
    Count("gpu.query_samples_dropped");
}

Profiler::GpuScope::~GpuScope()
{
    if (slot >= 0)
    {
        Query& query = queries[slot];
        glQueryCounter(query.end, GL_TIMESTAMP);
        query.ended = true;
    }
}

void Profiler::BeginFrame()
{
    if (!enabled)
    {
        return;
    }
    if (!frameId)
    {
        Report("startup");
    }
    const auto now = Clock::now();
    ++frameId;
    if (hasPreviousFrame)
    {
        const double interval = Ms(now - previousFrame);
        frameIntervals.Add(interval);
        if (intervalSamples.size() < 4096)
        {
            intervalSamples.push_back(interval);
        }
        if (interval > 33.333 && Ms(now - lastSpike) >= 1000)
        {
            std::ostringstream out;
            out.imbue(std::locale::classic());
            out << "{\"schema\":\"simplegame.profile.v1\",\"event\":\"frame_interval_spike\","
                << "\"frame_id\":" << frameId << ",\"interval_ms\":" << interval
                << ",\"threshold_ms\":33.333}";
            Emit(out.str());
            lastSpike = now;
        }
    }
    previousFrame = now;
    hasPreviousFrame = true;
    frameStart = now;
    PollGpu();
}

void Profiler::EndFrame(std::uint64_t drawCalls, std::uint64_t sourcePrimitives)
{
    if (!enabled)
    {
        return;
    }
    ++windowFrames;
    frameSubmission.Add(Ms(Clock::now() - frameStart));
    draws.Add(static_cast<double>(drawCalls));
    primitives.Add(static_cast<double>(sourcePrimitives));
    if (Ms(Clock::now() - windowStart) >= 1000)
    {
        Report();
    }
}

void Profiler::Shutdown()
{
    if (!enabled)
    {
        return;
    }
    PollGpu();
    Report("shutdown");
    if (gpuSupported)
    {
        for (Query& query : queries)
        {
            glDeleteQueries(1, &query.begin);
            glDeleteQueries(1, &query.end);
        }
    }
    Emit("{\"schema\":\"simplegame.profile.v1\",\"event\":\"session_end\"}");
    logFile.close();
    enabled = false;
}
