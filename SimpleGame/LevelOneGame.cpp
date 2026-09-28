#include "stdafx.h"
#include "LevelOneGame.h"
#include "LevelOneMap.h"
#include "RpgStats.h"
#include "UiText.h"
#include "RenderAssets.h"
#include "WorldGeometry.h"
#include "RenderStats.h"
#include "RenderQueue.h"
#include "Profiler.h"
#include "SceneGraph.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace
{
    using Point = RenderPoint;
    using Color = RenderColor;

    float Distance(Point a, Point b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    std::uint32_t NewSeed()
    {
        return std::random_device{}() ^ GetTickCount();
    }

    Color C(float r, float g, float b, float a = 1)
    {
        return {r, g, b, a};
    }

    void Box(float x, float y, float width, float height, Color color)
    {
        RenderQueue::Color(color.r, color.g, color.b, color.a);
        RenderQueue::Begin(GL_QUADS);
        RenderQueue::Vertex(x, y);
        RenderQueue::Vertex(x + width, y);
        RenderQueue::Vertex(x + width, y + height);
        RenderQueue::Vertex(x, y + height);
        RenderQueue::End();
    }

    void Ring(Point p, float rx, float ry, Color color)
    {
        RenderQueue::Color(color.r, color.g, color.b, color.a);
        RenderQueue::Begin(GL_LINE_LOOP);
        for (int i = 0; i < 48; ++i)
        {
            float angle = i * WorldGeometry::Pi / 24.f;
            RenderQueue::Vertex(p.x + std::cos(angle) * rx, p.y + std::sin(angle) * ry);
        }

        RenderQueue::End();
    }

    struct Enemy : Actor
    {
        Enemy() : Actor("Enemy")
        {
        }

        Point home = {0, 0}, waypoint = {0, 0};
        int species = 0;
        int health = 32;
        float phase = 0;
        float facing = 1;
        float cooldown = 0;
        float windup = 0;
        float pathTimer = 0;
        float respawnTimer = 0;
        float hurtFlash = 0;

        int MaxHealth() const
        {
            return species == 0 ? 32 : 48;
        }
    };

    struct Loot : Actor
    {
        Loot(bool potion, int amount)
            : Actor(potion ? "Potion" : "Coins"), potion(potion), amount(amount)
        {
        }

        bool potion;
        int amount;
    };
} // namespace

struct LevelOneGame::Impl
{
    Renderer renderer{1280, 800};
    UiText text;
    SceneGraph scene;
    Actor& world = scene.Spawn(scene.Root(), "LevelOne");
    Actor& terrain = scene.Spawn(world, "Terrain");
    Actor& player = scene.Spawn(world, "Player");
    Actor& scenery = scene.Spawn(world, "Scenery");
    Actor& enemyGroup = scene.Spawn(world, "Enemies");
    Actor& lootGroup = scene.Spawn(world, "Loot");
    LevelOneMap map{NewSeed()};
    RpgStats stats;
    Point camera = WorldGeometry::Project(LevelOneMap::Camp());
    std::vector<Actor::Id> enemies;
    std::vector<Actor::Id> loot;
    bool keys[256] = {};
    bool quit = false, statsOpen = false, help = true, dead = false, complete = false;
    float time = 0, stride = 0, attackTimer = 0, slashTimer = 0, hurtTimer = 0;
    float slashAngle = 0, noticeTimer = 0;
    int direction = 0, kills = 0;
    bool moving = false;
    std::wstring notice;

    Impl()
    {
        SetupScene();
        Populate();
    }

    void Message(const std::wstring& value)
    {
        notice = value;
        noticeTimer = 4.f;
    }

    void SetupScene()
    {
        terrain.layer = ActorLayer::Terrain;
        terrain.cull = false;
        terrain.onDraw = [this](const Actor& actor, const SceneView&)
        {
            renderer.DrawHuntingTerrain(map, actor.Position(), actor.Height());
        };

        player.onUpdate = [this](Actor&, float dt)
        {
            UpdatePlayer(dt);
        };
        player.onDraw = [this](const Actor& actor, const SceneView& view)
        {
            if (hurtTimer <= 0 || static_cast<int>(time * 18) % 2 == 0)
            {
                renderer.DrawCharacter(view.Screen(actor),
                                       7,
                                       direction,
                                       moving ? 1 + static_cast<int>(stride) % 7 : 0,
                                       true);
            }
        };
        player.onShadow = [](const Actor& actor, const SceneView& view)
        {
            view.renderer.DrawShadow(view.Screen(actor), 13, 50);
        };

        Actor& slash = scene.Spawn(player, "AttackEffect");
        slash.layer = ActorLayer::Effect;
        slash.SetLocalHeight(20);
        slash.onDraw = [this](const Actor& actor, const SceneView& view)
        {
            if (slashTimer <= 0)
            {
                return;
            }

            Point p = view.Screen(actor);
            RenderQueue::Color(.9f, .88f, .63f, slashTimer / .2f);
            RenderQueue::Begin(GL_LINE_STRIP);
            for (int i = 0; i <= 20; ++i)
            {
                float angle = slashAngle - 1 + i * .1f;
                RenderQueue::Vertex(p.x + std::cos(angle) * 60, p.y + std::sin(angle) * 35);
            }

            RenderQueue::End();
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

    void SpawnLoot(Point position, bool potion, int amount)
    {
        Loot& item = scene.Spawn<Loot>(lootGroup, potion, amount);
        item.SetPosition(position);
        item.onDraw = [this](const Actor& actor, const SceneView& view)
        {
            const Loot& item = static_cast<const Loot&>(actor);
            Point p = view.Screen(item);
            renderer.DrawCachedModel(
                p, item.potion ? RenderAssets::Potion : RenderAssets::Coins, 0, 1);
            if (Distance(player.Position(), item.Position()) < 1.5f)
            {
                Ring(p, 13, 7, C(.92f, .78f, .35f));
            }
        };
        loot.push_back(item.GetId());
    }

    void Populate()
    {
        Profiler::Scope profile("world.populate");
        renderer.PrepareHuntingMap(map);
        // IDs never own actors. Reset category indexes when rebuilding the map.
        enemies.clear();
        loot.clear();
        scene.ClearChildren(scenery);
        scene.ClearChildren(enemyGroup);
        scene.ClearChildren(lootGroup);

        for (int y = 0; y < LevelOneMap::Height; ++y)
        {
            for (int x = 0; x < LevelOneMap::Width; ++x)
            {
                const auto tile = map.At(x, y);
                if (tile != LevelOneMap::Tile::Tree && tile != LevelOneMap::Tile::Rock)
                {
                    continue;
                }

                const bool tree = tile == LevelOneMap::Tile::Tree;
                Actor& prop = scene.Spawn(scenery, tree ? "Tree" : "Rock");
                prop.SetPosition({x + .5f, y + .5f});
                prop.onShadow = [tree](const Actor& actor, const SceneView& view)
                {
                    view.renderer.DrawShadow(view.Screen(actor), tree ? 20.f : 24.f, 50);
                };
                prop.onDraw = [this, tree](const Actor& actor, const SceneView& view)
                {
                    float alpha =
                        tree && Distance(player.Position(), actor.Position()) < 2.f ? .4f : 1.f;
                    renderer.DrawCachedModel(view.Screen(actor),
                                             tree ? RenderAssets::ForestTree
                                                  : RenderAssets::Boulder,
                                             0,
                                             1,
                                             alpha);
                };
            }
        }

        const auto& positions = map.SpawnCells();
        for (size_t i = 0; i < 14 && i < positions.size(); ++i)
        {
            Enemy& enemy = scene.Spawn<Enemy>(enemyGroup);
            enemy.SetPosition(positions[i]);
            enemy.home = enemy.waypoint = positions[i];
            enemy.species = static_cast<int>(i % 2);
            enemy.health = enemy.MaxHealth();
            enemy.onUpdate = [this](Actor& actor, float dt)
            {
                if (!help && !statsOpen && !dead)
                {
                    UpdateEnemy(static_cast<Enemy&>(actor), dt);
                }
            };
            enemy.onShadow = [](const Actor& actor, const SceneView& view)
            {
                view.renderer.DrawShadow(view.Screen(actor), 20, 24);
            };
            enemy.onDraw = [this](const Actor& actor, const SceneView& view)
            {
                DrawEnemy(static_cast<const Enemy&>(actor), view);
            };
            enemies.push_back(enemy.GetId());
        }

        Point camp = LevelOneMap::Camp();
        Actor& campfire = scene.Spawn(scenery, "Campfire");
        campfire.SetPosition({camp.x - 1.2f, camp.y - .8f});
        campfire.onDraw = [](const Actor& actor, const SceneView& view)
        {
            view.renderer.DrawCachedModel(view.Screen(actor), RenderAssets::Campfire, 0, 1);
        };
        Actor& flame = scene.Spawn(campfire, "Flame");
        flame.SetLocalHeight(10);
        // Match the parent's depth while retaining the flame's local height.
        flame.depthBias = 10;
        flame.onDraw = [](const Actor& actor, const SceneView& view)
        {
            view.renderer.DrawFire(view.Screen(actor), 1.f);
        };

        SpawnLoot({camp.x + 1.5f, camp.y}, true, 1);
        player.SetPosition(camp);
        camera = WorldGeometry::Project(player.Position());
        ClearKeys();
    }

    void ClearKeys()
    {
        std::fill(keys, keys + 256, false);
    }

    bool Move(Actor& actor, Point desired)
    {
        Point position = actor.Position();
        const Point previous = position;
        if (map.Walkable({desired.x, position.y}))
        {
            position.x = desired.x;
        }

        if (map.Walkable({position.x, desired.y}))
        {
            position.y = desired.y;
        }

        actor.SetPosition(position);
        return Distance(position, previous) > .0001f;
    }

    void Attack()
    {
        Profiler::Scope profile("game.combat");
        if (!player.IsActive() || help || statsOpen || dead || attackTimer > 0)
        {
            return;
        }

        attackTimer = stats.AttackCooldown();
        slashTimer = .20f;
        const float facingAngles[] = {
            WorldGeometry::Pi * .5f, 0, -WorldGeometry::Pi * .5f, WorldGeometry::Pi};
        slashAngle = facingAngles[direction];

        // Nearest visible melee target: keyboard and mouse attack share this rule.
        Enemy* target = nullptr;
        float nearestDistance = 1.85f;
        for (Actor::Id id : enemies)
        {
            auto* enemyActor = static_cast<Enemy*>(scene.Find(id));
            if (!enemyActor)
            {
                continue;
            }

            Enemy& enemy = *enemyActor;
            float distance = Distance(player.Position(), enemy.Position());
            if (enemy.IsActive() && enemy.health > 0 && distance < nearestDistance &&
                map.ClearSegment(player.Position(), enemy.Position()))
            {
                nearestDistance = distance;
                target = &enemy;
            }
        }

        if (!target)
        {
            return;
        }

        Point delta = WorldGeometry::Project({target->Position().x - player.Position().x,
                                              target->Position().y - player.Position().y});
        slashAngle = std::atan2(delta.y, delta.x);
        direction =
            std::abs(delta.x) > std::abs(delta.y) ? (delta.x > 0 ? 1 : 3) : (delta.y > 0 ? 0 : 2);
        target->health = std::max(0, target->health - stats.Attack());
        target->hurtFlash = .18f;

        if (target->health == 0)
        {
            ++kills;
            target->SetVisible(false);
            target->windup = 0;
            target->respawnTimer = 18.f;
            const int gainedXp = target->species == 0 ? 25 : 40;
            int gainedLevels = stats.GainExperience(gainedXp);
            SpawnLoot(target->Position(), false, target->species == 0 ? 4 : 7);
            if (kills % 3 == 0)
            {
                SpawnLoot(target->Position(), true, 1);
            }

            Message(gainedLevels > 0
                        ? L"레벨 상승! 최대 체력·공격력·방어력 증가 / C를 눌러 능력 포인트 배분"
                        : L"처치! 경험치 +" + std::to_wstring(gainedXp) + L" / E로 전리품 획득");
        }
    }

    void Interact()
    {
        Profiler::Scope profile("game.interaction");
        if (!player.IsActive() || help || statsOpen || dead)
        {
            return;
        }

        int found = 0;
        for (auto it = loot.begin(); it != loot.end();)
        {
            auto* item = static_cast<Loot*>(scene.Find(*it));
            if (!item)
            {
                it = loot.erase(it);
                continue;
            }

            if (item->IsActive() && Distance(player.Position(), item->Position()) < 1.5f &&
                map.ClearSegment(player.Position(), item->Position()))
            {
                if (item->potion)
                {
                    stats.potions += item->amount;
                }
                else
                {
                    stats.gold += item->amount;
                }

                scene.Destroy(*item);
                it = loot.erase(it);
                ++found;
            }
            else
            {
                ++it;
            }
        }

        if (found > 0)
        {
            Message(L"주변 전리품을 획득했습니다. 회복약은 Q로 사용합니다.");
        }
        else if (Distance(player.Position(), LevelOneMap::Camp()) < 2.5f)
        {
            stats.health = stats.MaxHealth();
            Message(L"야영지에서 휴식했습니다. 체력을 모두 회복합니다.");
        }
        else
        {
            Message(L"가까이 있는 전리품에 접근한 뒤 E를 누르세요.");
        }
    }

    void UsePotion()
    {
        if (!player.IsActive() || dead || help || statsOpen || stats.potions == 0 ||
            stats.health == stats.MaxHealth())
        {
            return;
        }
        --stats.potions;
        stats.health = std::min(stats.MaxHealth(), stats.health + 45);
        Message(L"회복약 사용: 체력 +45");
    }

    void UpdateEnemy(Enemy& enemy, float dt)
    {
        Profiler::Scope profile("ai.enemy_update");
        enemy.hurtFlash = std::max(0.f, enemy.hurtFlash - dt);
        if (enemy.health == 0)
        {
            enemy.respawnTimer -= dt;
            if (enemy.respawnTimer <= 0 && Distance(player.Position(), enemy.home) > 5.f)
            {
                enemy.SetPosition(enemy.home);
                enemy.SetVisible(true);
                enemy.health = enemy.MaxHealth();
                enemy.cooldown = 1.f;
                enemy.pathTimer = 0;
            }

            return;
        }

        enemy.cooldown = std::max(0.f, enemy.cooldown - dt);
        float distance = Distance(player.Position(), enemy.Position());
        bool chase = distance < 8.f && Distance(player.Position(), LevelOneMap::Camp()) > 3.f;

        if (enemy.windup > 0)
        {
            enemy.windup = std::max(0.f, enemy.windup - dt);
            if (enemy.windup == 0)
            {
                if (chase && distance < 1.35f &&
                    map.ClearSegment(enemy.Position(), player.Position()) && hurtTimer <= 0)
                {
                    int damage = std::max(1, (enemy.species == 0 ? 11 : 16) - stats.Defense());
                    stats.health = std::max(0, stats.health - damage);
                    hurtTimer = .55f;
                    if (stats.health == 0)
                    {
                        dead = true;
                        ClearKeys();
                        return;
                    }
                }

                enemy.cooldown = 1.25f;
            }

            return;
        }

        if (chase && distance < 1.15f && map.ClearSegment(enemy.Position(), player.Position()))
        {
            if (enemy.cooldown <= 0)
            {
                enemy.windup = .45f;
            }

            return;
        }

        Point destination = chase ? player.Position() : enemy.home;
        if (Distance(enemy.Position(), destination) < .15f)
        {
            return;
        }

        enemy.pathTimer -= dt;
        if (enemy.pathTimer <= 0 || Distance(enemy.Position(), enemy.waypoint) < .12f)
        {
            enemy.waypoint = map.NextStep(enemy.Position(), destination);
            enemy.pathTimer = .4f;
        }

        float length = Distance(enemy.Position(), enemy.waypoint);
        if (length > .01f)
        {
            float step = std::min(length, dt * (enemy.species == 0 ? 1.8f : 1.35f));
            Point previous = enemy.Position();
            Point desired = {
                enemy.Position().x + (enemy.waypoint.x - enemy.Position().x) / length * step,
                enemy.Position().y + (enemy.waypoint.y - enemy.Position().y) / length * step};
            if (Move(enemy, desired))
            {
                enemy.phase += dt * 9;
                Point delta = WorldGeometry::Project(
                    {enemy.Position().x - previous.x, enemy.Position().y - previous.y});
                if (std::abs(delta.x) > .001f)
                {
                    enemy.facing = delta.x;
                }
            }
            else
            {
                enemy.pathTimer = 0;
            }
        }
    }

    void Update(float dt)
    {
        Profiler::Gauge("game.paused", help || statsOpen || dead ? 1 : 0);
        Profiler::Gauge("game.loot_count", static_cast<double>(loot.size()));
        time += dt;
        noticeTimer = std::max(0.f, noticeTimer - dt);
        scene.Update(dt);

        if (!help && !statsOpen && !dead)
        {
            Point target = WorldGeometry::Project(player.Position());
            float smoothing = 1 - std::exp(-dt * 8);
            camera.x += (target.x - camera.x) * smoothing;
            camera.y += (target.y - camera.y) * smoothing;
        }
    }

    void UpdatePlayer(float dt)
    {
        Profiler::Scope profile("game.player_update");
        moving = false;
        if (help || statsOpen || dead)
        {
            return;
        }

        attackTimer = std::max(0.f, attackTimer - dt);
        slashTimer = std::max(0.f, slashTimer - dt);
        hurtTimer = std::max(0.f, hurtTimer - dt);
        float sx = (keys['d'] ? 1.f : 0.f) - (keys['a'] ? 1.f : 0.f);
        float sy = (keys['s'] ? 1.f : 0.f) - (keys['w'] ? 1.f : 0.f);
        float length = std::hypot(sx, sy);
        if (length > 0)
        {
            sx /= length;
            sy /= length;
            Point next = {player.Position().x + (sx / 84 + sy / 42) * 145 * dt,
                          player.Position().y + (-sx / 84 + sy / 42) * 145 * dt};
            moving = Move(player, next);
            if (moving)
            {
                stride += dt * 9;
            }

            direction = std::abs(sx) > std::abs(sy) ? (sx > 0 ? 1 : 3) : (sy > 0 ? 0 : 2);
        }

        if (keys[' '])
        {
            Attack();
        }
    }

    void Panel(float x, float y, float width, float height)
    {
        Box(x, y, width, height, C(.025f, .05f, .057f, .96f));
        Box(x, y, 3, height, C(.78f, .64f, .36f));
    }

    void Hud()
    {
        Profiler::Scope profile("ui.hud_build");
        const Color ink = C(.85f, .88f, .82f), gold = C(.9f, .72f, .4f);
        Panel(24, 20, 690, 132);
        text.Draw(42, 29, L"레벨 1 · 잿빛 사냥터", gold, 1.15f);
        text.Draw(42,
                  69,
                  complete ? L"목표 완료! 자유롭게 사냥을 계속할 수 있습니다."
                           : L"목표: 레벨 3 달성 + 능력 포인트 6개 배분 [C]",
                  ink);
        text.Draw(42,
                  111,
                  L"WASD 이동  SPACE / 좌클릭 공격  E 획득  Q 회복약  C 능력  H 안내",
                  ink,
                  .78f);

        Panel(24, 676, 740, 103);
        Box(43, 692, 330, 16, C(.2f, .11f, .12f));
        Box(43, 692, 330.f * stats.health / stats.MaxHealth(), 16, C(.65f, .24f, .19f));
        Box(43, 716, 330, 8, C(.12f, .16f, .18f));
        Box(43,
            716,
            330.f * stats.experience / stats.NextLevelExperience(),
            8,
            C(.74f, .61f, .32f));
        text.Draw(391,
                  682,
                  L"HP " + std::to_wstring(stats.health) + L" / " +
                      std::to_wstring(stats.MaxHealth()),
                  ink,
                  .85f);
        text.Draw(391,
                  714,
                  L"Lv." + std::to_wstring(stats.level) + L"  XP " +
                      std::to_wstring(stats.experience) + L" / " +
                      std::to_wstring(stats.NextLevelExperience()),
                  gold,
                  .85f);
        text.Draw(43,
                  741,
                  L"회복약 " + std::to_wstring(stats.potions) + L"   금화 " +
                      std::to_wstring(stats.gold) + L"   능력 포인트 " +
                      std::to_wstring(stats.abilityPoints) + L"   처치 " + std::to_wstring(kills),
                  ink,
                  .85f);

        // Full-map overview: enemy locations stay discoverable even behind terrain.
        Panel(1000, 20, 253, 236);
        text.Draw(1014, 29, L"사냥터 · 금색: 야영지", gold, .78f);
        renderer.DrawMinimap(1019, 65, LevelOneMap::Width * 5.f, LevelOneMap::Height * 4.5f);

        auto dot = [&](Point p, Color color)
        {
            Box(1017 + p.x * 5, 63 + p.y * 4.5f, 5, 5, color);
        };
        for (Actor::Id id : enemies)
        {
            auto* enemyActor = static_cast<Enemy*>(scene.Find(id));
            if (!enemyActor)
            {
                continue;
            }

            Enemy& enemy = *enemyActor;
            if (enemy.IsVisible() && enemy.health > 0)
            {
                dot(enemy.Position(), C(.8f, .33f, .22f));
            }
        }

        dot(LevelOneMap::Camp(), gold);
        dot(player.Position(), ink);
        text.Draw(1012, 228, L"시드 " + std::to_wstring(map.Seed()), ink, .62f);

        if (noticeTimer > 0)
        {
            Panel(180, 597, 950, 49);
            text.Draw(200, 608, notice, gold, .85f);
        }
        else if (Distance(player.Position(), LevelOneMap::Camp()) < 2.5f && !help && !statsOpen &&
                 !dead)
        {
            text.Draw(410, 613, L"E 휴식 / 주변 획득   N 새 사냥터 (능력·소지품 유지)", gold, .8f);
        }

        if (statsOpen)
        {
            Panel(300, 204, 690, 355);
            text.Draw(324,
                      220,
                      L"능력 수치 · 남은 포인트 " + std::to_wstring(stats.abilityPoints),
                      gold,
                      1.1f);
            text.Draw(324,
                      274,
                      L"[1] 힘 " + std::to_wstring(stats.strength) + L"     +1당 공격력 +2",
                      ink);
            text.Draw(324,
                      314,
                      L"[2] 체력 " + std::to_wstring(stats.vitality) +
                          L"  +1당 최대 HP +10 / 2당 방어 +1",
                      ink);
            text.Draw(324,
                      354,
                      L"[3] 민첩 " + std::to_wstring(stats.agility) +
                          L"  +1당 공격 간격 0.025초 감소",
                      ink);
            text.Draw(324,
                      407,
                      L"공격 " + std::to_wstring(stats.Attack()) + L"   방어 " +
                          std::to_wstring(stats.Defense()) + L"   최대 HP " +
                          std::to_wstring(stats.MaxHealth()),
                      gold);
            text.Draw(
                324, 451, L"레벨 상승: 최대 HP +10 / 공격 +2 / 방어 +1 / 능력 포인트 +3", ink, .8f);
            text.Draw(324,
                      501,
                      L"1~3으로 배분, C / ESC로 닫기 · 능력 창에서는 전투가 멈춥니다",
                      ink,
                      .8f);
        }

        if (help)
        {
            Panel(238, 202, 804, 371);
            text.Draw(266, 222, L"첫 사냥 · 살아남으며 강해지기", gold, 1.15f);
            text.Draw(266, 282, L"붉은 점으로 표시된 오염된 늑대와 멧돼지를 사냥하세요.", ink);
            text.Draw(
                266, 324, L"SPACE / 좌클릭: 가까운 적 공격. 붉은 공격 예고를 보고 피하세요.", ink);
            text.Draw(
                266, 366, L"E로 전리품을 줍고 Q로 회복합니다. 야영지는 안전한 휴식처입니다.", ink);
            text.Draw(
                266, 408, L"경험치로 레벨 3에 도달하고 C 창에서 포인트 6개를 배분하세요.", ink);
            text.Draw(
                266, 465, L"맵은 매번 달라집니다. 모든 보행 구역은 하나로 연결됩니다.", ink, .9f);
            text.Draw(266, 520, L"H / E / ESC로 시작 · 진행은 현재 실행 중 유지됩니다", gold, .9f);
        }

        if (dead)
        {
            Panel(350, 290, 580, 183);
            text.Draw(378, 314, L"사냥에 실패했습니다", gold, 1.2f);
            text.Draw(378, 361, L"R: 야영지에서 재시도 (경험치·능력 유지)", ink);
            text.Draw(378, 411, L"세계관 속 부활이 아닌 플레이 재시도입니다. ESC 종료", ink, .8f);
        }
    }

    void DrawEnemy(const Enemy& enemy, const SceneView& view)
    {
        Point p = view.Screen(enemy);
        renderer.DrawCachedModel(p,
                                 enemy.species == 0 ? RenderAssets::Wolf : RenderAssets::Boar,
                                 static_cast<int>(enemy.phase) % 4,
                                 enemy.facing,
                                 enemy.hurtFlash > 0 ? .55f : 1.f);
        Box(p.x - 22, p.y - 51, 44, 4, C(.17f, .07f, .07f));
        Box(p.x - 22, p.y - 51, 44.f * enemy.health / enemy.MaxHealth(), 4, C(.73f, .29f, .19f));
        if (enemy.windup > 0)
        {
            Ring(p, 40, 20, C(1, .25f, .12f, .8f));
        }
    }

    void Render()
    {
        scene.Render({renderer, camera, time});
    }
};

LevelOneGame::LevelOneGame() : m(new Impl)
{
}

LevelOneGame::~LevelOneGame() = default;

void LevelOneGame::Resize(int width, int height)
{
    m->renderer.Resize(width, height);
}

void LevelOneGame::Update(float dt)
{
    m->Update(std::max(0.f, std::min(.05f, dt)));
}

void LevelOneGame::Render()
{
    m->Render();
}

void LevelOneGame::ClearKeys()
{
    m->ClearKeys();
}

bool LevelOneGame::WantsQuit() const
{
    return m->quit;
}

void LevelOneGame::KeyUp(unsigned char key)
{
    if (key >= 'A' && key <= 'Z')
    {
        key = key - 'A' + 'a';
    }

    m->keys[key] = false;
}

void LevelOneGame::KeyDown(unsigned char key)
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

    if (m->dead)
    {
        if (key == 'r')
        {
            m->dead = false;
            m->stats.health = m->stats.MaxHealth();
            m->player.SetPosition(LevelOneMap::Camp());
            m->camera = WorldGeometry::Project(m->player.Position());
            m->hurtTimer = 1.5f;
            m->ClearKeys();
        }

        if (key == 27)
        {
            m->quit = true;
        }

        return;
    }

    if (m->help)
    {
        if (key == 'h' || key == 'e' || key == 27)
        {
            m->help = false;
            m->ClearKeys();
        }

        return;
    }

    if (key == 'c' || (m->statsOpen && key == 27))
    {
        m->statsOpen = !m->statsOpen;
        m->ClearKeys();
        return;
    }

    if (m->statsOpen)
    {
        if (key >= '1' && key <= '3' && m->stats.Allocate(key - '1'))
        {
            if (!m->complete && m->stats.level >= 3 && m->stats.spentPoints >= 6)
            {
                m->complete = true;
                m->Message(L"레벨 1 목표 완료! 성장과 능력 배분을 익혔습니다.");
            }
        }

        return;
    }

    if (key == 'h')
    {
        m->help = true;
        m->ClearKeys();
    }

    if (key == ' ')
    {
        m->Attack();
    }

    if (key == 'e')
    {
        m->Interact();
    }

    if (key == 'q')
    {
        m->UsePotion();
    }

    if (key == 27)
    {
        m->quit = true;
    }

    if (key == 'n' && Distance(m->player.Position(), LevelOneMap::Camp()) < 2.5f)
    {
        m->map = LevelOneMap(NewSeed());
        m->Populate();
        m->stats.health = m->stats.MaxHealth();
        m->Message(L"새 사냥터를 생성했습니다. 경험치·능력·소지품은 유지됩니다.");
    }
}
