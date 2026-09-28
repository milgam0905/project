#pragma once

#include "Actor.h"
#include <memory>
#include <unordered_map>
#include <utility>

// Coordinates use the renderer's fixed 1280 x 800 logical viewport.
struct SceneView
{
    Renderer& renderer;
    RenderPoint camera;
    float time;

    RenderPoint Screen(RenderPoint world, float height = 0) const;
    RenderPoint Screen(const Actor& actor) const;
    bool InView(const Actor& actor) const;
};

class SceneGraph
{
  public:
    SceneGraph();
    SceneGraph(const SceneGraph&) = delete;
    SceneGraph& operator=(const SceneGraph&) = delete;

    Actor& Root();
    Actor* Find(Actor::Id id) const;

    template <typename T = Actor, typename... Args> T& Spawn(Actor& parent, Args&&... args)
    {
        std::unique_ptr<T> actor(new T(std::forward<Args>(args)...));
        T& result = *actor;
        Add(std::move(actor), parent);
        return result;
    }

    // Reparenting rejects cycles and foreign actors. World transform is preserved by default.
    bool Reparent(Actor& actor, Actor& parent, bool keepWorldPosition = true);
    void Destroy(Actor& actor);
    void ClearChildren(Actor& parent);
    void Update(float dt);
    void Render(const SceneView& view);

  private:
    std::vector<std::unique_ptr<Actor>> actors;
    std::unordered_map<Actor::Id, Actor*> index;
    Actor::Id nextId = 1;
    bool traversing = false;

    void Add(std::unique_ptr<Actor> actor, Actor& parent);
    void CollectDestroyed();
    void Gather(Actor& actor, std::vector<Actor*>& result) const;
};
