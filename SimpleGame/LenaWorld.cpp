#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "LenaWorld.h"
#include "ArcanaWorld.h"
#include "Profiler.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <queue>
#include <Windows.h>

namespace Lena
{
    namespace
    {
        float Clamp(float value)
        {
            return std::max(0.f, std::min(100.f, value));
        }

        bool Human(const Person& p)
        {
            return p.species == Species::Human;
        }

        bool Alive(const Person* p)
        {
            return p && p->IsActive() && !p->state.dead;
        }

        bool Guard(const Person& p)
        {
            return p.role == Role::Guard;
        }

        bool Civilian(const Person& p)
        {
            return Human(p) && p.role != Role::Bandit;
        }
    } // namespace

    float Distance(RenderPoint a, RenderPoint b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    const wchar_t* Label(Action action)
    {
        switch (action)
        {
        case Action::Investigate:
            return L"조사";
        case Action::Wander:
            return L"배회";
        case Action::Travel:
            return L"이동";
        case Action::Sleep:
            return L"수면";
        case Action::Eat:
            return L"섭식";
        case Action::Drink:
            return L"음수";
        case Action::Work:
            return L"노동";
        case Action::Rest:
            return L"휴식";
        case Action::Talk:
            return L"소식 전달";
        case Action::Trade:
            return L"거래";
        case Action::Help:
            return L"치료";
        case Action::Follow:
            return L"동행";
        case Action::Warn:
            return L"경고";
        case Action::Threaten:
            return L"위협";
        case Action::Steal:
            return L"절도";
        case Action::Chase:
            return L"추격";
        case Action::Surrender:
            return L"항복";
        case Action::Attack:
            return L"공격";
        case Action::Defend:
            return L"방어";
        case Action::Flee:
            return L"도주";
        case Action::Protect:
            return L"보호";
        case Action::Hunt:
            return L"사냥";
        case Action::Kill:
            return L"의도적 살해";
        case Action::InspectBody:
            return L"사체 조사";
        case Action::Loot:
            return L"유품 수거";
        case Action::Mourn:
            return L"애도";
        case Action::Report:
            return L"신고 전달";
        default:
            return L"대기";
        }
    }

    Person::Person(const std::string& key, const std::wstring& name, Species species, Role role)
        : Actor(key), key(key), displayName(name), species(species), role(role)
    {
    }

    Person* World::Find(Actor::Id id) const
    {
        return dynamic_cast<Person*>(scene.Find(id));
    }

    Person& World::Player() const
    {
        return *Find(playerId);
    }

    Person& World::Spawn(
        const std::string& key, const std::wstring& name, Species species, Role role, RenderPoint p)
    {
        auto& person = scene.Spawn<Person>(scene.Root(), key, name, species, role);
        person.SetPosition(Safe(p));
        person.home = person.work = person.Position();
        person.state.destination = person.Position();
        person.state.decision = static_cast<float>(people.size() % 8) * .125f;
        people.push_back(person.GetId());
        return person;
    }

    World::~World() = default;

    World::World(bool arcanaMode)
    {
        Profiler::Scope scope("lena.initialize");
        epoch = WorldMonitor::NextEpoch();
        houses = {{-6, 0}, {5, 1}, {-5, 8}, {5, 9}, {-9, 15}, {0, 16}, {9, 17}, {-13, 5}};
        if (arcanaMode)
        {
            houses = {{-7, 0}, {5, 1}, {-5, 8}, {5, 9}, {-9, 15}, {0, 16}, {9, 17}, {12, 3}};
        }
        for (int i = 0; i < 130; ++i)
        {
            RenderPoint p{static_cast<float>((i * 37) % 49 - 24),
                          static_cast<float>((i * 19) % 49 - 24)};
            if (arcanaMode
                    ? (p.x < -12 && std::abs(p.x - p.y) > 3 || p.y > 23)
                    : (p.x > 10 && p.y > 6 && std::abs(p.x - p.y) > 2 || p.x < -20 || p.y > 23))
            {
                trees.push_back(p);
            }
        }
        BuildNavigation();
        if (arcanaMode)
        {
            arcana = std::make_unique<Arcana::Module>(*this);
            arcana->Seed();
            return;
        }
        playerId =
            Spawn("NPC_PLAYER", L"기억을 잃은 주인공", Species::Human, Role::Player, canal).GetId();
        Player().settlement = -1;
        auto& lia = Spawn("NPC_LIA", L"리아", Species::Human, Role::Scholar, {-15, -16});
        liaId = lia.GetId();
        lia.traits = {{62, 78, 15, 48, 20, 95, 72}};
        lia.home = {-4, 5};
        lia.work = canal;
        auto& edwin = Spawn("NPC_EDWIN", L"에드윈", Species::Human, Role::Resident, {-4, 1});
        edwinId = edwin.GetId();
        edwin.traits = {{55, 72, 20, 70, 25, 40, 90}};
        edwin.state.coins = 45;
        auto& mara = Spawn("NPC_MARA", L"마라", Species::Human, Role::Merchant, {3, 4});
        mara.traits = {{48, 45, 18, 60, 78, 50, 55}};
        mara.state.coins = 120;
        auto& darian = Spawn("NPC_DARIAN", L"다리안", Species::Human, Role::Guard, {7, 5});
        darianId = darian.GetId();
        darian.traits = {{82, 40, 45, 88, 15, 35, 80}};
        auto& noah = Spawn("NPC_NOAH", L"노아", Species::Human, Role::Scholar, {-8, 12});
        noah.traits = {{70, 64, 35, 25, 30, 72, 85}};
        auto& selin = Spawn("NPC_SELIN", L"셀린", Species::Human, Role::Healer, {-4, 5});
        selinId = selin.GetId();
        selin.traits = {{58, 95, 5, 68, 10, 65, 75}};
        selin.state.medicine = 12;
        for (int i = 0; i < 78; ++i)
        {
            Role role = i < 5    ? Role::Guard
                        : i < 12 ? Role::Merchant
                        : i < 21 ? Role::Healer
                                 : Role::Resident;
            auto& p = Spawn("NPC_GEN_" + std::to_string(i + 1),
                            L"레나 주민 " + std::to_wstring(i + 1),
                            Species::Human,
                            role,
                            {static_cast<float>(i % 10 - 5), static_cast<float>(i / 10 + 3)});
            p.traits = {{35 + i % 55, 40 + i % 50, 10 + i % 25, 50 + i % 40, 20 + i % 60, 50, 65}};
            p.work = Safe({static_cast<float>(i % 8 - 10), static_cast<float>(i % 7 + 10)});
            p.state.coins = 5;
            if (role == Role::Healer)
            {
                p.state.medicine = 2;
            }
            if (role == Role::Guard)
            {
                p.work = Safe({8.f + i, 8.f});
            }
        }
        for (int i = 0; i < 4; ++i)
        {
            auto& p = Spawn("BANDIT_" + std::to_string(i),
                            L"길목 약탈자 " + std::to_wstring(i + 1),
                            Species::Human,
                            Role::Bandit,
                            {11.f + i, 9.f});
            p.settlement = -1;
            p.traits = {{i == 0 ? 95 : 35, 10, i == 0 ? 95 : 65, 8, 90, 30, 55}};
            p.state.hunger = 80;
        }
        for (int i = 0; i < 32; ++i)
        {
            auto& p = Spawn("DEER_" + std::to_string(i),
                            L"사슴 " + std::to_wstring(i + 1),
                            Species::Deer,
                            Role::Resident,
                            {13.f + i % 7, 11.f + i / 7});
            p.settlement = -1;
            p.state.health = 45;
        }
        for (int i = 0; i < 7; ++i)
        {
            auto& p = Spawn("WOLF_" + std::to_string(i),
                            L"늑대 " + std::to_wstring(i + 1),
                            Species::Wolf,
                            Role::Resident,
                            {20.f + i % 3, 18.f + i / 3});
            p.settlement = -1;
            p.state.health = 60;
            p.state.hunger = 72.f + i * 2;
        }
        RebuildIndex();
        Log(L"물 아래에서 들린 이름 — 수로 벽의 인장이 손목과 함께 빛난다.");
        Log(L"리아: 살아 계시군요. 세금 명부보다 먼저 발견해서 다행이에요.");
    }

    int World::Cell(RenderPoint p) const
    {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || std::abs(p.x) > Extent ||
            std::abs(p.y) > Extent)
        {
            return -1;
        }
        int x = static_cast<int>(std::floor((p.x + Extent) * 2));
        int y = static_cast<int>(std::floor((p.y + Extent) * 2));
        return x >= 0 && y >= 0 && x < Side && y < Side ? y * Side + x : -1;
    }

