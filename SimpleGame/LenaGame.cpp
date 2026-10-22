#include "stdafx.h"
#include "LenaGame.h"
#include "LenaWorld.h"
#include "ArcanaWorld.h"
#include "UiText.h"
#include "RenderAssets.h"
#include "WorldGeometry.h"
#include "ResourceSprites.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <iterator>

namespace
{
    using Point = RenderPoint;
    using Color = RenderColor;

    Color C(float r, float g, float b, float a = 1)
    {
        return {r, g, b, a};
    }

    void Box(float x, float y, float w, float h, Color color)
    {
        RenderQueue::EnableTexture(false);
        RenderQueue::Color(color.r, color.g, color.b, color.a);
        RenderQueue::Begin(GL_QUADS);
        RenderQueue::Vertex(x, y);
        RenderQueue::Vertex(x + w, y);
        RenderQueue::Vertex(x + w, y + h);
        RenderQueue::Vertex(x, y + h);
        RenderQueue::End();
    }

    std::wstring SavePath(bool arcanaMode)
    {
        wchar_t root[MAX_PATH];
        DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", root, MAX_PATH);
        if (!count || count >= MAX_PATH)
        {
            return arcanaMode ? L"arcana-save-v2.txt" : L"lena-save-v1.txt";
        }
        std::wstring directory = std::wstring(root) + L"\\SimpleGame";
        CreateDirectoryW(directory.c_str(), nullptr);
        return directory + (arcanaMode ? L"\\arcana-save-v2.txt" : L"\\lena-save-v1.txt");
    }
} // namespace

struct LenaGame::Impl
{
    Renderer renderer{1280, 800};
    UiText text;
    Lena::World world;
    ResourceSprites resources;
    bool keys[256]{};
    bool help = true, quit = false;
    float time = 0;
    Point camera = WorldGeometry::Project(world.canal);
    GLuint grass = 0, dirt = 0, stone = 0;
    std::vector<Point> lake;

    struct GroundPatch
    {
        std::array<Point, 4> corners;
        GLuint texture;
        Color color;
    };

    struct MapVertex
    {
        Point position;
        Color color;
    };

    std::vector<GroundPatch> groundPatches;
    std::vector<MapVertex> minimapVertices;
    std::wstring savePath;
    WorldMonitor::Snapshot monitorSnapshot;
    size_t monitorCursor = 0;
    bool monitorReady = false;

    void PrepareMonitor()
    {
        if (!WorldMonitor::Enabled() || monitorReady)
        {
            return;
        }
        Profiler::Scope scope("monitor.capture_slice");
        auto part = world.Capture(help, monitorCursor, 16, false);
        for (auto& entity : part.entities)
        {
            monitorSnapshot.entities.push_back(std::move(entity));
        }
        monitorCursor += 16;
        if (monitorCursor >= world.people.size())
        {
            auto summary = world.Capture(help, world.people.size(), 0, true);
            summary.entities = std::move(monitorSnapshot.entities);
            monitorSnapshot = std::move(summary);
            monitorReady = true;
        }
    }

