#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winhttp.h>
#include "WorldMonitor.h"
#include "Profiler.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <iomanip>
#include <locale>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

namespace
{
    using Clock = std::chrono::steady_clock;

    std::string Utf8(const std::wstring& value)
    {
        if (value.empty())
        {
            return {};
        }
        int size = WideCharToMultiByte(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0)
        {
            return {};
        }
        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8,
                            0,
                            value.data(),
                            static_cast<int>(value.size()),
                            &result[0],
                            size,
                            nullptr,
                            nullptr);
        return result;
    }

    std::string Quote(const std::string& value)
    {
        std::string result = "\"";
        const char hex[] = "0123456789abcdef";
        for (unsigned char c : value)
        {
            if (c == '"' || c == '\\')
            {
                result += '\\';
                result += static_cast<char>(c);
            }
            else if (c < 32)
            {
                result += "\\u00";
                result += hex[c >> 4];
                result += hex[c & 15];
            }
            else
            {
                result += static_cast<char>(c);
            }
        }
        return result + '"';
    }

    void Number(std::ostream& out, double value, bool unknown = false)
    {
        if (!std::isfinite(value) || unknown)
        {
            out << "null";
        }
        else
        {
            out << value;
        }
    }

    void Metrics(std::ostream& out, const std::map<std::string, double>& values)
    {
        out << '{';
        bool first = true;
        for (const auto& pair : values)
        {
            out << (first ? "" : ",") << Quote(pair.first) << ':';
            Number(out, pair.second);
            first = false;
        }
        out << '}';
    }

    struct HttpHandle
    {
        HINTERNET value = nullptr;

        explicit HttpHandle(HINTERNET value) : value(value)
        {
        }

        ~HttpHandle()
        {
            if (value)
            {
                WinHttpCloseHandle(value);
            }
        }

        HttpHandle(const HttpHandle&) = delete;
        HttpHandle& operator=(const HttpHandle&) = delete;
    };

    bool Send(const std::string& body)
    {
        // Fixed loopback destination, no proxies, no redirects, no game-thread networking.
        HttpHandle session(WinHttpOpen(L"SimpleGameMonitor/1",
                                       WINHTTP_ACCESS_TYPE_NO_PROXY,
                                       WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS,
                                       0));
        if (!session.value)
        {
            return false;
        }
        WinHttpSetTimeouts(session.value, 250, 250, 250, 250);
        HttpHandle connection(WinHttpConnect(session.value, L"127.0.0.1", 8765, 0));
        if (!connection.value)
        {
            return false;
        }
        HttpHandle request(WinHttpOpenRequest(connection.value,
                                              L"POST",
                                              L"/api/snapshots",
                                              nullptr,
                                              WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES,
                                              0));
        if (!request.value)
        {
            return false;
        }
        DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!WinHttpSetOption(
                request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects)))
        {
            return false;
        }
        const wchar_t headers[] = L"Content-Type: application/json; charset=utf-8\r\n";
        if (!WinHttpSendRequest(request.value,
                                headers,
                                static_cast<DWORD>(-1),
                                const_cast<char*>(body.data()),
                                static_cast<DWORD>(body.size()),
                                static_cast<DWORD>(body.size()),
                                0) ||
            !WinHttpReceiveResponse(request.value, nullptr))
        {
            return false;
        }
        DWORD status = 0, size = sizeof(status);
        return WinHttpQueryHeaders(request.value,
                                   WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                   WINHTTP_HEADER_NAME_BY_INDEX,
                                   &status,
                                   &size,
                                   WINHTTP_NO_HEADER_INDEX) &&
               status == 202;
    }

    std::string Serialize(const WorldMonitor::Snapshot& snapshot,
                          const std::string& worldId,
                          const std::string& instanceId,
                          std::uint64_t sequence)
    {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::fixed << std::setprecision(3) << std::boolalpha;
        out << "{\"schema\":\"world-monitor.v1\",\"type\":\"snapshot\",\"game_id\":\"simplegame\","
            << "\"world_id\":" << Quote(worldId) << ",\"instance_id\":" << Quote(instanceId)
            << ",\"sequence\":" << sequence << ",\"epoch\":" << snapshot.epoch
            << ",\"paused\":" << snapshot.paused << ",\"world_time_minutes\":";
        Number(out, snapshot.worldMinutes, snapshot.worldMinutes < 0);
        out << ",\"bounds\":{\"min_x\":" << snapshot.minX << ",\"min_y\":" << snapshot.minY
            << ",\"max_x\":" << snapshot.maxX << ",\"max_y\":" << snapshot.maxY << "},\"metrics\":";
        Metrics(out, snapshot.metrics);
        out << ",\"settlements\":[";
        bool first = true;
        for (const auto& town : snapshot.settlements)
        {
            out << (first ? "" : ",") << "{\"id\":" << Quote(town.id)
                << ",\"name\":" << Quote(Utf8(town.name)) << ",\"metrics\":";
            Metrics(out, town.metrics);
            out << '}';
            first = false;
        }
        out << "],\"entities\":[";
        first = true;
        for (const auto& entity : snapshot.entities)
        {
            out << (first ? "" : ",") << "{\"id\":" << Quote(entity.id)
                << ",\"name\":" << Quote(Utf8(entity.name)) << ",\"kind\":" << Quote(entity.kind)
                << ",\"settlement_id\":"
                << (entity.settlementId.empty() ? "null" : Quote(entity.settlementId))
                << ",\"role\":" << Quote(entity.role) << ",\"action\":" << Quote(entity.action)
                << ",\"action_label\":" << Quote(Utf8(entity.actionLabel))
                << ",\"target_id\":" << (entity.targetId.empty() ? "null" : Quote(entity.targetId))
                << ",\"position\":{\"x\":" << entity.x << ",\"y\":" << entity.y << "},\"health\":";
            Number(out, entity.health, entity.health < 0);
            out << ",\"max_health\":";
            Number(out, entity.maxHealth, entity.maxHealth < 0);
            out << ",\"alive\":" << entity.alive << ",\"active\":" << entity.active
                << ",\"visible\":" << entity.visible << ",\"hostile\":" << entity.hostile
                << ",\"metrics\":";
            Metrics(out, entity.metrics);
            out << ",\"personality\":";
            Metrics(out, entity.personality);
            out << '}';
            first = false;
        }
        out << "]}";
        return out.str();
    }

    struct Client
    {
        std::string worldId, instanceId;
        std::unique_ptr<WorldMonitor::Snapshot> pending;
        std::uint64_t sequence = 0;
        std::mutex mutex;
        std::condition_variable changed;
        bool stopping = false;
        std::thread worker;
        Clock::time_point next = Clock::now();
        std::atomic<std::uint64_t> sent{0}, failed{0};
        std::uint64_t reportedSent = 0, reportedFailed = 0;
        std::atomic<int> lastStatus{0};
        std::atomic<double> lastSendMs{0}, lastSerializeMs{0};
        std::atomic<std::uint64_t> serializedBytes{0}, oversized{0};

        explicit Client(const std::string& world) : worldId(world)
        {
            FILETIME time;
            GetSystemTimeAsFileTime(&time);
            ULARGE_INTEGER stamp;
            stamp.LowPart = time.dwLowDateTime;
            stamp.HighPart = time.dwHighDateTime;
            instanceId =
                std::to_string(GetCurrentProcessId()) + "-" + std::to_string(stamp.QuadPart);
            worker = std::thread(
                [this]
                {
                    for (;;)
                    {
                        std::unique_ptr<WorldMonitor::Snapshot> snapshot;
                        {
                            std::unique_lock<std::mutex> lock(mutex);
                            changed.wait(lock,
                                         [this]
                                         {
                                             return stopping || pending != nullptr;
                                         });
                            if (stopping)
                            {
                                return;
                            }
                            snapshot = std::move(pending);
                        }
                        const auto serializationStarted = Clock::now();
                        std::string body;
                        try
                        {
                            body = Serialize(*snapshot, worldId, instanceId, ++sequence);
                        }
                        catch (...)
                        {
                            ++failed;
                            lastStatus = -1;
                            continue;
                        }
                        snapshot.reset();
                        lastSerializeMs = std::chrono::duration<double, std::milli>(
                                              Clock::now() - serializationStarted)
                                              .count();
                        if (body.size() > 1024 * 1024)
                        {
                            ++oversized;
                            continue;
                        }
                        serializedBytes.fetch_add(body.size());
                        const auto started = Clock::now();
                        const bool delivered = Send(body);
                        lastSendMs =
                            std::chrono::duration<double, std::milli>(Clock::now() - started)
                                .count();
                        if (delivered)
                        {
                            ++sent;
                            lastStatus = 1;
                        }
                        else
                        {
                            ++failed;
                            lastStatus = -1;
                            std::unique_lock<std::mutex> lock(mutex);
                            changed.wait_for(lock,
                                             std::chrono::seconds(2),
                                             [this]
                                             {
                                                 return stopping;
                                             });
                            if (stopping)
                            {
                                return;
                            }
                        }
                    }
                });
        }

        ~Client()
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                stopping = true;
                pending.reset();
            }
            changed.notify_one();
            if (worker.joinable())
            {
                worker.join();
            }
        }
    };

    std::unique_ptr<Client> client;
} // namespace