    RenderPoint World::Position(int cell) const
    {
        return {(cell % Side + .5f) * .5f - Extent, (cell / Side + .5f) * .5f - Extent};
    }

    bool World::Water(RenderPoint p) const
    {
        float x = (p.x - 14) / 5, y = (p.y + 8) / 4;
        return x * x + y * y < 1 + .12f * std::sin(std::atan2(y, x) * 3);
    }

    void World::BuildNavigation()
    {
        Profiler::Scope scope("lena.navigation_build");
        for (int i = 0; i < Side * Side; ++i)
        {
            auto p = Position(i);
            bool open = std::abs(p.x) < 25 && std::abs(p.y) < 25 && !Water(p);
            for (auto h : houses)
            {
                if (std::abs(p.x - h.x) < 1.5f && std::abs(p.y - h.y) < 1.2f)
                {
                    open = false;
                }
            }
            for (auto t : trees)
            {
                if (Distance(p, t) < .45f)
                {
                    open = false;
                }
            }
            walk[i] = open;
        }
        std::array<unsigned char, Side * Side> connected{};
        std::queue<int> queue;
        int start = Cell(canal);
        connected[start] = 1;
        queue.push(start);
        while (!queue.empty())
        {
            int at = queue.front();
            queue.pop();
            for (int offset : {-1, 1, -Side, Side})
            {
                int next = at + offset;
                if (next < 0 || next >= Side * Side ||
                    (std::abs(offset) == 1 && at / Side != next / Side) || !walk[next] ||
                    connected[next])
                {
                    continue;
                }
                connected[next] = 1;
                queue.push(next);
            }
        }
        walk = connected;
    }

    bool World::Walkable(RenderPoint p) const
    {
        int cell = Cell(p);
        return cell >= 0 && walk[cell];
    }

    RenderPoint World::Safe(RenderPoint p) const
    {
        if (Walkable(p))
        {
            return p;
        }
        float best = 1e9f;
        RenderPoint result = canal;
        for (int i = 0; i < Side * Side; ++i)
        {
            if (walk[i] && Distance(p, Position(i)) < best)
            {
                best = Distance(p, Position(i));
                result = Position(i);
            }
        }
        return result;
    }

    bool World::Visible(RenderPoint a, RenderPoint b) const
    {
        int steps = std::max(1, static_cast<int>(Distance(a, b) * 5));
        for (int i = 1; i <= steps; ++i)
        {
            float t = static_cast<float>(i) / steps;
            if (!Walkable({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}))
            {
                return false;
            }
        }
        return true;
    }

    bool World::Route(Person& p, RenderPoint destination)
    {
        if (routeBudget <= 0)
        {
            Profiler::Count("lena.path_deferred");
            return false;
        }
        --routeBudget;
        Profiler::Scope scope("lena.pathfind");
        std::array<int, Side * Side> previous;
        previous.fill(-1);
        std::queue<int> queue;
        int start = Cell(p.Position()), goal = Cell(destination);
        if (start < 0 || goal < 0 || !walk[goal])
        {
            return false;
        }
        previous[start] = start;
        queue.push(start);
        while (!queue.empty() && previous[goal] == -1)
        {
            int at = queue.front();
            queue.pop();
            for (int offset : {-1, 1, -Side, Side})
            {
                int next = at + offset;
                if (next < 0 || next >= Side * Side ||
                    (std::abs(offset) == 1 && at / Side != next / Side) || !walk[next] ||
                    previous[next] != -1)
                {
                    continue;
                }
                previous[next] = at;
                queue.push(next);
            }
        }
        p.path.clear();
        p.pathIndex = 0;
        p.routeTimer = 1.5f;
        if (previous[goal] < 0)
        {
            return false;
        }
        for (int at = goal; at != start; at = previous[at])
        {
            p.path.push_back(Position(at));
        }
        std::reverse(p.path.begin(), p.path.end());
        p.path.push_back(destination);
        Profiler::Count("lena.path_requests");
        return true;
    }

    bool World::Move(Person& p, RenderPoint destination, float step)
    {
        if (p.state.dead || (p.state.health <= 12 && p.role != Role::Player) || p.state.detained)
        {
            return false;
        }
        RenderPoint from = p.Position(), next = destination;
        if (!Visible(from, destination))
        {
            if (p.role == Role::Player)
            {
                return false;
            }
            if (p.routeTimer <= 0 && !Route(p, destination))
            {
                return false;
            }
            if (p.pathIndex >= p.path.size())
            {
                return false;
            }
            next = p.path[p.pathIndex];
            if (Distance(from, next) < .15f)
            {
                ++p.pathIndex;
                return false;
            }
        }
        float distance = Distance(from, next);
        // Player input is a per-tick displacement, not an AI arrival tolerance.
        // At 60 Hz a horizontal screen step is only ~0.048 world units.
        if (step <= 0 || distance < (p.role == Role::Player ? .000001f : .05f))
        {
            return false;
        }
        float scale = std::min(1.f, step / distance);
        RenderPoint to{from.x + (next.x - from.x) * scale, from.y + (next.y - from.y) * scale};
        if (!Visible(from, to))
        {
            p.routeTimer = 0;
            return false;
        }
        p.SetPosition(to);
        if (arcana && p.role == Role::Player)
        {
            float length = Distance(from, to);
            if (length > .000001f)
            {
                arcana->moveDirection = {(to.x - from.x) / length, (to.y - from.y) / length};
            }
        }
        p.state.facing = to.x >= from.x ? 1.f : -1.f;
        p.state.stride += step * 4;
        return true;
    }

    void World::RebuildIndex()
    {
        Profiler::Scope scope("lena.spatial_index");
        for (auto& bucket : buckets)
        {
            bucket.clear();
        }
        for (auto id : people)
        {
            auto* p = Find(id);
            if (!p)
            {
                continue;
            }
            int x = std::max(0, std::min(8, static_cast<int>((p->Position().x + Extent) / 6)));
            int y = std::max(0, std::min(8, static_cast<int>((p->Position().y + Extent) / 6)));
            buckets[y * 9 + x].push_back(id);
        }
    }

