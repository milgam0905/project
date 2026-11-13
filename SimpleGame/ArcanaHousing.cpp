#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "ArcanaWorld.h"
#include "WorldGeometry.h"
#include "Profiler.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace Arcana
{
    namespace
    {
        float RoadDistance(RenderPoint point, RenderPoint start, RenderPoint end)
        {
            const float dx = end.x - start.x, dy = end.y - start.y;
            const float t = std::clamp(((point.x - start.x) * dx + (point.y - start.y) * dy) /
                                           std::max(.01f, dx * dx + dy * dy),
                                       0.f,
                                       1.f);
            return Lena::Distance(point, {start.x + t * dx, start.y + t * dy});
        }
    } // namespace

    void BuildVillageHouses(Lena::World& world)
    {
        Profiler::Scope scope("housing.village_layout_build");
        world.houses.clear();

        const std::array<RenderPoint, 6> original = {
            {{-6, 0}, {6, 0}, {0, -6}, {0, 6}, {-7, 3}, {7, -3}}};
        std::vector<RenderPoint> candidates;
        for (int y = -16; y <= 16; y += 4)
        {
            for (int x = -16; x <= 16; x += 4)
            {
                const RenderPoint p{static_cast<float>(x), static_cast<float>(y)};
                const float distance = Lena::Distance(p, {0, 0});
                if (distance >= 8 && distance <= 18)
                {
                    candidates.push_back(p);
                }
            }
        }

        std::stable_sort(candidates.begin(),
                         candidates.end(),
                         [](RenderPoint a, RenderPoint b)
                         {
                             return a.x * a.x + a.y * a.y < b.x * b.x + b.y * b.y;
                         });

        for (const auto& village : Villages())
        {
            const size_t begin = world.houses.size();
            for (auto offset : original)
            {
                world.houses.push_back({village.center.x + offset.x, village.center.y + offset.y});
            }

            for (auto offset : candidates)
            {
                const RenderPoint p{village.center.x + offset.x, village.center.y + offset.y};
                bool blocked = Lena::Distance(p, world.canal) < 4;
                for (auto end : Villages())
                {
                    blocked = blocked || RoadDistance(p, world.square, end.center) < 3.2f;
                }

                blocked = blocked || RoadDistance(p, world.canal, world.square) < 3.2f ||
                          RoadDistance(p, world.square, {-4, 5}) < 3.2f ||
                          Lena::Distance(p, {7, 5}) < 3.5f;
                for (auto house : world.houses)
                {
                    blocked = blocked ||
                              (std::abs(p.x - house.x) < 4.f && std::abs(p.y - house.y) < 3.8f);
                }

                // Reserve the footprint, doorway and a small shore clearance.
                for (float dx : {-1.7f, 0.f, 1.7f})
                {
                    for (float dy : {-1.4f, 0.f, 2.f})
                    {
                        blocked = blocked || world.Water({p.x + dx, p.y + dy});
                    }
                }

                if (!blocked)
                {
                    world.houses.push_back(p);
                }
                if (world.houses.size() - begin == HousesPerVillage)
                {
                    break;
                }
            }

            if (world.houses.size() - begin != HousesPerVillage)
            {
                throw std::runtime_error(
                    "ARCANA village layout has insufficient clear building sites");
            }
        }

        Profiler::Gauge("housing.building_count", static_cast<double>(world.houses.size()));
    }

    void Module::AssignHomes()
    {
        Profiler::Scope scope("housing.assign_homes");
        std::array<int, 5> residents{};
        for (auto id : world.people)
        {
            auto* p = world.Find(id);
            if (!p || p->species != Lena::Species::Human || p->role == Lena::Role::Player ||
                p->settlement < 0 || p->settlement >= 5)
            {
                continue;
            }
            const int slot = residents[p->settlement]++;
            p->household = p->settlement * HousesPerVillage + slot % HousesPerVillage;
            const auto house = world.houses.at(p->household);
            // Home is a walkable doorway, not the blocked center of the building.
            p->home = world.Safe({house.x, house.y + 1.8f});
            if (p->key.compare(0, 7, "arcana:") != 0)
            {
                continue;
            }
            const auto center = Villages()[p->settlement].center;
            const float angle = static_cast<float>(slot) * 2.399963f;
            const float radius =
                p->role == Lena::Role::Guard ? 10.f : 3.5f + static_cast<float>(slot % 3);
            p->work = world.Safe(
                {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius});
            if (p->role == Lena::Role::Merchant)
            {
                p->work = world.Safe({center.x + 2.5f + static_cast<float>(slot % 3) * .7f,
                                      center.y + 1.5f + static_cast<float>(slot % 2) * .9f});
            }
        }
    }

    bool Module::WorkDue(const Lena::Person& p) const
    {
        if (p.role == Lena::Role::Guard ||
            (p.key == "bran_01" && progress.step >= 3 && progress.step <= 5))
        {
            return true;
        }
        if (p.courier || p.key == "serdin_01")
        {
            return false;
        }
        const double hour = std::fmod(world.minutes / 60 + 6, 24);
        const double phase =
            std::fmod(world.minutes * 60 + static_cast<double>((p.GetId() % 120) * 37), 120);
        return hour >= 6 && hour < 20 && phase < 40;
    }

    bool Module::WantsHome(const Lena::Person& p) const
    {
        using namespace Lena;
        if (p.species != Species::Human || p.role == Role::Player || p.household < 0 ||
            p.state.detained || p.state.surrendered || p.state.threatSeconds > 0 ||
            p.outdoorVisitSeconds > 0 || p.key == "serdin_01" ||
            (p.key == "bran_01" && progress.step >= 3 && progress.step <= 5))
        {
            return false;
        }
        if (p.courier && Location(p.Position()) != p.settlement)
        {
            // Waiting at a foreign market is part of a delivery, not unemployment.
            return false;
        }
        switch (p.state.action)
        {
        case Action::Idle:
        case Action::Wander:
        case Action::Sleep:
        case Action::Rest:
        case Action::Eat:
        case Action::Mourn:
            return true;
        case Action::Work:
            return !WorkDue(p);
        default:
            return false;
        }
    }

    bool Module::ManageHousing(Lena::Person& p, float dt)
    {
        if (p.species != Lena::Species::Human || p.role == Lena::Role::Player || p.household < 0)
        {
            return false;
        }
        Profiler::Scope scope("housing.update");
        if (!WantsHome(p))
        {
            p.returningHome = false;
            if (p.indoors)
            {
                p.indoors = false;
                p.SetVisible(true);
                p.path.clear();
                p.pathIndex = 0;
                p.sightTimer = 0;
                p.routeTimer = 0;
                Profiler::Count("housing.exits");
            }
            return false;
        }
        if (p.state.action == Lena::Action::Work)
        {
            world.SetAction(p, Lena::Action::Idle);
        }
        if (!p.indoors)
        {
            if (!p.returningHome)
            {
                p.path.clear();
                p.pathIndex = 0;
                p.routeTimer = 0;
                p.sightTimer = 0;
                Profiler::Count("housing.returns_started");
            }
            p.returningHome = true;
            p.state.destination = p.home;
            if (Lena::Distance(p.Position(), p.home) > .45f)
            {
                world.Move(p, p.home, 2.4f * dt);
                return true;
            }
            p.SetPosition(p.home);
            p.indoors = true;
            p.returningHome = false;
            p.motionSeconds = 0;
            p.SetVisible(false);
            Profiler::Count("housing.entries");
        }
        // Sleep/eat/rest still affect needs indoors. Hidden actors remain active.
        return p.state.action == Lena::Action::Idle || p.state.action == Lena::Action::Wander;
    }

    bool Module::CallResident()
    {
        Lena::Person* resident = nullptr;
        float nearest = 2.f;
        for (auto id : world.people)
        {
            auto* p = world.Find(id);
            if (!p || !p->indoors || p->state.dead || p->state.detained || p->state.surrendered)
            {
                continue;
            }
            const float distance = Lena::Distance(world.Player().Position(), p->home);
            if (distance < nearest && world.Visible(world.Player().Position(), p->home))
            {
                nearest = distance;
                resident = p;
            }
        }
        if (!resident)
        {
            return false;
        }
        resident->outdoorVisitSeconds = 10.f;
        world.SetAction(*resident, Lena::Action::Talk, world.playerId);
        resident->state.decision = 0;
        ManageHousing(*resident, 0);
        world.RebuildIndex();
        world.Log(resident->displayName + L": 부르셨나요?", Lena::MessageKind::Dialogue);
        Profiler::Count("housing.door_calls");
        return true;
    }
} // namespace Arcana
