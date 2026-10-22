#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "ArcanaWorld.h"
#include "Profiler.h"
#include "WorldGeometry.h"
#include "RenderQueue.h"
#include <sqlite3.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>

namespace
{
    class BlueOrb final : public Actor
    {
      public:
        RenderPoint direction;
        float remaining = 1.2f;

        explicit BlueOrb(RenderPoint direction) : Actor("ArcanaBlueOrb"), direction(direction)
        {
            SetLocalHeight(26);
        }

        void Draw(const SceneView& view) const override
        {
            auto center = view.Screen(*this);
            auto screenDirection = WorldGeometry::Project(direction);
            float length = std::hypot(screenDirection.x, screenDirection.y);
            screenDirection.x /= length;
            screenDirection.y /= length;
            auto disk = [](RenderPoint p, float radius, RenderColor color)
            {
                RenderQueue::EnableTexture(false);
                RenderQueue::Color(color.r, color.g, color.b, color.a);
                RenderQueue::Begin(GL_TRIANGLE_FAN);
                RenderQueue::Vertex(p.x, p.y);
                for (int i = 0; i <= 16; ++i)
                {
                    float angle = i * WorldGeometry::Pi / 8;
                    RenderQueue::Vertex(p.x + std::cos(angle) * radius,
                                        p.y + std::sin(angle) * radius);
                }
                RenderQueue::End();
            };
            for (int i = 3; i >= 1; --i)
            {
                disk({center.x - screenDirection.x * i * 7, center.y - screenDirection.y * i * 7},
                     7.f - i,
                     {.12f, .4f, 1.f, .15f});
            }
            disk(center, 15, {.08f, .3f, 1.f, .18f});
            disk(center, 9, {.12f, .55f, 1.f, .9f});
            disk({center.x - 2, center.y - 2}, 4, {.7f, .92f, 1.f, 1.f});
        }
    };