    explicit Impl(bool arcanaMode)
        : renderer(1280, 800, !arcanaMode), world(arcanaMode), savePath(SavePath(arcanaMode))
    {
        Profiler::Scope scope("lena.scene_setup");
        if (arcanaMode)
        {
            resources.Load();
            grass = resources.Ground(0);
            dirt = resources.Ground(1);
            stone = resources.Ground(2);
        }
        else
        {
            renderer.PrepareVillageAtlas();
            grass = RenderAssets::MakeMaterial(RenderAssets::Grass);
            dirt = RenderAssets::MakeMaterial(RenderAssets::Dirt);
            stone = RenderAssets::MakeMaterial(RenderAssets::Stone);
        }
        for (int i = 0; i < 64; ++i)
        {
            float angle = i * WorldGeometry::Pi / 32;
            float r = std::sqrt(1 + .12f * std::sin(angle * 3));
            lake.push_back({world.lakeCenter.x + 5 * r * std::cos(angle),
                            world.lakeCenter.y + 4 * r * std::sin(angle)});
        }
        BuildGround();
        BuildMinimap();
        auto& terrain = world.scene.Spawn(world.scene.Root(), "LenaTerrain");
        terrain.layer = ActorLayer::Terrain;
        terrain.cull = false;
        terrain.onDraw = [this](const Actor&, const SceneView& view)
        {
            DrawTerrain(view);
        };
        auto& simulation = world.scene.Spawn(world.scene.Root(), "LenaSimulation");
        simulation.SetVisible(false);
        simulation.onUpdate = [this](Actor&, float dt)
        {
            world.Update(dt);
        };
        int variant = 0;
        for (auto position : world.houses)
        {
            auto& house = world.scene.Spawn(world.scene.Root(), "LenaHouse");
            house.SetPosition(position);
            const int palette = variant++ % 8;
            house.onDraw = [this, palette](const Actor& actor, const SceneView& view)
            {
                if (world.arcana)
                {
                    resources.Draw("prefab_cottage", view.Screen(actor), 28.f);
                }
                else
                {
                    renderer.DrawHouse(view.Screen(actor), palette, .94f);
                }
            };
            house.onShadow = [this](const Actor& actor, const SceneView& view)
            {
                renderer.DrawShadow(view.Screen(actor), 60, 145);
            };
        }
        for (auto position : world.trees)
        {
            auto& tree = world.scene.Spawn(world.scene.Root(), "LenaTree");
            tree.SetPosition(position);
            const int palette = variant++ % 3;
            tree.onDraw = [this, palette](const Actor& actor, const SceneView& view)
            {
                if (world.arcana)
                {
                    const char* models[] = {"pine_tall", "oak_mature", "spruce_young"};
                    resources.Draw(models[palette], view.Screen(actor), 30.f);
                }
                else
                {
                    renderer.DrawTree(view.Screen(actor), palette, .85f);
                }
            };
            tree.onShadow = [this](const Actor& actor, const SceneView& view)
            {
                renderer.DrawShadow(view.Screen(actor), 25, 100);
            };
        }
        for (auto id : world.people)
        {
            auto* person = world.Find(id);
            person->onDraw = [this](const Actor& actor, const SceneView& view)
            {
                DrawPerson(static_cast<const Lena::Person&>(actor), view);
            };
            person->onShadow = [this](const Actor& actor, const SceneView& view)
            {
                renderer.DrawShadow(view.Screen(actor), 15, 12);
            };
        }
        if (world.arcana)
        {
            auto& ruin = world.scene.Spawn(world.scene.Root(), "ArcanaBurnedHouse");
            ruin.SetPosition(world.canal);
            ruin.onDraw = [this](const Actor& actor, const SceneView& view)
            {
                auto p = view.Screen(actor);
                resources.Draw("foundation_2x2", p, 44.f, 1.f, true);
                for (int i = 0; i < 5; ++i)
                {
                    resources.Draw("post_2p5m", {p.x - 58 + i * 27.f, p.y - 8}, 23.f, 1.f, true);
                }
                if (world.arcana->State().step < 2)
                {
                    resources.Draw("chest_rigged", {p.x + 18, p.y - 10}, 27.f);
                }
            };
            PlaceResources();
            Landmark(world.canal, L"북부 숲 / 재가 된 집 [E]", true);
            Landmark(world.arcana->forge, L"벨하임 / 브란의 대장간 [E·F]", true);
            Landmark(world.arcana->outpost, L"기사단 초소 / 은빛 문장 [E]", false);
            Landmark(world.arcana->territory, L"흑아 멧돼지의 영역", false);
            for (const auto& village : Arcana::Villages())
            {
                Landmark(village.center, village.name, false);
                Landmark({village.center.x + 1, village.center.y - 2}, L"우물 / E 음수", false);
            }
            auto& effect = world.scene.Spawn(world.scene.Root(), "ArcanaFireSpell");
            effect.layer = ActorLayer::Effect;
            effect.cull = false;
            effect.onDraw = [this](const Actor&, const SceneView& view)
            {
                if (world.arcana->fireSeconds > 0)
                {
                    renderer.DrawFire(view.Screen(world.arcana->firePosition), 1.2f, false);
                }
            };
        }
        else
        {
            Landmark(world.canal, L"아르카 수로 / 깨어난 곳", true);
            Landmark(world.board, L"레나 / 창고 게시판", false);
            Landmark(world.forest, L"사슴 숲 / 먹이 이동 통로", false);
            Landmark({22, 20}, L"늑대 능선", false);
        }
        Landmark(world.arcana ? Point{world.lakeCenter.x - 6, world.lakeCenter.y} : Point{8, -7},
                 L"수원 / E 음수",
                 false);
        auto& hud = world.scene.Spawn(world.scene.Root(), "LenaHUD");
        hud.layer = ActorLayer::Interface;
        hud.cull = false;
        hud.onDraw = [this](const Actor&, const SceneView&)
        {
            Hud();
        };
    }

    ~Impl()
    {
        RenderQueue::Flush();
        if (!world.arcana)
        {
            for (GLuint texture : {grass, dirt, stone})
            {
                glDeleteTextures(1, &texture);
            }
        }
    }

