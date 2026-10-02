#pragma once

#include "Actor.h"
#include "Profiler.h"
#include "WorldGeometry.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

// Testable subset of docs/story1.md. Node IDs remain identical to WBG_01..39.
namespace VillageBehavior
{
    enum class Action
    {
        Investigate = 3,
        Idle = 6,
        Wander = 7,
        Travel = 8,
        Sleep = 9,
        Eat = 10,
        Drink = 11,
        Work = 12,
        Rest = 13,
        Talk = 14,
        Trade = 16,
        Help = 17,
        Threaten = 23,
        Surrender = 27,
        Attack = 28,
        Flee = 31,
        Protect = 33
    };

    inline const wchar_t* Label(Action action)
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
            return L"식사";
        case Action::Drink:
            return L"음수";
        case Action::Work:
            return L"업무";
        case Action::Rest:
            return L"휴식";
        case Action::Talk:
            return L"대화";
        case Action::Trade:
            return L"거래";
        case Action::Help:
            return L"치료";
        case Action::Threaten:
            return L"위협";
        case Action::Surrender:
            return L"항복";
        case Action::Attack:
            return L"공격";
        case Action::Flee:
            return L"도주";
        case Action::Protect:
            return L"보호";
        default:
            return L"대기";
        }
    }

    enum class Role
    {
        Resident,
        Merchant,
        Guard,
        Healer,
        Scholar,
        Bandit,
        Pilgrim
    };

    struct Traits
    {
        int courage, empathy, aggression, lawfulness, greed, curiosity, loyalty;
    };

    struct Profile
    {
        const wchar_t* name;
        Role role;
        Traits traits;
        RenderPoint home, work;
        const wchar_t* line;
    };

    inline const std::array<Profile, 20>& NewResidents()
    {
        static const std::array<Profile, 20> profiles = {
            {{L"리아",
              Role::Scholar,
              {62, 78, 15, 48, 20, 95, 72},
              {-4, 9},
              {6, -7},
              L"수로의 기억을 조사하고 있어요. 기억을 되찾는 값이 누구의 삶인지 먼저 알아야죠."},
             {L"에드윈",
              Role::Resident,
              {55, 72, 20, 70, 25, 40, 90},
              {-9, 5},
              {-3, 2},
              L"흉년을 기억합니다. 배급 장부는 기적보다 먼저 확인해야 해요."},
             {L"상단 중개인 마라",
              Role::Merchant,
              {48, 45, 18, 60, 78, 50, 55},
              {9, 8},
              {2, 6},
              L"안전한 길에는 비용이 들어요. 은화 5개로 식량 하나를 드리죠. [T] 거래"},
             {L"다리안",
              Role::Guard,
              {82, 40, 45, 88, 15, 35, 80},
              {7, 3},
              {18, 12},
              L"항복자는 구금합니다. 의심만으로 사람을 죽이는 건 질서가 아닙니다."},
             {L"노아",
              Role::Scholar,
              {70, 64, 35, 25, 30, 72, 85},
              {-11, -4},
              {-8, -5},
              L"약초를 찾습니다. 왕국이 감춘 기록도요. 소문과 증거는 다른 물건이지요."},
             {L"셀린",
              Role::Healer,
              {58, 95, 5, 68, 10, 65, 75},
              {-4, 9},
              {-2, 7},
              L"기억보다 먼저 상처를 살펴요. 약초가 남아 있다면 도울 수 있어요. [T] 치료"},
             {L"성직자 아벨",
              Role::Healer,
              {60, 92, 8, 82, 8, 55, 88},
              {14, 7},
              {6, -7},
              L"죽음을 없애겠다는 약속 대신, 오늘 살아 있는 이를 돌보겠습니다. [T] 치료"},
             {L"헌금징수인 로단",
              Role::Merchant,
              {45, 18, 25, 52, 95, 30, 35},
              {16, 6},
              {5, 5},
              L"기도는 무료입니다. 기도서를 보관하는 데 돈이 들 뿐이죠. [T] 거래"},
             {L"상인 이네스",
              Role::Merchant,
              {55, 80, 12, 78, 35, 62, 65},
              {18, 9},
              {0, 8},
              L"굶주린 이에게는 가격을 낮춰요. 장사가 내일도 있으려면요. [T] 거래"},
             {L"밀수상 케른",
              Role::Merchant,
              {70, 22, 52, 12, 88, 70, 25},
              {23, 14},
              {16, 14},
              L"출처는 묻지 마세요. 적어도 당신에게서 훔친 물건은 아니니까. [T] 거래"},
             {L"호위병 유나",
              Role::Guard,
              {90, 85, 35, 75, 12, 48, 90},
              {19, 14},
              {25, 20},
              L"칼은 돌아갈 사람이 있는 이들을 지키는 데 쓰겠습니다."},
             {L"겁 많은 경비 폴",
              Role::Guard,
              {30, 65, 18, 85, 25, 30, 70},
              {-13, 8},
              {-18, 10},
              L"용감함도 좋지만 퇴로가 있으면 더 좋습니다."},
             {L"농부 다나",
              Role::Resident,
              {48, 88, 8, 70, 20, 45, 85},
              {-16, 12},
              {-22, 14},
              L"씨앗은 남겨야 해요. 오늘의 배고픔으로 내년까지 먹을 수는 없죠."},
             {L"장의사 조수 루스",
              Role::Pilgrim,
              {55, 90, 5, 70, 10, 60, 80},
              {-14, -10},
              {6, -7},
              L"남은 사람의 이름도 장례 장부에 적어요. 잊지 않고 찾아가려고요."},
             {L"유물학자 오스",
              Role::Scholar,
              {72, 30, 18, 40, 65, 98, 45},
              {20, -15},
              {6, -7},
              L"이 문양은 죽은 이보다 산 이의 기억에 더 강하게 반응합니다."},
             {L"순례자 네브",
              Role::Pilgrim,
              {40, 75, 5, 60, 15, 85, 50},
              {-23, -17},
              {6, -7},
              L"답을 찾으러 왔습니다. 질문은 예상보다 많이 얻었네요."},
             {L"산적 우두머리 바르",
              Role::Bandit,
              {85, 12, 90, 5, 85, 30, 72},
              {26, 19},
              {23, 16},
              L"이 길의 통행세는 우리가 정한다. 목숨까지 낼 필요는 없겠지."},
             {L"산적 렉스",
              Role::Bandit,
              {60, 8, 85, 8, 92, 25, 35},
              {29, 21},
              {25, 17},
              L"돈이 없으면 돌아가. 나는 좋은 사람이 아니지만 계산은 할 줄 알아."},
             {L"산적 탈주자 테오",
              Role::Bandit,
              {22, 65, 28, 25, 50, 55, 20},
              {-25, 18},
              {-21, 16},
              L"더는 누구도 해치고 싶지 않아. 칼을 내리면 나도 내리겠어."},
             {L"산적 감시꾼 니아",
              Role::Bandit,
              {50, 25, 62, 10, 72, 75, 80},
              {31, 17},
              {27, 14},
              L"굶주리기 전엔 나도 장터에서 값을 치렀지. 핑계라는 건 알아."}}};
        return profiles;
    }

    struct State
    {
        Action action = Action::Idle;
        Actor::Id threat = 0;
        float hunger = 20, thirst = 20, fatigue = 20, health = 100;
        float decision = 0, perception = 0, held = 0, progress = 0, cooldown = 0;
        float threatMemory = 0, memoryFear = 0, serviceCooldown = 0;
        int food = 5, coins = 30, medicine = 8;
        unsigned tick = 0;
        bool surrendered = false, warned = false;
        RenderPoint destination = {0, 0};
        RenderPoint lastThreatPosition = {0, 0};
        std::vector<RenderPoint> path;
        size_t waypoint = 0;
        int goalCell = -1;
        float repath = 0;
    };

    // Half-unit occupancy, one connected component, bounded searches per update.
    // Geometry is baked after placement; no actor pointer is retained by navigation.
    class Navigation
    {
      public:
        struct Obstacle
        {
            RenderPoint position;
            float x, y;
        };

        static constexpr int Width = 225, Height = 209;
        std::vector<unsigned char> open;
        std::vector<int> parent, frontier, nearestOpen;
        int budget = 0;

        RenderPoint Position(int cell) const
        {
            return {(cell % Width) * .5f - WorldGeometry::HalfWidth,
                    (cell / Width) * .5f - WorldGeometry::HalfHeight};
        }

        int Cell(RenderPoint p) const
        {
            int x = static_cast<int>(std::lround((p.x + WorldGeometry::HalfWidth) * 2));
            int y = static_cast<int>(std::lround((p.y + WorldGeometry::HalfHeight) * 2));
            return x >= 0 && x < Width && y >= 0 && y < Height ? y * Width + x : -1;
        }

        bool Valid(RenderPoint p) const
        {
            int cell = Cell(p);
            return cell >= 0 && !open.empty() && open[cell] != 0;
        }

        void Build(const std::vector<Obstacle>& obstacles)
        {
            Profiler::Scope scope("navigation.village_build");
            open.assign(Width * Height, 0);
            parent.resize(open.size());
            frontier.reserve(open.size());
            for (int i = 0; i < Width * Height; ++i)
            {
                auto p = Position(i);
                open[i] = std::abs(p.x) < WorldGeometry::HalfWidth - .5f &&
                          std::abs(p.y) < WorldGeometry::HalfHeight - .5f &&
                          !WorldGeometry::Water(p, .14f);
            }
            for (const auto& obstacle : obstacles)
            {
                int cx = static_cast<int>((obstacle.position.x + WorldGeometry::HalfWidth) * 2);
                int cy = static_cast<int>((obstacle.position.y + WorldGeometry::HalfHeight) * 2);
                int rx = static_cast<int>(std::ceil((obstacle.x + .4f) * 2));
                int ry = static_cast<int>(std::ceil((obstacle.y + .4f) * 2));
                for (int y = std::max(0, cy - ry); y <= std::min(Height - 1, cy + ry); ++y)
                {
                    for (int x = std::max(0, cx - rx); x <= std::min(Width - 1, cx + rx); ++x)
                    {
                        auto p = Position(y * Width + x);
                        if (std::abs(p.x - obstacle.position.x) < obstacle.x + .36f &&
                            std::abs(p.y - obstacle.position.y) < obstacle.y + .36f)
                        {
                            open[y * Width + x] = 0;
                        }
                    }
                }
            }
            Search(Cell({0, 3}), -1);
            int accessible = 0;
            for (size_t i = 0; i < open.size(); ++i)
            {
                open[i] = parent[i] != -1;
                accessible += open[i];
            }
            Profiler::Gauge("navigation.village_reachable_cells", accessible);
            // Multi-source flood: O(cell count) once, O(1) safe destination lookup afterwards.
            nearestOpen.assign(open.size(), -1);
            frontier.clear();
            for (int i = 0; i < static_cast<int>(open.size()); ++i)
            {
                if (open[i])
                {
                    nearestOpen[i] = i;
                    frontier.push_back(i);
                }
            }
            for (size_t head = 0; head < frontier.size(); ++head)
            {
                int cell = frontier[head];
                const int offsets[] = {-1, 1, -Width, Width};
                for (int offset : offsets)
                {
                    int next = cell + offset;
                    if (next < 0 || next >= Width * Height ||
                        (std::abs(offset) == 1 && cell / Width != next / Width) ||
                        nearestOpen[next] >= 0)
                    {
                        continue;
                    }
                    nearestOpen[next] = nearestOpen[cell];
                    frontier.push_back(next);
                }
            }
        }

        RenderPoint Safe(RenderPoint p) const
        {
            if (Valid(p))
            {
                return p;
            }
            Profiler::Scope scope("navigation.village_safe_point");
            Profiler::Count("navigation.village_snap_requests");
            p.x = std::max(-WorldGeometry::HalfWidth, std::min(WorldGeometry::HalfWidth, p.x));
            p.y = std::max(-WorldGeometry::HalfHeight, std::min(WorldGeometry::HalfHeight, p.y));
            int cell = Cell(p);
            if (cell < 0 || nearestOpen.empty() || nearestOpen[cell] < 0)
            {
                return {0, 3};
            }
            Profiler::Count("navigation.village_safe_cache_hits");
            return Position(nearestOpen[cell]);
        }

        bool Segment(RenderPoint a, RenderPoint b) const
        {
            float dx = b.x - a.x, dy = b.y - a.y;
            int steps = std::max(1, static_cast<int>(std::ceil(std::sqrt(dx * dx + dy * dy) * 5)));
            for (int i = 1; i <= steps; ++i)
            {
                float t = static_cast<float>(i) / steps;
                if (!Valid({a.x + dx * t, a.y + dy * t}))
                {
                    return false;
                }
            }
            return true;
        }

        bool Route(RenderPoint from, RenderPoint to, std::vector<RenderPoint>& path)
        {
            if (budget <= 0)
            {
                Profiler::Count("navigation.village_budget_deferred");
                return false;
            }
            --budget;
            Profiler::Scope scope("navigation.village_pathfind");
            Profiler::Count("navigation.village_path_requests");
            int start = Cell(from), goal = Cell(to);
            Search(start, goal);
            path.clear();
            if (goal < 0 || parent[goal] == -1)
            {
                Profiler::Count("navigation.village_path_failures");
                return true;
            }
            for (int cell = goal; cell != start; cell = parent[cell])
            {
                path.push_back(Position(cell));
            }
            std::reverse(path.begin(), path.end());
            path.insert(path.begin(), Position(start));
            path.push_back(to);
            return true;
        }

      private:
        void Search(int start, int goal)
        {
            std::fill(parent.begin(), parent.end(), -1);
            frontier.clear();
            if (start < 0 || !open[start])
            {
                return;
            }
            parent[start] = start;
            frontier.push_back(start);
            for (size_t head = 0; head < frontier.size(); ++head)
            {
                int cell = frontier[head];
                if (cell == goal)
                {
                    break;
                }
                const int offsets[] = {-1, 1, -Width, Width};
                for (int offset : offsets)
                {
                    int next = cell + offset;
                    if (next < 0 || next >= Width * Height ||
                        (std::abs(offset) == 1 && cell / Width != next / Width) || !open[next] ||
                        parent[next] != -1)
                    {
                        continue;
                    }
                    parent[next] = cell;
                    frontier.push_back(next);
                }
            }
            Profiler::Count("navigation.village_visited_cells", frontier.size());
        }
    };
} // namespace VillageBehavior
