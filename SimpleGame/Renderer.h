#pragma once

#include <memory>

struct RenderPoint { float x, y; };
struct RenderColor { float r, g, b, a; };

class Renderer
{
public:
    Renderer(int windowSizeX, int windowSizeY);
    ~Renderer();
    bool IsInitialized() const;
    bool HasPostProcessing() const;
    void Resize(int width, int height);
    void BeginScene(RenderPoint projectedCamera, float time);
    void DrawTerrain();
    void DrawShadow(RenderPoint foot, float width, float height);
    void DrawHouse(RenderPoint foot, int variant, float opacity);
    void DrawTree(RenderPoint foot, int variant, float opacity);
    void DrawCharacter(RenderPoint foot, int variant, int direction, int frame, bool player);
    void DrawAnimal(RenderPoint foot, int species, float phase, float facing);
    void DrawFire(RenderPoint base, float scale, bool mystical = false);
    // Present grades only the world. Draw crisp HUD/text after this call.
    void Present();
    void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a);
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

