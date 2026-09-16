#include "stdafx.h"
#include "LevelOneMap.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <random>

namespace
{
    const int OffsetX[] = {1, -1, 0, 0};
    const int OffsetY[] = {0, 0, 1, -1};
} // namespace

LevelOneMap::LevelOneMap(std::uint32_t value) : seed(value)
{
    cells.fill(Tile::Ground);
    std::mt19937 random(seed);
    const RenderPoint camp = Camp();

    // Every accepted obstacle preserves a single four-neighbour ground component.
    // Generation is bounded and cannot hang searching for a valid seed.
    for (int attempt = 0; attempt < 180; ++attempt)
    {
        int x = 2 + static_cast<int>(random() % (Width - 4));
        int y = 2 + static_cast<int>(random() % (Height - 4));
        if (std::hypot(x + .5f - camp.x, y + .5f - camp.y) < 5.f)
        {
            continue;
        }

        auto previous = cells;
        if (attempt < 5)
        {
            // Round water patches, with a fully walkable outside perimeter.
            for (int dy = -2; dy <= 2; ++dy)
            {
                for (int dx = -3; dx <= 3; ++dx)
                {
                    int px = x + dx, py = y + dy;
                    if (px > 1 && py > 1 && px < Width - 2 && py < Height - 2 &&
                        dx * dx / 8.f + dy * dy / 4.f < 1.f &&
                        std::hypot(px + .5f - camp.x, py + .5f - camp.y) > 5.f)
                    {
                        cells[py * Width + px] = Tile::Water;
                    }
                }
            }
        }
        else
        {
            cells[y * Width + x] = random() % 3 == 0 ? Tile::Rock : Tile::Tree;
        }

        if (!Connected())
        {
            cells = previous;
        }
    }

    // Defensive fallback guarantees reachability even after future generator edits.
    if (!Connected())
    {
        cells.fill(Tile::Ground);
    }

    for (int y = 0; y < Height; ++y)
    {
        for (int x = 0; x < Width; ++x)
        {
            if (Ground(x, y))
            {
                ++reachableCells;
                RenderPoint p = {x + .5f, y + .5f};
                if (std::hypot(p.x - camp.x, p.y - camp.y) > 6.f)
                {
                    spawnCells.push_back(p);
                }
            }
        }
    }

    std::shuffle(spawnCells.begin(), spawnCells.end(), random);
}

LevelOneMap::Tile LevelOneMap::At(int x, int y) const
{
    if (x < 0 || y < 0 || x >= Width || y >= Height)
    {
        return Tile::Rock;
    }

    return cells[y * Width + x];
}

bool LevelOneMap::Ground(int x, int y) const
{
    return At(x, y) == Tile::Ground;
}

bool LevelOneMap::Walkable(RenderPoint p) const
{
    // Player/monster clearance. Four-neighbour cell-centre routes always fit this radius.
    const float radius = .20f;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            if (!Ground(static_cast<int>(std::floor(p.x + x * radius)),
                        static_cast<int>(std::floor(p.y + y * radius))))
            {
                return false;
            }
        }
    }

    return true;
}

bool LevelOneMap::Connected() const
{
    std::array<bool, Width * Height> visited = {};
    std::queue<int> frontier;
    const int start = static_cast<int>(Camp().y) * Width + static_cast<int>(Camp().x);
    if (cells[start] != Tile::Ground)
    {
        return false;
    }

    frontier.push(start);
    visited[start] = true;
    int count = 0;

    while (!frontier.empty())
    {
        int current = frontier.front();
        frontier.pop();
        ++count;
        for (int i = 0; i < 4; ++i)
        {
            int x = current % Width + OffsetX[i], y = current / Width + OffsetY[i];
            if (!Ground(x, y))
            {
                continue;
            }

            int index = y * Width + x;
            if (!visited[index])
            {
                visited[index] = true;
                frontier.push(index);
            }
        }
    }

    return count == std::count(cells.begin(), cells.end(), Tile::Ground);
}

bool LevelOneMap::ClearSegment(RenderPoint from, RenderPoint to) const
{
    float distance = std::hypot(to.x - from.x, to.y - from.y);
    int samples = std::max(1, static_cast<int>(std::ceil(distance / .12f)));
    for (int i = 0; i <= samples; ++i)
    {
        float t = i / static_cast<float>(samples);
        if (!Walkable({from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t}))
        {
            return false;
        }
    }

    return true;
}

RenderPoint LevelOneMap::NextStep(RenderPoint from, RenderPoint to) const
{
    if (ClearSegment(from, to))
    {
        return to;
    }

    int sx = static_cast<int>(std::floor(from.x)), sy = static_cast<int>(std::floor(from.y));
    int tx = static_cast<int>(std::floor(to.x)), ty = static_cast<int>(std::floor(to.y));
    if (!Ground(sx, sy) || !Ground(tx, ty))
    {
        return from;
    }

    std::array<int, Width * Height> parent;
    parent.fill(-1);
    std::queue<int> frontier;
    const int start = sy * Width + sx, goal = ty * Width + tx;
    parent[start] = start;
    frontier.push(start);
    while (!frontier.empty() && parent[goal] == -1)
    {
        int current = frontier.front();
        frontier.pop();
        for (int i = 0; i < 4; ++i)
        {
            int x = current % Width + OffsetX[i], y = current / Width + OffsetY[i];
            if (!Ground(x, y))
            {
                continue;
            }

            int index = y * Width + x;
            if (parent[index] == -1)
            {
                parent[index] = current;
                frontier.push(index);
            }
        }
    }

    if (parent[goal] == -1 || start == goal)
    {
        return {sx + .5f, sy + .5f};
    }

    int next = goal;
    while (parent[next] != start)
    {
        next = parent[next];
    }

    RenderPoint waypoint = {next % Width + .5f, next / Width + .5f};
    // Move to our cell centre first if a diagonal shortcut would clip an obstacle.
    return ClearSegment(from, waypoint) ? waypoint : RenderPoint{sx + .5f, sy + .5f};
}
