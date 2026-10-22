#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "ArcanaWorld.h"
#include "WorldGeometry.h"
#include "Profiler.h"
#include "RenderQueue.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Arcana
{
    const std::array<Village, 5>& Villages()
    {
        // Each spoke projects to 3360 pixels: 19.76 seconds at 170 pixels/second.
        static const std::array<Village, 5> villages = {
            {{"belheim", "belheim_region", L"벨하임", {0, 3}},
             {"arsen", "arsen_region", L"아르센", {40, -37}},
             {"kardin", "kardin_region", L"카르딘", {-40, 43}},
             {"elysia_outpost", "elysia_region", L"엘리시아 전초마을", {-80, -77}},
             {"north_refuge", "north_forest", L"북부 숲 피난촌", {80, 83}}}};
        return villages;
    }

    Lena::Town& Module::Town(int village)
    {
        return village <= 0 ? world.town : progress.towns.at(village);
    }

    const Lena::Town& Module::Town(int village) const
    {
        return village <= 0 ? world.town : progress.towns.at(village);
    }

    int Module::Location(RenderPoint point, float radius) const
    {
        int result = -1;
        for (int i = 0; i < 5; ++i)
        {
            float distance = Lena::Distance(point, Villages()[i].center);
            if (distance < radius)
            {
                result = i;
                radius = distance;
            }
        }
        return result;
    }

    Lena::Person* Module::LocalGuard(const Lena::Person& reporter) const
    {
        int village = Location(reporter.Position());
        if (village < 0)
        {
            village = reporter.settlement;
        }
        Lena::Person* result = nullptr;
        float best = 1e9f;
        for (auto id : world.people)
        {
            auto* p = world.Find(id);
            if (!p || p->state.dead || !p->IsActive() || p->role != Lena::Role::Guard ||
                p->settlement != village)
            {
                continue;
            }
            float distance = Lena::Distance(reporter.Position(), p->Position());
            if (distance < best)
            {
                result = p;
                best = distance;
            }
        }
        return result;
    }

    RenderPoint Module::DrinkingPoint(const Lena::Person& person) const
    {
        int village = Location(person.Position(), 1e9f);
        const auto center = Villages()[village].center;
        return world.Safe({center.x + 1, center.y - 2});
    }

    void Module::PrepareMonsterSpawns()
    {
        Profiler::Scope scope("arcana.spawn_points_build");
        for (int branch = 1; branch < 5; ++branch)
        {
            auto a = Villages()[0].center;
            auto b = Villages()[branch].center;
            float length = Lena::Distance(a, b);
            RenderPoint normal{-(b.y - a.y) / length, (b.x - a.x) / length};
            for (int i = 0; i < 36; ++i)
            {
                float t = .43f + (i / 2) * .017f;
                float side = i % 2 == 0 ? -6.f : 6.f;
                RenderPoint p{a.x + (b.x - a.x) * t + normal.x * side,
                              a.y + (b.y - a.y) * t + normal.y * side};
                p = world.Safe(p);
                bool duplicate = std::any_of(monsterSpawns.begin(),
                                             monsterSpawns.end(),
                                             [&](RenderPoint other)
                                             {
                                                 return Lena::Distance(p, other) < .6f;
                                             });
                if (!duplicate && Location(p, 14) < 0 && Lena::Distance(p, world.canal) > 26 &&
                    world.Walkable(p))
                {
                    monsterSpawns.push_back(p);
                }
            }
        }
        if (monsterSpawns.size() < 100)
        {
            throw std::runtime_error("ARCANA expansion has insufficient connected monster spawns");
        }
        Profiler::Gauge("arcana.monster_spawn_points", static_cast<double>(monsterSpawns.size()));
    }

    bool Module::Respawn(Lena::Person& p)
    {
        for (size_t attempt = 0; attempt < monsterSpawns.size(); ++attempt)
        {
            auto candidate = monsterSpawns[(++progress.respawnSequence) % monsterSpawns.size()];
            if (Lena::Distance(candidate, world.canal) < 26 ||
                Lena::Distance(candidate, p.Position()) < 12 ||
                Lena::Distance(candidate, world.Player().Position()) < 12)
            {
                continue;
            }
            bool occupied = false;
            for (auto id : world.people)
            {
                auto* other = world.Find(id);
                occupied = occupied || (other && !other->state.dead &&
                                        Lena::Distance(candidate, other->Position()) < .7f);
            }
            if (occupied)
            {
                continue;
            }
            const unsigned generation = p.state.generation + 1;
            if (p.state.playerCredit && !p.state.absorbed &&
                (world.minutes - p.state.deathMinute) * 60 <= content.mana.windowSeconds)
            {
                Remnant remnant;
                remnant.slot = p.GetId();
                remnant.generation = p.state.generation;
                remnant.position = p.Position();
                remnant.minute = p.state.deathMinute;
                remnant.mana = p.state.manaMax;
                ShowRemnant(remnant);
                progress.remnants.push_back(remnant);
            }
            scheduler->clear(p.key);
            p.state = Lena::State{};
            p.state.generation = generation;
            p.state.health = p.maxHealth;
            p.state.manaCurrent = p.state.manaMax =
                static_cast<float>(definitions.at(p.key).manaMax);
            p.state.hunger = 35;
            p.state.destination = p.state.habitat = candidate;
            p.home = p.work = candidate;
            p.SetPosition(candidate);
            p.path.clear();
            p.pathIndex = 0;
            p.routeTimer = 0;
            p.sightTimer = 0;
            p.motionSeconds = 0;
            Profiler::Count("arcana.monsters_respawned");
            return true;
        }
        Profiler::Count("arcana.monster_respawns_deferred");
        return false;
    }

    void Module::Share(Lena::Person& source, Lena::Person& target)
    {
        Profiler::Scope scope("arcana.rumor_contact");
        if (source.state.dead || target.state.dead ||
            Lena::Distance(source.Position(), target.Position()) >= 1.4f ||
            !world.Visible(source.Position(), target.Position()))
        {
            return;
        }
        for (auto factId : source.state.known)
        {
            const auto* fact = world.GetFact(factId);
            if (fact && target.state.known.insert(factId).second)
            {
                target.state.grief = std::min(100.f, target.state.grief + (fact->death ? 12 : 0));
                target.state.fear = std::min(100.f, target.state.fear + (fact->crime ? 8 : 0));
                Profiler::Count("arcana.rumors_delivered");
                if (source.settlement != target.settlement)
                {
                    Profiler::Count("arcana.rumors_cross_village");
                }
            }
        }
    }

    void Module::UpdateExpansion(float dt)
    {
        Profiler::Scope scope("arcana.expansion_update");
        int monsters = 0, carriers = 0;
        bool indexChanged = false;
        auto& remnants = progress.remnants;
        for (auto it = remnants.begin(); it != remnants.end();)
        {
            if ((world.minutes - it->minute) * 60 > content.mana.windowSeconds)
            {
                if (auto* actor = world.scene.Find(it->visual))
                {
                    world.scene.Destroy(*actor);
                }
                it = remnants.erase(it);
            }
            else
            {
                ++it;
            }
        }
        for (auto id : world.people)
        {
            auto* p = world.Find(id);
            if (!p)
            {
                continue;
            }
            if (p->beast)
            {
                if (p->state.dead && (world.minutes - p->state.deathMinute) * 60 >= 5)
                {
                    indexChanged = Respawn(*p) || indexChanged;
                }
                monsters += !p->state.dead;
            }
            if (!p->courier || p->state.dead || p->state.detained || p->state.surrendered)
            {
                continue;
            }
            ++carriers;
            auto& s = p->state;
            s.travelWait = std::max(0.f, s.travelWait - dt);
            if (s.travelWait <= 0 && s.travelTown >= 0 &&
                Lena::Distance(p->Position(), Villages()[s.travelTown].center) < 1.5f)
            {
                s.travelTown = s.travelTown == 0
                                   ? static_cast<int>((world.tick / 60 + p->GetId()) % 4) + 1
                                   : 0;
                s.travelWait = 12;
                s.decision = 0;
                Profiler::Count("arcana.courier_arrivals");
            }
        }
        if (indexChanged)
        {
            world.RebuildIndex();
        }
        double seconds = world.minutes * 60 - progress.lastEconomy;
        if (seconds >= 10)
        {
            UpdateEconomy(seconds);
            progress.lastEconomy = world.minutes * 60;
        }
        Profiler::Gauge("arcana.monsters_alive", monsters);
        Profiler::Gauge("arcana.couriers_alive", carriers);
    }

    void Module::ShowRemnant(Remnant& remnant)
    {
        auto& actor = world.scene.Spawn(world.scene.Root(), "ArcanaManaRemnant");
        actor.SetPosition(remnant.position);
        actor.onDraw = [](const Actor& object, const SceneView& view)
        {
            auto p = view.Screen(object);
            RenderQueue::EnableTexture(false);
            RenderQueue::Color(.1f, .6f, 1.f, .7f);
            RenderQueue::Begin(GL_QUADS);
            for (auto offset : {RenderPoint{-12, -7},
                                RenderPoint{12, -7},
                                RenderPoint{12, -2},
                                RenderPoint{-12, -2}})
            {
                RenderQueue::Vertex(p.x + offset.x, p.y + offset.y);
            }
            RenderQueue::End();
        };
        remnant.visual = actor.GetId();
    }

    bool Module::AbsorbRemnant()
    {
        auto& remnants = progress.remnants;
        for (auto it = remnants.begin(); it != remnants.end(); ++it)
        {
            if (Near(it->position, 2) &&
                (world.minutes - it->minute) * 60 <= content.mana.windowSeconds)
            {
                auto& player = world.Player().state;
                double gain = bp::native::manaGain(
                    it->mana, player.manaMax, progress.boarKills, content.mana);
                player.manaMax += static_cast<float>(gain);
                ++progress.boarKills;
                if (auto* actor = world.scene.Find(it->visual))
                {
                    world.scene.Destroy(*actor);
                }
                remnants.erase(it);
                world.Log(L"남겨진 잔류 마력 흡수: 최대 마력 +" + std::to_wstring(gain));
                Profiler::Count("arcana.mana_absorptions");
                return true;
            }
        }
        return false;
    }

    void Module::UpdateEconomy(double seconds)
    {
        Profiler::Scope scope("arcana.village_economy");
        for (auto id : world.people)
        {
            auto* p = world.Find(id);
            if (!p || p->state.dead)
            {
                continue;
            }
            if (p->state.hunger > 95 || p->state.thirst > 95 || p->state.exposure > 80)
            {
                float damage = static_cast<float>(seconds / 86400 * 6);
                world.Damage(*p, *p, damage);
            }
        }
        for (int v = 0; v < 5; ++v)
        {
            auto& town = Town(v);
            int workers = 0, guards = 0;
            for (auto id : world.people)
            {
                auto* p = world.Find(id);
                if (!p || p->settlement != v || p->state.dead)
                {
                    continue;
                }
                guards += p->role == Lena::Role::Guard;
                workers += p->state.action == Lena::Action::Work &&
                           Lena::Distance(p->Position(), p->work) < 2;
                if (p->state.food < 1 && town.food >= 1 && Location(p->Position()) == v)
                {
                    town.food -= 1;
                    p->state.food += 1;
                }
            }
            double produced = workers * seconds / 600;
            town.food += produced;
            town.production += produced;
            town.security = std::min(100.0, town.security + guards * seconds / 36000);
        }
    }

    void Module::FillVillages(WorldMonitor::Snapshot& snapshot) const
    {
        Profiler::Scope scope("arcana.village_statistics");
        snapshot.settlements.clear();
        for (int v = 0; v < 5; ++v)
        {
            WorldMonitor::Settlement entry;
            entry.id = Villages()[v].id;
            entry.name = Villages()[v].name;
            const auto& town = Town(v);
            int count = 0, present = 0, dead = 0;
            double fear = 0, grief = 0, anger = 0, hunger = 0;
            std::set<std::uint64_t> known;
            for (auto id : world.people)
            {
                const auto* p = world.Find(id);
                if (!p || p->role == Lena::Role::Player || p->species != Lena::Species::Human)
                {
                    continue;
                }
                if (p->settlement == v)
                {
                    dead += p->state.dead;
                    if (!p->state.dead)
                    {
                        ++count;
                        entry.metrics["resident_food_total"] += p->state.food;
                        entry.metrics["resident_coins_total"] += p->state.coins;
                        entry.metrics["resident_medicine_total"] += p->state.medicine;
                    }
                }
                if (!p->state.dead && Location(p->Position()) == v)
                {
                    ++present;
                    fear += p->state.fear;
                    grief += p->state.grief;
                    anger += p->state.anger;
                    hunger += p->state.hunger;
                    known.insert(p->state.known.begin(), p->state.known.end());
                }
            }
            double divisor = std::max(1, present);
            fear /= divisor;
            grief /= divisor;
            anger /= divisor;
            hunger /= divisor;
            int recent = 0;
            for (const auto& fact : world.facts)
            {
                recent += Location(fact.position) == v && (world.minutes - fact.minute) * 60 <= 120;
            }
            double calm =
                present ? std::max(0.0, 100 - fear * .5 - grief * .25 - anger * .25 - recent * 3)
                        : 0;
            entry.metrics.insert({{"population", count},
                                  {"npc_capacity", 20},
                                  {"npcs_present", present},
                                  {"resident_deaths", dead},
                                  {"food_stock", town.food},
                                  {"treasury", town.treasury},
                                  {"security", town.security},
                                  {"morale", town.morale},
                                  {"food_produced", town.production},
                                  {"food_consumed", town.consumption},
                                  {"mood_calm", calm},
                                  {"mood_anxiety", fear},
                                  {"mood_grief", grief},
                                  {"mood_hostility", anger},
                                  {"mood_hunger", hunger},
                                  {"recent_incidents", recent},
                                  {"known_rumors", static_cast<double>(known.size())},
                                  {"center_x", Villages()[v].center.x},
                                  {"center_y", Villages()[v].center.y}});
            snapshot.settlements.push_back(std::move(entry));
        }
    }
} // namespace Arcana
