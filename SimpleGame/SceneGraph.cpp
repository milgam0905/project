#include "stdafx.h"
#include "SceneGraph.h"
#include "WorldGeometry.h"
#include "RenderQueue.h"
#include "Profiler.h"
#include <algorithm>
#include <stdexcept>

namespace
{
    // Restore traversal state even if a gameplay callback throws.
    class TraversalScope
    {
      public:
        explicit TraversalScope(bool& value) : value(value)
        {
            if (value)
            {
                throw std::logic_error("SceneGraph traversal cannot be nested");
            }

            value = true;
        }

        ~TraversalScope()
        {
            value = false;
        }

      private:
        bool& value;
    };
} // namespace

RenderPoint SceneView::Screen(RenderPoint world, float height) const
{
    const RenderPoint projected = WorldGeometry::Project(world);
    return {projected.x - camera.x + 640.f, projected.y - camera.y + 420.f - height};
}

RenderPoint SceneView::Screen(const Actor& actor) const
{
    return Screen(actor.Position(), actor.Height());
}

bool SceneView::InView(const Actor& actor) const
{
    const RenderPoint foot = Screen(actor);
    return !actor.cull ||
           (foot.x >= -200.f && foot.x <= 1480.f && foot.y >= -100.f && foot.y <= 1100.f);
}

SceneGraph::SceneGraph()
{
    std::unique_ptr<Actor> root(new Actor("Root"));
    root->id = nextId++;
    root->owner = this;
    index.emplace(root->id, root.get());
    actors.push_back(std::move(root));
}

Actor& SceneGraph::Root()
{
    return *actors.front();
}

Actor* SceneGraph::Find(Actor::Id id) const
{
    const auto found = index.find(id);
    return found != index.end() && !found->second->pendingDestroy ? found->second : nullptr;
}

void SceneGraph::Add(std::unique_ptr<Actor> actor, Actor& parent)
{
    if (parent.owner != this || parent.pendingDestroy)
    {
        throw std::invalid_argument("Actor parent must belong to this scene and be alive");
    }

    actor->id = nextId++;
    actor->owner = this;
    actor->parent = &parent;
    if (actors.size() == actors.capacity())
    {
        actors.reserve(actors.capacity() * 2 + 1);
    }

    if (parent.children.size() == parent.children.capacity())
    {
        parent.children.reserve(parent.children.capacity() * 2 + 1);
    }

    index.emplace(actor->id, actor.get());
    parent.children.push_back(actor.get());
    actors.push_back(std::move(actor));
}

bool SceneGraph::Reparent(Actor& actor, Actor& parent, bool keepWorldPosition)
{
    if (actor.owner != this || parent.owner != this || &actor == &Root() || actor.pendingDestroy ||
        parent.pendingDestroy)
    {
        return false;
    }

    for (const Actor* ancestor = &parent; ancestor; ancestor = ancestor->parent)
    {
        if (ancestor == &actor)
        {
            return false;
        }
    }

    const RenderPoint position = actor.Position();
    const float height = actor.Height();
    if (parent.children.size() == parent.children.capacity())
    {
        parent.children.reserve(parent.children.capacity() * 2 + 1);
    }

    auto& siblings = actor.parent->children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), &actor), siblings.end());
    actor.parent = &parent;
    parent.children.push_back(&actor);
    if (keepWorldPosition)
    {
        actor.SetPosition(position);
        actor.SetLocalHeight(height - parent.Height());
    }

    return true;
}

void SceneGraph::Destroy(Actor& actor)
{
    if (actor.owner != this || &actor == &Root() || actor.pendingDestroy)
    {
        return;
    }

    actor.pendingDestroy = true;
    destructionPending = true;
    for (Actor* child : actor.children)
    {
        Destroy(*child);
    }
}

void SceneGraph::ClearChildren(Actor& parent)
{
    if (parent.owner != this)
    {
        return;
    }

    for (Actor* child : parent.children)
    {
        Destroy(*child);
    }

    CollectDestroyed();
}

