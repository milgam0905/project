#pragma once

#include "Renderer.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class SceneGraph;
struct SceneView;

enum class ActorLayer
{
    Terrain,
    World,
    Effect,
    Interface
};

// SceneGraph owns actors. Gameplay code may keep non-owning references until destruction.
class Actor
{
  public:
    using Id = std::uint64_t;
    using UpdateAction = std::function<void(Actor&, float)>;
    using DrawAction = std::function<void(const Actor&, const SceneView&)>;

    explicit Actor(std::string name = "Actor");
    virtual ~Actor() = default;
    Actor(const Actor&) = delete;
    Actor& operator=(const Actor&) = delete;

    Id GetId() const;
    const std::string& Name() const;
    const Actor* Parent() const;
    const std::vector<Actor*>& Children() const;

    RenderPoint Position() const;
    void SetPosition(RenderPoint worldPosition);
    RenderPoint LocalPosition() const;
    void SetLocalPosition(RenderPoint position);
    float Height() const;
    float LocalHeight() const;
    void SetLocalHeight(float height);

    bool IsActive() const;
    bool IsVisible() const;
    bool IsPendingDestroy() const;
    void SetActive(bool active);
    void SetVisible(bool visible);

    ActorLayer layer = ActorLayer::World;
    float depthBias = 0;
    bool cull = true;
    UpdateAction onUpdate;
    DrawAction onDraw;
    DrawAction onShadow;

    virtual void Update(float dt);
    virtual void Draw(const SceneView& view) const;
    virtual void DrawShadow(const SceneView& view) const;

  private:
    friend class SceneGraph;
    Id id = 0;
    std::string name;
    SceneGraph* owner = nullptr;
    Actor* parent = nullptr;
    std::vector<Actor*> children;
    RenderPoint position = {0, 0};
    float height = 0;
    bool active = true;
    bool visible = true;
    bool pendingDestroy = false;
};
