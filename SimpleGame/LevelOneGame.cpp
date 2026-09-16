#include "stdafx.h"
#include "LevelOneGame.h"
#include "LevelOneMap.h"
#include "RpgStats.h"
#include "UiText.h"
#include "RenderAssets.h"
#include "WorldGeometry.h"
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
        glColor4f(color.r, color.g, color.b, color.a);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + width, y);
        glVertex2f(x + width, y + height);
        glVertex2f(x, y + height);
        glEnd();
    }

    void Ring(Point p, float rx, float ry, Color color)
    {
        glColor4f(color.r, color.g, color.b, color.a);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 48; ++i)
        {
            float angle = i * WorldGeometry::Pi / 24.f;
            glVertex2f(p.x + std::cos(angle) * rx, p.y + std::sin(angle) * ry);
        }

        glEnd();
    }

    struct Enemy
    {
        Point p, home, waypoint;
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

    struct Loot
    {
        Point p;
        bool potion;
        int amount;
    };
} // namespace

struct LevelOneGame::Impl
{
    Renderer renderer{1280, 800};
    UiText text;
    LevelOneMap map{NewSeed()};
    RpgStats stats;
    Point player = LevelOneMap::Camp();
    Point camera = WorldGeometry::Project(player);
    std::vector<Enemy> enemies;
    std::vector<Loot> loot;
    bool keys[256] = {};
    bool quit = false, statsOpen = false, help = true, dead = false, complete = false;
    float time = 0, stride = 0, attackTimer = 0, slashTimer = 0, hurtTimer = 0;
    float slashAngle = 0, noticeTimer = 0;
    int direction = 0, kills = 0;
    bool moving = false;
    std::wstring notice;

    Impl()
    {
        Populate();
    }

    Point Screen(Point world) const
    {
        Point p = WorldGeometry::Project(world);
        return {p.x - camera.x + 640, p.y - camera.y + 420};
    }

    void Message(const std::wstring& value)
    {
        notice = value;
        noticeTimer = 4.f;
    }

    void Populate()
    {
        enemies.clear();
        loot.clear();
        const auto& positions = map.SpawnCells();
        for (size_t i = 0; i < 14 && i < positions.size(); ++i)
        {
            Enemy enemy;
            enemy.p = enemy.home = enemy.waypoint = positions[i];
            enemy.species = static_cast<int>(i % 2);
            enemy.health = enemy.MaxHealth();
            enemies.push_back(enemy);
        }

        Point camp = LevelOneMap::Camp();
        loot.push_back({{camp.x + 1.5f, camp.y}, true, 1});
        player = camp;
        camera = WorldGeometry::Project(player);
        ClearKeys();
    }

    void ClearKeys()
    {
        std::fill(keys, keys + 256, false);
    }

    bool Move(Point& position, Point desired)
    {
        const Point previous = position;
        if (map.Walkable({desired.x, position.y}))
        {
            position.x = desired.x;
        }

        if (map.Walkable({position.x, desired.y}))
        {
            position.y = desired.y;
        }

        return Distance(position, previous) > .0001f;
    }

    void Attack()
    {
        if (help || statsOpen || dead || attackTimer > 0)
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
        for (Enemy& enemy : enemies)
        {
            float distance = Distance(player, enemy.p);
            if (enemy.health > 0 && distance < nearestDistance && map.ClearSegment(player, enemy.p))
            {
                nearestDistance = distance;
                target = &enemy;
            }
        }

        if (!target)
        {
            return;
        }

        Point delta = WorldGeometry::Project({target->p.x - player.x, target->p.y - player.y});
        slashAngle = std::atan2(delta.y, delta.x);
        direction =
            std::abs(delta.x) > std::abs(delta.y) ? (delta.x > 0 ? 1 : 3) : (delta.y > 0 ? 0 : 2);
        target->health = std::max(0, target->health - stats.Attack());
        target->hurtFlash = .18f;

        if (target->health == 0)
        {
            ++kills;
            target->windup = 0;
            target->respawnTimer = 18.f;
            const int gainedXp = target->species == 0 ? 25 : 40;
            int gainedLevels = stats.GainExperience(gainedXp);
            loot.push_back({target->p, false, target->species == 0 ? 4 : 7});
            if (kills % 3 == 0)
            {
                loot.push_back({target->p, true, 1});
            }

            Message(gainedLevels > 0
                        ? L"레벨 상승! 최대 체력·공격력·방어력 증가 / C를 눌러 능력 포인트 배분"
                        : L"처치! 경험치 +" + std::to_wstring(gainedXp) + L" / E로 전리품 획득");
        }
    }

