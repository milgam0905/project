#pragma once

#include "bp/native.hpp"
#include "LenaWorld.h"
#include <iosfwd>
#include <memory>

namespace Arcana
{
    struct Progress
    {
        int step = 0;
        bool combustion = false;
        bool dispatched = false;
        unsigned boarKills = 0;
        double disturbance = 10;
        RenderPoint dispatchPosition{7, 5};
        std::map<std::string, bp::native::ActiveAction> actions;
    };

    // Content IDs remain database TEXT IDs; SceneGraph alone owns the live Actors.
    class Module
    {
      public:
        explicit Module(Lena::World& world);
        void Seed();
        void Decide(Lena::Person& person);
        void Update(float dt);
        void Interact();
        void Cast();
        void Attack();
        RenderPoint moveDirection{.7071068f, .7071068f};
        float attackSeconds = 0;
        void Rest(float dt);
        float fireSeconds = 0;
        RenderPoint firePosition{0, 0};
        void Hit(Lena::Person& attacker, Lena::Person& victim);
        void Enrich(WorldMonitor::Snapshot& snapshot) const;
        std::wstring Objective() const;
        void Write(std::ostream& out) const;
        bool Read(std::istream& in, Progress& result, double minutes) const;
        void Restore(Progress progress);
        const Progress& State() const;
        const RenderPoint forge{-4, 5};
        const RenderPoint outpost{7, 5};
        const RenderPoint territory{18, 16};

      private:
        Lena::World& world;
        bp::native::Content content;
        std::unique_ptr<bp::native::Scheduler> scheduler;
        Progress progress;
        std::map<std::string, std::wstring> objectives;
        std::map<std::string, bp::native::Entity> definitions;
        double fireCost = 20;
        double fireEfficiency = 2.5;
        double pulse = 0;
        std::vector<Actor::Id> projectiles;
        void UpdateProjectiles(float dt);
        void ClearProjectiles();
        bool Enemy(const Lena::Person& person) const;
        bool Near(RenderPoint point, float distance = 2.5f) const;
        Lena::Person* Named(const std::string& id) const;
    };
} // namespace Arcana
