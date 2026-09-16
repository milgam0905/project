#pragma once
#include "Dependencies/glew.h"

namespace RenderAssets
{
    enum Material
    {
        Grass,
        Dirt,
        Stone,
        Plaster,
        Timber,
        Shingles,
        MaterialCount
    };

    GLuint MakeMaterial(Material material);
    // 8 palettes x 4 directions x 8 frames, cells 40x64, total 320x2048.
    GLuint MakeCharacterAtlas();

    enum Model
    {
        ForestTree,
        Boulder,
        Wolf,
        Boar,
        Coins,
        Potion,
        Campfire,
        ModelCount
    };

    GLuint MakeModelAtlas();
} // namespace RenderAssets