void SceneGraph::CollectDestroyed()
{
    if (traversing || !destructionPending)
    {
        return;
    }

    Profiler::Scope profile("scene.collect_destroyed");
    destructionPending = false;

    // Unlink while every parent/child pointer is still valid, then release ownership.
    for (const auto& actor : actors)
    {
        if (actor->pendingDestroy)
        {
            index.erase(actor->id);
        }

        auto& children = actor->children;
        children.erase(std::remove_if(children.begin(),
                                      children.end(),
                                      [](const Actor* child)
                                      {
                                          return child->pendingDestroy;
                                      }),
                       children.end());
    }

    actors.erase(std::remove_if(actors.begin(),
                                actors.end(),
                                [](const std::unique_ptr<Actor>& actor)
                                {
                                    return actor->pendingDestroy;
                                }),
                 actors.end());
}

void SceneGraph::Gather(Actor& actor, std::vector<Actor*>& result) const
{
    if (!actor.IsActive())
    {
        return;
    }

    result.push_back(&actor);
    for (Actor* child : actor.children)
    {
        Gather(*child, result);
    }
}

void SceneGraph::Update(float dt)
{
    Profiler::Scope profile("scene.update");
    Profiler::Gauge("scene.actor_count", static_cast<double>(actors.size()));
    CollectDestroyed();
    {
        TraversalScope scope(traversing);
        auto& snapshot = updateSnapshot;
        snapshot.clear();
        {
            Profiler::Scope gatherProfile("scene.gather");
            Gather(Root(), snapshot);
        }
        std::uint64_t updates = 0;
        for (Actor* actor : snapshot)
        {
            if (actor->IsActive())
            {
                ++updates;
                actor->Update(dt);
            }
        }
        Profiler::Count("scene.actor_updates", updates);
    }

    CollectDestroyed();
}

void SceneGraph::Render(const SceneView& view)
{
    Profiler::Scope profile("scene.render");
    CollectDestroyed();
    {
        TraversalScope scope(traversing);
        auto& snapshot = renderSnapshot;
        snapshot.clear();
        {
            Profiler::Scope gatherProfile("scene.gather");
            Gather(Root(), snapshot);
        }
        {
            Profiler::Scope sortProfile("scene.sort");
            visibleEntries.clear();
            std::uint64_t culled = 0;
            for (Actor* actor : snapshot)
            {
                if (!actor->IsVisible() || !view.InView(*actor))
                {
                    ++culled;
                    continue;
                }
                visibleEntries.push_back({actor,
                                          actor->layer,
                                          WorldGeometry::Project(actor->Position()).y -
                                              actor->Height() + actor->depthBias,
                                          actor->GetId()});
            }
            Profiler::Count("scene.hidden_or_culled", culled);
            Profiler::Gauge("scene.sort_candidates", static_cast<double>(visibleEntries.size()));
            std::sort(visibleEntries.begin(),
                      visibleEntries.end(),
                      [](const RenderEntry& a, const RenderEntry& b)
                      {
                          if (a.layer != b.layer)
                          {
                              return a.layer < b.layer;
                          }
                          return a.depth == b.depth ? a.id < b.id : a.depth < b.depth;
                      });
        }

        auto drawLayer = [&](ActorLayer layer)
        {
            const char* key = layer == ActorLayer::Terrain  ? "render.terrain"
                              : layer == ActorLayer::World  ? "render.world"
                              : layer == ActorLayer::Effect ? "render.effects"
                                                            : "render.ui";
            Profiler::Scope passProfile(key);
            RenderStats::SetPass(key);
            Profiler::GpuScope gpuProfile(key);
            std::uint64_t drawn = 0;
            for (const RenderEntry& entry : visibleEntries)
            {
                const Actor* actor = entry.actor;
                if (entry.layer == layer && actor->IsVisible())
                {
                    ++drawn;
                    actor->Draw(view);
                }
            }
            Profiler::Count("scene.actor_draws", drawn);
            RenderQueue::Flush();
        };

        view.renderer.BeginScene(view.camera, view.time);
        drawLayer(ActorLayer::Terrain);
        {
            Profiler::Scope shadowProfile("render.shadows");
            RenderStats::SetPass("render.shadows");
            Profiler::GpuScope gpuProfile("render.shadows");
            for (const RenderEntry& entry : visibleEntries)
            {
                const Actor* actor = entry.actor;
                if (entry.layer == ActorLayer::World && actor->IsVisible())
                {
                    actor->DrawShadow(view);
                }
            }

            RenderQueue::Flush();
        }
        drawLayer(ActorLayer::World);
        drawLayer(ActorLayer::Effect);
        view.renderer.Present();
        drawLayer(ActorLayer::Interface);
    }

    CollectDestroyed();
}
