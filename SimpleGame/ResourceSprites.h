#pragma once

#include "Renderer.h"
#include <map>
#include <string>
#include <vector>

// Offline-baked GLB meshes retain the game's ordered 2.5D RenderQueue path.
class ResourceSprites
{
  public:
    ResourceSprites() = default;
    ~ResourceSprites();
    ResourceSprites(const ResourceSprites&) = delete;
    ResourceSprites& operator=(const ResourceSprites&) = delete;

    void Load();
    void Draw(const std::string& name,
              RenderPoint foot,
              float pixelsPerMeter = 42.f,
              float opacity = 1.f,
              bool burned = false) const;
    void Animate(const std::string& model,
                 const char* clip,
                 int direction,
                 int frame,
                 RenderPoint foot) const;
    unsigned Ground(int material) const;

  private:
    struct Sprite
    {
        int page, x, y, width, height;
        float anchorX, anchorY, scale;
    };

    std::map<std::string, Sprite> sprites;
    std::vector<unsigned> pages;
    unsigned ground[3]{};
};