    void Interact()
    {
        if (help || statsOpen || dead)
        {
            return;
        }

        int found = 0;
        for (auto it = loot.begin(); it != loot.end();)
        {
            if (Distance(player, it->p) < 1.5f && map.ClearSegment(player, it->p))
            {
                if (it->potion)
                {
                    stats.potions += it->amount;
                }
                else
                {
                    stats.gold += it->amount;
                }

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
        else if (Distance(player, LevelOneMap::Camp()) < 2.5f)
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
        if (dead || help || statsOpen || stats.potions == 0 || stats.health == stats.MaxHealth())
        {
            return;
        }
        --stats.potions;
        stats.health = std::min(stats.MaxHealth(), stats.health + 45);
        Message(L"회복약 사용: 체력 +45");
    }

    void UpdateEnemies(float dt)
    {
        for (Enemy& enemy : enemies)
        {
            enemy.hurtFlash = std::max(0.f, enemy.hurtFlash - dt);
            if (enemy.health == 0)
            {
                enemy.respawnTimer -= dt;
                if (enemy.respawnTimer <= 0 && Distance(player, enemy.home) > 5.f)
                {
                    enemy.p = enemy.home;
                    enemy.health = enemy.MaxHealth();
                    enemy.cooldown = 1.f;
                    enemy.pathTimer = 0;
                }

                continue;
            }

            enemy.cooldown = std::max(0.f, enemy.cooldown - dt);
            float distance = Distance(player, enemy.p);
            bool chase = distance < 8.f && Distance(player, LevelOneMap::Camp()) > 3.f;

            if (enemy.windup > 0)
            {
                enemy.windup = std::max(0.f, enemy.windup - dt);
                if (enemy.windup == 0)
                {
                    if (chase && distance < 1.35f && map.ClearSegment(enemy.p, player) &&
                        hurtTimer <= 0)
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

                continue;
            }

            if (chase && distance < 1.15f && map.ClearSegment(enemy.p, player))
            {
                if (enemy.cooldown <= 0)
                {
                    enemy.windup = .45f;
                }

                continue;
            }

            Point destination = chase ? player : enemy.home;
            if (Distance(enemy.p, destination) < .15f)
            {
                continue;
            }

            enemy.pathTimer -= dt;
            if (enemy.pathTimer <= 0 || Distance(enemy.p, enemy.waypoint) < .12f)
            {
                enemy.waypoint = map.NextStep(enemy.p, destination);
                enemy.pathTimer = .4f;
            }

            float length = Distance(enemy.p, enemy.waypoint);
            if (length > .01f)
            {
                float step = std::min(length, dt * (enemy.species == 0 ? 1.8f : 1.35f));
                Point previous = enemy.p;
                Point desired = {enemy.p.x + (enemy.waypoint.x - enemy.p.x) / length * step,
                                 enemy.p.y + (enemy.waypoint.y - enemy.p.y) / length * step};
                if (Move(enemy.p, desired))
                {
                    enemy.phase += dt * 9;
                    Point delta =
                        WorldGeometry::Project({enemy.p.x - previous.x, enemy.p.y - previous.y});
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
    }

    void Update(float dt)
    {
        time += dt;
        noticeTimer = std::max(0.f, noticeTimer - dt);
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
            Point next = {player.x + (sx / 84 + sy / 42) * 145 * dt,
                          player.y + (-sx / 84 + sy / 42) * 145 * dt};
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

        UpdateEnemies(dt);
        Point target = WorldGeometry::Project(player);
        float smoothing = 1 - std::exp(-dt * 8);
        camera.x += (target.x - camera.x) * smoothing;
        camera.y += (target.y - camera.y) * smoothing;
    }

    void Panel(float x, float y, float width, float height)
    {
        Box(x, y, width, height, C(.025f, .05f, .057f, .96f));
        Box(x, y, 3, height, C(.78f, .64f, .36f));
    }

    void Hud()
    {
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
        for (int y = 0; y < LevelOneMap::Height; ++y)
        {
            for (int x = 0; x < LevelOneMap::Width; ++x)
            {
                Color color = map.At(x, y) == LevelOneMap::Tile::Ground  ? C(.20f, .28f, .23f)
                              : map.At(x, y) == LevelOneMap::Tile::Water ? C(.12f, .31f, .37f)
                                                                         : C(.09f, .14f, .12f);
                Box(1019 + x * 5.f, 65 + y * 4.5f, 5, 4.5f, color);
            }
        }

        auto dot = [&](Point p, Color color) { Box(1017 + p.x * 5, 63 + p.y * 4.5f, 5, 5, color); };
        for (const Enemy& enemy : enemies)
        {
            if (enemy.health > 0)
            {
                dot(enemy.p, C(.8f, .33f, .22f));
            }
        }

        dot(LevelOneMap::Camp(), gold);
        dot(player, ink);
        text.Draw(1012, 228, L"시드 " + std::to_wstring(map.Seed()), ink, .62f);

        if (noticeTimer > 0)
        {
            Panel(180, 597, 950, 49);
            text.Draw(200, 608, notice, gold, .85f);
        }
        else if (Distance(player, LevelOneMap::Camp()) < 2.5f && !help && !statsOpen && !dead)
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

    void Render()
    {
        renderer.BeginScene(camera, time);
        renderer.DrawHuntingTerrain(map);

        struct DrawEntry
        {
            Point p;
            int kind;
            int index;
        };

        std::vector<DrawEntry> entries;
        for (int y = 0; y < LevelOneMap::Height; ++y)
        {
            for (int x = 0; x < LevelOneMap::Width; ++x)
            {
                auto tile = map.At(x, y);
                if (tile != LevelOneMap::Tile::Tree && tile != LevelOneMap::Tile::Rock)
                {
                    continue;
                }

                Point world = {x + .5f, y + .5f}, p = Screen(world);
                if (p.x < -150 || p.x > 1430 || p.y < -100 || p.y > 980)
                {
                    continue;
                }

                renderer.DrawShadow(p, tile == LevelOneMap::Tile::Tree ? 20.f : 24.f, 50);
                entries.push_back({world,
                                   0,
                                   tile == LevelOneMap::Tile::Tree ? RenderAssets::ForestTree
                                                                   : RenderAssets::Boulder});
            }
        }

        for (size_t i = 0; i < enemies.size(); ++i)
        {
            if (enemies[i].health <= 0)
            {
                continue;
            }

            renderer.DrawShadow(Screen(enemies[i].p), 20, 24);
            entries.push_back({enemies[i].p, 1, static_cast<int>(i)});
        }

        for (size_t i = 0; i < loot.size(); ++i)
        {
            entries.push_back({loot[i].p, 2, static_cast<int>(i)});
        }

        renderer.DrawShadow(Screen(player), 13, 50);
        entries.push_back({player, 3, 0});
        Point camp = LevelOneMap::Camp();
        entries.push_back({{camp.x - 1.2f, camp.y - .8f}, 4, 0});
        std::stable_sort(entries.begin(),
                         entries.end(),
                         [](const DrawEntry& a, const DrawEntry& b)
                         { return a.p.x + a.p.y < b.p.x + b.p.y; });

        for (const DrawEntry& entry : entries)
        {
            Point p = Screen(entry.p);
            if (p.x < -150 || p.x > 1430 || p.y < -100 || p.y > 980)
            {
                continue;
            }

            if (entry.kind == 0)
            {
                float alpha =
                    entry.index == RenderAssets::ForestTree && Distance(player, entry.p) < 2.f
                        ? .4f
                        : 1.f;
                renderer.DrawCachedModel(p, entry.index, 0, 1, alpha);
            }
            else if (entry.kind == 1)
            {
                const Enemy& enemy = enemies[entry.index];
                renderer.DrawCachedModel(p,
                                         enemy.species == 0 ? RenderAssets::Wolf
                                                            : RenderAssets::Boar,
                                         static_cast<int>(enemy.phase) % 4,
                                         enemy.facing,
                                         enemy.hurtFlash > 0 ? .55f : 1.f);
                Box(p.x - 22, p.y - 51, 44, 4, C(.17f, .07f, .07f));
                Box(p.x - 22,
                    p.y - 51,
                    44.f * enemy.health / enemy.MaxHealth(),
                    4,
                    C(.73f, .29f, .19f));
                if (enemy.windup > 0)
                {
                    Ring(p, 40, 20, C(1, .25f, .12f, .8f));
                }
            }
            else if (entry.kind == 2)
            {
                const Loot& item = loot[entry.index];
                renderer.DrawCachedModel(
                    p, item.potion ? RenderAssets::Potion : RenderAssets::Coins, 0, 1);
                if (Distance(player, item.p) < 1.5f)
                {
                    Ring(p, 13, 7, C(.92f, .78f, .35f));
                }
            }
            else if (entry.kind == 3)
            {
                if (hurtTimer <= 0 || static_cast<int>(time * 18) % 2 == 0)
                {
                    renderer.DrawCharacter(
                        p, 7, direction, moving ? 1 + static_cast<int>(stride) % 7 : 0, true);
                }
            }
            else
            {
                renderer.DrawCachedModel(p, RenderAssets::Campfire, 0, 1);
                renderer.DrawFire({p.x, p.y - 10}, 1.f);
            }
        }

        if (slashTimer > 0)
        {
            Point p = Screen(player);
            glColor4f(.9f, .88f, .63f, slashTimer / .2f);
            glBegin(GL_LINE_STRIP);
            for (int i = 0; i <= 20; ++i)
            {
                float angle = slashAngle - 1 + i * .1f;
                glVertex2f(p.x + std::cos(angle) * 60, p.y - 20 + std::sin(angle) * 35);
            }

            glEnd();
        }

        renderer.Present();
        Hud();
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
            m->player = LevelOneMap::Camp();
            m->camera = WorldGeometry::Project(m->player);
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

    if (key == 'n' && Distance(m->player, LevelOneMap::Camp()) < 2.5f)
    {
        m->map = LevelOneMap(NewSeed());
        m->Populate();
        m->stats.health = m->stats.MaxHealth();
        m->Message(L"새 사냥터를 생성했습니다. 경험치·능력·소지품은 유지됩니다.");
    }
}