    void Prop(const std::string& model, Point position, float scale = 42.f)
    {
        auto& actor = world.scene.Spawn(world.scene.Root(), "ArcanaResourceProp");
        actor.SetPosition(position);
        actor.onDraw = [this, model, scale](const Actor& object, const SceneView& view)
        {
            resources.Draw(model, view.Screen(object), scale);
        };
    }

    void PlaceResources()
    {
        Profiler::Scope scope("resource.place_props");
        for (const auto& village : Arcana::Villages())
        {
            const auto c = village.center;
            // The well occupies the existing drinking interaction, not a new random marker.
            Prop("well", {c.x + 1, c.y - 2}, 35.f);
            Prop("table", {c.x + 3.2f, c.y + 2.2f}, 34.f);
            Prop("bench", {c.x + 3.2f, c.y + 3}, 34.f);
            Prop("crate_open", {c.x + 4.2f, c.y + 1.5f}, 36.f);
            Prop("sack", {c.x + 4.5f, c.y + 2.2f}, 36.f);
            Prop("barrel", {c.x - 3.1f, c.y + 1.1f}, 35.f);
            Prop("bucket", {c.x + 1.6f, c.y - 2.4f}, 36.f);
        }
        const auto forge = world.arcana->forge;
        Prop("anvil", forge, 45.f);
        Prop("hammer", {forge.x + .4f, forge.y + .1f}, 42.f);
        Prop("firewood_stack", {forge.x - 1.4f, forge.y + .4f}, 38.f);
        Prop("barrel", {forge.x + 1.5f, forge.y + .2f}, 38.f);
        size_t index = 0;
        for (auto house : world.houses)
        {
            Prop(index++ % 2 ? "lantern" : "clay_pot", {house.x + 1.5f, house.y + 1.4f}, 34.f);
        }
        index = 0;
        for (auto tree : world.trees)
        {
            // Reuse clear ground near existing vegetation; never cover a path or water.
            Point p{tree.x + 1.1f, tree.y + .6f};
            if (++index % 3 != 0 || !world.Walkable(p) || world.arcana->Location(p, 12) >= 0)
            {
                continue;
            }
            const char* plants[] = {"fern", "shrub", "grass_clump_01", "mushrooms", "rock_01"};
            Prop(plants[(index / 3) % 5], p, 38.f);
        }
        for (int i = 0; i < 18; ++i)
        {
            float angle = i * WorldGeometry::Pi / 9;
            Point p{world.lakeCenter.x + 5.8f * std::cos(angle),
                    world.lakeCenter.y + 4.8f * std::sin(angle)};
            if (world.Walkable(p))
            {
                Prop(i % 3 ? "reeds" : "rock_02", p, 38.f);
            }
        }
    }

    void Landmark(Point position, const std::wstring& name, bool mystical)
    {
        auto& actor = world.scene.Spawn(world.scene.Root(), "LenaLandmark");
        actor.SetPosition(position);
        actor.onDraw = [this, name, mystical](const Actor& object, const SceneView& view)
        {
            auto p = view.Screen(object);
            Box(p.x - 10, p.y - 36, 20, 36, C(.27f, .28f, .25f));
            if (mystical)
            {
                renderer.DrawFire({p.x, p.y - 30}, .6f, true);
            }
            text.Draw(p.x - 100, p.y - 78, name, C(.79f, .78f, .59f), .72f);
        };
    }

    void Ground(
        const SceneView& view, Point a, Point b, Point c, Point d, GLuint texture, Color color)
    {
        RenderQueue::EnableTexture(true);
        RenderQueue::Texture(texture);
        RenderQueue::Color(color.r, color.g, color.b, color.a);
        RenderQueue::Begin(GL_QUADS);
        for (Point p : {a, b, c, d})
        {
            auto screen = view.Screen(p);
            RenderQueue::TexCoord(p.x * .7f, p.y * .7f);
            RenderQueue::Vertex(screen.x, screen.y);
        }
        RenderQueue::End();
        RenderQueue::EnableTexture(false);
    }

