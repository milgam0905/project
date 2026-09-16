#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Dependencies/glew.h"
#include <Windows.h>
#include "TutorialGame.h"
#include "UiText.h"
#include "Renderer.h"
#include "WorldGeometry.h"
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
        glColor4f(c.r, c.g, c.b, c.a);
    }

    void Poly(std::initializer_list<Point> points, Color c)
    {
        Tint(c);
        glBegin(GL_TRIANGLE_FAN);
        for (Point p : points)
        {
            glVertex2f(p.x, p.y);
        }

        glEnd();
    }

    void Rect(float x, float y, float w, float h, Color c)
    {
        Poly({{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}, c);
    }

    void Ellipse(float x, float y, float rx, float ry, Color c)
    {
        Tint(c);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y);
        for (int i = 0; i <= 32; ++i)
        {
            float a = i * Pi / 16;
            glVertex2f(x + std::cos(a) * rx, y + std::sin(a) * ry);
        }

        glEnd();
    }

    void Line(Point a, Point b, Color c)
    {
        Tint(c);
        glBegin(GL_LINES);
        glVertex2f(a.x, a.y);
        glVertex2f(b.x, b.y);
        glEnd();
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
        Beast,
        Bonfire
    };

    struct Object
    {
        Point p;
        Kind kind;
        int variant;
        Point home = {0, 0};
        float phase = 0;
        int direction = 0;
        bool moving = false;
    };

    struct Animal
    {
        Point p, home;
        int species;
        float phase, facing;
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
    std::vector<Animal> animals;
    bool keys[256] = {};
    bool help = false, choice = false, accepted = false, quit = false;
    Point player = {0, 3}, camera = Iso(player);
    Quest quest = Quest::Meet;
    float time = 0, elapsed = 0, stride = 0;
    std::vector<Object> objects;
    std::vector<Page> pages;
    size_t page = 0;
    const Point elder = {0, 0}, herb = {-8, -5}, shrine = {6, -7};
    const Color ink = C(.84f, .87f, .81f), gold = C(.88f, .70f, .39f);

    Impl()
    {
        objects = {{{-3, 0}, Kind::House, 0},      {{1, -4}, Kind::House, 1},
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
        for (Object& object : objects)
        {
            object.home = object.p;
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
            animals.push_back({homes[i], homes[i], i % 3, static_cast<float>(i), 1.f});
        }
        // Expand both world axes by two. Keep original short quest destinations intact.
        for (int x = -27; x <= 27; ++x)
        {
            for (int y = -25; y <= 25; ++y)
            {
                Point p = {static_cast<float>(x), static_cast<float>(y)};
                const int seed = (x + 40) * 139 + (y + 40) * 53;
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
                for (const Object& object : objects)
                {
                    if (object.kind != Kind::Tree && Distance(p, object.p) < 2.3f)
                    {
                        reserved = true;
                    }
                }

                for (const Animal& animal : animals)
                {
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
                    objects.push_back({p, Kind::Tree, seed % 3});
                }
            }
        }
    }

    bool Water(Point p) const
    {
        return WorldGeometry::Water(p, .04f);
    }

    bool Blocked(Point p) const
    {
        if (std::abs(p.x) > WorldGeometry::HalfWidth || std::abs(p.y) > WorldGeometry::HalfHeight ||
            Water(p))
        {
            return true;
        }

        for (const Object& o : objects)
        {
            if (o.kind == Kind::House && std::abs(p.x - o.p.x) < 1.35f &&
                std::abs(p.y - o.p.y) < 1.15f)
            {
                return true;
            }

            if (o.kind == Kind::Tree && Distance(p, o.p) < .42f)
            {
                return true;
            }
        }

        return false;
    }

    Point Screen(Point p) const
    {
        Point s = Iso(p);
        return {s.x - camera.x + 640, s.y - camera.y + 420};
    }

    Point Target() const
    {
        if (quest == Quest::Herb)
        {
            return herb;
        }

        if (quest == Quest::Lake)
        {
            return shrine;
        }

        return elder;
    }

    const Object* Nearest() const
    {
        const Object* best = nullptr;
        float nearest = 1.65f;
        for (const Object& o : objects)
        {
            if (o.kind != Kind::Villager && o.kind != Kind::Herb && o.kind != Kind::Shrine)
            {
                continue;
            }

            float d = Distance(player, o.p);
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
        const Object* o = Nearest();
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
        time += dt;
        if (quest != Quest::Complete)
        {
            elapsed += dt;
        }

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
                Point previous = player;
                if (!Blocked({player.x + dx, player.y}))
                {
                    player.x += dx;
                }

                if (!Blocked({player.x, player.y + dy}))
                {
                    player.y += dy;
                }

                playerMoving = Distance(previous, player) > .0001f;
                if (playerMoving)
                {
                    stride += dt * 9;
                }

                playerDirection = std::abs(sx) > std::abs(sy) ? (sx > 0 ? 1 : 3) : (sy > 0 ? 0 : 2);
            }
        }

        if (pages.empty() && !help)
        {
            for (Object& object : objects)
            {
                if (object.kind != Kind::Villager || object.variant < 8)
                {
                    continue;
                }

                object.moving = false;
                if (Distance(player, object.p) < 1.9f)
                {
                    continue;
                }

                const float seed = static_cast<float>(object.variant);
                Point target = {object.home.x + std::sin(time * .18f + seed) * 1.35f,
                                object.home.y + std::cos(time * .14f + seed) * 1.0f};
                float length = Distance(target, object.p);
                if (length < .18f)
                {
                    continue;
                }

                Point next = {object.p.x + (target.x - object.p.x) / length * dt * .75f,
                              object.p.y + (target.y - object.p.y) / length * dt * .75f};
                if (!Blocked(next))
                {
                    Point delta = Iso({next.x - object.p.x, next.y - object.p.y});
                    object.direction = std::abs(delta.x) > std::abs(delta.y)
                                           ? (delta.x > 0 ? 1 : 3)
                                           : (delta.y > 0 ? 0 : 2);
                    object.p = next;
                    object.phase += dt * 7;
                    object.moving = true;
                }
            }

            for (Animal& animal : animals)
            {
                Point target = {animal.home.x + std::sin(time * .23f + animal.species) * 2.5f,
                                animal.home.y + std::cos(time * .19f + animal.phase * .02f) * 2.f};
                bool flee = Distance(animal.p, player) < 3.8f;
                if (flee)
                {
                    target = {animal.p.x + (animal.p.x - player.x) * 2,
                              animal.p.y + (animal.p.y - player.y) * 2};
                }

                float length = Distance(animal.p, target);
                if (length < .1f)
                {
                    continue;
                }

                float speed = flee ? 3.5f : 1.05f;
                Point next = {animal.p.x + (target.x - animal.p.x) / length * dt * speed,
                              animal.p.y + (target.y - animal.p.y) / length * dt * speed};
                // Axis sliding keeps animals off water and out of buildings/trees.
                Point previous = animal.p;
                if (!Blocked({next.x, animal.p.y}))
                {
                    animal.p.x = next.x;
                }

                if (!Blocked({animal.p.x, next.y}))
                {
                    animal.p.y = next.y;
                }

                if (Distance(previous, animal.p) > .001f)
                {
                    animal.phase += dt * (flee ? 13.f : 6.f);
                    Point delta = Iso({animal.p.x - previous.x, animal.p.y - previous.y});
                    if (std::abs(delta.x) > .001f)
                    {
                        animal.facing = delta.x;
                    }
                }
            }
        }

        Point desired = Iso(player);
        float smoothing = 1 - std::exp(-dt * 7);
        camera.x += (desired.x - camera.x) * smoothing;
        camera.y += (desired.y - camera.y) * smoothing;
    }

    void Ground()
    {
        renderer.DrawTerrain();
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

    void DrawObject(const Object& o)
    {
        Point p = Screen(o.p);
        if (p.x < -200 || p.x > 1480 || p.y < -100 || p.y > 1080)
        {
            return;
        }

        if (o.kind == Kind::Villager)
        {
            Person(p, o.variant, false, o.direction, o.phase, o.moving && pages.empty() && !help);
        }
        else if (o.kind == Kind::Tree)
        {
            float alpha = Distance(player, o.p) < 2.5f ? .38f : 1.f;
            renderer.DrawTree(p, o.variant, alpha);
        }
        else if (o.kind == Kind::House)
        {
            Point ps = Screen(player);
            float alpha =
                (std::abs(ps.x - p.x) < 100 && ps.y < p.y + 25 && ps.y > p.y - 145) ? .4f : 1.f;
            renderer.DrawHouse(p, o.variant, alpha);
        }
        else if (o.kind == Kind::Beast)
        {
            const Animal& animal = animals[o.variant];
            renderer.DrawAnimal(p, animal.species, animal.phase, animal.facing);
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
            renderer.DrawFire(p, 1.f);
        }
        else if (o.kind == Kind::Lamp)
        {
            Rect(p.x - 3, p.y - 68, 6, 68, C(.18f, .20f, .17f));
            Glow({p.x, p.y - 66}, 65, C(1, .59f, .20f, .55f));
            Rect(p.x - 6, p.y - 74, 12, 17, C(.94f, .69f, .30f));
            Rect(p.x - 8, p.y - 77, 16, 4, C(.13f, .16f, .15f));
            renderer.DrawFire({p.x, p.y - 59}, .55f);
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
            renderer.DrawFire({p.x, p.y - 40}, .65f, true);
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
                  L"WASD 이동   E 대화 / 조사   H 도움말   ESC 닫기 / 종료",
                  C(.56f, .66f, .64f),
                  .78f);
        Panel(995, 24, 257, 180);
        text.Draw(1012, 33, L"마을과 주변", gold, .85f);
        auto mini = [](Point p) -> Point
        {
            Point v = Iso(p);
            return {1125 + v.x * .043f, 125 + v.y * .043f};
        };
        for (const Object& o : objects)
        {
            if (o.kind == Kind::Tree || o.kind == Kind::Lamp || o.kind == Kind::Bonfire)
            {
                continue;
            }

            Point p = mini(o.p);
            Ellipse(p.x, p.y, o.kind == Kind::House ? 4.f : 2.f, 2, C(.40f, .51f, .46f));
        }

        Tint(C(.14f, .38f, .42f));
        glBegin(GL_TRIANGLE_FAN);
        Point lake = mini({11, -6});
        glVertex2f(lake.x, lake.y);
        for (int i = 0; i <= 64; ++i)
        {
            Point edge = mini(WorldGeometry::LakeEdge(i * 2 * Pi / 64.f));
            glVertex2f(edge.x, edge.y);
        }

        glEnd();
        for (const Animal& animal : animals)
        {
            Point p = mini(animal.p);
            Ellipse(p.x, p.y, 1.8f, 1.8f, C(.71f, .51f, .29f));
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

        Point me = mini(player);
        Ellipse(me.x, me.y, 4, 4, C(.90f, .95f, .89f));
        text.Draw(1012, 171, L"금색: 목적지   흰색: 나", ink, .7f);
        if (pages.empty() && !help)
        {
            const Object* nearbyObject = Nearest();
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
            text.Draw(270, 420, L"금색 표식 : 다음 목표    물과 건물은 통과할 수 없습니다", ink);
            text.Draw(270, 477, L"H / ESC : 안내 닫기    완료 후 R : 처음부터", gold);
        }
    }

    void Render()
    {
        renderer.BeginScene(camera, time);
        Ground();
        // Ground-only shadow pass prevents actors from receiving incorrectly overlaid blobs.
        for (const Object& object : objects)
        {
            Point foot = Screen(object.p);
            if (foot.x < -200 || foot.x > 1480 || foot.y < -100 || foot.y > 1000)
            {
                continue;
            }

            if (object.kind == Kind::House)
            {
                renderer.DrawShadow(foot, 82, 135);
            }
            else if (object.kind == Kind::Tree)
            {
                renderer.DrawShadow(foot, 22, 100);
            }
            else if (object.kind == Kind::Villager)
            {
                renderer.DrawShadow(foot, 13, 50);
            }
        }

        renderer.DrawShadow(Screen(player), 13, 50);
        for (const Animal& animal : animals)
        {
            renderer.DrawShadow(Screen(animal.p), 20, 22);
        }

        std::vector<Object> sorted = objects;
        sorted.push_back({player, Kind::Villager, -1});
        for (size_t i = 0; i < animals.size(); ++i)
        {
            sorted.push_back({animals[i].p, Kind::Beast, static_cast<int>(i)});
        }

        std::stable_sort(sorted.begin(),
                         sorted.end(),
                         [](const Object& a, const Object& b)
                         { return a.p.x + a.p.y < b.p.x + b.p.y; });
        for (const Object& o : sorted)
        {
            if (o.variant == -1)
            {
                Person(Screen(player), 0, true);
            }
            else
            {
                DrawObject(o);
            }
        }

        Atmosphere();
        renderer.Present();
        Hud();
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

        if (key == 'e' || key == ' ')
        {
            if (++m->page == m->pages.size())
            {
                m->pages.clear();
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

    if (key == 27)
    {
        m->quit = true;
    }

    if (key == 'r' && m->quest == Quest::Complete)
    {
        int width = m->windowWidth, height = m->windowHeight;
        m.reset(new Impl);
        Resize(width, height);
    }
}
