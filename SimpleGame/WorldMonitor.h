#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Read-only telemetry DTOs. No Actor pointers or gameplay commands cross this boundary.
namespace WorldMonitor
{
    struct Entity
    {
        std::string id, kind, settlementId, role, action, targetId;
        std::wstring name, actionLabel;
        double x = 0, y = 0, health = -1, maxHealth = -1;
        bool alive = true, active = true, visible = true, hostile = false;
        std::map<std::string, double> metrics, personality;
    };

    struct Settlement
    {
        std::string id;
        std::wstring name;
        std::map<std::string, double> metrics;
    };

    struct Snapshot
    {
        std::uint64_t epoch = 0;
        bool paused = false;
        double worldMinutes = -1;
        double minX = 0, minY = 0, maxX = 1, maxY = 1;
        std::vector<Settlement> settlements;
        std::vector<Entity> entities;
        std::map<std::string, double> metrics;
    };

    // Public API, DTO collection and profiler calls: main thread only.
    // The worker owns JSON serialization and HTTP; it publishes atomic measurements only.
    std::uint64_t NextEpoch();
    void Start(const std::string& worldId);
    bool IsDue();
    bool Enabled();
    void Publish(Snapshot snapshot);
    void Stop();
} // namespace WorldMonitor