    void BuildGround()
    {
        auto ground = [&](Point a, Point b, Point c, Point d, GLuint texture, Color color)
        {
            groundPatches.push_back({{a, b, c, d}, texture, color});
        };
        float extent = world.extent;
        ground({-extent, -extent},
               {extent, -extent},
               {extent, extent},
               {-extent, extent},
               grass,
               C(.52f, .59f, .49f));
        ground({-13, 9}, {-6, 9}, {-6, 20}, {-13, 20}, dirt, C(.63f, .57f, .40f));
        // Curved continuous paths, not square walk tiles. Collision stays in world coordinates.
        for (int branch = 0; branch < (world.arcana ? 6 : 3); ++branch)
        {
            Point start = branch == 0 ? world.canal : world.square;
            Point end = branch == 0 ? world.square : branch == 1 ? world.forest : Point{-10, 18};
            if (world.arcana && branch > 0)
            {
                end = branch <= 4 ? Arcana::Villages()[branch].center : world.arcana->forge;
            }
            Point previous = start;
            for (int i = 1; i <= 28; ++i)
            {
                float t = i / 28.f;
                Point next{start.x + (end.x - start.x) * t +
                               std::sin(t * WorldGeometry::Pi) * (world.arcana ? .4f : 2.f),
                           start.y + (end.y - start.y) * t};
                float length = std::max(.01f, Lena::Distance(previous, next));
                Point normal{-(next.y - previous.y) / length * .9f,
                             (next.x - previous.x) / length * .9f};
                ground({previous.x + normal.x, previous.y + normal.y},
                       {next.x + normal.x, next.y + normal.y},
                       {next.x - normal.x, next.y - normal.y},
                       {previous.x - normal.x, previous.y - normal.y},
                       dirt,
                       C(.62f, .60f, .51f));
                previous = next;
            }
        }
        ground({-20, -21}, {-13, -21}, {-13, -14}, {-20, -14}, stone, C(.51f, .59f, .58f));
        ground({-3, 1}, {3, 1}, {3, 7}, {-3, 7}, stone, C(.67f, .62f, .51f));
        if (world.arcana)
        {
            for (const auto& village : Arcana::Villages())
            {
                const auto p = village.center;
                ground({p.x - 3, p.y - 3},
                       {p.x + 3, p.y - 3},
                       {p.x + 3, p.y + 3},
                       {p.x - 3, p.y + 3},
                       stone,
                       C(.63f, .59f, .48f));
            }
        }
    }

    void DrawTerrain(const SceneView& view)
    {
        Profiler::Scope scope("lena.terrain_submit");
        for (const auto& patch : groundPatches)
        {
            Ground(view,
                   patch.corners[0],
                   patch.corners[1],
                   patch.corners[2],
                   patch.corners[3],
                   patch.texture,
                   patch.color);
        }
        renderer.DrawWaterPolygon(lake);
    }

    Point MapPoint(Point worldPosition) const
    {
        auto projected = WorldGeometry::Project(worldPosition);
        const float scale = 246.f / (world.extent * 168.f);
        return {1125 + projected.x * scale, 135 + projected.y * scale};
    }

    void BuildMinimap()
    {
        Profiler::Scope scope("arcana.minimap_build");
        auto triangle = [&](Point a, Point b, Point c, Color color)
        {
            for (auto p : {a, b, c})
            {
                minimapVertices.push_back({MapPoint(p), color});
            }
        };
        auto quad = [&](Point a, Point b, Point c, Point d, Color color)
        {
            triangle(a, b, c, color);
            triangle(a, c, d, color);
        };
        // Reuse the actual terrain patches and shoreline, in the same drawing order.
        for (const auto& patch : groundPatches)
        {
            quad(patch.corners[0],
                 patch.corners[1],
                 patch.corners[2],
                 patch.corners[3],
                 patch.color);
        }
        for (size_t i = 0; i < lake.size(); ++i)
        {
            triangle(world.lakeCenter, lake[i], lake[(i + 1) % lake.size()], C(.18f, .43f, .63f));
        }
        for (auto p : world.trees)
        {
            quad({p.x - .6f, p.y - .6f},
                 {p.x + .6f, p.y - .6f},
                 {p.x + .6f, p.y + .6f},
                 {p.x - .6f, p.y + .6f},
                 C(.13f, .29f, .18f));
        }
        for (auto p : world.houses)
        {
            quad({p.x - 1.5f, p.y - 1.2f},
                 {p.x + 1.5f, p.y - 1.2f},
                 {p.x + 1.5f, p.y + 1.2f},
                 {p.x - 1.5f, p.y + 1.2f},
                 C(.74f, .50f, .26f));
        }
        Profiler::Count("arcana.minimap_builds");
        Profiler::Gauge("arcana.minimap_cache_bytes",
                        static_cast<double>(minimapVertices.size() * sizeof(MapVertex)));
    }