std::uint64_t WorldMonitor::NextEpoch()
{
    static std::uint64_t epoch = 0;
    return ++epoch;
}

void WorldMonitor::Start(const std::string& worldId)
{
    Stop();
    client.reset(new Client(worldId));
}

bool WorldMonitor::IsDue()
{
    if (!client || Clock::now() < client->next)
    {
        return false;
    }
    client->next = Clock::now() + std::chrono::seconds(1);
    return true;
}

bool WorldMonitor::Enabled()
{
    return client != nullptr;
}

void WorldMonitor::Publish(Snapshot snapshot)
{
    if (!client)
    {
        return;
    }
    Profiler::Scope scope("monitor.enqueue");
    Profiler::Gauge("monitor.entity_count", static_cast<double>(snapshot.entities.size()));
    auto pending = std::make_unique<Snapshot>(std::move(snapshot));
    {
        std::lock_guard<std::mutex> lock(client->mutex);
        if (client->pending)
        {
            Profiler::Count("monitor.coalesced_snapshots");
        }
        client->pending = std::move(pending);
    }
    client->changed.notify_one();
    const auto sent = client->sent.load(), failed = client->failed.load();
    Profiler::Count("monitor.sent", sent - client->reportedSent);
    Profiler::Count("monitor.send_failures", failed - client->reportedFailed);
    client->reportedSent = sent;
    client->reportedFailed = failed;
    Profiler::Count("monitor.snapshot_bytes", client->serializedBytes.exchange(0));
    Profiler::Count("monitor.oversize_snapshots", client->oversized.exchange(0));
    Profiler::Gauge("monitor.last_serialize_wall_ms", client->lastSerializeMs.load());
    Profiler::Gauge("monitor.last_send_status", client->lastStatus.load());
    Profiler::Gauge("monitor.last_send_wall_ms", client->lastSendMs.load());
}

void WorldMonitor::Stop()
{
    client.reset();
}
