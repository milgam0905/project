#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Dependencies/glew.h"
#include <Windows.h>
#include "TutorialGame.h"
#include "UiText.h"
#include "Renderer.h"
#include "RenderStats.h"
#include "RenderQueue.h"
#include "Profiler.h"
#include "WorldGeometry.h"
#include "SceneGraph.h"
#include "VillageBehavior.h"
#include "RenderAssets.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace
{
    const float Pi = 3.14159265f;
    const float ViewW = 1280, ViewH = 800;
    using Point = RenderPoint;
    using Color = RenderColor;
    using Action = VillageBehavior::Action;
    using Role = VillageBehavior::Role;

    Color C(float r, float g, float b, float a = 1)
    {
        return {r, g, b, a};
    }

    float Distance(Point a, Point b)
    {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    }

    Point Iso(Point p)
    {
        return {(p.x - p.y) * 42, (p.x + p.y) * 21};
    }

    void Tint(Color c)
    {
        RenderQueue::Color(c.r, c.g, c.b, c.a);
    }

    void Poly(std::initializer_list<Point> points, Color c)
    {
        Tint(c);
        RenderQueue::Begin(GL_TRIANGLE_FAN);
        for (Point p : points)
        {
            RenderQueue::Vertex(p.x, p.y);
        }

        RenderQueue::End();
    }

    void Rect(float x, float y, float w, float h, Color c)
    {
        Poly({{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}, c);
    }

    void Ellipse(float x, float y, float rx, float ry, Color c)
    {
        Tint(c);
        RenderQueue::Begin(GL_TRIANGLE_FAN);
        RenderQueue::Vertex(x, y);
        static const std::vector<Point> circle = []
        {
            std::vector<Point> points;
            for (int i = 0; i <= 32; ++i)
            {
                float angle = i * Pi / 16;
                points.push_back({std::cos(angle), std::sin(angle)});
            }
            return points;
        }();
        for (const Point& unit : circle)
        {
            RenderQueue::Vertex(x + unit.x * rx, y + unit.y * ry);
        }

        RenderQueue::End();
    }

    void Line(Point a, Point b, Color c)
    {
        Tint(c);
        RenderQueue::Begin(GL_LINES);
        RenderQueue::Vertex(a.x, a.y);
        RenderQueue::Vertex(b.x, b.y);
        RenderQueue::End();
    }

    void Glow(Point p, float radius, Color c)
    {
        for (int i = 5; i >= 1; --i)
        {
            Ellipse(p.x, p.y, radius * i / 5, radius * i / 7, C(c.r, c.g, c.b, c.a / 7));
        }
    }

    using TextPainter = UiText;

    enum class Quest
    {
        Meet,
        Herb,
        Lake,
        Return,
        Complete
    };
    enum class Kind
    {
        House,
        Tree,
        Villager,
        Herb,
        Shrine,
        Lamp,
        Bonfire
    };

    struct Placement
    {
        Point p;
        Kind kind;
        int variant;
    };

    struct TutorialActor : Actor
    {
        TutorialActor(Kind kind, int variant)
            : Actor("TutorialObject"), kind(kind), variant(variant)
        {
        }

        Kind kind;
        int variant;
        Point home = {0, 0};
        float phase = 0;
        int direction = 0;
        bool moving = false;
        VillageBehavior::State brain;
        VillageBehavior::Profile profile = {L"주민",
                                            Role::Resident,
                                            {50, 60, 15, 65, 35, 45, 65},
                                            {0, 0},
                                            {0, 0},
                                            L"오늘도 살아갈 일이 남아 있지요."};
    };

    struct Animal : Actor
    {
        Animal(Point home, int species, float phase)
            : Actor("Wildlife"), home(home), species(species), phase(phase)
        {
            SetPosition(home);
        }

        Point home;
        int species;
        float phase;
        float facing = 1;
        VillageBehavior::State brain;
        float respawn = 0;
        bool hostile = false;
    };

    struct Page
    {
        std::wstring speaker, line1, line2;
    };
} // namespace

struct TutorialGame::Impl
{
    Renderer renderer{1280, 800};
    TextPainter text;
    int windowWidth = 1280, windowHeight = 800, playerDirection = 0;
    bool playerMoving = false;
    SceneGraph scene;
    Actor& world = scene.Spawn(scene.Root(), "Tutorial");
    Actor& terrain = scene.Spawn(world, "Terrain");
    Actor& player = scene.Spawn(world, "Player");
    Actor& village = scene.Spawn(world, "Village");
    Actor& wildlife = scene.Spawn(world, "Wildlife");
    std::vector<Actor::Id> animals;
    bool keys[256] = {};
    bool help = false, choice = false, accepted = false, quit = false;
    Point camera = Iso({0, 3});
    Quest quest = Quest::Meet;
    float time = 0, elapsed = 0, stride = 0;
    std::vector<Actor::Id> objects;
    std::vector<Actor::Id> residents;
    VillageBehavior::Navigation navigation;
    float worldMinutes = 360, attackCooldown = 0, invulnerable = 0;
    int playerHealth = 100, playerCoins = 30, defeated = 0;
    bool populationRefill = true;
    Actor::Id elderId = 0, herbId = 0, shrineId = 0;
    std::vector<Page> pages;
    size_t page = 0;
    const Point elder = {0, 0}, herb = {-8, -5}, shrine = {6, -7};
    const Color ink = C(.84f, .87f, .81f), gold = C(.88f, .70f, .39f);

    Impl()
    {
        renderer.PrepareVillageAtlas();
        SetupScene();
        const Placement placements[] = {
            {{-3, 0}, Kind::House, 0},      {{1, -4}, Kind::House, 1},
            {{4, 1}, Kind::House, 2},       {{-3, 5}, Kind::House, 3},
            {elder, Kind::Villager, 0},     {{-1, 2}, Kind::Villager, 1},
            {{2, 3}, Kind::Villager, 2},    {{-5, 2}, Kind::Villager, 3},
            {{3, -2}, Kind::Villager, 4},   {{0, 6}, Kind::Villager, 5},
            {{5, -4}, Kind::Villager, 6},   {{-6, -4}, Kind::Villager, 7},
            {{-5, 7}, Kind::Villager, 8},   {{-1, 10}, Kind::Villager, 9},
            {{4, 7}, Kind::Villager, 10},   {{8, 6}, Kind::Villager, 11},
            {{10, 9}, Kind::Villager, 12},  {{-7, -8}, Kind::Villager, 13},
            {{-10, 0}, Kind::Villager, 14}, {{2, -9}, Kind::Villager, 15},
            {{-7, 8}, Kind::House, 4},      {{6, 8}, Kind::House, 5},
            {{-8, -10}, Kind::House, 6},    {{12, 7}, Kind::House, 7},
            {{3, 11}, Kind::House, 8},      {{-1, 5}, Kind::Bonfire, 0},
            {herb, Kind::Herb, 0},          {shrine, Kind::Shrine, 0},
            {{-1, -1}, Kind::Lamp, 0},      {{2, 5}, Kind::Lamp, 0},
            {{5, -6}, Kind::Lamp, 0}};
        for (const Placement& placement : placements)
        {
            SpawnObject(placement);
        }

        for (int i = 0; i < 20; ++i)
        {
            const auto& profile = VillageBehavior::NewResidents()[i];
            SpawnObject({profile.home, Kind::Villager, i + 16});
        }
        const Placement outskirts[] = {{{-16, 9}, Kind::House, 1},
                                       {{19, 9}, Kind::House, 2},
                                       {{-22, 17}, Kind::House, 3},
                                       {{23, 19}, Kind::House, 4},
                                       {{-25, -20}, Kind::House, 5},
                                       {{32, 25}, Kind::House, 6},
                                       {{-35, 28}, Kind::House, 0},
                                       {{35, -24}, Kind::House, 7},
                                       {{25, 20}, Kind::Bonfire, 0},
                                       {{-20, 14}, Kind::Lamp, 0}};
        for (const auto& placement : outskirts)
        {
            SpawnObject(placement);
        }

        const Point homes[] = {{-10, -7},
                               {-16, -13},
                               {-20, -19},
                               {-10, 12},
                               {14, 10},
                               {18, 15},
                               {21, -13},
                               {22, 18},
                               {-21, 18}};
        for (int i = 0; i < 9; ++i)
        {
            Animal& animal = scene.Spawn<Animal>(wildlife, homes[i], i % 3, static_cast<float>(i));
            animal.onUpdate = [this](Actor& actor, float dt)
            {
                if (pages.empty() && !help)
                {
                    UpdateAnimal(static_cast<Animal&>(actor), dt);
                }
            };
            animal.onDraw = [](const Actor& actor, const SceneView& view)
            {
                const Animal& animal = static_cast<const Animal&>(actor);
                view.renderer.DrawAnimal(
                    view.Screen(animal), animal.species, animal.phase, animal.facing);
            };
            animal.onShadow = [](const Actor& actor, const SceneView& view)
            {
                view.renderer.DrawShadow(view.Screen(actor), 20, 22);
            };
            animals.push_back(animal.GetId());
        }
        // Expand both world axes by two. Keep original short quest destinations intact.
        for (int x = -55; x <= 55; ++x)
        {
            for (int y = -51; y <= 51; ++y)
            {
                Point p = {static_cast<float>(x), static_cast<float>(y)};
                const int seed = (x + 60) * 139 + (y + 60) * 53;
                if ((std::abs(x) > 27 || std::abs(y) > 25) && (x % 2 || y % 2))
                {
                    continue;
                }
                if (seed % 7 || (std::abs(x) < 6 && std::abs(y) < 7) || Water(p))
                {
                    continue;
                }

                if (Distance(p, herb) < 2.5f || Distance(p, shrine) < 2.5f)
                {
                    continue;
                }
                // Reserve the same Bezier trails used by the terrain renderer.
                if (WorldGeometry::OnTrail(p))
                {
                    continue;
                }

                bool reserved = false;
                for (Actor::Id id : objects)
                {
                    auto* objectActor = static_cast<TutorialActor*>(scene.Find(id));
                    if (!objectActor)
                    {
                        continue;
                    }

                    const TutorialActor& object = *objectActor;
                    if (object.kind != Kind::Tree && Distance(p, object.Position()) < 2.3f)
                    {
                        reserved = true;
                    }
                }

                for (Actor::Id id : animals)
                {
                    auto* animalActor = static_cast<Animal*>(scene.Find(id));
                    if (!animalActor)
                    {
                        continue;
                    }

                    const Animal& animal = *animalActor;
                    if (Distance(p, animal.home) < 1.5f)
                    {
                        reserved = true;
                    }
                }

                if (reserved)
                {
                    continue;
                }
                // Deterministic sub-cell jitter prevents a regular tree grid.
                p.x += (seed % 11 - 5) * .065f;
                p.y += (seed % 13 - 6) * .065f;
                if (!Water(p))
                {
                    SpawnObject({p, Kind::Tree, seed % 3});
                }
            }
        }
        PrepareVillage();
    }

    void PrepareVillage()
    {
        Profiler::Scope scope("world.village_prepare");
        std::vector<VillageBehavior::Navigation::Obstacle> obstacles;
        for (Actor::Id id : objects)
        {
            auto* object = static_cast<TutorialActor*>(scene.Find(id));
            if (!object)
            {
                continue;
            }
            if (object->kind == Kind::House)
            {
                obstacles.push_back({object->Position(), 1.35f, 1.15f});
            }
            else if (object->kind == Kind::Tree)
            {
                obstacles.push_back({object->Position(), .42f, .42f});
            }
        }
        navigation.Build(obstacles);
        for (Actor::Id id : residents)
        {
            auto* npc = static_cast<TutorialActor*>(scene.Find(id));
            if (!npc)
            {
                continue;
            }
            npc->home = navigation.Safe(npc->home);
            npc->profile.home = npc->home;
            npc->profile.work = navigation.Safe(npc->profile.work);
            npc->SetPosition(npc->home);
            npc->brain.destination = npc->home;
        }
        for (Actor::Id id : animals)
        {
            auto* animal = static_cast<Animal*>(scene.Find(id));
            if (animal)
            {
                animal->home = navigation.Safe(animal->home);
                animal->SetPosition(animal->home);
            }
        }
        // Fixed test population: 48 hostile beasts in three rings, never on the quest route.
        for (int i = 0; i < 48; ++i)
        {
            float angle = i * 2.f * Pi / 16.f + (i / 16) * .19f;
            float radius = 17.f + (i / 16) * 12.f;
            Point home = navigation.Safe({std::cos(angle) * radius, std::sin(angle) * radius});
            Animal& beast = scene.Spawn<Animal>(wildlife, home, i % 2, static_cast<float>(i));
            beast.hostile = true;
            beast.brain.health = 60;
            beast.brain.decision = (i % 10) * .1f;
            beast.onUpdate = [this](Actor& actor, float dt)
            {
                if (pages.empty() && !help)
                {
                    UpdateBeast(static_cast<Animal&>(actor), dt);
                }
            };
            beast.onDraw = [](const Actor& actor, const SceneView& view)
            {
                const auto& beast = static_cast<const Animal&>(actor);
                Point p = view.Screen(actor);
                view.renderer.DrawCachedModel(p,
                                              beast.species == 0 ? RenderAssets::Wolf
                                                                 : RenderAssets::Boar,
                                              static_cast<int>(beast.phase) % 4,
                                              beast.facing);
                Rect(p.x - 18, p.y - 53, 36, 4, C(.18f, .09f, .08f));
                Rect(p.x - 18, p.y - 53, 36 * beast.brain.health / 60.f, 4, C(.75f, .25f, .15f));
            };
            beast.onShadow = [](const Actor& actor, const SceneView& view)
            {
                view.renderer.DrawShadow(view.Screen(actor), 23, 28);
            };
            animals.push_back(beast.GetId());
        }
        Profiler::Gauge("village.npc_count", static_cast<double>(residents.size()));
        Profiler::Gauge("village.hostile_capacity", 48);
    }

    void MoveAgent(
        Actor& actor, VillageBehavior::State& state, Point destination, float speed, float dt)
    {
        Profiler::Scope scope("ai.village_movement");
        state.repath = std::max(0.f, state.repath - dt);
        Point from = actor.Position();
        float distance = Distance(from, destination);
        if (distance < .12f)
        {
            return;
        }
        Point target = destination;
        if (!navigation.Segment(from, destination))
        {
            int goal = navigation.Cell(destination);
            if (state.repath <= 0 &&
                (goal != state.goalCell || state.waypoint >= state.path.size()))
            {
                if (navigation.Route(from, destination, state.path))
                {
                    state.goalCell = goal;
                    state.waypoint = 0;
                    state.repath = 1.5f;
                }
            }
            if (state.waypoint >= state.path.size())
            {
                return;
            }
            target = state.path[state.waypoint];
            if (Distance(from, target) < .12f)
            {
                ++state.waypoint;
                return;
            }
        }
        distance = Distance(from, target);
        float step = std::min(distance, speed * dt);
        if (distance <= .001f)
        {
            return;
        }
        Point next = {from.x + (target.x - from.x) / distance * step,
                      from.y + (target.y - from.y) / distance * step};
        if (navigation.Segment(from, next))
        {
            actor.SetPosition(next);
        }
        else
        {
            state.path.clear();
            state.goalCell = -1;
        }
    }

    void DamagePlayer(int damage)
    {
        if (invulnerable > 0)
        {
            return;
        }
        playerHealth = std::max(0, playerHealth - damage);
        invulnerable = .65f;
        Profiler::Count("village.player_hits");
        if (playerHealth == 0)
        {
            player.SetPosition({0, 3});
            playerHealth = 100;
            invulnerable = 5;
            Profiler::Count("village.player_test_recoveries");
        }
    }

    void DamageBeast(Animal& beast, float damage, bool reward)
    {
        if (beast.brain.health <= 0)
        {
            return;
        }
        beast.brain.health = std::max(0.f, beast.brain.health - damage);
        if (beast.brain.health == 0)
        {
            beast.SetVisible(false);
            beast.respawn = 25.f;
            if (reward)
            {
                ++defeated;
                playerCoins += 3;
            }
            Profiler::Count("village.beast_defeats");
        }
    }

    void Subdue(TutorialActor& npc, float damage)
    {
        if (npc.brain.surrendered)
        {
            return;
        }
        npc.brain.health = std::max(15.f, npc.brain.health - damage);
        npc.brain.memoryFear = 70;
        npc.brain.decision = 0;
        if (npc.brain.health <= 25)
        {
            npc.brain.surrendered = true;
            npc.brain.action = Action::Surrender;
            Profiler::Count("village.surrenders");
        }
    }

    void Attack()
    {
        Profiler::Scope scope("game.village_combat");
        if (attackCooldown > 0)
        {
            return;
        }
        attackCooldown = .45f;
        Profiler::Count("village.player_attacks");
        for (Actor::Id id : animals)
        {
            auto* beast = static_cast<Animal*>(scene.Find(id));
            if (beast && beast->hostile && Distance(player.Position(), beast->Position()) < 2.1f &&
                navigation.Segment(player.Position(), beast->Position()))
            {
                DamageBeast(*beast, 25, true);
            }
        }
        for (Actor::Id id : residents)
        {
            auto* npc = static_cast<TutorialActor*>(scene.Find(id));
            if (!npc || npc->profile.role != Role::Bandit || npc->brain.surrendered ||
                Distance(player.Position(), npc->Position()) > 2.1f ||
                !navigation.Segment(player.Position(), npc->Position()))
            {
                continue;
            }
            Subdue(*npc, 30);
            npc->brain.threat = player.GetId();
            npc->brain.threatMemory = 8;
            npc->brain.lastThreatPosition = player.Position();
        }
    }

    void UpdateBeast(Animal& beast, float dt)
    {
        Profiler::Scope scope("ai.village_beast_update");
        auto& state = beast.brain;
        if (state.health <= 0)
        {
            if (!populationRefill)
            {
                return;
            }
            beast.respawn -= dt;
            if (beast.respawn <= 0 && Distance(player.Position(), beast.home) > 10)
            {
                beast.SetPosition(beast.home);
                state = VillageBehavior::State{};
                state.health = 60;
                beast.SetVisible(true);
                Profiler::Count("village.test_beast_respawns");
            }
            return;
        }
        state.cooldown = std::max(0.f, state.cooldown - dt);
        state.decision -= dt;
        if (state.decision <= 0)
        {
            state.decision = .25f;
            bool seesPlayer = Distance(player.Position(), beast.Position()) < 6 &&
                              Distance(player.Position(), beast.home) < 9 &&
                              Distance(player.Position(), {0, 3}) > 12 &&
                              navigation.Segment(beast.Position(), player.Position());
            state.action = seesPlayer ? Action::Attack : Action::Wander;
            if (seesPlayer)
            {
                state.destination = player.Position();
            }
            else
            {
                float angle = worldMinutes * .025f + static_cast<float>(beast.GetId());
                state.destination = navigation.Safe(
                    {beast.home.x + std::cos(angle) * 2, beast.home.y + std::sin(angle) * 2});
            }
            Profiler::Count("ai.village_beast_perceptions");
        }
        Point previous = beast.Position();
        MoveAgent(beast, state, state.destination, state.action == Action::Attack ? 2.3f : .8f, dt);
        if (Distance(previous, beast.Position()) > .001f)
        {
            beast.phase += dt * 7;
            beast.facing =
                Iso({beast.Position().x - previous.x, beast.Position().y - previous.y}).x;
        }
        if (state.action == Action::Attack && state.cooldown <= 0 &&
            Distance(player.Position(), beast.Position()) < 1.25f &&
            navigation.Segment(beast.Position(), player.Position()))
        {
            DamagePlayer(8);
            state.cooldown = 1.3f;
        }
    }

    void Service()
    {
        const TutorialActor* nearbyObject = Nearest();
        if (!nearbyObject || nearbyObject->kind != Kind::Villager)
        {
            return;
        }
        auto* npc = static_cast<TutorialActor*>(scene.Find(nearbyObject->GetId()));
        if (!npc || npc->brain.serviceCooldown > 0)
        {
            return;
        }
        auto& state = npc->brain;
        if (npc->profile.role == Role::Merchant)
        {
            int price = npc->profile.traits.greed >= 75 ? 5 : 3;
            if (state.food <= 0 || playerCoins < price)
            {
                Talk({{npc->profile.name,
                       L"재고나 대금이 부족합니다.",
                       L"이번 거래는 이루어지지 않았습니다."}});
                return;
            }
            --state.food;
            state.coins += price;
            playerCoins -= price;
            playerHealth = std::min(100, playerHealth + 20);
            state.action = Action::Trade;
            Profiler::Count("village.trades");
        }
        else if (npc->profile.role == Role::Healer && state.medicine > 0 && playerHealth < 100)
        {
            --state.medicine;
            playerHealth = std::min(100, playerHealth + 35);
            state.action = Action::Help;
            Profiler::Count("village.heals");
        }
        else
        {
            return;
        }
        state.held = 0;
        state.serviceCooldown = 3;
    }

    void SetupScene()
    {
        terrain.layer = ActorLayer::Terrain;
        terrain.cull = false;
        terrain.onDraw = [](const Actor& actor, const SceneView& view)
        {
            view.renderer.DrawTerrain(actor.Position(), actor.Height());
        };

        player.SetPosition({0, 3});
        player.onUpdate = [this](Actor&, float dt)
        {
            UpdatePlayer(dt);
        };
        player.onDraw = [this](const Actor& actor, const SceneView& view)
        {
            Person(view.Screen(actor), 0, true);
            if (attackCooldown > .25f)
            {
                Point p = view.Screen(actor);
                Ellipse(p.x, p.y - 12, 55, 20, C(.95f, .75f, .32f, .18f));
            }
        };
        player.onShadow = [](const Actor& actor, const SceneView& view)
        {
            view.renderer.DrawShadow(view.Screen(actor), 13, 50);
        };

        Actor& atmosphere = scene.Spawn(world, "Atmosphere");
        atmosphere.layer = ActorLayer::Effect;
        atmosphere.cull = false;
        atmosphere.onDraw = [this](const Actor& actor, const SceneView&)
        {
            const Point offset = actor.Position();
            RenderQueue::PushMatrix();
            RenderQueue::Translate(offset.x, offset.y - actor.Height(), 0);
            Atmosphere();
            RenderQueue::PopMatrix();
        };

        Actor& hud = scene.Spawn(scene.Root(), "HUD");
        hud.layer = ActorLayer::Interface;
        hud.cull = false;
        hud.onDraw = [this](const Actor& actor, const SceneView&)
        {
            const Point offset = actor.Position();
            RenderQueue::PushMatrix();
            RenderQueue::Translate(offset.x, offset.y - actor.Height(), 0);
            Hud();
            RenderQueue::PopMatrix();
        };
    }

    void SpawnObject(const Placement& placement)
    {
        TutorialActor& object =
            scene.Spawn<TutorialActor>(village, placement.kind, placement.variant);
        object.SetPosition(placement.p);
        object.home = placement.p;
        if (placement.kind == Kind::Villager)
        {
            if (placement.variant >= 16)
            {
                object.profile = VillageBehavior::NewResidents()[placement.variant - 16];
            }
            else
            {
                static const wchar_t* names[] = {L"장의사 마라",
                                                 L"빵 굽는 오렌",
                                                 L"견습 장의사",
                                                 L"목수 브람",
                                                 L"경비병 세라",
                                                 L"뱃사공의 딸",
                                                 L"어부 노엘",
                                                 L"약초꾼 이브",
                                                 L"대장장이 로크",
                                                 L"양치기 미라",
                                                 L"직조공 엘렌",
                                                 L"행상인 토마",
                                                 L"수습 수도사",
                                                 L"벌목꾼 헤른",
                                                 L"도공 아샤",
                                                 L"순례자 벤"};
                object.profile.name = names[placement.variant];
                object.profile.home = placement.p;
                object.profile.work = {placement.p.x + 1.5f, placement.p.y};
                if (placement.variant == 4)
                {
                    object.profile.role = Role::Guard;
                }
                if (placement.variant == 1 || placement.variant == 11)
                {
                    object.profile.role = Role::Merchant;
                }
                if (placement.variant == 2 || placement.variant == 7 || placement.variant == 12)
                {
                    object.profile.role = Role::Healer;
                    object.profile.traits.empathy = 85;
                }
                if (placement.variant == 15)
                {
                    object.profile.role = Role::Pilgrim;
                }
                object.profile.traits.courage += (placement.variant % 5) * 7 - 10;
                object.profile.traits.curiosity += (placement.variant % 4) * 8;
            }
            object.brain.decision = (placement.variant % 10) * .1f;
            object.brain.perception = (placement.variant % 5) * .05f;
            object.brain.hunger += (placement.variant % 4) * 8;
            object.brain.fatigue += (placement.variant % 3) * 12;
            object.brain.food = object.profile.role == Role::Merchant ? 30 : 5;
            residents.push_back(object.GetId());
        }
        if (placement.kind == Kind::Villager)
        {
            object.onUpdate = [this](Actor& actor, float dt)
            {
                if (pages.empty() && !help)
                {
                    UpdateObject(static_cast<TutorialActor&>(actor), dt);
                }
            };
        }
        object.onDraw = [this](const Actor& actor, const SceneView& view)
        {
            DrawObject(static_cast<const TutorialActor&>(actor), view);
        };
        object.onShadow = [](const Actor& actor, const SceneView& view)
        {
            const TutorialActor& object = static_cast<const TutorialActor&>(actor);
            Point foot = view.Screen(object);
            if (object.kind == Kind::House)
            {
                view.renderer.DrawShadow(foot, 82, 135);
            }
            else if (object.kind == Kind::Tree)
            {
                view.renderer.DrawShadow(foot, 22, 100);
            }
            else if (object.kind == Kind::Villager)
            {
                view.renderer.DrawShadow(foot, 13, 50);
            }
        };
        objects.push_back(object.GetId());

        if (placement.kind == Kind::Villager && placement.variant == 0)
        {
            elderId = object.GetId();
        }
        else if (placement.kind == Kind::Herb)
        {
            herbId = object.GetId();
        }
        else if (placement.kind == Kind::Shrine)
        {
            shrineId = object.GetId();
        }

        if (placement.kind == Kind::Bonfire || placement.kind == Kind::Lamp ||
            placement.kind == Kind::Shrine)
        {
            Actor& flame = scene.Spawn(object, "Flame");
            const float height = placement.kind == Kind::Lamp     ? 59.f
                                 : placement.kind == Kind::Shrine ? 40.f
                                                                  : 0.f;
            const float scale = placement.kind == Kind::Lamp     ? .55f
                                : placement.kind == Kind::Shrine ? .65f
                                                                 : 1.f;
            const bool mystical = placement.kind == Kind::Shrine;
            flame.SetLocalHeight(height);
            flame.depthBias = height;
            flame.onDraw = [scale, mystical](const Actor& actor, const SceneView& view)
            {
                view.renderer.DrawFire(view.Screen(actor), scale, mystical);
            };
        }
    }

    bool Water(Point p) const
    {
        return WorldGeometry::Water(p, .04f);
    }

    bool Blocked(Point p) const
    {
        Profiler::Count("collision.tutorial_queries");
        if (!navigation.open.empty())
        {
            return !navigation.Valid(p);
        }
        if (std::abs(p.x) > WorldGeometry::HalfWidth || std::abs(p.y) > WorldGeometry::HalfHeight ||
            Water(p))
        {
            return true;
        }

        for (Actor::Id id : objects)
        {
            auto* objectActor = static_cast<TutorialActor*>(scene.Find(id));
            if (!objectActor)
            {
                continue;
            }

            const TutorialActor& o = *objectActor;
            if (!o.IsActive())
            {
                continue;
            }

            if (o.kind == Kind::House && std::abs(p.x - o.Position().x) < 1.35f &&
                std::abs(p.y - o.Position().y) < 1.15f)
            {
                return true;
            }

            if (o.kind == Kind::Tree && Distance(p, o.Position()) < .42f)
            {
                return true;
            }
        }

        return false;
    }

    Point Screen(Point p) const
    {
        Point projected = WorldGeometry::Project(p);
        return {projected.x - camera.x + 640, projected.y - camera.y + 420};
    }

    Point Target() const
    {
        Actor::Id id = quest == Quest::Herb ? herbId : quest == Quest::Lake ? shrineId : elderId;
        const Actor* target = scene.Find(id);
        return target ? target->Position() : player.Position();
    }

    const TutorialActor* Nearest() const
    {
        const TutorialActor* best = nullptr;
        float nearest = 1.65f;
        for (Actor::Id id : objects)
        {
            auto* objectActor = static_cast<TutorialActor*>(scene.Find(id));
            if (!objectActor)
            {
                continue;
            }

            const TutorialActor& o = *objectActor;
            if (!o.IsActive())
            {
                continue;
            }

            if (o.kind != Kind::Villager && o.kind != Kind::Herb && o.kind != Kind::Shrine)
            {
                continue;
            }

            float d = Distance(player.Position(), o.Position());
            if (d < nearest)
            {
                nearest = d;
                best = &o;
            }
        }

        return best;
    }

    void Talk(std::initializer_list<Page> dialogue)
    {
        pages = dialogue;
        page = 0;
        std::fill(keys, keys + 256, false);
    }

    void Interact()
    {
        Profiler::Scope profile("game.interaction");
        if (!player.IsActive())
        {
            return;
        }

        const TutorialActor* o = Nearest();
        if (!o)
        {
            return;
        }

        if (o->kind == Kind::Villager && o->variant == 0)
        {
            if (quest == Quest::Meet)
            {
                quest = Quest::Herb;
                Talk({{L"장의사 마라",
                       L"오늘 아침, 이 마을의 뱃사공이 죽었어요.",
                       L"그런데 호수에서는 아직 그 사람의 노 젓는 소리가 들려요."},
                      {L"장의사 마라",
                       L"숲길 끝에서 은빛잎을 한 줌 가져다 호숫가 제단에 놓아 주세요.",
                       L"돌아오게 하는 약이 아니에요. 마지막 작별을 위한 향이지요."},
                      {L"장의사 마라",
                       L"물이 말을 걸어도, 무엇을 가져가는지 먼저 물어보세요.",
                       L"이곳에서 공짜인 건 안개뿐이에요. 장례비도 세 번 나눠 받죠."}});
            }
            else if (quest == Quest::Return)
            {
                quest = Quest::Complete;
                if (accepted)
                {
                    Talk({{L"장의사 마라",
                           L"당신의 손이 차갑군요. 호수가 대가를 받아 갔나요?",
                           L"뱃사공은 돌아오지 않았어요. 하지만 마지막 말은 남았네요."},
                          {L"당신",
                           L"“내 자리를 비워 두지 마.” 그 말을 전해 달라고 했어요.",
                           L"내 가장 따뜻했던 하루는 이제, 무슨 일이 있었는지 떠오르지 않는다."},
                          {L"장의사 마라",
                           L"남의 삶으로 값을 치르지 않은 건 다행이에요.",
                           L"그래도 추억 하나가 말 한마디와 같은 무게인지는... 당신만 알겠죠."}});
                }
                else
                {
                    Talk({{L"장의사 마라",
                           L"향 냄새가 나네요. 이제 호수도 조용해졌어요.",
                           L"그가 마지막으로 무슨 말을 했을지는 끝내 모를 거예요."},
                          {L"당신",
                           L"알 수 없다고 해서, 함께 살았던 날까지 사라지는 건 아니죠.",
                           L"그의 자리에 꽃을 놓았다. 물은 아무 대답도 하지 않았다."},
                          {L"장의사 마라",
                           L"내일은 남은 사람들과 밥을 먹으러 와요.",
                           L"산 사람을 먹이는 건 장례보다 싸고, 가끔은 더 어려우니까."}});
                }
            }
            else
            {
                Talk({{L"장의사 마라",
                       L"은빛잎은 마을 왼쪽 위 숲길에, 제단은 오른쪽 위 호숫가에 있어요.",
                       L"서두르지 않아도 돼요. 작별에도 자기 시간이 있으니까요."}});
            }
        }
        else if (o->kind == Kind::Herb)
        {
            if (quest == Quest::Herb)
            {
                quest = Quest::Lake;
                Talk({{L"은빛잎을 얻었다",
                       L"차가운 잎맥 사이로 희미한 빛이 흐른다.",
                       L"잎이 떨어진 자리에는 새순이 남아 있다. 생명은 다른 생명을 위한 자리를 "
                       L"낸다."},
                      {L"여행 기록",
                       L"은빛잎을 호숫가의 제단에 놓자.",
                       L"가방에 은빛잎 1개가 들어왔다. 다음 목적지는 오른쪽 위 호숫가다."}});
            }
            else
            {
                Talk({{L"은빛잎", L"젖은 흙에서 작은 새순이 자란다.", L"필요한 만큼만 가져가자."}});
            }
        }
        else if (o->kind == Kind::Shrine)
        {
            if (quest == Quest::Lake)
            {
                Talk({{L"호숫가 제단",
                       L"은빛잎을 내려놓자 물결이 거슬러 흐른다. 잎은 재가 된다.",
                       L"물속의 별들은 하늘에 있는 별과 다르다."},
                      {L"물 아래의 목소리",
                       L"죽은 자를 돌려줄 수는 없다. 남은 말 한마디는 건져 줄 수 있다.",
                       L"너의 가장 따뜻한 하루의 기억을 주어라. 다른 사람의 것은 받지 않겠다."},
                      {L"당신의 선택",
                       L"목소리가 정말 뱃사공의 것인지 확인할 방법은 없다.",
                       L"값을 알고도, 그 말을 듣고 싶은가?"}});
                choice = true;
            }
            else
            {
                Talk({{L"호숫가 제단",
                       L"물 아래에서 종소리 같은 것이 울린다.",
                       L"누군가 나를 보고 있다는 느낌만이 남는다."}});
            }
        }
        else if (o->kind == Kind::Villager && o->variant >= 16)
        {
            std::wstring state = L"현재 행동: ";
            state += VillageBehavior::Label(o->brain.action);
            state += L" | E 대화, T 거래/치료 (별도 동의)";
            Talk({{o->profile.name, o->profile.line, state}});
        }
        else
        {
            static const Page lines[] = {
                {L"마라", L"작별도 살아 있는 사람의 일이에요.", L""},
                {L"빵 굽는 오렌",
                 L"죽은 사람이 돌아오면 외상도 돌아오는 걸까요?",
                 L"...뱃사공에게는 내일 빵을 가져다주려 했는데."},
                {L"견습 장의사",
                 L"촛불은 죽은 사람이 길을 잃지 말라고 켠대요.",
                 L"저는 남은 사람이 혼자 있지 말라고 켜요."},
                {L"목수 브람",
                 L"숲길을 따라 왼쪽 위로 가면 은빛잎이 있어요.",
                 L"호수에서 들리는 목소리를 따라 물에 들어가지는 마세요."},
                {L"경비병 세라",
                 L"호숫가 제단은 오른쪽 위예요. 물가에서 멈추세요.",
                 L"괴물보다 젖은 장화가 무서워요. 말려도 냄새가 안 빠져요."},
                {L"뱃사공의 딸",
                 L"아버지가 돌아오면 좋겠어요. 그런데 다른 사람이 아프면 싫어요.",
                 L"내일은 아버지가 심은 나무에 물을 줄 거예요."},
                {L"어부 노엘",
                 L"물속의 별을 세지 마세요. 셀 때마다 하나씩 늘어요.",
                 L"어젯밤에는 제 이름도 알고 있더군요."},
                {L"약초꾼 이브",
                 L"은빛잎은 이 길 끝에 있어요. 뿌리는 남겨 주세요.",
                 L"내년에도 누군가는 이 향이 필요할 테니까요."},
                {L"대장장이 로크",
                 L"불은 쇠를 살리지만, 손까지 살려 주진 않죠.",
                 L"좋은 칼에도 먼저 지불한 누군가의 시간이 들어 있어요."},
                {L"양치기 미라",
                 L"숲에는 사슴과 여우, 산토끼가 살아요.",
                 L"다가가면 달아나니 조금 떨어져서 지켜보세요."},
                {L"직조공 엘렌",
                 L"죽은 이의 옷을 풀어서 아이의 담요를 짜고 있어요.",
                 L"잊으려는 게 아니에요. 다른 모양으로 간직하는 거죠."},
                {L"행상인 토마",
                 L"기억을 판다는 소문을 들었어요.",
                 L"외상 장부만 잊어버리는 약은 아직 없더군요."},
                {L"수습 수도사",
                 L"기도문은 외웠지만, 왜 죽어야 하는지는 모르겠어요.",
                 L"아는 척하는 것보다 곁에 있는 게 낫다고 배웠죠."},
                {L"벌목꾼 헤른",
                 L"안쪽 숲길은 오래 비어 있었어요. 짐승들이 먼저 돌아왔지요.",
                 L"큰 나무는 남겨 둡니다. 우리가 늙기 전에 자란 것들이니까요."},
                {L"도공 아샤",
                 L"깨진 그릇을 붙여도 금은 남아요.",
                 L"그렇다고 물을 담을 수 없는 건 아니죠."},
                {L"순례자 벤",
                 L"이곳의 호수는 밤마다 다른 별을 비춘다지요.",
                 L"답을 찾으러 왔는데, 질문만 늘었어요."}};
            if (quest == Quest::Complete && o->variant == 5)
            {
                Talk({{L"뱃사공의 딸",
                       accepted ? L"아버지의 말을 전해 줘서 고마워요."
                                : L"꽃을 놓아 줬다고 들었어요. 고마워요.",
                       L"오늘은 오렌 아저씨와 빵을 구울 거예요. 아버지도 늘 좋아했어요."}});
            }
            else
            {
                Talk({lines[o->variant]});
            }
        }
    }

    void Choose(bool exchange)
    {
        accepted = exchange;
        choice = false;
        quest = Quest::Return;
        if (exchange)
        {
            Talk({{L"교환이 이루어졌다",
                   L"“내 자리를 비워 두지 마.” 물 아래에서 짧은 말이 떠오른다.",
                   L"따뜻한 기억 하나가 사라졌다. 뱃사공의 삶은 돌아오지 않았다."},
                  {L"여행 기록",
                   L"은빛잎을 소모했다. 대가: 가장 따뜻했던 하루의 기억.",
                   L"장의사 마라에게 돌아가 마지막 말을 전하자."}});
        }
        else
        {
            Talk({{L"작별의 향",
                   L"“그 말은 그 사람과 함께 보내겠다.” 은빛 향이 호수 위로 흩어진다.",
                   L"돌아오는 사람도, 대답도 없다. 남아 있는 기억을 지킨다."},
                  {L"여행 기록",
                   L"은빛잎을 소모했다. 목소리의 거래를 거절했다.",
                   L"장의사 마라에게 돌아가 작별을 마쳤다고 전하자."}});
        }
    }

    void Update(float dt)
    {
        Profiler::Gauge("game.paused", help || !pages.empty() ? 1 : 0);
        time += dt;
        navigation.budget = 3;
        if (!help && pages.empty())
        {
            worldMinutes += dt * 1.2f;
            attackCooldown = std::max(0.f, attackCooldown - dt);
            invulnerable = std::max(0.f, invulnerable - dt);
            if (keys[' '])
            {
                Attack();
            }
        }
        Profiler::Gauge("village.world_minutes", worldMinutes);
        Profiler::Gauge("village.test_refill_enabled", populationRefill ? 1 : 0);
        if (quest != Quest::Complete)
        {
            elapsed += dt;
        }

        scene.Update(dt);
        int aliveBeasts = 0, fleeing = 0, surrendered = 0;
        for (Actor::Id id : animals)
        {
            const auto* animal = static_cast<Animal*>(scene.Find(id));
            if (animal && animal->hostile && animal->brain.health > 0)
            {
                ++aliveBeasts;
            }
        }
        for (Actor::Id id : residents)
        {
            const auto* npc = static_cast<TutorialActor*>(scene.Find(id));
            if (!npc)
            {
                continue;
            }
            fleeing += npc->brain.action == Action::Flee;
            surrendered += npc->brain.surrendered;
        }
        Profiler::Gauge("village.hostiles_alive", aliveBeasts);
        Profiler::Gauge("village.npcs_fleeing", fleeing);
        Profiler::Gauge("village.npcs_surrendered", surrendered);

        Point desired = Iso(player.Position());
        float smoothing = 1 - std::exp(-dt * 7);
        camera.x += (desired.x - camera.x) * smoothing;
        camera.y += (desired.y - camera.y) * smoothing;
    }

    void UpdatePlayer(float dt)
    {
        Profiler::Scope profile("game.player_update");
        playerMoving = false;
        if (pages.empty() && !help)
        {
            float sx = (keys['d'] ? 1.f : 0) - (keys['a'] ? 1.f : 0);
            float sy = (keys['s'] ? 1.f : 0) - (keys['w'] ? 1.f : 0);
            float length = std::sqrt(sx * sx + sy * sy);
            if (length > 0)
            {
                sx /= length;
                sy /= length;
                // Inverse projection gives screen-relative WASD and equal diagonal speed.
                float dx = (sx / 84 + sy / 42) * 115 * dt;
                float dy = (-sx / 84 + sy / 42) * 115 * dt;
                Point previous = player.Position();
                Point next = previous;
                if (!Blocked({player.Position().x + dx, player.Position().y}))
                {
                    next.x += dx;
                }

                if (!Blocked({next.x, next.y + dy}))
                {
                    next.y += dy;
                }

                player.SetPosition(next);
                playerMoving = Distance(previous, player.Position()) > .0001f;
                if (playerMoving)
                {
                    stride += dt * 9;
                }

                playerDirection = std::abs(sx) > std::abs(sy) ? (sx > 0 ? 1 : 3) : (sy > 0 ? 0 : 2);
            }
        }
    }

    void UpdateObject(TutorialActor& object, float dt)
    {
        if (object.kind != Kind::Villager)
        {
            return;
        }
        Profiler::Scope profile("ai.npc_update");
        auto& state = object.brain;
        const auto& traits = object.profile.traits;
        const Role role = object.profile.role;
        state.hunger = std::min(100.f, state.hunger + dt * .075f);
        state.thirst = std::min(100.f, state.thirst + dt * .10f);
        state.fatigue = std::min(100.f, state.fatigue + dt * .045f);
        state.held += dt;
        state.cooldown = std::max(0.f, state.cooldown - dt);
        state.serviceCooldown = std::max(0.f, state.serviceCooldown - dt);
        state.threatMemory = std::max(0.f, state.threatMemory - dt);
        state.memoryFear = std::max(0.f, state.memoryFear - dt);
        state.perception -= dt;
        object.moving = false;
        if (state.surrendered)
        {
            state.action = Action::Surrender;
            return;
        }

        if (state.perception <= 0)
        {
            Profiler::Scope perception("ai.village_perception");
            state.perception = .25f;
            float nearest = 6.f;
            Actor::Id observed = 0;
            for (Actor::Id id : animals)
            {
                const auto* beast = static_cast<Animal*>(scene.Find(id));
                if (!beast || !beast->hostile || beast->brain.health <= 0)
                {
                    continue;
                }
                float distance = Distance(object.Position(), beast->Position());
                if (distance < nearest && navigation.Segment(object.Position(), beast->Position()))
                {
                    nearest = distance;
                    observed = id;
                }
            }
            if (role != Role::Bandit)
            {
                for (Actor::Id id : residents)
                {
                    const auto* other = static_cast<TutorialActor*>(scene.Find(id));
                    if (!other || other->profile.role != Role::Bandit || other->brain.surrendered ||
                        other->brain.action != Action::Attack)
                    {
                        continue;
                    }
                    float distance = Distance(object.Position(), other->Position());
                    if (distance < nearest &&
                        navigation.Segment(object.Position(), other->Position()))
                    {
                        nearest = distance;
                        observed = id;
                    }
                }
            }
            if (role == Role::Bandit && Distance(player.Position(), object.Position()) < 5 &&
                Distance(player.Position(), {0, 3}) > 12 &&
                navigation.Segment(object.Position(), player.Position()))
            {
                observed = player.GetId();
            }
            if (observed)
            {
                state.threat = observed;
                state.threatMemory = 6;
                state.memoryFear = std::max(state.memoryFear, 35.f);
                if (const Actor* target = scene.Find(observed))
                {
                    state.lastThreatPosition = target->Position();
                }
            }
            if (state.threatMemory <= 0 || !scene.Find(state.threat))
            {
                state.threat = 0;
                state.warned = false;
            }
            Profiler::Count("ai.village_perceptions");
        }

        state.decision -= dt;
        if (state.decision <= 0)
        {
            Profiler::Scope decision("ai.village_decision");
            state.decision = 1.f;
            ++state.tick;
            const Actor* threat = scene.Find(state.threat);
            const bool danger = threat && state.threatMemory > 0;
            const bool closeThreat =
                danger && Distance(object.Position(), state.lastThreatPosition) < 3;
            const int hour = static_cast<int>(worldMinutes / 60) % 24;
            const bool night = hour >= 21 || hour < 6;
            // Runtime eligibility first, then bounded personality / need / memory / goal terms.
            // Control nodes Evaluate/Observe/Remember/Decide do not compete with actions.
            std::vector<std::pair<Action, float>> candidates;
            auto offer = [&](Action action,
                             bool eligible,
                             float need,
                             float personality,
                             float environment,
                             float goal,
                             float memory)
            {
                if (!eligible)
                {
                    return;
                }
                unsigned seed = static_cast<unsigned>(object.variant + 1) * 2654435761u ^
                                state.tick * 2246822519u ^
                                static_cast<unsigned>(action) * 3266489917u;
                float variation = static_cast<float>(seed % 7) - 3.f;
                candidates.push_back(
                    {action, 10.f + need + personality + environment + goal + memory + variation});
            };
            offer(Action::Idle, true, 0, 0, 0, 0, 0);
            offer(Action::Work,
                  !danger && !night && role != Role::Bandit && role != Role::Scholar &&
                      role != Role::Pilgrim,
                  0,
                  traits.loyalty * .15f,
                  0,
                  28,
                  0);
            offer(Action::Investigate,
                  !danger && !night && (role == Role::Scholar || role == Role::Pilgrim),
                  0,
                  traits.curiosity * .3f,
                  0,
                  28,
                  0);
            offer(Action::Wander,
                  !danger && !night,
                  0,
                  traits.curiosity * .1f,
                  0,
                  role == Role::Bandit ? 30.f : 5.f,
                  0);
            offer(Action::Eat,
                  !danger && state.food > 0 && state.hunger > 45,
                  state.hunger,
                  0,
                  0,
                  10,
                  0);
            offer(Action::Drink, !danger && state.thirst > 50, state.thirst, 0, 0, 10, 0);
            offer(Action::Rest, !danger && state.fatigue > 55, state.fatigue, 0, 0, 10, 0);
            offer(Action::Sleep, !danger && night, state.fatigue, 0, 25, 20, 0);
            offer(Action::Help,
                  !danger && role == Role::Healer && state.medicine > 0 && playerHealth < 80 &&
                      Distance(player.Position(), object.Position()) < 4,
                  0,
                  traits.empathy * .5f,
                  15,
                  25,
                  0);
            offer(Action::Flee,
                  danger,
                  15,
                  (100 - traits.courage) * .6f,
                  closeThreat ? 35.f : 10.f,
                  10,
                  state.memoryFear * .3f);
            offer(Action::Protect,
                  danger && role == Role::Guard,
                  0,
                  traits.courage * .45f + traits.empathy * .1f,
                  20,
                  30,
                  0);
            offer(Action::Threaten,
                  danger && role == Role::Bandit && state.threat == player.GetId(),
                  0,
                  traits.aggression * .45f,
                  20,
                  20,
                  0);
            offer(Action::Attack,
                  danger && role == Role::Bandit && state.warned && traits.aggression >= 60 &&
                      state.threat == player.GetId(),
                  0,
                  traits.aggression * .45f,
                  25,
                  40,
                  0);
            Action chosen = state.action;
            float best = -10000, current = -10000;
            for (const auto& candidate : candidates)
            {
                if (candidate.first == state.action)
                {
                    current = candidate.second;
                }
                if (candidate.second > best ||
                    (candidate.second == best &&
                     (candidate.first == state.action ||
                      (chosen != state.action &&
                       static_cast<int>(candidate.first) < static_cast<int>(chosen)))))
                {
                    best = candidate.second;
                    chosen = candidate.first;
                }
            }
            if (current == -10000 || (state.held >= 2 && best >= current + 12) ||
                (closeThreat && state.action != Action::Flee && state.action != Action::Protect &&
                 state.action != Action::Threaten && state.action != Action::Attack))
            {
                state.action = chosen;
                state.held = 0;
                state.progress = 0;
                state.goalCell = -1;
                state.path.clear();
                state.waypoint = 0;
                Profiler::Count("ai.village_action_changes");
            }
            Point destination = object.profile.work;
            if (state.action == Action::Sleep || state.action == Action::Rest ||
                state.action == Action::Eat)
            {
                destination = object.home;
            }
            if (state.action == Action::Drink)
            {
                destination = {6, -5};
            }
            if (state.action == Action::Help)
            {
                destination = player.Position();
            }
            if (state.action == Action::Wander ||
                (state.action == Action::Work && role == Role::Guard))
            {
                float angle = worldMinutes * .012f + object.variant;
                destination = {object.profile.work.x + std::cos(angle) * 2,
                               object.profile.work.y + std::sin(angle) * 2};
            }
            // Edwin's schedule is explicitly clock-driven, unlike other work locations.
            if (object.variant == 17 && state.action == Action::Work)
            {
                destination = hour < 9 ? Point{-3, 2} : hour < 18 ? Point{0, 1} : Point{0, 6};
            }
            if (state.action == Action::Flee && threat)
            {
                Point p = object.Position(), q = state.lastThreatPosition;
                float length = std::max(.1f, Distance(p, q));
                destination = {p.x + (p.x - q.x) / length * 5, p.y + (p.y - q.y) / length * 5};
            }
            if (state.action == Action::Protect && threat)
            {
                destination = state.lastThreatPosition;
            }
            if ((state.action == Action::Threaten || state.action == Action::Attack) && threat)
            {
                destination =
                    state.action == Action::Attack ? state.lastThreatPosition : object.Position();
            }
            state.destination = navigation.Safe(destination);
            Profiler::Count("ai.village_decisions");
            switch (state.action)
            {
            case Action::Work:
                Profiler::Count("ai.wbg_12_work_decisions");
                break;
            case Action::Investigate:
                Profiler::Count("ai.wbg_03_investigate_decisions");
                break;
            case Action::Eat:
                Profiler::Count("ai.wbg_10_eat_decisions");
                break;
            case Action::Drink:
                Profiler::Count("ai.wbg_11_drink_decisions");
                break;
            case Action::Sleep:
                Profiler::Count("ai.wbg_09_sleep_decisions");
                break;
            case Action::Rest:
                Profiler::Count("ai.wbg_13_rest_decisions");
                break;
            case Action::Help:
                Profiler::Count("ai.wbg_17_help_decisions");
                break;
            case Action::Flee:
                Profiler::Count("ai.wbg_31_flee_decisions");
                break;
            case Action::Protect:
                Profiler::Count("ai.wbg_33_protect_decisions");
                break;
            case Action::Threaten:
                Profiler::Count("ai.wbg_23_threaten_decisions");
                break;
            case Action::Attack:
                Profiler::Count("ai.wbg_28_attack_decisions");
                break;
            default:
                Profiler::Count("ai.village_other_decisions");
                break;
            }
        }

        if (state.action == Action::Threaten && state.held > 3)
        {
            state.warned = true;
        }

        Point previous = object.Position();
        // Quest giver remains reachable at the original quest marker.
        if (object.variant != 0 &&
            (Distance(player.Position(), previous) > 1.6f || state.action == Action::Protect ||
             state.action == Action::Flee || state.action == Action::Threaten ||
             state.action == Action::Attack))
        {
            float speed = state.action == Action::Flee      ? 2.8f
                          : state.action == Action::Protect ? 1.8f
                                                            : .9f;
            MoveAgent(object, state, state.destination, speed, dt);
        }
        Point current = object.Position();
        object.moving = Distance(previous, current) > .001f;
        if (object.moving)
        {
            Point delta = Iso({current.x - previous.x, current.y - previous.y});
            object.direction = std::abs(delta.x) > std::abs(delta.y) ? (delta.x > 0 ? 1 : 3)
                                                                     : (delta.y > 0 ? 0 : 2);
            object.phase += dt * 7;
        }
        bool arrived = Distance(current, state.destination) < .8f || object.variant == 0;
        if (arrived)
        {
            state.progress += dt;
            if (state.action == Action::Eat && state.progress >= 2 && state.food > 0)
            {
                --state.food;
                state.hunger = std::max(0.f, state.hunger - 55);
                state.progress = 0;
                state.decision = 0;
                Profiler::Count("village.food_consumed");
            }
            if (state.action == Action::Drink)
            {
                state.thirst = std::max(0.f, state.thirst - dt * 20);
            }
            if (state.action == Action::Rest || state.action == Action::Sleep)
            {
                state.fatigue =
                    std::max(0.f, state.fatigue - dt * (state.action == Action::Sleep ? 5.f : 2.f));
            }
            if (state.action == Action::Work)
            {
                Profiler::Count("village.work_ticks");
            }
        }
        if (state.action == Action::Help && state.cooldown <= 0 && state.medicine > 0 &&
            playerHealth < 80 && Distance(current, player.Position()) < 1.8f &&
            navigation.Segment(current, player.Position()))
        {
            --state.medicine;
            playerHealth = std::min(100, playerHealth + 20);
            state.cooldown = 5;
            Profiler::Count("village.heals");
        }
        Actor* threat = scene.Find(state.threat);
        if (state.action == Action::Protect && threat && threat != &player && state.cooldown <= 0 &&
            Distance(current, threat->Position()) < 1.6f &&
            navigation.Segment(current, threat->Position()))
        {
            auto* beast = dynamic_cast<Animal*>(threat);
            auto* bandit = dynamic_cast<TutorialActor*>(threat);
            if (beast)
            {
                DamageBeast(*beast, 18, false);
            }
            else if (bandit && bandit->profile.role == Role::Bandit)
            {
                Subdue(*bandit, 18);
            }
            state.cooldown = 1.2f;
            if ((beast && beast->brain.health <= 0) || (bandit && bandit->brain.surrendered))
            {
                state.threat = 0;
                state.threatMemory = 0;
                state.decision = 0;
            }
            Profiler::Count("village.guard_attacks");
        }
        if (state.action == Action::Attack && traits.aggression >= 60 && state.cooldown <= 0 &&
            Distance(current, player.Position()) < 1.4f &&
            navigation.Segment(current, player.Position()))
        {
            DamagePlayer(7);
            state.cooldown = 1.5f;
            Profiler::Count("village.bandit_attacks");
        }
    }

    void UpdateAnimal(Animal& animal, float dt)
    {
        Profiler::Scope profile("ai.animal_update");
        Point target = {animal.home.x + std::sin(time * .23f + animal.species) * 2.5f,
                        animal.home.y + std::cos(time * .19f + animal.phase * .02f) * 2.f};
        bool flee = Distance(animal.Position(), player.Position()) < 3.8f;
        if (flee)
        {
            target = {animal.Position().x + (animal.Position().x - player.Position().x) * 2,
                      animal.Position().y + (animal.Position().y - player.Position().y) * 2};
        }

        float length = Distance(animal.Position(), target);
        if (length < .1f)
        {
            return;
        }

        float speed = flee ? 3.5f : 1.05f;
        Point next = {animal.Position().x + (target.x - animal.Position().x) / length * dt * speed,
                      animal.Position().y + (target.y - animal.Position().y) / length * dt * speed};
        // Axis sliding keeps animals off water and out of buildings/trees.
        Point previous = animal.Position();
        Point position = previous;
        if (!Blocked({next.x, animal.Position().y}))
        {
            position.x = next.x;
        }

        if (!Blocked({position.x, next.y}))
        {
            position.y = next.y;
        }

        animal.SetPosition(position);
        if (Distance(previous, animal.Position()) > .001f)
        {
            animal.phase += dt * (flee ? 13.f : 6.f);
            Point delta = Iso({animal.Position().x - previous.x, animal.Position().y - previous.y});
            if (std::abs(delta.x) > .001f)
            {
                animal.facing = delta.x;
            }
        }
    }

    void Person(Point p,
                int variant,
                bool isPlayer,
                int direction = 0,
                float phase = 0,
                bool walking = false)
    {
        if (isPlayer)
        {
            direction = playerDirection;
            phase = stride;
            walking = playerMoving;
        }

        int frame = walking ? 1 + static_cast<int>(phase) % 7 : 0;
        p.y += walking ? 0.f : std::sin(time * 1.8f + variant) * .6f;
        renderer.DrawCharacter(p, variant, direction, frame, isPlayer);
    }

    void DrawObject(const TutorialActor& o, const SceneView& view)
    {
        Point p = view.Screen(o);
        if (p.x < -200 || p.x > 1480 || p.y < -100 || p.y > 1080)
        {
            return;
        }

        if (o.kind == Kind::Villager)
        {
            Person(p, o.variant, false, o.direction, o.phase, o.moving && pages.empty() && !help);
            if (o.profile.role == Role::Bandit)
            {
                Rect(p.x - 16, p.y - 84, 32, 4, C(.2f, .12f, .1f));
                Rect(p.x - 16,
                     p.y - 84,
                     32 * o.brain.health / 100.f,
                     4,
                     o.brain.surrendered ? C(.7f, .7f, .7f) : C(.8f, .25f, .2f));
            }
        }
        else if (o.kind == Kind::Tree)
        {
            float alpha = Distance(player.Position(), o.Position()) < 2.5f ? .38f : 1.f;
            renderer.DrawTree(p, o.variant, alpha);
        }
        else if (o.kind == Kind::House)
        {
            Point ps = Screen(player.Position());
            float alpha =
                (std::abs(ps.x - p.x) < 100 && ps.y < p.y + 25 && ps.y > p.y - 145) ? .4f : 1.f;
            renderer.DrawHouse(p, o.variant, alpha);
        }
        else if (o.kind == Kind::Bonfire)
        {
            for (int i = 0; i < 8; ++i)
            {
                float a = i * Pi / 4;
                Ellipse(p.x + std::cos(a) * 17, p.y + std::sin(a) * 8, 6, 4, C(.35f, .36f, .31f));
            }

            Line({p.x - 14, p.y + 3}, {p.x + 12, p.y - 4}, C(.23f, .14f, .075f));
            Line({p.x - 12, p.y - 4}, {p.x + 14, p.y + 3}, C(.23f, .14f, .075f));
        }
        else if (o.kind == Kind::Lamp)
        {
            Rect(p.x - 3, p.y - 68, 6, 68, C(.18f, .20f, .17f));
            Glow({p.x, p.y - 66}, 65, C(1, .59f, .20f, .55f));
            Rect(p.x - 6, p.y - 74, 12, 17, C(.94f, .69f, .30f));
            Rect(p.x - 8, p.y - 77, 16, 4, C(.13f, .16f, .15f));
        }
        else if (o.kind == Kind::Herb)
        {
            bool picked =
                quest == Quest::Lake || quest == Quest::Return || quest == Quest::Complete;
            if (!picked)
            {
                Glow(p, 40, C(.50f, .80f, .70f, .55f));
            }

            for (int i = 0; i < 5; ++i)
            {
                float offset = (i - 2) * 5.f;
                Line({p.x, p.y}, {p.x + offset, p.y - 12 - i % 2 * 7.f}, C(.37f, .54f, .36f));
                Ellipse(p.x + offset,
                        p.y - 12 - i % 2 * 7.f,
                        4,
                        picked ? 2.f : 5.f,
                        C(.61f, .75f, .65f));
            }
        }
        else if (o.kind == Kind::Shrine)
        {
            Poly({{p.x - 35, p.y}, {p.x, p.y - 18}, {p.x + 35, p.y}, {p.x, p.y + 18}},
                 C(.32f, .38f, .35f));
            Rect(p.x - 12, p.y - 45, 24, 45, C(.36f, .43f, .40f));
            Ellipse(p.x, p.y - 45, 12, 7, C(.43f, .48f, .42f));
            Glow({p.x, p.y - 25}, 55, C(.42f, .74f, .70f, .45f));
            Poly({{p.x, p.y - 38}, {p.x + 5, p.y - 25}, {p.x, p.y - 17}, {p.x - 5, p.y - 25}},
                 C(.60f, .83f, .73f));
            if (quest == Quest::Return || quest == Quest::Complete)
            {
                for (int i = 0; i < 5; ++i)
                {
                    Ellipse(p.x + std::sin(time + i) * 8,
                            p.y - 55 - i * 9.f,
                            5,
                            8,
                            C(.65f, .73f, .67f, .12f));
                }
            }
        }
    }

    void Atmosphere()
    {
        for (int i = 0; i < 13; ++i)
        {
            float x = std::fmod(i * 173.f + time * 9, 1600.f) - 160;
            float y = 170.f + static_cast<float>((i * 97) % 530);
            Ellipse(x, y, 190, 20, C(.40f, .56f, .54f, .028f));
        }

        for (int i = 0; i < 95; ++i)
        {
            float x = std::fmod(i * 149.f + time * 30, 1280.f);
            float y = std::fmod(i * 97.f + time * 290, 800.f);
            Line({x, y}, {x - 3, y + 12}, C(.55f, .69f, .70f, .13f));
        }
    }

    void Panel(float x, float y, float w, float h)
    {
        Rect(x, y, w, h, C(.025f, .055f, .062f, .94f));
        Rect(x, y, 3, h, gold);
        Line({x + 3, y}, {x + w, y}, C(.57f, .48f, .31f, .55f));
    }

    void Hud()
    {
        Profiler::Scope profile("ui.hud_build");
        Panel(28, 24, 620, 120);
        text.Draw(46, 34, L"잔향의 마을  /  마지막 한마디", gold, 1.1f);
        const wchar_t* objectives[] = {L"01  장의사 마라에게 다가가 E로 대화하기",
                                       L"02  왼쪽 위 숲길에서 은빛잎 채집하기 [E]",
                                       L"03  오른쪽 위 호숫가 제단에 은빛잎 놓기 [E]",
                                       L"04  마을의 장의사 마라에게 돌아가기 [E]",
                                       L"완료  —  작별 뒤에도 마을의 삶은 이어집니다"};
        text.Draw(46, 76, objectives[static_cast<int>(quest)], ink);
        text.Draw(46,
                  110,
                  L"WASD 이동  E 대화  SPACE 공격  T 거래/치료  H 도움말",
                  C(.56f, .66f, .64f),
                  .78f);
        Panel(995, 24, 257, 180);
        text.Draw(1012, 33, L"마을과 주변", gold, .85f);
        auto mini = [](Point p) -> Point
        {
            Point v = Iso(p);
            return {1125 + v.x * .0215f, 125 + v.y * .0215f};
        };
        for (Actor::Id id : objects)
        {
            auto* objectActor = static_cast<TutorialActor*>(scene.Find(id));
            if (!objectActor)
            {
                continue;
            }

            const TutorialActor& o = *objectActor;
            if (!o.IsVisible() || o.kind == Kind::Tree || o.kind == Kind::Lamp ||
                o.kind == Kind::Bonfire)
            {
                continue;
            }

            Point p = mini(o.Position());
            Ellipse(p.x, p.y, o.kind == Kind::House ? 4.f : 2.f, 2, C(.40f, .51f, .46f));
        }

        Tint(C(.14f, .38f, .42f));
        RenderQueue::Begin(GL_TRIANGLE_FAN);
        Point lake = mini({11, -6});
        RenderQueue::Vertex(lake.x, lake.y);
        for (int i = 0; i <= 64; ++i)
        {
            Point edge = mini(WorldGeometry::LakeEdge(i * 2 * Pi / 64.f));
            RenderQueue::Vertex(edge.x, edge.y);
        }

        RenderQueue::End();
        for (Actor::Id id : animals)
        {
            auto* animalActor = static_cast<Animal*>(scene.Find(id));
            if (!animalActor)
            {
                continue;
            }

            const Animal& animal = *animalActor;
            if (!animal.IsVisible())
            {
                continue;
            }

            Point p = mini(animal.Position());
            Ellipse(p.x, p.y, 1.8f, 1.8f, animal.hostile ? C(.9f, .25f, .2f) : C(.71f, .51f, .29f));
        }

        if (quest != Quest::Complete)
        {
            Point p = mini(Target());
            Ellipse(p.x, p.y, 5, 5, gold);
            Point marker = Screen(Target());
            marker.x = std::max(38.f, std::min(1240.f, marker.x));
            marker.y = std::max(226.f, std::min(640.f, marker.y - 64));
            float pulse = std::sin(time * 3) * 3;
            Poly({{marker.x, marker.y - 8 + pulse},
                  {marker.x + 6, marker.y + pulse},
                  {marker.x, marker.y + 8 + pulse},
                  {marker.x - 6, marker.y + pulse}},
                 gold);
        }

        Point me = mini(player.Position());
        Ellipse(me.x, me.y, 4, 4, C(.90f, .95f, .89f));
        text.Draw(1012, 171, L"금색: 목적지   흰색: 나", ink, .7f);
        if (pages.empty() && !help)
        {
            const int hour = static_cast<int>(worldMinutes / 60) % 24;
            const int minute = static_cast<int>(worldMinutes) % 60;
            std::wstring status = L"마을 테스트 | NPC 36 | HP " + std::to_wstring(playerHealth) +
                                  L" | 은화 " + std::to_wstring(playerCoins) + L" | 처치 " +
                                  std::to_wstring(defeated);
            text.Draw(35, 151, status, ink, .75f);
            std::wstring clock =
                L"세계 시각 " + std::to_wstring(hour) + L":" + (minute < 10 ? L"0" : L"") +
                std::to_wstring(minute) +
                (populationRefill ? L" | F 몹 보충 ON (테스트)" : L" | F 몹 보충 OFF");
            text.Draw(35, 178, clock, gold, .75f);
            // Labels are submitted by the HUD actor after postprocessing, not individual raw GL
            // calls.
            for (Actor::Id id : residents)
            {
                const auto* npc = static_cast<TutorialActor*>(scene.Find(id));
                if (!npc || Distance(player.Position(), npc->Position()) > 7)
                {
                    continue;
                }
                Point p = Screen(npc->Position());
                if (p.x < 80 || p.x > 1190 || p.y < 300 || p.y > 610)
                {
                    continue;
                }
                std::wstring label = npc->profile.name;
                label += L" / ";
                label += VillageBehavior::Label(npc->brain.action);
                text.Draw(p.x - 48, p.y - 98, label, ink, .6f);
            }
        }
        if (pages.empty() && !help)
        {
            const TutorialActor* nearbyObject = Nearest();
            if (nearbyObject)
            {
                Panel(450, 622, 380, 48);
                text.Draw(470,
                          631,
                          nearbyObject->kind == Kind::Villager ? L"[E] 주민과 이야기하기"
                                                               : L"[E] 조사 / 상호작용",
                          ink);
            }

            if (quest == Quest::Complete)
            {
                Panel(265, 678, 750, 98);
                text.Draw(286, 689, L"튜토리얼 완료 — 남은 사람들의 내일", gold);
                text.Draw(286,
                          725,
                          accepted ? L"선택: 기억을 내어 마지막 말을 들었다.  R 재시작"
                                   : L"선택: 기억을 지키고 작별했다.  R 재시작",
                          ink,
                          .9f);
            }
            else
            {
                text.Draw(35,
                          755,
                          quest == Quest::Lake
                              ? L"소지품: 은빛잎 1   |   목표 플레이 시간 3~4분"
                              : L"목표 플레이 시간 3~4분   |   금색 표식을 따라 이동하세요",
                          ink,
                          .85f);
            }
        }

        if (!pages.empty())
        {
            Panel(100, 548, 1080, 230);
            const Page& p = pages[page];
            text.Draw(126, 565, p.speaker, gold);
            text.Draw(126, 611, p.line1, ink);
            text.Draw(126, 649, p.line2, ink);
            if (choice && page + 1 == pages.size())
            {
                text.Draw(
                    126, 718, L"[1] 기억을 지불하고 듣는다     [2] 거래를 거절하고 작별한다", gold);
            }
            else
            {
                text.Draw(
                    126, 725, L"E / SPACE  다음     ESC  대화 닫기", C(.55f, .65f, .63f), .85f);
            }
        }

        if (help)
        {
            Panel(240, 220, 800, 320);
            text.Draw(270, 239, L"짧은 여행 안내", gold, 1.2f);
            text.Draw(270, 291, L"WASD : 화면 기준 이동 (대각선 속도 동일)", ink);
            text.Draw(270, 334, L"E : 가까운 주민 / 약초 / 제단과 상호작용", ink);
            text.Draw(270, 377, L"E 또는 SPACE : 대화 진행    1 / 2 : 호수에서 선택", ink);
            text.Draw(270, 420, L"SPACE 공격 | T 거래/치료 | F 테스트용 몹 보충 전환", ink);
            text.Draw(
                270, 477, L"H / ESC 안내 닫기 | R 초기화 | 대화/도움말 중 세계 일시정지", gold);
        }
    }

    void Render()
    {
        scene.Render({renderer, camera, time});
    }
};

TutorialGame::TutorialGame() : m(new Impl)
{
}

TutorialGame::~TutorialGame() = default;

void TutorialGame::Resize(int width, int height)
{
    m->windowWidth = std::max(1, width);
    m->windowHeight = std::max(1, height);
    m->renderer.Resize(m->windowWidth, m->windowHeight);
}

void TutorialGame::Update(float dt)
{
    m->Update(std::max(0.f, std::min(.05f, dt)));
}

void TutorialGame::Render()
{
    m->Render();
}

void TutorialGame::ClearKeys()
{
    std::fill(m->keys, m->keys + 256, false);
}

bool TutorialGame::WantsQuit() const
{
    return m->quit;
}

void TutorialGame::KeyUp(unsigned char key)
{
    if (key >= 'A' && key <= 'Z')
    {
        key = key - 'A' + 'a';
    }

    m->keys[key] = false;
}

void TutorialGame::KeyDown(unsigned char key)
{
    if (key >= 'A' && key <= 'Z')
    {
        key = key - 'A' + 'a';
    }

    if (m->keys[key])
    {
        return;
    }

    m->keys[key] = true;
    if (key == 'h')
    {
        m->help = !m->help;
        ClearKeys();
        return;
    }

    if (m->help)
    {
        if (key == 27)
        {
            m->help = false;
        }

        return;
    }

    if (!m->pages.empty())
    {
        if (m->choice && m->page + 1 == m->pages.size())
        {
            if (key == '1' || key == '2')
            {
                m->Choose(key == '1');
            }

            return; // A consequential choice cannot be dismissed accidentally.
        }

        if (key == 't' && !m->choice)
        {
            m->pages.clear();
            m->Service();
        }
        else if (key == 'e' || key == ' ')
        {
            if (++m->page == m->pages.size())
            {
                m->pages.clear();
                ClearKeys();
            }
        }
        else if (key == 27)
        {
            if (m->choice)
            {
                m->page = m->pages.size() - 1;
            }
            else
            {
                m->pages.clear();
            }
        }

        return;
    }

    if (key == 'e')
    {
        m->Interact();
    }
    if (key == 't')
    {
        m->Service();
    }
    if (key == 'f')
    {
        m->populationRefill = !m->populationRefill;
    }

    if (key == 27)
    {
        m->quit = true;
    }

    if (key == 'r')
    {
        int width = m->windowWidth, height = m->windowHeight;
        m.reset(new Impl);
        Resize(width, height);
    }
}