    void DrawMinimap()
    {
        Profiler::Scope scope("arcana.minimap_submit");
        Box(990, 16, 270, 220, C(.025f, .04f, .05f, .94f));
        text.Draw(1006, 26, L"지형도 · 화면과 같은 방향", C(.88f, .74f, .46f), .7f);
        RenderQueue::EnableTexture(false);
        RenderQueue::Begin(GL_TRIANGLES);
        for (const auto& vertex : minimapVertices)
        {
            const auto& color = vertex.color;
            RenderQueue::Color(color.r, color.g, color.b, color.a);
            RenderQueue::Vertex(vertex.position.x, vertex.position.y);
        }
        RenderQueue::End();
        auto mark = [&](Point position, Color color, float size)
        {
            auto p = MapPoint(position);
            Box(p.x - size * .5f, p.y - size * .5f, size, size, color);
        };
        for (auto id : world.people)
        {
            const auto* p = world.Find(id);
            if (p && !p->state.dead && p->role != Lena::Role::Player &&
                (p->species == Lena::Species::Human || p->beast))
            {
                mark(p->Position(), p->beast ? C(.95f, .2f, .2f) : C(.3f, .85f, .6f), 2);
            }
        }
        mark(world.canal, C(.95f, .4f, .2f), 5);
        mark(world.arcana->forge, C(1, .85f, .3f), 5);
        mark(world.arcana->outpost, C(.7f, .8f, 1), 5);
        for (const auto& village : Arcana::Villages())
        {
            mark(village.center, C(1, .8f, .3f), 4);
            auto p = MapPoint(village.center);
            text.Draw(p.x + 3, p.y - 13, village.name.substr(0, 5), C(1, .92f, .7f), .45f);
        }
        mark(world.Player().Position(), C(0, 0, 0), 8);
        mark(world.Player().Position(), C(1, 1, 1), 5);
        text.Draw(1006, 213, L"흰색: 나 · 파랑: 물 · 갈색: 건물", C(.85f, .87f, .82f), .57f);
    }

    void DrawPerson(const Lena::Person& person, const SceneView& view)
    {
        const auto& s = person.state;
        auto foot = view.Screen(person);
        if (world.arcana)
        {
            const std::string model =
                person.species == Lena::Species::Deer   ? "deer"
                : person.species == Lena::Species::Wolf ? (person.beast ? "boar" : "wolf")
                : person.role == Lena::Role::Player     ? "protagonist"
                : person.role == Lena::Role::Guard || person.role == Lena::Role::Bandit ? "guard"
                : person.key == "bran_01" ? "blacksmith"
                                          : "villager";
            const int direction = WorldGeometry::FacingDirection(person.visualDirection);
            bool attack = s.action == Lena::Action::Attack || s.action == Lena::Action::Kill;
            if (person.role == Lena::Role::Player)
            {
                attack = world.arcana->attackSeconds > 0;
            }
            const char* clip = s.dead                     ? "dead"
                               : attack                   ? "attack"
                               : person.motionSeconds > 0 ? "walk"
                                                          : "idle";
            int frame =
                s.dead ? 0
                : attack && person.role == Lena::Role::Player
                    ? std::min(5, static_cast<int>((1 - world.arcana->attackSeconds / .22f) * 6))
                    : static_cast<int>((person.motionSeconds > 0 ? s.stride : time * 4) +
                                       static_cast<float>(person.GetId() % 6)) %
                          6;
            resources.Animate(model, clip, direction, frame, foot);
        }
        if (s.dead)
        {
            if (!world.arcana)
            {
                Box(foot.x - 17, foot.y - 9, 34, 10, C(.23f, .16f, .14f, .85f));
            }
            if (Lena::Distance(person.Position(), world.Player().Position()) < 3)
            {
                text.Draw(foot.x - 36,
                          foot.y - 32,
                          person.displayName + L" †",
                          C(.62f, .57f, .51f),
                          .65f);
            }
            return;
        }
        if (!world.arcana)
        {
            int frame = static_cast<int>(s.stride) % 8;
            if (person.species == Lena::Species::Human)
            {
                renderer.DrawCharacter(foot,
                                       static_cast<int>(person.role),
                                       s.facing > 0 ? 1 : 3,
                                       frame,
                                       person.role == Lena::Role::Player);
            }
            else if (person.species == Lena::Species::Wolf)
            {
                renderer.DrawCachedModel(foot, RenderAssets::Wolf, frame % 4, s.facing);
            }
            else
            {
                renderer.DrawAnimal(foot, 0, s.stride, s.facing);
            }
        }
        float maximum = world.arcana                             ? person.maxHealth
                        : person.species == Lena::Species::Human ? 100.f
                        : person.species == Lena::Species::Wolf  ? 60.f
                                                                 : 45.f;
        if (s.health < maximum)
        {
            Box(foot.x - 18, foot.y - 65, 36, 4, C(.14f, .09f, .09f));
            Box(foot.x - 18, foot.y - 65, 36 * s.health / maximum, 4, C(.7f, .25f, .21f));
        }
        if (Lena::Distance(person.Position(), world.Player().Position()) < 3 &&
            person.role != Lena::Role::Player)
        {
            text.Draw(foot.x - 45,
                      foot.y - 92,
                      person.displayName + L" · " + Lena::Label(s.action),
                      C(.86f, .84f, .72f),
                      .65f);
        }
    }