    std::wstring Wide(const std::string& text)
    {
        int size = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (size == 0 && !text.empty())
        {
            throw std::runtime_error("ARCANA content is not valid UTF-8");
        }
        std::wstring result(size, L'\0');
        MultiByteToWideChar(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
        return result;
    }

    std::string ContentPath()
    {
        wchar_t executable[32768];
        DWORD size = GetModuleFileNameW(nullptr, executable, 32768);
        if (!size || size == 32768)
        {
            throw std::runtime_error("Cannot resolve ARCANA content directory");
        }
        auto path = std::filesystem::path(executable).parent_path() / L"assets" / L"arcana" /
                    L"blood_principle_game.db";
        if (!std::filesystem::is_regular_file(path))
        {
            throw std::runtime_error(
                "ARCANA content missing: assets/arcana/blood_principle_game.db; rebuild the project to copy content");
        }
        return path.u8string();
    }

    class Database
    {
      public:
        sqlite3* handle = nullptr;

        explicit Database(const std::string& path)
        {
            if (sqlite3_open_v2(path.c_str(), &handle, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
            {
                std::string error = handle ? sqlite3_errmsg(handle) : "SQLite open failed";
                sqlite3_close(handle);
                throw std::runtime_error(error);
            }
        }

        ~Database()
        {
            sqlite3_close(handle);
        }

        Database(const Database&) = delete;
        Database& operator=(const Database&) = delete;
    };

    class Query
    {
      public:
        sqlite3_stmt* statement = nullptr;

        explicit Query(Database& database, const char* sql)
        {
            if (sqlite3_prepare_v2(database.handle, sql, -1, &statement, nullptr) != SQLITE_OK)
            {
                sqlite3_finalize(statement);
                throw std::runtime_error(sqlite3_errmsg(database.handle));
            }
        }

        ~Query()
        {
            sqlite3_finalize(statement);
        }

        Query(const Query&) = delete;
        Query& operator=(const Query&) = delete;

        bool Next()
        {
            int result = sqlite3_step(statement);
            if (result != SQLITE_ROW && result != SQLITE_DONE)
            {
                throw std::runtime_error("ARCANA content query failed");
            }
            return result == SQLITE_ROW;
        }

        std::string Text(int column) const
        {
            auto value = sqlite3_column_text(statement, column);
            return value ? reinterpret_cast<const char*>(value) : "";
        }

        double Number(int column) const
        {
            return sqlite3_column_double(statement, column);
        }
    };

    class Host final : public bp::native::Host
    {
      public:
        std::map<int, bp::native::Candidate> candidates;
        std::optional<bp::native::Intent> selected;

        std::optional<bp::native::Candidate> probe(const bp::native::Entity&,
                                                   const bp::native::Node& node) override
        {
            auto it = candidates.find(node.id);
            return it == candidates.end() ? std::nullopt
                                          : std::optional<bp::native::Candidate>(it->second);
        }

        void execute(const bp::native::Intent& intent) override
        {
            selected = intent;
        }
    };
} // namespace

namespace Arcana
{
    Module::Module(Lena::World& value) : world(value)
    {
        Profiler::Scope scope("arcana.content_load");
        const auto path = ContentPath();
        content = bp::native::load(path, "demo");
        scheduler = std::make_unique<bp::native::Scheduler>(content);
        Database database(path);
        Query steps(database, "SELECT id,objective FROM quest_steps ORDER BY quest_id,step_order");
        while (steps.Next())
        {
            objectives[steps.Text(0)] = Wide(steps.Text(1));
        }
        for (const char* id : {"p1", "p2", "s1", "f1", "f2", "f3", "k1"})
        {
            if (!objectives.count(id))
            {
                throw std::runtime_error("ARCANA opening quest contract is incomplete");
            }
        }
        Query fire(
            database,
            "SELECT s.base_mana_cost,k.efficiency_multiplier FROM spells s "
            "JOIN spell_knowledge k ON k.spell_id=s.id WHERE s.id='fire' "
            "AND k.knowledge_id='combustion' AND json_extract(k.conditions_json,'$.requires_fuel')=1 "
            "AND json_extract(k.conditions_json,'$.requires_oxygen')=1");
        if (!fire.Next() || fire.Number(0) <= 0 || fire.Number(1) < 1)
        {
            throw std::runtime_error("Unsupported ARCANA combustion policy");
        }
        fireCost = fire.Number(0);
        fireEfficiency = fire.Number(1);
    }

    void Module::Seed()
    {
        Profiler::Scope scope("arcana.seed_actors");
        using namespace Lena;
        Database database(ContentPath());
        std::map<std::string, float> maximum;
        Query species(database, "SELECT id,base_health FROM species");
        while (species.Next())
        {
            maximum[species.Text(0)] = static_cast<float>(species.Number(1));
        }
        PrepareMonsterSpawns();
        std::map<std::string, RenderPoint> positions = {{"player", world.canal},
                                                        {"serdin_01", {-13, -14}},
                                                        {"bran_01", forge},
                                                        {"leon_01", outpost},
                                                        {"thomas_01", {-4, 3}},
                                                        {"ella_01", {3, 4}},
                                                        {"isela_01", Villages()[1].center},
                                                        {"beast_01", monsterSpawns.front()},
                                                        {"wolf_01", {-25, 10}}};
        auto add = [&](bp::native::Entity entity, RenderPoint position, Role role, int village)
        {
            Species type = entity.kind == "human"          ? Species::Human
                           : entity.profile == "herbivore" ? Species::Deer
                                                           : Species::Wolf;
            auto& p = world.Spawn(entity.id, Wide(entity.name), type, role, position);
            p.profile = entity.profile;
            p.speciesId = entity.species;
            p.region = village >= 0 ? Villages()[village].region : entity.region;
            p.beast = entity.kind == "beast";
            p.maxHealth = maximum.at(entity.species);
            p.state.health = static_cast<float>(entity.health);
            p.state.manaCurrent = static_cast<float>(entity.manaCurrent);
            p.state.manaMax = static_cast<float>(entity.manaMax);
            p.state.hunger = p.beast ? 35.f : static_cast<float>(entity.hunger);
            p.state.thirst = static_cast<float>(entity.thirst);
            p.state.fatigue = static_cast<float>(entity.fatigue);
            p.state.coins = static_cast<float>(entity.wealth);
            p.state.habitat = p.home;
            p.settlement = village;
            entity.region = p.region;
            definitions.emplace(entity.id, std::move(entity));
            actorIds[p.key] = p.GetId();
            return p.GetId();
        };
        for (const auto& entity : content.entities)
        {
            auto position = positions.find(entity.id);
            if (position == positions.end())
            {
                continue;
            }
            Role role = entity.id == "player"              ? Role::Player
                        : entity.occupation == "knight"    ? Role::Guard
                        : entity.occupation == "herbalist" ? Role::Healer
                        : entity.occupation == "mage" || entity.occupation == "oracle_mage"
                            ? Role::Scholar
                            : Role::Resident;
            int village = entity.kind != "human" || role == Role::Player ? -1
                          : entity.id == "isela_01"                      ? 1
                                                                         : 0;
            auto id = add(entity, position->second, role, village);
            if (role == Role::Player)
            {
                world.playerId = id;
            }
            if (entity.id == "leon_01")
            {
                world.darianId = id;
            }
            if (role == Role::Healer)
            {
                world.Find(id)->state.medicine = 8;
            }
        }
        if (!world.playerId || !Named("serdin_01") || !Named("bran_01") || !world.darianId)
        {
            throw std::runtime_error("ARCANA opening characters missing");
        }
        Query towns(
            database,
            "SELECT settlement_id,food,wealth,security FROM settlement_states WHERE save_id='demo'");
        while (towns.Next())
        {
            for (int v = 0; v < 5; ++v)
            {
                if (Villages()[v].id == towns.Text(0))
                {
                    Town(v).food = towns.Number(1);
                    Town(v).treasury = towns.Number(2);
                    Town(v).security = towns.Number(3);
                }
            }
        }
        for (int v = 0; v < 5; ++v)
        {
            Town(v).pollution = 0;
            int named = 0;
            for (auto id : world.people)
            {
                named += world.Find(id)->settlement == v;
            }
            const auto center = Villages()[v].center;
            for (int i = named; i < 20; ++i)
            {
                auto entity = definitions.at("thomas_01");
                entity.id = "arcana:" + Villages()[v].id + ":resident:" + std::to_string(i);
                entity.name = "주민 " + std::to_string(i + 1);
                Role role = i >= 18      ? Role::Merchant
                            : i == 17    ? Role::Bandit
                            : i % 7 == 0 ? Role::Healer
                            : i % 5 == 0 ? Role::Guard
                            : i % 4 == 0 ? Role::Merchant
                                         : Role::Resident;
                auto id = add(entity, {center.x + i % 5 - 2, center.y + i / 5 - 1}, role, v);
                auto* p = world.Find(id);
                p->courier = i >= 18;
                p->work = world.Safe({center.x - 2 + i % 5, center.y - 3});
                p->state.food = 1;
                Town(v).food = std::max(0.0, Town(v).food - 1);
                if (role == Role::Healer)
                {
                    p->state.medicine = 8;
                }
                if (role == Role::Bandit)
                {
                    p->state.hunger = 80;
                    p->traits = {{75, 10, 85, 10, 90, 30, 20}};
                }
                if (p->courier)
                {
                    p->displayName = Villages()[v].name + L" 행상 " + std::to_wstring(i - 17);
                    p->state.travelTown = v == 0 ? (i - 18) + 1 : 0;
                    p->state.travelWait = 5.f + (i - 18) * 4;
                }
            }
        }
        for (int i = 1; i < 100; ++i)
        {
            auto e = definitions.at("beast_01");
            e.id = "arcana:monster:" + std::to_string(i);
            e.name = "흑아 멧돼지 " + std::to_string(i + 1);
            add(e, monsterSpawns[i * monsterSpawns.size() / 100], Role::Resident, -1);
        }
        for (int i = 0; i < 28; ++i)
        {
            bp::native::Entity e;
            e.id = "arcana:deer:" + std::to_string(i);
            e.save = "demo";
            e.species = "deer";
            e.name = "사슴 " + std::to_string(i + 1);
            e.kind = "animal";
            e.profile = "herbivore";
            e.region = e.homeRegion = "north_forest";
            e.health = maximum.at("deer");
            add(e, {-25.f + i % 7, 12.f + i / 7}, Role::Resident, -1);
        }
        Query traits(database,
                     "SELECT entity_id,trait,value FROM entity_traits ORDER BY entity_id,trait");
        const std::map<std::string, int> slots = {
            {"empathy", 1}, {"justice", 3}, {"greed", 4}, {"curiosity", 5}, {"loyalty", 6}};
        while (traits.Next())
        {
            auto* p = Named(traits.Text(0));
            auto slot = slots.find(traits.Text(1));
            if (p && slot != slots.end())
            {
                p->traits[slot->second] = static_cast<int>(traits.Number(2));
            }
        }
        world.minutes = content.worldHours * 60;
        progress.lastEconomy = world.minutes * 60;
        const float relief = static_cast<float>(std::min(2.0, world.town.treasury));
        world.Player().state.coins += relief;
        world.town.treasury -= relief;
        for (auto id : world.people)
        {
            auto* p = world.Find(id);
            if (p->settlement >= 0 && p->role == Role::Merchant)
            {
                auto& town = Town(p->settlement);
                float stock = static_cast<float>(std::min(2.0, town.food));
                p->state.food += stock;
                town.food -= stock;
            }
        }
        world.RebuildIndex();
        world.Log(L"아르카나 : ARCANA — 다섯 마을과 숲길. 소식은 사람의 발로 전해진다.");
        world.Log(L"[E] 불탄 집 조사. 중앙 벨하임에서 네 마을로 이어지는 도로를 이용하세요.");
    }

    Lena::Person* Module::Named(const std::string& id) const
    {
        auto found = actorIds.find(id);
        return found == actorIds.end() ? nullptr : world.Find(found->second);
    }

    bool Module::Near(RenderPoint point, float distance) const
    {
        return Lena::Distance(world.Player().Position(), point) < distance &&
               world.Visible(world.Player().Position(), world.Safe(point));
    }

    void Module::Decide(Lena::Person& p)
    {
        Profiler::Scope scope("arcana.wbg_decide");
        using namespace Lena;
        Host host;
        auto offer = [&](Action action, Person* target, double utility)
        {
            bp::native::Candidate c;
            c.target = target ? target->key : "";
            c.requiresTarget = target != nullptr;
            c.terms[0] = utility / 100;
            int node = static_cast<int>(action);
            auto old = host.candidates.find(node);
            if (old == host.candidates.end() || old->second.terms[0] < c.terms[0])
            {
                host.candidates[node] = c;
            }
        };
        auto& s = p.state;
        offer(Action::Idle, nullptr, 0);
        offer(Action::Wander, nullptr, 5);
        if (p.species == Species::Human && p.role != Role::Bandit)
        {
            offer(Action::Work, nullptr, 25);
        }
        if (s.food >= 1 || p.species == Species::Deer && world.town.plants > 0)
        {
            offer(Action::Eat, nullptr, s.hunger > 40 ? s.hunger + 15 : -100);
        }
        if (s.thirst > 55)
        {
            offer(Action::Drink, nullptr, s.thirst);
        }
        if (s.fatigue > 60)
        {
            offer(Action::Sleep, nullptr, s.fatigue);
        }
        if (s.health < p.maxHealth * .4f)
        {
            offer(Action::Rest, nullptr, 85);
        }
        if (s.report && p.species == Species::Human && p.role != Role::Guard)
        {
            offer(Action::Report, nullptr, 90);
        }
        if (s.grief > 35)
        {
            offer(Action::Mourn, nullptr, s.grief);
        }
        if (p.key == "serdin_01")
        {
            offer(progress.step < 3 ? Action::Idle : Action::Follow,
                  progress.step < 3 ? nullptr : &world.Player(),
                  45);
        }
        if (p.courier && !s.detained && !s.surrendered)
        {
            if (s.travelWait <= 0 && s.travelTown >= 0)
            {
                s.destination = world.Safe(Villages()[s.travelTown].center);
                offer(Action::Travel, nullptr, 65);
            }
            else
            {
                offer(Action::Idle, nullptr, 45);
            }
        }
        bool emergency = false;
        bool leave = false;
        auto entity = definitions.at(p.key);
        entity.alive = !s.dead;
        entity.health = s.health;
        entity.hunger = s.hunger;
        entity.thirst = s.thirst;
        entity.fatigue = s.fatigue;
        entity.fear = s.fear;
        entity.anger = s.anger;
        entity.manaCurrent = s.manaCurrent;
        entity.manaMax = s.manaMax;
        entity.wealth = s.coins;
        entity.region = p.region;
        if (p.beast)
        {
            bp::native::Territory area;
            area.owner = p.key;
            area.region = p.region;
            area.disturbance = s.anger;
            leave = bp::native::shouldLeaveTerritory(entity, area, content.events);
            if (leave)
            {
                p.state.destination = Villages()[Location(p.Position(), 1e9f)].center;
                offer(Action::Travel, nullptr, 60);
            }
        }
        for (auto id : world.Nearby(p.Position(), 9))
        {
            auto* other = world.Find(id);
            if (!other || other == &p || !other->IsActive() ||
                !world.Visible(p.Position(), other->Position()))
            {
                continue;
            }
            float distance = Distance(p.Position(), other->Position());
            if (other->state.dead)
            {
                if (p.species == Species::Wolf && other->state.corpseFood > 0 && s.hunger > 40)
                {
                    offer(Action::Eat, other, s.hunger + 30);
                }
                continue;
            }
            bool threatened = s.aggressor == id && s.threatSeconds > 0;
            bool attacking =
                other->state.target == p.GetId() &&
                (other->state.action == Action::Attack || other->state.action == Action::Hunt);
            if (threatened || attacking)
            {
                emergency = true;
                if (p.role == Role::Bandit && s.health < 25)
                {
                    offer(Action::Surrender, nullptr, 250);
                }
                else
                {
                    bool fight = p.role == Role::Guard || p.role == Role::Bandit ||
                                 p.species == Species::Wolf && s.health > 20;
                    offer(fight ? Action::Attack : Action::Flee, other, 200);
                }
            }
            if (p.role == Role::Guard && !other->state.surrendered)
            {
                auto* victim = world.Find(other->state.target);
                if (victim && victim->species == Species::Human && victim->role != Role::Bandit &&
                    (other->state.action == Action::Attack || other->state.action == Action::Hunt))
                {
                    offer(Action::Attack, other, 180);
                    emergency = true;
                }
            }
            if (p.species == Species::Wolf && s.hunger > 65 &&
                (other->species == Species::Deer ||
                 p.beast && leave && other->species == Species::Human))
            {
                offer(Action::Hunt, other, s.hunger + 10 - distance);
            }
            if (p.beast && other->role == Role::Player && distance < 4)
            {
                offer(Action::Attack, other, 110);
            }
            if (p.species == Species::Deer && other->species == Species::Wolf && distance < 5)
            {
                offer(Action::Flee, other, 150);
            }
            if (p.role == Role::Healer && s.medicine >= 1 && other->species == Species::Human &&
                other->state.health < 60 && other->role != Role::Bandit)
            {
                offer(Action::Help, other, 90);
            }
            if (p.species == Species::Human && p.role != Role::Bandit && s.food < 1 &&
                s.coins >= 2 && other->role == Role::Merchant && other->state.food >= 1 &&
                s.hunger > 45)
            {
                offer(Action::Trade, other, s.hunger + 12);
            }
            if (p.role == Role::Bandit && other->species == Species::Human &&
                other->role != Role::Guard && other->role != Role::Bandit && s.hunger > 65)
            {
                offer(Action::Threaten, other, 85 - distance);
                if (s.action == Action::Threaten && s.target == id && s.held >= 2)
                {
                    offer(other->state.food >= 1 ? Action::Steal : Action::Attack, other, 125);
                }
            }
            bool differentKnowledge = s.known != other->state.known;
            if (p.species == Species::Human && other->species == Species::Human &&
                other->role != Role::Player && distance < (p.courier ? 8.f : 2.f) &&
                differentKnowledge && s.cooldown <= 0 && p.role != Role::Bandit)
            {
                offer(Action::Talk, other, (p.courier ? 85 : 35) - distance);
            }
        }
        if (s.detained || s.surrendered)
        {
            host.candidates.clear();
            offer(Action::Surrender, nullptr, 0);
        }
        auto intent = scheduler->decide(entity,
                                        world.minutes * 60,
                                        host,
                                        emergency ? bp::native::Interrupt::LethalThreat
                                                  : bp::native::Interrupt::None);
        if (intent)
        {
            auto* target = Named(intent->target);
            auto action = static_cast<Action>(intent->node);
            s.surrendered = s.surrendered || action == Action::Surrender;
            world.SetAction(p, action, target ? target->GetId() : 0);
        }
        else
        {
            world.SetAction(p, Action::Idle);
        }
        Profiler::Count("arcana.wbg_decisions");
        Profiler::Count("arcana.wbg_valid_candidates",
                        static_cast<std::uint64_t>(host.candidates.size()));
    }

    void Module::Update(float dt)
    {
        Profiler::Scope scope("arcana.story_update");
        attackSeconds = std::max(0.f, attackSeconds - dt);
        UpdateProjectiles(dt);
        UpdateExpansion(dt);
        fireSeconds = std::max(0.f, fireSeconds - dt);
        pulse -= dt;
        if (pulse > 0)
        {
            return;
        }
        pulse = 1;
        int reports = 0;
        for (const auto& fact : world.facts)
        {
            auto* suspect = world.Find(fact.actor);
            reports += fact.crime && fact.reported && suspect && !suspect->state.dead &&
                       !suspect->state.detained;
        }
        auto* leon = Named("leon_01");
        if (!progress.dispatched && leon && !leon->state.dead &&
            bp::native::shouldDispatch(world.town.security, reports, 1, content.events))
        {
            for (const auto& fact : world.facts)
            {
                auto* suspect = world.Find(fact.actor);
                if (fact.reported && fact.crime && suspect && !suspect->state.dead &&
                    !suspect->state.detained)
                {
                    // A reported position is an investigation destination, not permission to kill.
                    progress.dispatchPosition = world.Safe(fact.position);
                    leon->work = progress.dispatchPosition;
                    progress.dispatched = true;
                    world.Log(L"레온이 접수된 신고의 현장으로 출동했다. 목격 전투에만 개입한다.");
                    Profiler::Count("arcana.knight_dispatches");
                    break;
                }
            }
        }
        Profiler::Gauge("arcana.quest_step", progress.step);
        Profiler::Gauge("arcana.player_mana_current", world.Player().state.manaCurrent);
        Profiler::Gauge("arcana.player_mana_max", world.Player().state.manaMax);
        Profiler::Gauge("arcana.territory_disturbance", progress.disturbance);
    }

    std::wstring Module::Objective() const
    {
        static const char* steps[] = {"p1", "p2", "s1", "f1", "f2", "f3", "k1"};
        if (progress.step >= 7)
        {
            return L"첫 장 완료 · 숲과 벨하임 탐험 / 다음 장: 사라진 가문의 기록 (미구현)";
        }
        return objectives.at(steps[progress.step]) +
               (progress.step == 5 ? L" — 대장간에서 [F]" : L" — 현장에서 [E]");
    }

    void Module::Interact()
    {
        Profiler::Scope scope("arcana.interaction");
        if (AbsorbRemnant())
        {
            return;
        }
        if (Near(world.canal, 3))
        {
            if (progress.step == 0)
            {
                ++progress.step;
                world.Log(
                    L"재가 된 집 — 부모의 죽음을 확인했다. 재 속에 수첩과 은빛 문장이 보인다.");
            }
            else if (progress.step == 1)
            {
                ++progress.step;
                world.Log(L"부모의 수첩 · 은빛 문장을 획득했다. 숲길의 세르딘을 찾아가자.");
            }
            else
            {
                world.Log(L"유품은 이미 챙겼다. 남은 재는 부모를 되돌려주지 않는다.");
            }
            return;
        }
        auto* serdin = Named("serdin_01");
        if (progress.step == 2 && serdin && !serdin->state.dead && Near(serdin->Position()))
        {
            ++progress.step;
            world.Log(
                L"세르딘: 그 수첩은 함부로 펼치지 마라. 벨하임의 브란이라면 풀무 그림을 읽겠지.");
            return;
        }
        auto* bran = Named("bran_01");
        if (Near(forge) && progress.step >= 3 && progress.step <= 5)
        {
            if (progress.step == 3)
            {
                ++progress.step;
                world.Log(bran && !bran->state.dead
                              ? L"브란: 공기를 넣으면 불이 세진다. 신탁보다 풀무 수리비가 확실하지."
                              : L"브란은 죽었다. 남겨진 풀무 장치의 공기 통로를 관찰했다.");
            }
            else if (progress.step == 4)
            {
                ++progress.step;
                world.Log(L"수첩의 도식: 연료와 공기. [F]로 풀무 앞의 화염을 시험하자.");
            }
            else
            {
                world.Log(L"[F] 화염 실험. 지식은 소모 마력을 줄일 뿐 최대 마력을 늘리지 않는다.");
            }
            return;
        }
        if (progress.step == 6 && Near(outpost))
        {
            ++progress.step;
            world.Log(
                L"기사단 기록과 은빛 문장이 일치한다. 누군가 이 가문의 흔적을 추적하고 있다.");
            return;
        }
        if (progress.step == 2 && serdin && serdin->state.dead && Near(serdin->Position()))
        {
            ++progress.step;
            world.Log(
                L"세르딘의 유품에서 브란에게 가는 편지를 찾았다. 죽은 동행자는 돌아오지 않는다.");
            return;
        }
        if (Near({world.lakeCenter.x - 6, world.lakeCenter.y}, 2) ||
            (Location(world.Player().Position()) >= 0 && Near(DrinkingPoint(world.Player()), 2)))
        {
            world.Player().state.thirst = 20;
            world.Log(L"호숫가의 물을 마셨다.");
            return;
        }
        for (auto id : world.Nearby(world.Player().Position(), 2))
        {
            auto* p = world.Find(id);
            if (!p || p->GetId() == world.playerId || !Near(p->Position(), 2))
            {
                continue;
            }
            if (p->beast && p->state.dead && !p->state.absorbed)
            {
                // Only the protagonist's explicitly authored mage lineage is eligible here.
                bool withinWindow =
                    (world.minutes - p->state.deathMinute) * 60 <= content.mana.windowSeconds;
                if (p->state.playerCredit && withinWindow)
                {
                    auto& player = world.Player().state;
                    double gain = bp::native::manaGain(
                        p->state.manaMax, player.manaMax, progress.boarKills, content.mana);
                    player.manaMax += static_cast<float>(gain);
                    p->state.absorbed = true;
                    ++progress.boarKills;
                    world.Log(L"잔류 마력 흡수: 최대 마력 +" + std::to_wstring(gain) +
                              L". 현재 마력은 그대로다.");
                    Profiler::Count("arcana.mana_absorptions");
                }
                else
                {
                    world.Log(
                        L"흡수 불가: 직접 전투 기여와 사망 후 세계 시간 60초 이내 접근이 필요하다.");
                }
                return;
            }
            if (world.Service(*p))
            {
                return;
            }
            world.Log(p->displayName + L": " + Lena::Label(p->state.action) + L" 중입니다.");
            return;
        }
        world.Log(Objective());
    }

    void Module::Cast()
    {
        Profiler::Scope scope("arcana.spell_cast");
        auto& player = world.Player();
        if (player.state.dead || player.state.cooldown > 0)
        {
            return;
        }
        const bool experiment = progress.step == 5 && Near(forge);
        const bool fuelAndOxygen = Near(forge);
        const bool efficient = (progress.combustion || experiment) && fuelAndOxygen;
        double cost = fireCost / (efficient ? fireEfficiency : 1);
        if (player.state.manaCurrent < cost)
        {
            world.Log(L"현재 마력이 부족하다. 대장간의 풀무 옆에서 [Q]로 휴식할 수 있다.");
            return;
        }
        Lena::Person* target = nullptr;
        float distance = 6;
        if (!experiment)
        {
            for (auto id : world.Nearby(player.Position(), 6))
            {
                auto* p = world.Find(id);
                if (p && p != &player && !p->state.dead && p->IsActive() &&
                    world.Visible(player.Position(), p->Position()) &&
                    Lena::Distance(player.Position(), p->Position()) < distance)
                {
                    target = p;
                    distance = Lena::Distance(player.Position(), p->Position());
                }
            }
        }
        if (!experiment && !target)
        {
            world.Log(L"화염 사거리 안에 보이는 대상이 없다.");
            return;
        }
        player.state.manaCurrent -= static_cast<float>(cost);
        player.state.cooldown = .8f;
        fireSeconds = .6f;
        firePosition = experiment ? forge : target->Position();
        if (experiment)
        {
            progress.combustion = true;
            ++progress.step;
            world.Log(L"연소와 공기를 발견했다. 연료·산소 조건에서 화염 소모 마력이 줄었다.");
        }
        else
        {
            world.Damage(player, *target, 40);
        }
        Profiler::Count("arcana.spells_cast");
    }

    bool Module::Enemy(const Lena::Person& p) const
    {
        return p.GetId() != world.playerId && p.IsActive() && !p.state.dead &&
               !p.state.surrendered && !p.state.detained &&
               (p.beast || p.role == Lena::Role::Bandit ||
                (p.state.target == world.playerId &&
                 (p.state.action == Lena::Action::Attack || p.state.action == Lena::Action::Hunt)));
    }

    void Module::Attack()
    {
        Profiler::Scope scope("arcana.orb_launch");
        auto& player = world.Player();
        if (player.state.dead || player.state.cooldown > 0)
        {
            return;
        }
        RenderPoint direction = moveDirection;
        float nearest = 8;
        for (auto id : world.people)
        {
            auto* p = world.Find(id);
            if (!p || !Enemy(*p))
            {
                continue;
            }
            float distance = Lena::Distance(player.Position(), p->Position());
            if (distance < nearest && distance > .000001f &&
                world.Visible(player.Position(), p->Position()))
            {
                nearest = distance;
                direction = {(p->Position().x - player.Position().x) / distance,
                             (p->Position().y - player.Position().y) / distance};
            }
        }
        auto& orb = world.scene.Spawn<BlueOrb>(world.scene.Root(), direction);
        orb.SetPosition(player.Position());
        projectiles.push_back(orb.GetId());
        player.state.cooldown = .55f;
        player.state.facing = WorldGeometry::Project(direction).x >= 0 ? 1.f : -1.f;
        player.visualDirection = direction;
        attackSeconds = .22f;
        Profiler::Count("arcana.orbs_launched");
    }

    void Module::UpdateProjectiles(float dt)
    {
        Profiler::Scope scope("arcana.orb_update");
        for (auto id : projectiles)
        {
            auto* orb = dynamic_cast<BlueOrb*>(world.scene.Find(id));
            if (!orb)
            {
                continue;
            }
            const float travel = std::min(dt, orb->remaining) * 10.f;
            const int steps = std::max(1, static_cast<int>(std::ceil(travel / .1f)));
            bool remove = false;
            // Small swept steps test both blockers and contacts, avoiding frame-rate tunnelling.
            for (int step = 0; step < steps && !remove; ++step)
            {
                const auto from = orb->Position();
                RenderPoint to{from.x + orb->direction.x * travel / steps,
                               from.y + orb->direction.y * travel / steps};
                if (!world.Visible(from, to))
                {
                    remove = true;
                    break;
                }
                orb->SetPosition(to);
                Lena::Person* hit = nullptr;
                float nearest = .55f;
                for (auto personId : world.people)
                {
                    auto* p = world.Find(personId);
                    if (!p || !Enemy(*p))
                    {
                        continue;
                    }
                    float distance = Lena::Distance(to, p->Position());
                    if (distance < nearest && world.Visible(to, p->Position()))
                    {
                        hit = p;
                        nearest = distance;
                    }
                }
                if (hit)
                {
                    world.Damage(world.Player(), *hit, BasicAttackDamage);
                    Profiler::Count("arcana.orb_hits");
                    remove = true;
                }
            }
            orb->remaining -= dt;
            if (remove || orb->remaining <= 0)
            {
                world.scene.Destroy(*orb);
            }
        }
        projectiles.erase(std::remove_if(projectiles.begin(),
                                         projectiles.end(),
                                         [&](Actor::Id id)
                                         {
                                             return world.scene.Find(id) == nullptr;
                                         }),
                          projectiles.end());
        Profiler::Gauge("arcana.orbs_active", static_cast<double>(projectiles.size()));
    }

    void Module::ClearProjectiles()
    {
        for (auto id : projectiles)
        {
            if (auto* actor = world.scene.Find(id))
            {
                world.scene.Destroy(*actor);
            }
        }
        projectiles.clear();
        attackSeconds = 0;
        moveDirection = {.7071068f, .7071068f};
    }

    void Module::Rest(float dt)
    {
        if (!Near(forge) || world.Player().state.threatSeconds > 0)
        {
            return;
        }
        for (auto id : world.Nearby(forge, 5))
        {
            const auto* p = world.Find(id);
            if (p && !p->state.dead &&
                (p->state.action == Lena::Action::Attack || p->state.action == Lena::Action::Hunt))
            {
                return;
            }
        }
        auto& state = world.Player().state;
        state.manaCurrent = std::min(state.manaMax, state.manaCurrent + dt * 2);
    }

    void Module::Hit(Lena::Person& attacker, Lena::Person& victim)
    {
        if (attacker.GetId() == world.playerId && victim.beast)
        {
            victim.state.playerCredit = true;
            progress.disturbance = std::min(100.0, progress.disturbance + 15);
        }
        if (victim.state.health <= 0)
        {
            victim.state.deathMinute = world.minutes;
            scheduler->clear(victim.key);
            int local = Location(victim.Position());
            if (local >= 0)
            {
                auto& town = Town(local);
                town.morale = std::max(0.0, town.morale - 2);
            }
        }
    }

    const Progress& Module::State() const
    {
        return progress;
    }

    void Module::Enrich(WorldMonitor::Snapshot& snapshot, bool statistics) const
    {
        if (statistics)
        {
            snapshot.metrics["arcana.quest_step"] = progress.step;
            snapshot.metrics["arcana.combustion_known"] = progress.combustion ? 1 : 0;
            snapshot.metrics["arcana.territory_disturbance"] = progress.disturbance;
            snapshot.metrics["arcana.knight_dispatched"] = progress.dispatched ? 1 : 0;
            snapshot.metrics["arcana.loaded_behavior_nodes"] =
                static_cast<double>(content.nodes.size());
            FillVillages(snapshot);
            snapshot.metrics["arcana.village_count"] = 5;
            snapshot.metrics["arcana.monster_capacity"] = 100;
        }
        for (auto& entry : snapshot.entities)
        {
            auto* p = Named(entry.id);
            if (p)
            {
                entry.maxHealth = p->maxHealth;
                if (p->species != Lena::Species::Human)
                {
                    entry.role = p->speciesId;
                    entry.kind = p->beast ? "monster" : "wildlife";
                }
                entry.settlementId = p->settlement >= 0 ? Villages()[p->settlement].id : "";
                entry.metrics["physical_village"] = Location(p->Position());
                entry.metrics["courier"] = p->courier ? 1 : 0;
                entry.metrics["travel_destination"] = p->state.travelTown;
                entry.metrics["known_rumors"] = static_cast<double>(p->state.known.size());
                entry.metrics["generation"] = p->state.generation;
                entry.metrics["respawn_remaining_seconds"] =
                    p->beast && p->state.dead
                        ? std::max(0.0, 5 - (world.minutes - p->state.deathMinute) * 60)
                        : 0;
                entry.metrics["mana_current"] = p->state.manaCurrent;
                entry.metrics["mana_max"] = p->state.manaMax;
                entry.metrics["is_beast"] = p->beast ? 1 : 0;
                entry.metrics["mana_absorbed"] = p->state.absorbed ? 1 : 0;
            }
        }
    }

    void Module::Write(std::ostream& out) const
    {
        out << "ARCANA_PROGRESS_2 " << progress.step << ' ' << progress.combustion << ' '
            << progress.dispatched << ' ' << progress.boarKills << ' ' << progress.disturbance
            << ' ' << progress.dispatchPosition.x << ' ' << progress.dispatchPosition.y << ' '
            << progress.respawnSequence << ' ' << progress.lastEconomy << '\n';
        for (int v = 0; v < 5; ++v)
        {
            const auto& town = Town(v);
            out << town.food << ' ' << town.water << ' ' << town.treasury << ' ' << town.morale
                << ' ' << town.security << ' ' << town.pollution << ' ' << town.plants << ' '
                << town.production << ' ' << town.consumption << '\n';
        }
        out << progress.remnants.size() << '\n';
        for (const auto& remnant : progress.remnants)
        {
            out << remnant.slot << ' ' << remnant.generation << ' ' << remnant.position.x << ' '
                << remnant.position.y << ' ' << remnant.minute << ' ' << remnant.mana << '\n';
        }
        const auto& actions = scheduler->actions();
        out << actions.size() << '\n';
        for (const auto& pair : actions)
        {
            const auto& a = pair.second;
            out << std::quoted(pair.first) << ' ' << a.node << ' ' << std::quoted(a.target) << ' '
                << a.startedSeconds << ' ' << a.minimumEndSeconds << ' ' << a.cooldownUntilSeconds
                << '\n';
        }
    }

    bool Module::Read(std::istream& in, Progress& result, double minutes) const
    {
        std::string tag;
        size_t count = 0;
        in >> tag >> result.step >> result.combustion >> result.dispatched >> result.boarKills >>
            result.disturbance >> result.dispatchPosition.x >> result.dispatchPosition.y >>
            result.respawnSequence >> result.lastEconomy;
        if (!in || tag != "ARCANA_PROGRESS_2" || result.step < 0 || result.step > 7 ||
            result.combustion != (result.step >= 6) || result.boarKills > 1000000 ||
            !std::isfinite(result.disturbance) || result.disturbance < 0 ||
            result.disturbance > 100 || !world.Walkable(result.dispatchPosition) ||
            !std::isfinite(result.lastEconomy) || result.lastEconomy < 0 ||
            result.lastEconomy > minutes * 60)
        {
            return false;
        }
        for (auto& town : result.towns)
        {
            in >> town.food >> town.water >> town.treasury >> town.morale >> town.security >>
                town.pollution >> town.plants >> town.production >> town.consumption;
            for (double value : {town.food,
                                 town.water,
                                 town.treasury,
                                 town.morale,
                                 town.security,
                                 town.pollution,
                                 town.plants,
                                 town.production,
                                 town.consumption})
            {
                if (!in || !std::isfinite(value) || value < 0 || value >= 1e9)
                {
                    return false;
                }
            }
            if (town.morale > 100 || town.security > 100 || town.pollution > 100)
            {
                return false;
            }
        }
        in >> count;
        if (!in || count > 1000)
        {
            return false;
        }
        std::set<std::pair<Actor::Id, unsigned>> residueIds;
        for (size_t i = 0; i < count; ++i)
        {
            Remnant remnant;
            in >> remnant.slot >> remnant.generation >> remnant.position.x >> remnant.position.y >>
                remnant.minute >> remnant.mana;
            auto* p = world.Find(remnant.slot);
            if (!in || !p || !p->beast || !world.Walkable(remnant.position) ||
                !std::isfinite(remnant.minute) || remnant.minute < 0 || remnant.minute > minutes ||
                !std::isfinite(remnant.mana) || remnant.mana < 0 || remnant.mana > 1e9 ||
                remnant.generation > 1000000 ||
                !residueIds.insert({remnant.slot, remnant.generation}).second)
            {
                return false;
            }
            result.remnants.push_back(remnant);
        }
        in >> count;
        if (!in || count > world.people.size())
        {
            return false;
        }
        for (size_t i = 0; i < count; ++i)
        {
            std::string id;
            bp::native::ActiveAction a;
            in >> std::quoted(id) >> a.node >> std::quoted(a.target) >> a.startedSeconds >>
                a.minimumEndSeconds >> a.cooldownUntilSeconds;
            auto* p = Named(id);
            if (!in || !p || a.node < 1 || a.node > 39 || (!a.target.empty() && !Named(a.target)) ||
                !std::isfinite(a.startedSeconds) || !std::isfinite(a.minimumEndSeconds) ||
                !std::isfinite(a.cooldownUntilSeconds) || a.startedSeconds < 0 ||
                a.startedSeconds > minutes * 60 || a.minimumEndSeconds < a.startedSeconds ||
                a.cooldownUntilSeconds < 0 || !content.profiles.at(p->profile).at(a.node).enabled ||
                !result.actions.emplace(id, a).second)
            {
                return false;
            }
        }
        return true;
    }

    void Module::Restore(Progress value)
    {
        ClearProjectiles();
        for (const auto& remnant : progress.remnants)
        {
            if (auto* actor = world.scene.Find(remnant.visual))
            {
                world.scene.Destroy(*actor);
            }
        }
        progress = std::move(value);
        world.town = progress.towns[0];
        for (auto& remnant : progress.remnants)
        {
            ShowRemnant(remnant);
        }
        auto restored = content;
        restored.activeActions = progress.actions;
        scheduler = std::make_unique<bp::native::Scheduler>(restored);
        auto* leon = Named("leon_01");
        if (leon)
        {
            leon->work = progress.dispatched ? progress.dispatchPosition : world.Safe(outpost);
        }
        fireSeconds = 0;
        pulse = 0;
    }
} // namespace Arcana