    std::vector<Actor::Id> World::Nearby(RenderPoint p, float radius) const
    {
        std::vector<Actor::Id> result;
        int minX = std::max(0, static_cast<int>((p.x - radius + Extent) / 6));
        int minY = std::max(0, static_cast<int>((p.y - radius + Extent) / 6));
        int maxX = std::min(8, static_cast<int>((p.x + radius + Extent) / 6));
        int maxY = std::min(8, static_cast<int>((p.y + radius + Extent) / 6));
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                for (auto id : buckets[y * 9 + x])
                {
                    const auto* other = Find(id);
                    if (other && Distance(p, other->Position()) <= radius)
                    {
                        result.push_back(id);
                    }
                }
            }
        }
        std::sort(result.begin(), result.end());
        Profiler::Count("lena.perception_candidates", result.size());
        return result;
    }

    void World::Log(const std::wstring& text)
    {
        messages.push_back(text);
        if (messages.size() > 7)
        {
            messages.pop_front();
        }
    }

    void World::SetAction(Person& p, Action action, Actor::Id target)
    {
        if (p.state.action == action && p.state.target == target)
        {
            return;
        }
        p.state.action = action;
        p.state.target = target;
        p.state.held = 0;
        p.path.clear();
        p.pathIndex = 0;
        p.routeTimer = 0;
        Profiler::Count("lena.action_changes");
    }

    Fact* World::GetFact(std::uint64_t id)
    {
        auto found = std::find_if(facts.begin(),
                                  facts.end(),
                                  [id](const Fact& fact)
                                  {
                                      return fact.id == id;
                                  });
        return found == facts.end() ? nullptr : &*found;
    }

    void World::Observe(Person& p)
    {
        Profiler::Scope scope("lena.observe");
        for (const auto& fact : facts)
        {
            if (p.state.known.count(fact.id) || Distance(p.Position(), fact.position) > 7 ||
                !Visible(p.Position(), fact.position))
            {
                continue;
            }
            // An old body reveals a death, not the identity of an unseen killer.
            if (!fact.death && minutes - fact.minute > .6)
            {
                continue;
            }
            p.state.known.insert(fact.id);
            const auto* victim = Find(fact.victim);
            if (fact.death && victim && victim->species == p.species)
            {
                p.state.grief = Clamp(p.state.grief + 25 + p.traits[6] * .25f);
                p.state.fear = Clamp(p.state.fear + 20);
            }
            if (Human(p) && p.role != Role::Bandit && (fact.crime || fact.death))
            {
                p.state.report = fact.id;
            }
            bool perceivedThreat =
                Human(p) ? fact.crime
                         : victim && victim->species == p.species && fact.actor != fact.victim;
            if (minutes - fact.minute < .6 && perceivedThreat && fact.actor != p.GetId())
            {
                p.state.aggressor = fact.actor;
                p.state.threatSeconds = 12;
                p.state.decision = 0;
            }
        }
    }

    bool World::CanKill(const Person& a, const Person& b) const
    {
        if (a.state.dead || b.state.dead || b.state.health > 12 || b.state.surrendered ||
            Distance(a.Position(), b.Position()) > 1.4f || !Visible(a.Position(), b.Position()))
        {
            return false;
        }
        // This opening has no confirmed human execution mandate. Resentment alone never enables
        // Kill.
        return a.species == Species::Wolf && b.species == Species::Deer && a.state.hunger >= 70 &&
               a.state.action == Action::Hunt && a.state.held >= 2;
    }

    void World::Damage(Person& a, Person& b, float amount, bool intentional)
    {
        Profiler::Scope scope("lena.damage_transaction");
        if (a.state.dead || b.state.dead ||
            (&a != &b && (a.state.surrendered || b.state.surrendered && Civilian(a))))
        {
            return;
        }
        if (intentional && !CanKill(a, b))
        {
            return;
        }
        bool newAssault = b.state.aggressor != a.GetId() || b.state.threatSeconds <= 0;
        bool crime = &a != &b && Human(a) && Human(b) && b.role != Role::Bandit &&
                     !(a.state.aggressor == b.GetId() && a.state.threatSeconds > 0);
        b.state.aggressor = a.GetId();
        b.state.threatSeconds = 14;
        b.state.fear = Clamp(b.state.fear + amount);
        b.state.anger = Clamp(b.state.anger + amount);
        b.state.health = std::max(
            0.f, b.state.health - (b.state.action == Action::Defend ? amount * .5f : amount));
        b.state.decision = 0;
        Profiler::Count("lena.hits");
        const bool died = b.state.health <= 0;
        if (arcana)
        {
            arcana->Hit(a, b);
        }
        if (died || newAssault)
        {
            Fact fact;
            fact.id = ++factSequence;
            fact.actor = a.GetId();
            fact.victim = b.GetId();
            fact.minute = minutes;
            fact.position = b.Position();
            fact.death = died;
            fact.intentional = intentional;
            fact.crime = crime;
            facts.push_back(fact);
        }
        if (!died)
        {
            return;
        }
        b.state.dead = true;
        b.state.target = 0;
        b.state.report = 0;
        b.path.clear();
        b.state.corpseFood = b.species == Species::Deer ? 24 : b.species == Species::Wolf ? 10 : 0;
        // The Actor becomes the corpse container; inventory is not copied or spawned twice.
        if (b.settlement == 0)
        {
            town.morale = std::max(0.0, town.morale - 2);
        }
        if (b.GetId() == playerId)
        {
            gameOver = true;
        }
        Log(b.displayName + L" 사망 — " +
            (&a == &b ? L"누적된 결핍과 노출"
                      : a.displayName + (intentional ? L"의 사냥 마무리" : L"의 공격으로 치명상")));
        Profiler::Count("lena.deaths");
    }

    void World::Decide(Person& p)
    {
        if (arcana)
        {
            arcana->Decide(p);
            return;
        }
        Profiler::Scope scope("lena.decide");
        auto& s = p.state;
        if (s.detained || s.surrendered)
        {
            SetAction(p, Action::Surrender);
            return;
        }
        if (s.health <= 12)
        {
            SetAction(p, Action::Rest);
            return;
        }
        Person* threat = Find(s.aggressor);
        if (!Alive(threat) || s.threatSeconds <= 0 ||
            Distance(p.Position(), threat->Position()) > 10)
        {
            threat = nullptr;
        }
        Person* prey = nullptr;
        Person* body = nullptr;
        Person* patient = nullptr;
        Person* merchant = nullptr;
        Person* victim = nullptr;
        Person* friendNpc = nullptr;
        float preyDistance = 100,
              threatDistance = threat ? Distance(p.Position(), threat->Position()) : 100;
        for (auto id : Nearby(p.Position(), 8))
        {
            auto* other = Find(id);
            if (!other || other == &p || !Visible(p.Position(), other->Position()))
            {
                continue;
            }
            float distance = Distance(p.Position(), other->Position());
            if (other->state.dead)
            {
                if (other->state.corpseFood > 0 &&
                    (!body || distance < Distance(p.Position(), body->Position())))
                {
                    body = other;
                }
                continue;
            }
            bool danger =
                other->state.target == p.GetId() &&
                (other->state.action == Action::Attack || other->state.action == Action::Hunt ||
                 other->state.action == Action::Threaten);
            if (p.species == Species::Deer && other->species == Species::Wolf &&
                other->state.hunger > 50)
            {
                danger = true;
            }
            const auto* threatened = Find(other->state.target);
            if (Guard(p) && Alive(threatened) && Civilian(*threatened) &&
                (other->state.action == Action::Attack || other->state.action == Action::Hunt) &&
                !other->state.surrendered)
            {
                danger = true;
            }
            if (Guard(p) && hunting && other->species == Species::Wolf)
            {
                danger = true;
            }
            if (danger && distance < threatDistance)
            {
                threat = other;
                threatDistance = distance;
            }
            if (p.species == Species::Wolf && other->species == Species::Deer &&
                distance < preyDistance)
            {
                prey = other;
                preyDistance = distance;
            }
            if (p.species == Species::Wolf && s.hunger > 92 && Human(*other) && !Guard(*other) &&
                !prey && distance < 4)
            {
                prey = other;
                preyDistance = distance;
            }
            if (p.role == Role::Healer && Civilian(*other) && other->state.health < 70 &&
                (!patient || other->state.health < patient->state.health))
            {
                patient = other;
            }
            if (other->role == Role::Merchant && other->state.food >= 1)
            {
                merchant = other;
            }
            if (p.role == Role::Bandit && Civilian(*other) && other->role != Role::Player &&
                !other->state.surrendered &&
                (!victim || distance < Distance(p.Position(), victim->Position())))
            {
                victim = other;
            }
            if (Civilian(p) && Civilian(*other) && other->role != Role::Player && distance < 2)
            {
                friendNpc = other;
            }
        }

        if (threat)
        {
            s.aggressor = threat->GetId();
            s.threatSeconds = std::max(2.f, s.threatSeconds);
            if (Human(p) && p.role == Role::Bandit && s.health < 25 && p.traits[0] < 70)
            {
                s.surrendered = true;
                SetAction(p, Action::Surrender);
                Log(p.displayName + L": 항복한다! 더는 싸우지 않겠다.");
                return;
            }
            bool fight = Guard(p) || p.role == Role::Bandit && p.traits[0] > 65 ||
                         p.species == Species::Wolf && s.health > 20 ||
                         p.species == Species::Deer && threatDistance < 1.2f && s.health < 25;
            SetAction(p, fight ? Action::Attack : Action::Flee, threat->GetId());
            return;
        }

        // Eligible local candidates only. Needs, traits and memory contribute to utility.
        Action bestAction = Action::Wander;
        Actor::Id bestTarget = 0;
        float best = 10;
        auto offer = [&](Action action, Actor::Id target, float score)
        {
            float hold = s.action == action && s.target == target ? 12.f : 0.f;
            if (score + hold > best)
            {
                best = score + hold;
                bestAction = action;
                bestTarget = target;
            }
        };
        if (s.hunger > 40 && (s.food >= 1 || p.species == Species::Deer && town.plants > 0 ||
                              p.species == Species::Wolf && body))
        {
            offer(Action::Eat, body ? body->GetId() : 0, s.hunger + 15);
        }
        if (s.thirst > 55)
        {
            offer(Action::Drink, 0, s.thirst);
        }
        if (s.fatigue > 60)
        {
            offer(Action::Sleep, 0, s.fatigue);
        }
        if (s.health < 40)
        {
            offer(Action::Rest, 0, 80);
        }
        if (prey && s.hunger >= 65)
        {
            offer(Action::Hunt, prey->GetId(), s.hunger + 10);
        }
        if (patient && s.medicine >= 1)
        {
            offer(Action::Help, patient->GetId(), 30 + p.traits[1] * .7f);
        }
        if (merchant && s.food < 1 && s.coins >= 2 && s.hunger > 45)
        {
            offer(Action::Trade, merchant->GetId(), s.hunger + 12);
        }
        if (victim && s.hunger > 65)
        {
            offer(Action::Threaten, victim->GetId(), 65 + p.traits[4] * .2f);
            if (s.action == Action::Threaten && s.held >= 2)
            {
                offer(
                    victim->state.food >= 1 ? Action::Steal : Action::Attack, victim->GetId(), 110);
            }
        }
        if (s.report && Human(p) && !Guard(p))
        {
            offer(Action::Report, 0, 65 + p.traits[3] * .2f);
        }
        if (s.grief > 35 && p.species != Species::Deer)
        {
            offer(Action::Mourn, 0, 20 + s.grief * .6f);
        }
        if (friendNpc && !s.known.empty() && s.cooldown <= 0)
        {
            offer(Action::Talk, friendNpc->GetId(), 35);
        }
        if (Human(p) && p.role != Role::Bandit)
        {
            offer(Action::Work, 0, 25);
        }
        if (p.GetId() == liaId && prologue == 0)
        {
            offer(Action::Idle, 0, 45);
        }
        if (p.GetId() == liaId && escorted && Distance(Player().Position(), square) > 3)
        {
            offer(Action::Follow, playerId, 55);
        }
        SetAction(p, bestAction, bestTarget);
    }

    void World::Act(Person& p, float dt)
    {
        auto& s = p.state;
        auto* target = Find(s.target);
        float speed = p.species == Species::Deer ? 3.8f : p.species == Species::Wolf ? 3.5f : 2.4f;
        auto approach = [&](RenderPoint point)
        {
            return Move(p, point, speed * dt);
        };
        bool contact = target && Distance(p.Position(), target->Position()) < 1.4f &&
                       Visible(p.Position(), target->Position());
        switch (s.action)
        {
        case Action::Travel:
            approach(s.destination);
            break;
        case Action::Attack:
        case Action::Hunt:
        {
            if (!Alive(target) || target->state.surrendered && Civilian(p))
            {
                s.decision = 0;
                break;
            }
            if (!contact)
            {
                approach(target->Position());
                break;
            }
            if (s.cooldown > 0)
            {
                break;
            }
            bool kill = CanKill(p, *target);
            Damage(p,
                   *target,
                   kill                         ? 100.f
                   : Guard(p)                   ? 18.f
                   : p.species == Species::Deer ? 10.f
                                                : 14.f,
                   kill);
            s.cooldown = 1.2f;
            break;
        }
        case Action::Flee:
        {
            if (!Alive(target))
            {
                s.decision = 0;
                break;
            }
            auto at = p.Position(), enemy = target->Position();
            float distance = std::max(.1f, Distance(at, enemy));
            RenderPoint away{at.x + (at.x - enemy.x) / distance * 3,
                             at.y + (at.y - enemy.y) / distance * 3};
            if (!Move(p, away, speed * dt) && distance < 1.5f &&
                (!arcana || p.profile != "herbivore"))
            {
                SetAction(p, Action::Attack, target->GetId());
            }
            break;
        }
        case Action::Eat:
            if (p.species == Species::Wolf)
            {
                if (!target || !target->state.dead || target->state.corpseFood <= 0)
                {
                    s.decision = 0;
                    break;
                }
                if (!contact)
                {
                    approach(target->Position());
                    break;
                }
                float amount = std::min(target->state.corpseFood, dt * 2);
                target->state.corpseFood -= amount;
                s.hunger = Clamp(s.hunger - amount * 4);
            }
            else if (p.species == Species::Deer)
            {
                double amount = std::min(town.plants, static_cast<double>(dt));
                town.plants -= amount;
                s.hunger = Clamp(s.hunger - static_cast<float>(amount) * 4);
            }
            else if (s.food >= 1 && s.cooldown <= 0)
            {
                s.food -= 1;
                s.hunger = Clamp(s.hunger - 45);
                s.cooldown = 3;
                town.consumption += 1;
            }
            if (s.hunger < 30)
            {
                s.decision = 0;
            }
            break;
        case Action::Drink:
            if (Distance(p.Position(), {8, -7}) > 1.5f)
            {
                approach({8, -7});
            }
            else
            {
                s.thirst = Clamp(s.thirst - dt * 15);
                s.exposure = Clamp(s.exposure + dt * static_cast<float>(town.pollution) * .01f);
            }
            break;
        case Action::Sleep:
            if (Distance(p.Position(), p.home) > 1)
            {
                approach(p.home);
            }
            else
            {
                s.fatigue = Clamp(s.fatigue - dt * 4);
            }
            break;
        case Action::Rest:
            s.fatigue = Clamp(s.fatigue - dt);
            // Incapacitated actors need actual aid; no automatic resurrection/recovery from
            // critical wounds.
            if (s.health > 12 && s.hunger < 70)
            {
                s.health = std::min(arcana                       ? p.maxHealth
                                    : Human(p)                   ? 100.f
                                    : p.species == Species::Wolf ? 60.f
                                                                 : 45.f,
                                    s.health + dt * .15f);
            }
            break;
        case Action::Help:
            if (!Alive(target) || s.medicine < 1)
            {
                s.decision = 0;
                break;
            }
            if (!contact)
            {
                approach(target->Position());
                break;
            }
            if (s.cooldown <= 0)
            {
                s.medicine -= 1;
                target->state.health = std::min(100.f, target->state.health + 25);
                s.cooldown = 6;
                Profiler::Count("lena.heal_transactions");
            }
            break;
        case Action::Trade:
            if (!Alive(target) || target->state.food < 1 || s.coins < 2)
            {
                s.decision = 0;
                break;
            }
            if (!contact)
            {
                approach(target->Position());
                break;
            }
            if (s.cooldown <= 0)
            {
                s.coins -= 2;
                target->state.coins += 2;
                target->state.food -= 1;
                s.food += 1;
                s.cooldown = 3;
                Profiler::Count("lena.trade_transactions");
            }
            break;
        case Action::Threaten:
            if (Alive(target) && !contact)
            {
                approach(target->Position());
            }
            break;
        case Action::Steal:
            if (!Alive(target))
            {
                s.decision = 0;
                break;
            }
            if (!contact)
            {
                approach(target->Position());
                break;
            }
            if (s.cooldown <= 0 && target->state.food >= 1)
            {
                target->state.food -= 1;
                s.food += 1;
                s.cooldown = 6;
                Fact fact;
                fact.id = ++factSequence;
                fact.actor = p.GetId();
                fact.victim = target->GetId();
                fact.minute = minutes;
                fact.position = p.Position();
                fact.crime = true;
                facts.push_back(fact);
                Log(p.displayName + L"이 식량을 훔쳤다. 목격·신고 여부는 별도다.");
                SetAction(p, Action::Flee, target->GetId());
                Profiler::Count("lena.theft_transactions");
            }
            break;
        case Action::Report:
        {
            Person* receiver = Find(darianId);
            if (!Alive(receiver))
            {
                receiver = nullptr;
                for (auto id : people)
                {
                    auto* other = Find(id);
                    if (Alive(other) && Guard(*other))
                    {
                        receiver = other;
                        break;
                    }
                }
            }
            if (!receiver)
            {
                SetAction(p, Action::Mourn);
                break;
            }
            if (Distance(p.Position(), receiver->Position()) > 1.5f)
            {
                approach(receiver->Position());
                break;
            }
            Fact* fact = GetFact(s.report);
            if (fact)
            {
                receiver->state.known.insert(fact->id);
                if (!fact->reported)
                {
                    fact->reported = true;
                    if (fact->crime)
                    {
                        town.security = std::max(0.0, town.security - 2);
                    }
                    Log(p.displayName + L"의 신고가 경비에게 도착했다. 조사 기록이 남았다.");
                    Profiler::Count("lena.reports_received");
                }
            }
            s.report = 0;
            s.decision = 0;
            break;
        }
        case Action::Talk:
            if (Alive(target) && contact && s.cooldown <= 0)
            {
                for (auto factId : s.known)
                {
                    const auto* fact = GetFact(factId);
                    if (fact && target->state.known.insert(factId).second && fact->death)
                    {
                        target->state.grief = Clamp(target->state.grief + 20);
                    }
                }
                s.cooldown = 15;
            }
            break;
        case Action::Mourn:
            s.grief = Clamp(s.grief - dt * 1.5f);
            break;
        case Action::Surrender:
            for (auto id : Nearby(p.Position(), 2))
            {
                auto* guard = Find(id);
                if (Alive(guard) && Guard(*guard) && Visible(p.Position(), guard->Position()))
                {
                    s.detained = true;
                }
            }
            break;
        case Action::Work:
        {
            double hour = std::fmod(minutes / 60 + 6, 24);
            RenderPoint location = hour >= 20 || hour < 6 ? p.home : p.work;
            approach(location);
            break;
        }
        case Action::Follow:
            if (Alive(target) && Distance(p.Position(), target->Position()) > 2)
            {
                approach(target->Position());
            }
            break;
        case Action::Wander:
            if (s.held > 4 || Distance(p.Position(), s.destination) < .3f)
            {
                float phase = static_cast<float>((tick / 60 + p.GetId() * 97) % 360) * .017453f;
                RenderPoint center =
                    p.species == Species::Deer && corridor ? RenderPoint{18, 17} : p.home;
                s.destination =
                    Safe({center.x + std::cos(phase) * 4, center.y + std::sin(phase) * 4});
                s.held = 0;
            }
            approach(s.destination);
            break;
        default:
            break;
        }
    }

    int World::Population(Species species, bool aliveOnly) const
    {
        int count = 0;
        for (auto id : people)
        {
            const auto* p = Find(id);
            if (p && p->species == species && (species != Species::Human || p->settlement == 0) &&
                (!aliveOnly || !p->state.dead))
            {
                ++count;
            }
        }
        return count;
    }

    void World::Economy(double elapsed)
    {
        Profiler::Scope scope("lena.economy");
        int workers = 0, guards = 0;
        for (auto id : people)
        {
            auto* p = Find(id);
            if (!Alive(p))
            {
                if (p)
                {
                    p->state.corpseFood =
                        std::max(0.f, p->state.corpseFood - static_cast<float>(elapsed / 1440));
                }
                continue;
            }
            if (p->state.hunger > 95 || p->state.thirst > 95 || p->state.exposure > 80)
            {
                float damage = static_cast<float>(elapsed / 1440 * 6);
                if (p->state.health <= damage)
                {
                    Damage(*p, *p, 100);
                    continue;
                }
                p->state.health -= damage;
            }
            if (p->settlement != 0)
            {
                continue;
            }
            if (p->role == Role::Resident && p->state.action == Action::Work &&
                Distance(p->Position(), p->work) < 2)
            {
                ++workers;
            }
            if (Guard(*p))
            {
                ++guards;
            }
            // Transfer actual rations, never manufacture a meal on an Eat/Trade decision.
            double amount =
                std::min(town.food, elapsed / 1440 * (p->role == Role::Merchant ? 2 : 1));
            if (p->state.food < 2)
            {
                town.food -= amount;
                p->state.food += static_cast<float>(amount);
            }
        }
        double produced =
            100 * elapsed / 1440 * std::min(1.0, workers / 28.0) * (1 - town.pollution / 150);
        town.food += produced;
        town.production += produced;
        town.plants = std::min(900.0, town.plants + elapsed / 1440 * 100);
        if (Population(Species::Wolf) == 0 && Population(Species::Deer) > 20)
        {
            town.food = std::max(0.0, town.food - elapsed / 1440 * 12);
        }
        town.security = std::min(100.0, town.security + guards * elapsed / 1440 * .1);
        town.morale = std::max(
            0.0,
            std::min(100.0,
                     town.morale -
                         (town.food < Population(Species::Human) ? elapsed / 1440 * 3 : 0)));
        Profiler::Gauge("lena.population", Population(Species::Human));
        Profiler::Gauge("lena.food_rations", town.food);
        Profiler::Gauge("lena.fact_count", static_cast<double>(facts.size()));
        Profiler::Gauge("lena.wolf_count", Population(Species::Wolf));
        Profiler::Gauge("lena.deer_count", Population(Species::Deer));
    }

    void World::Story(float dt)
    {
        if (arcana)
        {
            arcana->Update(dt);
            return;
        }
        Profiler::Scope scope("lena.story");
        if (prologue == 0 && minutes >= 30)
        {
            prologue = 1;
            prologueStarted = minutes;
            Log(L"리아의 구조 기록이 수로 벽에 남았다. [E]로 읽거나 레나 게시판으로 이동하자.");
        }
        if (prologue == 1 && investigation == 0 && minutes >= prologueStarted + 120)
        {
            investigation = 3;
            waterClue = forestClue = true;
            Log(L"주민 조사 기록: 물의 금속성 빛과 사슴 이동이 함께 확인됐다. 게시판에서 확인 가능.");
        }
        if (minutes >= 180 && Population(Species::Wolf) == 0 && hunting)
        {
            hunting = false;
            Log(L"늑대가 남지 않았다. 사냥 목표 만료 — 숲의 균형은 되돌아오지 않는다.");
        }
        if (corridorWork > 0 && Distance(Player().Position(), forest) < 4 && !Player().state.dead)
        {
            bool danger = false;
            for (auto id : Nearby(forest, 5))
            {
                auto* p = Find(id);
                if (Alive(p) &&
                    (p->state.action == Action::Attack || p->state.action == Action::Hunt))
                {
                    danger = true;
                }
            }
            if (!danger)
            {
                corridorWork = std::max(0.0, corridorWork - dt * 1.2);
            }
            if (corridorWork <= 0)
            {
                corridor = true;
                town.morale = std::min(100.0, town.morale + 3);
                Log(L"120분 작업 완료: 사슴의 이동 통로를 복구했다. 늑대의 허기는 사라지지 않는다.");
            }
        }
    }

    void World::Update(float dt)
    {
        if (gameOver)
        {
            return;
        }
        Profiler::Scope scope("lena.simulation");
        ++tick;
        minutes += arcana ? dt / 60.0 : dt * 1.2;
        routeBudget = 2;
        pulse -= dt;
        if (pulse <= 0)
        {
            RebuildIndex();
            for (auto id : people)
            {
                auto* observer = Find(id);
                if (Alive(observer) && observer->role != Role::Player)
                {
                    Observe(*observer);
                }
            }
            pulse = .25f;
        }
        // Stable SceneGraph/Actor ID order. Every hit and resource transfer rechecks current state.
        for (auto id : people)
        {
            auto* p = Find(id);
            if (!Alive(p))
            {
                continue;
            }
            auto& s = p->state;
            s.cooldown = std::max(0.f, s.cooldown - dt);
            s.threatSeconds = std::max(0.f, s.threatSeconds - dt);
            s.held += dt;
            s.decision -= dt;
            p->routeTimer -= dt;
            s.hunger = Clamp(s.hunger + dt * .05f);
            s.thirst = Clamp(s.thirst + dt * .07f);
            s.fatigue = Clamp(s.fatigue + dt * .03f);
            s.fear = Clamp(s.fear - dt * .3f);
            if (p->role == Role::Player)
            {
                continue;
            }
            if (s.decision <= 0)
            {
                Decide(*p);
                s.decision = 1;
            }
            Act(*p, dt);
        }
        if (minutes - economyMinute >= 60)
        {
            Economy(minutes - economyMinute);
            economyMinute = minutes;
        }
        Story(dt);
    }

    void World::PlayerAttack()
    {
        if (arcana)
        {
            arcana->Attack();
            return;
        }
        auto& player = Player();
        if (player.state.dead || player.state.cooldown > 0)
        {
            return;
        }
        Person* target = nullptr;
        for (auto id : Nearby(player.Position(), 1.8f))
        {
            auto* other = Find(id);
            if (!Alive(other) || other == &player || !Visible(player.Position(), other->Position()))
            {
                continue;
            }
            if (!target || Distance(player.Position(), other->Position()) <
                               Distance(player.Position(), target->Position()))
            {
                target = other;
            }
        }
        if (target)
        {
            Damage(player, *target, 24);
        }
        player.state.cooldown = .55f;
    }

    void World::Interact()
    {
        if (arcana)
        {
            arcana->Interact();
            return;
        }
        auto position = Player().Position();
        for (auto id : Nearby(position, 1.5f))
        {
            auto* p = Find(id);
            if (p && p->GetId() != playerId && Visible(position, p->Position()) && Service(*p))
            {
                return;
            }
        }
        if (Distance(position, canal) < 4)
        {
            waterClue = true;
            Log(minutes >= 720
                    ? L"수로 기록: 복원 대상 07 / 에덴. 기억의 보존은 생명의 부활과 같지 않다."
                    : L"수로 기록: 거대한 탑의 잔상, 물속의 금속성 빛. 손목 인장이 반응하지만 이름은 읽히지 않는다.");
            return;
        }
        if (Distance(position, {8, -7}) < 2)
        {
            Player().state.thirst = 20;
            Player().state.exposure =
                Clamp(Player().state.exposure + static_cast<float>(town.pollution) * .1f);
            Log(L"물을 마셨다. 갈증은 줄지만 오염 노출이 남는다.");
            return;
        }
        if (Distance(position, board) < 4)
        {
            Log(Alive(Find(edwinId))
                    ? L"게시판의 에드윈 메모: 늑대만 탓하기 전에 물과 곡물부터. [1] 수로 [2] 숲"
                    : L"에드윈의 빈자리. 창고 게시판의 기록이 조사 경로를 대신한다. [1] 수로 [2] 숲");
            return;
        }
        if (Distance(position, forest) < 4)
        {
            forestClue = true;
            Log(L"숲의 흔적: 사슴은 오염된 물을 피했고, 굶주린 늑대가 뒤따랐다.");
            return;
        }
        for (auto id : Nearby(position, 2))
        {
            auto* p = Find(id);
            if (!p || p->GetId() == playerId || !Visible(position, p->Position()))
            {
                continue;
            }
            if (p->state.dead)
            {
                Log(p->displayName + L"의 유품은 이미 비어 있다.");
                return;
            }
            Log(p->displayName + L": " + Label(p->state.action) +
                L" 중입니다. 살아 있는 사람부터 돌봐야죠.");
            return;
        }
        Log(L"가까운 사람·유품·수로 기록·마을 게시판·숲 흔적을 조사할 수 있습니다.");
    }

    bool World::Service(Person& p)
    {
        if (p.state.dead)
        {
            if (p.state.food + p.state.coins + p.state.medicine <= 0)
            {
                return false;
            }
            Player().state.food += p.state.food;
            Player().state.coins += p.state.coins;
            Player().state.medicine += p.state.medicine;
            p.state.food = p.state.coins = p.state.medicine = 0;
            Log(p.displayName + L"의 유품을 수거했다. 죽음은 되돌아가지 않는다.");
            return true;
        }
        if (p.state.surrendered || !p.IsActive())
        {
            return false;
        }
        if (p.role == Role::Healer && p.state.medicine >= 1 && Player().state.health < 100)
        {
            p.state.medicine -= 1;
            Player().state.health = std::min(100.f, Player().state.health + 25);
            Profiler::Count("lena.heal_transactions");
            Log(p.displayName + L"이 약품 1개로 상처를 치료했다.");
            return true;
        }
        if (p.role == Role::Merchant && p.state.food >= 1 && Player().state.coins >= 2)
        {
            p.state.food -= 1;
            Player().state.food += 1;
            Player().state.coins -= 2;
            p.state.coins += 2;
            Profiler::Count("lena.trade_transactions");
            Log(p.displayName + L"에게 은화 2개를 지불하고 식량 1배급을 받았다.");
            return true;
        }
        return false;
    }

    void World::Choose(int choice)
    {
        if (arcana)
        {
            return;
        }
        // Expired choices are processed before keyboard requests; player decisions are never
        // inferred.
        Story();
        if (prologue == 0)
        {
            auto* lia = Find(liaId);
            if (choice == 1 && (!Alive(lia) || Distance(lia->Position(), Player().Position()) > 4))
            {
                Log(L"리아가 곁에 없거나 사망해 동행할 수 없습니다. [2] 혼자 출구 찾기");
                return;
            }
            prologue = 1;
            prologueStarted = minutes;
            escorted = choice == 1;
            Log(escorted ? L"리아와 동행한다. 남동쪽 레나 게시판으로 이동하자."
                         : L"혼자 출구를 찾는다. 수로 기록과 손목 인장은 남는다.");
            return;
        }
        if (investigation == 0 && Distance(Player().Position(), board) < 4)
        {
            investigation = choice;
            waterClue = waterClue || choice == 1;
            forestClue = forestClue || choice == 2;
            Log(choice == 1 ? L"수질 조사 우선: 북서쪽 수로의 기록을 확인하자."
                            : L"숲 조사 우선: 동쪽 사슴 숲의 흔적을 확인하자.");
            return;
        }
        if (minutes >= 180 && Distance(Player().Position(), forest) < 4 && !corridor &&
            corridorWork == 0 && !hunting)
        {
            if (choice == 1)
            {
                corridorWork = 120;
                Log(L"통로 복구 시작. 현장에 머물고 위험을 해소해야 120분 작업이 진행됩니다.");
            }
            else if (Population(Species::Wolf) > 0)
            {
                hunting = true;
                Log(L"늑대 사냥을 선택했다. 경비가 실제 전투로 참여하며 사망 수는 미리 정해지지 않는다.");
            }
            return;
        }
        Log(L"이곳에는 현재 선택 가능한 사건이 없습니다. 게시판 또는 숲 흔적으로 이동하세요.");
    }

    WorldMonitor::Snapshot World::Capture(bool paused) const
    {
        Profiler::Scope scope("monitor.capture");
        WorldMonitor::Snapshot snapshot;
        snapshot.epoch = epoch;
        snapshot.paused = paused || gameOver;
        snapshot.worldMinutes = minutes;
        snapshot.minX = snapshot.minY = -Extent;
        snapshot.maxX = snapshot.maxY = Extent;
        WorldMonitor::Settlement settlement;
        settlement.id = "SET_LENA";
        settlement.name = L"레나";
        settlement.metrics = {{"population", Population(Species::Human)},
                              {"food_stock", town.food},
                              {"water_stock", town.water},
                              {"treasury", town.treasury},
                              {"security", town.security},
                              {"morale", town.morale},
                              {"aether_pollution", town.pollution},
                              {"plant_biomass", town.plants},
                              {"food_produced", town.production},
                              {"food_consumed", town.consumption}};
        for (auto id : people)
        {
            const auto* p = Find(id);
            if (p && p->settlement == 0 && !p->state.dead)
            {
                settlement.metrics["resident_food_total"] += p->state.food;
                settlement.metrics["resident_medicine_total"] += p->state.medicine;
                settlement.metrics["resident_coins_total"] += p->state.coins;
            }
        }
        snapshot.settlements.push_back(settlement);
        snapshot.metrics = {{"prologue", prologue},
                            {"investigation", investigation},
                            {"water_clue", waterClue ? 1 : 0},
                            {"forest_clue", forestClue ? 1 : 0},
                            {"death_facts",
                             static_cast<double>(std::count_if(facts.begin(),
                                                               facts.end(),
                                                               [](const Fact& f)
                                                               {
                                                                   return f.death;
                                                               }))},
                            {"fact_count", static_cast<double>(facts.size())},
                            {"corridor_work_minutes", corridorWork}};
        const char* roles[] = {
            "resident", "merchant", "guard", "healer", "scholar", "bandit", "player"};
        for (auto id : people)
        {
            const auto* p = Find(id);
            if (!p)
            {
                continue;
            }
            const auto& s = p->state;
            WorldMonitor::Entity entity;
            entity.id = p->key;
            entity.name = p->displayName;
            entity.kind = p->role == Role::Player       ? "player"
                          : Human(*p)                   ? "npc"
                          : p->species == Species::Wolf ? "monster"
                                                        : "wildlife";
            entity.settlementId = p->settlement == 0 ? "SET_LENA" : "";
            entity.role = Human(*p)                     ? roles[static_cast<int>(p->role)]
                          : p->species == Species::Wolf ? "wolf"
                                                        : "deer";
            entity.action = s.dead
                                ? "dead"
                                : std::string(static_cast<int>(s.action) < 10 ? "WBG_0" : "WBG_") +
                                      std::to_string(static_cast<int>(s.action));
            entity.actionLabel = s.dead ? L"사망 / 유한 사체" : Label(s.action);
            const auto* target = Find(s.target);
            if (target)
            {
                entity.targetId = target->key;
            }
            entity.health = s.health;
            entity.maxHealth = Human(*p) ? 100 : p->species == Species::Wolf ? 60 : 45;
            entity.alive = !s.dead;
            entity.active = p->IsActive();
            entity.visible = p->IsVisible();
            entity.hostile =
                !s.dead && !s.surrendered &&
                (p->role == Role::Bandit || s.action == Action::Attack || s.action == Action::Hunt);
            entity.x = p->Position().x;
            entity.y = p->Position().y;
            entity.metrics = {{"hunger", s.hunger},
                              {"thirst", s.thirst},
                              {"fatigue", s.fatigue},
                              {"fear_memory", s.fear},
                              {"grief", s.grief},
                              {"anger", s.anger},
                              {"exposure", s.exposure},
                              {"food", s.food},
                              {"medicine", s.medicine},
                              {"coins", s.coins},
                              {"corpse_food", s.corpseFood},
                              {"surrendered", s.surrendered ? 1 : 0},
                              {"detained", s.detained ? 1 : 0},
                              {"known_fact_count", static_cast<double>(s.known.size())},
                              {"report_pending", s.report ? 1 : 0}};
            if (Human(*p))
            {
                const char* keys[] = {"courage",
                                      "empathy",
                                      "aggression",
                                      "lawfulness",
                                      "greed",
                                      "curiosity",
                                      "loyalty"};
                for (int i = 0; i < 7; ++i)
                {
                    entity.personality[keys[i]] = p->traits[i];
                }
            }
            snapshot.entities.push_back(std::move(entity));
        }
        if (arcana)
        {
            arcana->Enrich(snapshot);
        }
        return snapshot;
    }

    bool World::Save(const std::wstring& file) const
    {
        Profiler::Scope scope("lena.save");
        if (facts.size() > 100000)
        {
            return false;
        }

        for (auto id : people)
        {
            const auto* person = Find(id);
            if (!person || person->state.known.size() > 100000)
            {
                return false;
            }
        }

        std::ofstream out((file + L".tmp").c_str(), std::ios::binary | std::ios::trunc);
        out.imbue(std::locale::classic());
        out << std::setprecision(17) << (arcana ? "ARCANA_SAVE_1\n" : "LENA_SAVE_1\n");
        out << minutes << ' ' << tick << ' ' << factSequence << ' ' << economyMinute << ' '
            << prologue << ' ' << investigation << ' ' << prologueStarted << ' ' << waterClue << ' '
            << forestClue << ' ' << escorted << ' ' << hunting << ' ' << corridor << ' '
            << corridorWork << ' ' << gameOver << '\n';
        out << town.food << ' ' << town.water << ' ' << town.treasury << ' ' << town.morale << ' '
            << town.security << ' ' << town.pollution << ' ' << town.plants << ' '
            << town.production << ' ' << town.consumption << '\n';
        out << people.size() << '\n';
        for (auto id : people)
        {
            const auto* p = Find(id);
            if (!p)
            {
                return false;
            }
            const auto& s = p->state;
            out << p->key << ' ' << p->Position().x << ' ' << p->Position().y << ' ' << s.health
                << ' ' << s.hunger << ' ' << s.thirst << ' ' << s.fatigue << ' ' << s.fear << ' '
                << s.grief << ' ' << s.anger << ' ' << s.exposure << ' ' << s.food << ' '
                << s.medicine << ' ' << s.coins << ' ' << s.corpseFood << ' ' << s.cooldown << ' '
                << s.decision << ' ' << s.held << ' ' << s.threatSeconds << ' ' << s.facing << ' '
                << s.stride << ' ' << s.dead << ' ' << s.surrendered << ' ' << s.detained << ' '
                << static_cast<int>(s.action) << ' ' << s.target << ' ' << s.aggressor << ' '
                << s.report << ' ' << s.destination.x << ' ' << s.destination.y << ' '
                << s.known.size();
            for (auto fact : s.known)
            {
                out << ' ' << fact;
            }
            if (arcana)
            {
                out << ' ' << s.manaCurrent << ' ' << s.manaMax << ' ' << s.deathMinute << ' '
                    << s.playerCredit << ' ' << s.absorbed;
            }
            out << '\n';
        }
        out << facts.size() << '\n';
        for (const auto& f : facts)
        {
            out << f.id << ' ' << f.actor << ' ' << f.victim << ' ' << f.minute << ' ' << f.death
                << ' ' << f.intentional << ' ' << f.crime << ' ' << f.reported << ' '
                << f.position.x << ' ' << f.position.y << '\n';
        }
        if (arcana)
        {
            arcana->Write(out);
        }
        out.flush();
        const auto size = out.tellp();
        bool ok = out.good() && size >= 0 && size <= 16 * 1024 * 1024;
        out.close();
        if (!ok || out.fail())
        {
            return false;
        }
        return MoveFileExW((file + L".tmp").c_str(),
                           file.c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    }

    bool World::Load(const std::wstring& file)
    {
        Profiler::Scope scope("lena.load");
        std::ifstream in(file.c_str(), std::ios::binary);
        in.imbue(std::locale::classic());
        if (!in)
        {
            return false;
        }
        in.seekg(0, std::ios::end);
        const auto size = in.tellg();
        if (size < 0 || size > 16 * 1024 * 1024)
        {
            return false;
        }
        in.seekg(0);
        std::string version;
        in >> version;
        if (version != (arcana ? "ARCANA_SAVE_1" : "LENA_SAVE_1"))
        {
            return false;
        }
        // Parse into temporary storage. Invalid saves must never partially change the live world.
        double savedMinutes, savedEconomy, savedStarted, savedWork;
        std::uint64_t savedTick, savedSequence;
        int savedPrologue, savedInvestigation;
        bool savedWater, savedForest, savedEscorted, savedHunting, savedCorridor, savedOver;
        Town savedTown;
        in >> savedMinutes >> savedTick >> savedSequence >> savedEconomy >> savedPrologue >>
            savedInvestigation >> savedStarted >> savedWater >> savedForest >> savedEscorted >>
            savedHunting >> savedCorridor >> savedWork >> savedOver;
        in >> savedTown.food >> savedTown.water >> savedTown.treasury >> savedTown.morale >>
            savedTown.security >> savedTown.pollution >> savedTown.plants >> savedTown.production >>
            savedTown.consumption;
        auto finite = [](double value)
        {
            return std::isfinite(value) && value >= 0 && value < 1e9;
        };
        if (!in || !finite(savedMinutes) || !finite(savedEconomy) || savedEconomy > savedMinutes ||
            !finite(savedStarted) || savedStarted > savedMinutes || !finite(savedWork) ||
            savedWork > 120 || savedPrologue < 0 || savedPrologue > 1 || savedInvestigation < 0 ||
            savedInvestigation > 3)
        {
            return false;
        }
        for (double value : {savedTown.food,
                             savedTown.water,
                             savedTown.treasury,
                             savedTown.morale,
                             savedTown.security,
                             savedTown.pollution,
                             savedTown.plants,
                             savedTown.production,
                             savedTown.consumption})
        {
            if (!finite(value))
            {
                return false;
            }
        }
        if (savedTown.morale > 100 || savedTown.security > 100 || savedTown.pollution > 100)
        {
            return false;
        }
        size_t count = 0;
        in >> count;
        if (!in || count != people.size())
        {
            return false;
        }
        std::vector<State> states(count);
        std::vector<RenderPoint> positions(count);
        for (size_t i = 0; i < count; ++i)
        {
            auto& s = states[i];
            std::string key;
            int action = 0;
            size_t knownCount = 0;
            in >> key >> positions[i].x >> positions[i].y >> s.health >> s.hunger >> s.thirst >>
                s.fatigue >> s.fear >> s.grief >> s.anger >> s.exposure >> s.food >> s.medicine >>
                s.coins >> s.corpseFood >> s.cooldown >> s.decision >> s.held >> s.threatSeconds >>
                s.facing >> s.stride >> s.dead >> s.surrendered >> s.detained >> action >>
                s.target >> s.aggressor >> s.report >> s.destination.x >> s.destination.y >>
                knownCount;
            if (!in || key != Find(people[i])->key || knownCount > 100000 || action < 3 ||
                action > 39 || !std::isfinite(positions[i].x) || !std::isfinite(positions[i].y) ||
                !Walkable(positions[i]) || !std::isfinite(s.destination.x) ||
                !std::isfinite(s.destination.y) || std::abs(s.destination.x) > Extent ||
                std::abs(s.destination.y) > Extent || (s.target && !Find(s.target)) ||
                (s.aggressor && !Find(s.aggressor)))
            {
                return false;
            }
            for (float value :
                 {s.hunger, s.thirst, s.fatigue, s.fear, s.grief, s.anger, s.exposure})
            {
                if (!finite(value) || value > 100)
                {
                    return false;
                }
            }
            for (float value : {s.food,
                                s.medicine,
                                s.coins,
                                s.corpseFood,
                                s.cooldown,
                                s.held,
                                s.threatSeconds,
                                s.stride})
            {
                if (!finite(value))
                {
                    return false;
                }
            }
            if (!finite(s.health) || s.health > (arcana ? Find(people[i])->maxHealth : 100.f) ||
                !std::isfinite(s.decision) || !std::isfinite(s.facing) ||
                (s.dead != (s.health <= 0)))
            {
                return false;
            }
            s.action = static_cast<Action>(action);
            for (size_t j = 0; j < knownCount; ++j)
            {
                std::uint64_t id = 0;
                in >> id;
                if (!in || !id || id > savedSequence)
                {
                    return false;
                }
                s.known.insert(id);
            }
            if (arcana)
            {
                in >> s.manaCurrent >> s.manaMax >> s.deathMinute >> s.playerCredit >> s.absorbed;
                if (!in || !finite(s.manaCurrent) || !finite(s.manaMax) ||
                    s.manaCurrent > s.manaMax || !finite(s.deathMinute) ||
                    s.deathMinute > savedMinutes ||
                    (s.absorbed && (!s.dead || !s.playerCredit || !Find(people[i])->beast)))
                {
                    return false;
                }
            }
        }
        size_t factCount = 0;
        in >> factCount;
        if (!in || factCount > 100000)
        {
            return false;
        }
        std::vector<Fact> savedFacts(factCount);
        std::set<std::uint64_t> factIds;
        std::set<Actor::Id> deathVictims;
        for (auto& f : savedFacts)
        {
            in >> f.id >> f.actor >> f.victim >> f.minute >> f.death >> f.intentional >> f.crime >>
                f.reported >> f.position.x >> f.position.y;
            if (!in || !f.id || f.id > savedSequence || !factIds.insert(f.id).second ||
                !Find(f.actor) || !Find(f.victim) || !finite(f.minute) || f.minute > savedMinutes ||
                !std::isfinite(f.position.x) || !std::isfinite(f.position.y) ||
                !Walkable(f.position))
            {
                return false;
            }
            if (f.death && !deathVictims.insert(f.victim).second)
            {
                return false;
            }
        }
        for (size_t i = 0; i < count; ++i)
        {
            const auto& s = states[i];
            if (s.dead != (deathVictims.count(people[i]) != 0) ||
                (s.report && !factIds.count(s.report)))
            {
                return false;
            }
            for (auto id : s.known)
            {
                if (!factIds.count(id))
                {
                    return false;
                }
            }
        }
        Arcana::Progress arcanaProgress;
        if (arcana && !arcana->Read(in, arcanaProgress, savedMinutes))
        {
            return false;
        }
        const auto playerIndex =
            static_cast<size_t>(std::find(people.begin(), people.end(), playerId) - people.begin());
        in >> std::ws;
        if (!in.eof() || playerIndex >= states.size() || savedOver != states[playerIndex].dead)
        {
            return false;
        }
        for (size_t i = 0; i < count; ++i)
        {
            auto* p = Find(people[i]);
            p->state = std::move(states[i]);
            p->SetPosition(positions[i]);
            p->path.clear();
            p->pathIndex = 0;
            p->routeTimer = 0;
        }
        minutes = savedMinutes;
        tick = savedTick;
        factSequence = savedSequence;
        economyMinute = savedEconomy;
        prologue = savedPrologue;
        investigation = savedInvestigation;
        prologueStarted = savedStarted;
        waterClue = savedWater;
        forestClue = savedForest;
        escorted = savedEscorted;
        hunting = savedHunting;
        corridor = savedCorridor;
        corridorWork = savedWork;
        gameOver = savedOver;
        town = savedTown;
        facts = std::move(savedFacts);
        if (arcana)
        {
            arcana->Restore(std::move(arcanaProgress));
        }
        messages.clear();
        pulse = 0;
        epoch = WorldMonitor::NextEpoch();
        RebuildIndex();
        Log(L"세계 전체 상태를 불러왔습니다. 사망·자원·기억·선택 결과도 함께 복원됩니다.");
        return true;
    }
} // namespace Lena