    void Hud()
    {
        Profiler::Scope scope("lena.hud");
        if (world.arcana)
        {
            ArcanaHud();
            return;
        }
        const Color ink = C(.85f, .87f, .82f), gold = C(.88f, .74f, .46f);
        Box(18, 16, 930, 126, C(.025f, .05f, .055f, .94f));
        text.Draw(34, 25, L"잔향의 땅 · 레나 계곡 / 물 아래에서 들린 이름", gold, 1.1f);
        std::wstring objective =
            world.prologue == 0 ? L"[1] 리아를 믿고 동행  [2] 혼자 출구 찾기 (30분 기한)"
            : world.investigation == 0
                ? L"레나 게시판으로 이동 → [1] 수로 조사 우선  [2] 숲 조사 우선"
            : !world.waterClue || !world.forestClue
                ? L"[E] 수로와 사슴 숲의 두 단서 확인 / 주요 인물이 없어도 기록은 남는다"
            : world.minutes < 180 ? L"두 원인을 확인했다. 숲의 후속 사건은 세계 시간 180분부터"
            : world.corridor      ? L"통로 복구 완료. 생태계와 주민의 선택은 계속된다."
            : world.hunting       ? L"늑대 사냥 중. 실제 전투 결과만 개체 수에 반영된다."
            : world.corridorWork > 0
                ? L"숲 현장 작업 잔여 " + std::to_wstring(static_cast<int>(world.corridorWork)) +
                      L"분 (위험 중단)"
                : L"숲 현장에서 [1] 통로 복구 (120분 노동)  [2] 늑대 사냥";
        text.Draw(34, 68, objective, ink, .84f);
        text.Draw(
            34,
            107,
            L"WASD 이동  SPACE 공격(주민도 피격)  E 조사·유품  K 저장  L 불러오기  H 일시정지",
            ink,
            .73f);
        Box(980, 16, 280, 330, C(.025f, .05f, .055f, .94f));
        text.Draw(995,
                  25,
                  L"레나 지도 · 인구 " + std::to_wstring(world.Population(Lena::Species::Human)),
                  gold,
                  .82f);
        auto marker = [&](Point p, Color color, float size)
        {
            Box(1118 + p.x * 4, 190 + p.y * 4, size, size, color);
        };
        for (auto p : world.houses)
        {
            marker(p, C(.62f, .53f, .35f), 7);
        }
        for (auto id : world.people)
        {
            const auto* p = world.Find(id);
            if (!p || p->state.dead)
            {
                continue;
            }
            marker(p->Position(),
                   p->role == Lena::Role::Player       ? gold
                   : p->species == Lena::Species::Wolf ? C(.8f, .25f, .2f)
                                                       : C(.35f, .67f, .51f),
                   p->role == Lena::Role::Player ? 6.f : 2.f);
        }
        marker(world.canal, C(.4f, .8f, .9f), 6);
        marker(world.board, gold, 6);
        marker(world.forest, C(.5f, .85f, .4f), 6);
        text.Draw(995, 312, L"청록: 수로 / 금색: 게시판 / 연두: 숲", ink, .57f);
        Box(18, 550, 1060, 232, C(.025f, .05f, .055f, .91f));
        int clock = (360 + static_cast<int>(world.minutes)) % 1440;
        text.Draw(34,
                  559,
                  L"HP " + std::to_wstring(static_cast<int>(world.Player().state.health)) +
                      L"   시각 " + std::to_wstring(clock / 60) + L":" +
                      std::to_wstring(clock % 60) + L"   식량 " +
                      std::to_wstring(static_cast<int>(world.town.food)) + L"   늑대 " +
                      std::to_wstring(world.Population(Lena::Species::Wolf)) + L"   사슴 " +
                      std::to_wstring(world.Population(Lena::Species::Deer)),
                  gold,
                  .85f);
        float y = 596;
        for (const auto& line : world.messages)
        {
            text.Draw(34, y, line, ink, .70f);
            y += 24;
        }
        if (help || world.gameOver)
        {
            Box(215, 178, 830, 350, C(.02f, .035f, .045f, .98f));
            text.Draw(244,
                      201,
                      world.gameOver ? L"죽음 — 세계 전체를 불러오거나 새로 시작하십시오"
                                     : L"아르카 수로에서 깨어나다",
                      gold,
                      1.12f);
            text.Draw(244,
                      252,
                      L"리아와 동행하거나 혼자 레나로 향하세요. 선택은 직접 합니다.",
                      ink,
                      .78f);
            text.Draw(244,
                      291,
                      L"늑대의 사냥, 약탈자의 위협, 경비의 개입이 실제 전투로 이어집니다.",
                      ink,
                      .75f);
            text.Draw(244,
                      330,
                      L"누구도 불사신이 아닙니다. 죽음은 유품·기억·신고·노동 공백을 남깁니다.",
                      ink,
                      .78f);
            text.Draw(244,
                      369,
                      L"E 상호작용 / 1·2 사건 선택 / K 전체 저장 / L 전체 불러오기 / R 새 세계",
                      ink,
                      .78f);
            text.Draw(
                244, 418, L"저장 슬롯: LOCALAPPDATA / SimpleGame / lena-save-v1.txt", ink, .72f);
            text.Draw(244,
                      474,
                      world.gameOver ? L"[L] 저장한 세계로 복귀  [R] 모든 상태를 초기화하여 새 게임"
                                     : L"[H] 시작 / 일시정지 해제",
                      gold,
                      .9f);
        }
    }

