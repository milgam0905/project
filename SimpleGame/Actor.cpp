#include "stdafx.h"
#include "Actor.h"
#include <utility>

Actor::Actor(std::string name) : name(std::move(name))
{
}

Actor::Id Actor::GetId() const
{
    return id;
}

const std::string& Actor::Name() const
{
    return name;
}

const Actor* Actor::Parent() const
{
    return parent;
}

const std::vector<Actor*>& Actor::Children() const
{
    return children;
}

RenderPoint Actor::Position() const
{
    const RenderPoint origin = parent ? parent->Position() : RenderPoint{0, 0};
    return {origin.x + position.x, origin.y + position.y};
}

void Actor::SetPosition(RenderPoint worldPosition)
{
    const RenderPoint origin = parent ? parent->Position() : RenderPoint{0, 0};
    position = {worldPosition.x - origin.x, worldPosition.y - origin.y};
}

RenderPoint Actor::LocalPosition() const
{
    return position;
}

void Actor::SetLocalPosition(RenderPoint value)
{
    position = value;
}

float Actor::Height() const
{
    return height + (parent ? parent->Height() : 0.f);
}

float Actor::LocalHeight() const
{
    return height;
}

void Actor::SetLocalHeight(float value)
{
    height = value;
}

bool Actor::IsActive() const
{
    return active && !pendingDestroy && (!parent || parent->IsActive());
}

bool Actor::IsVisible() const
{
    return IsActive() && visible && (!parent || parent->IsVisible());
}

bool Actor::IsPendingDestroy() const
{
    return pendingDestroy;
}

void Actor::SetActive(bool value)
{
    active = value;
}

void Actor::SetVisible(bool value)
{
    visible = value;
}

void Actor::Update(float dt)
{
    if (onUpdate)
    {
        onUpdate(*this, dt);
    }
}

void Actor::Draw(const SceneView& view) const
{
    if (onDraw)
    {
        onDraw(*this, view);
    }
}

void Actor::DrawShadow(const SceneView& view) const
{
    if (onShadow)
    {
        onShadow(*this, view);
    }
}
