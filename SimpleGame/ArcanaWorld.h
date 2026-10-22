#pragma once

#include "bp/native.hpp"
#include "LenaWorld.h"
#include <iosfwd>
#include <memory>

namespace Arcana
{
    constexpr float BasicAttackDamage = 60.f;

    struct Village
    {
        std::string id, region;
        std::wstring name;
        RenderPoint center;
    };

    const std::array<Village, 5>& Villages();

    struct Remnant
    {
        Actor::Id slot = 0, visual = 0;
        unsigned generation = 0;
        RenderPoint position{0, 0};
        double minute = 0, mana = 0;
    };

    struct Progress
    {
        int step = 0;
        bool combustion = false;
        bool dispatched = false;
        unsigned boarKills = 0;
        double disturbance = 10;
        RenderPoint dispatchPosition{7, 5};
        std::map<std::string, bp::native::ActiveAction> actions;
        std::array<Lena::Town, 5> towns;
        std::uint64_t respawnSequence = 0;
        double lastEconomy = 0;
        std::vector<Remnant> remnants;
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
        void Enrich(WorldMonitor::Snapshot& snapshot, bool statistics = true) const;
        std::wstring Objective() const;
        void Write(std::ostream& out) const;
        bool Read(std::istream& in, Progress& result, double minutes) const;
        void Restore(Progress progress);
        const Progress& State() const;
        const RenderPoint forge{-4, 5};
        const RenderPoint outpost{7, 5};
        const RenderPoint territory{18, 16};
        Lena::Town& Town(int village);
        const Lena::Town& Town(int village) const;
        int Location(RenderPoint point, float radius = 9) const;
        Lena::Person* LocalGuard(const Lena::Person& reporter) const;
        RenderPoint DrinkingPoint(const Lena::Person& person) const;
        void Share(Lena::Person& source, Lena::Person& target);

      private:
        Lena::World& world;
        bp::native::Content content;
        std::unique_ptr<bp::native::Scheduler> scheduler;
        Progress progress;
        std::map<std::string, std::wstring> objectives;
        std::map<std::string, bp::native::Entity> definitions;
        std::map<std::string, Actor::Id> actorIds;
        double fireCost = 20;
        double fireEfficiency = 2.5;
        double pulse = 0;
        std::vector<Actor::Id> projectiles;
        void UpdateProjectiles(float dt);
        void ClearProjectiles();
        bool Enemy(const Lena::Person& person) const;
        bool Near(RenderPoint point, float distance = 2.5f) const;
        Lena::Person* Named(const std::string& id) const;
        std::vector<RenderPoint> monsterSpawns;
        void PrepareMonsterSpawns();
        void UpdateExpansion(float dt);
        void UpdateEconomy(double seconds);
        void FillVillages(WorldMonitor::Snapshot& snapshot) const;
        bool Respawn(Lena::Person& person);
        void ShowRemnant(Remnant& remnant);
        bool AbsorbRemnant();
    };
} // namespace Arcana