    void ArcanaHud()
    {
        Profiler::Scope scope("arcana.hud");
        const Color ink = C(.85f, .87f, .82f), gold = C(.88f, .74f, .46f);
        Box(18, 16, 954, 152, C(.025f, .04f, .05f, .94f));
        text.Draw(34, 24, L"아르카나 : ARCANA · 다섯 마을과 숲길", gold, 1.15f);
        text.Draw(34, 66, world.arcana->Objective(), ink, .83f);
        const auto& state = world.Player().state;
        text.Draw(34,
                  108,
                  L"HP " + std::to_wstring(static_cast<int>(state.health)) + L"   마력 " +
                      std::to_wstring(static_cast<int>(state.manaCurrent)) + L" / " +
                      std::to_wstring(static_cast<int>(state.manaMax)) + L"   벨하임 주민 " +
                      std::to_wstring(world.Population(Lena::Species::Human)),
                  ink,
                  .72f);
        text.Draw(
            34, 138, L"WASD 이동 · E 조사/흡수 · F 화염 · SPACE 공격 · Q 대장간 휴식", ink, .72f);
        DrawMinimap();
        Box(18, 550, 1244, 232, C(.025f, .04f, .05f, .94f));
        text.Draw(
            34,
            559,
            L"K 저장 · L 불러오기 · H 일시정지 · R 새 게임 | 지식은 효율, 마수 흡수는 최대 마력",
            gold,
            .8f);
        float y = 596;
        for (const auto& line : world.messages)
        {
            text.Draw(34, y, line, ink, .67f);
            y += 24;
        }
        if (help || world.gameOver)
        {
            Box(170, 180, 930, 345, C(.02f, .03f, .04f, .99f));
            text.Draw(198,
                      202,
                      world.gameOver ? L"죽음 — 저장한 세계로 돌아가거나 새로 시작하세요"
                                     : L"아르카나 : ARCANA — 재가 된 집",
                      gold,
                      1.1f);
            text.Draw(198,
                      255,
                      L"부모의 유품을 챙겨 세르딘을 만나고, 벨하임에서 불꽃의 원리를 배우세요.",
                      ink,
                      .78f);
            text.Draw(
                198,
                298,
                L"마법 적성은 혈통으로, 효율은 자연의 이해로. 평민도 지식을 전할 수 있습니다.",
                ink,
                .76f);
            text.Draw(
                198,
                341,
                L"NPC와 동물은 서로 영향을 주며 죽을 수 있습니다. 공격은 주민에게도 맞습니다.",
                ink,
                .76f);
            text.Draw(198,
                      384,
                      L"마수 흡수: 직접 전투 기여 후 사망 지점에서 E (세계 시간 60초 이내).",
                      ink,
                      .78f);
            text.Draw(198,
                      427,
                      L"저장: LOCALAPPDATA / SimpleGame / arcana-save-v2.txt (확장 월드 전용)",
                      ink,
                      .7f);
            text.Draw(
                198, 475, world.gameOver ? L"[L] 불러오기   [R] 새 게임" : L"[H] 시작", gold, .95f);
        }
    }

