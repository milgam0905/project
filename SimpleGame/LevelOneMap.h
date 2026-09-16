#pragma once

#include "Renderer.h"
#include <array>
#include <cstdint>
#include <vector>

class LevelOneMap
{
  public:
    enum class Tile
    {
        Ground,
        Water,
        Tree,
        Rock
    };
    static constexpr int Width = 40;
    static constexpr int Height = 36;

    explicit LevelOneMap(std::uint32_t seed);
    Tile At(int x, int y) const;
    bool Walkable(RenderPoint position) const;
    bool ClearSegment(RenderPoint from, RenderPoint to) const;
    RenderPoint NextStep(RenderPoint from, RenderPoint to) const;

    const std::vector<RenderPoint>& SpawnCells() const
    {
        return spawnCells;
    }

    std::uint32_t Seed() const
    {
        return seed;
    }

    int ReachableCellCount() const
    {
        return reachableCells;
    }

    static RenderPoint Camp()
    {
        return {Width * .5f + .5f, Height * .5f + .5f};
    }

  private:
    bool Connected() const;
    bool Ground(int x, int y) const;
    std::array<Tile, Width * Height> cells;
    std::vector<RenderPoint> spawnCells;
    std::uint32_t seed;
    int reachableCells = 0;
};