    void Update(float dt)
    {
        time += dt;
        if (help || world.gameOver)
        {
            return;
        }
        auto& player = world.Player();
        float x = (keys['d'] ? 1.f : 0.f) - (keys['a'] ? 1.f : 0.f);
        float y = (keys['s'] ? 1.f : 0.f) - (keys['w'] ? 1.f : 0.f);
        float length = std::hypot(x, y);
        if (length > 0)
        {
            x /= length;
            y /= length;
            Point p = player.Position();
            Point delta = WorldGeometry::UnprojectDelta({x * 170 * dt, y * 170 * dt});
            Point to{p.x + delta.x, p.y + delta.y};
            float step = std::hypot(delta.x, delta.y);
            if (!world.Move(player, to, step))
            {
                world.Move(player, {to.x, p.y}, step);
                world.Move(player, {player.Position().x, to.y}, step);
            }
        }
        if (keys[' '])
        {
            world.PlayerAttack();
        }
        if (keys['q'] && world.arcana && length == 0)
        {
            world.arcana->Rest(dt);
        }
        world.scene.Update(dt);
        Point target = WorldGeometry::Project(player.Position());
        camera.x += (target.x - camera.x) * std::min(1.f, dt * 8);
        camera.y += (target.y - camera.y) * std::min(1.f, dt * 8);
    }
};

LenaGame::LenaGame(bool arcanaMode) : m(new Impl(arcanaMode))
{
}

LenaGame::~LenaGame() = default;

void LenaGame::Resize(int width, int height)
{
    m->renderer.Resize(width, height);
}

void LenaGame::Update(float dt)
{
    m->Update(dt);
}

void LenaGame::Render()
{
    m->world.scene.Render({m->renderer, m->camera, m->time});
}

void LenaGame::ClearKeys()
{
    std::fill(std::begin(m->keys), std::end(m->keys), false);
}

bool LenaGame::WantsQuit() const
{
    return m->quit;
}

WorldMonitor::Snapshot LenaGame::CaptureMonitor() const
{
    m->monitorReady = false;
    m->monitorCursor = 0;
    return std::move(m->monitorSnapshot);
}

bool LenaGame::MonitorReady() const
{
    return m->monitorReady;
}

void LenaGame::PrepareMonitor()
{
    m->PrepareMonitor();
}

void LenaGame::KeyDown(unsigned char key)
{
    key = static_cast<unsigned char>(std::tolower(key));
    if (key == 27)
    {
        m->quit = true;
        return;
    }
    if (key == 'h')
    {
        m->help = !m->help;
        ClearKeys();
        return;
    }
    if (key == 'r')
    {
        try
        {
            m.reset(new Impl(m->world.arcana != nullptr));
        }
        catch (const std::exception&)
        {
            m->world.Log(L"새 게임 생성 실패: 콘텐츠 파일을 확인하세요. 현재 세계는 유지됩니다.");
        }
        return;
    }
    if (key == 'l')
    {
        bool ok = m->world.Load(m->savePath);
        if (!ok)
        {
            m->world.Log(L"불러오기 실패: 파일 없음 또는 규격 오류. 현재 세계는 보존됩니다.");
        }
        else
        {
            m->camera = WorldGeometry::Project(m->world.Player().Position());
            m->monitorSnapshot = {};
            m->monitorCursor = 0;
            m->monitorReady = false;
        }
        ClearKeys();
        return;
    }
    if (key == 'k')
    {
        m->world.Log(m->world.Save(m->savePath) ? L"세계 전체 저장 완료."
                                                : L"저장 실패: 파일 경로와 권한을 확인하세요.");
        return;
    }
    if (m->help || m->world.gameOver)
    {
        return;
    }
    m->keys[key] = true;
    if (key == 'e')
    {
        m->world.Interact();
    }
    if (key == 'f' && m->world.arcana)
    {
        m->world.arcana->Cast();
    }
    if ((key == 'q' && !m->world.arcana) || (key == 'v' && m->world.arcana))
    {
        auto& state = m->world.Player().state;
        if (state.food >= 1)
        {
            state.food -= 1;
            state.hunger = std::max(0.f, state.hunger - 45);
            m->world.town.consumption += 1;
            m->world.Log(L"보유 식량 1배급을 먹었습니다.");
        }
        else
        {
            m->world.Log(L"보유 식량이 부족합니다. 마을 주민 또는 유품의 실제 식량이 필요합니다.");
        }
    }
    if (key == '1' || key == '2')
    {
        m->world.Choose(key - '0');
    }
}

void LenaGame::KeyUp(unsigned char key)
{
    m->keys[static_cast<unsigned char>(std::tolower(key))] = false;
}
