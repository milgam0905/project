#pragma once

#include "SceneGraph.h"
#include "WorldMonitor.h"
#include <array>
#include <deque>
#include <set>
#include <string>
#include <vector>
#include <memory>

namespace Arcana
{
    class Module;
}

namespace Lena
{
    enum class Species
    {
        Human,
        Wolf,
        Deer
    };
    enum class Role
    {
        Resident,
        Merchant,
        Guard,
        Healer,
        Scholar,
        Bandit,
        Player
    };
    enum class Action
    {
        Investigate = 3,
        Idle = 6,
        Wander = 7,
        Travel = 8,
        Sleep = 9,
        Eat = 10,
        Drink = 11,
        Work = 12,
        Rest = 13,
        Talk = 14,
        Trade = 16,
        Help = 17,
        Follow = 18,
        Warn = 20,
        Threaten = 23,
        Steal = 25,
        Chase = 26,
        Surrender = 27,
        Attack = 28,
        Defend = 29,
        Flee = 31,
        Protect = 33,
        Hunt = 34,
        Kill = 35,
        InspectBody = 36,
        Loot = 37,
        Mourn = 38,
        Report = 39
    };

    const wchar_t* Label(Action action);
    float Distance(RenderPoint a, RenderPoint b);

    struct State
    {
        float health = 100, hunger = 20, thirst = 20, fatigue = 20;
        float fear = 0, grief = 0, anger = 0, exposure = 0;
        float food = 0, medicine = 0, coins = 0, corpseFood = 0;
        float cooldown = 0, decision = 0, held = 0, threatSeconds = 0;
        float facing = 1, stride = 0;
        bool dead = false, surrendered = false, detained = false;
        Action action = Action::Idle;
        Actor::Id target = 0, aggressor = 0;
        std::uint64_t report = 0;
        RenderPoint destination{0, 0};
        std::set<std::uint64_t> known;
        float manaCurrent = 0, manaMax = 0;
        double deathMinute = 0;
        bool playerCredit = false, absorbed = false;
        unsigned generation = 0;
        int travelTown = -1;
        float travelWait = 0;
        RenderPoint habitat{0, 0};
    };

    class Person : public Actor
    {
      public:
        Person(const std::string& key, const std::wstring& name, Species species, Role role);
        std::string key;
        std::wstring displayName;
        Species species;
        Role role;
        int settlement = 0;
        std::array<int, 7> traits{{50, 50, 50, 50, 50, 50, 50}};
        RenderPoint home{0, 0}, work{0, 0};
        State state;
        std::vector<RenderPoint> path;
        size_t pathIndex = 0;
        float routeTimer = 0;
        float sightTimer = 0;
        bool goalVisible = false;
        RenderPoint sightGoal{0, 0};
        RenderPoint visualDirection{0, 1};
        float motionSeconds = 0;
        float maxHealth = 100;
        bool beast = false;
        std::string profile, speciesId, region;
        bool courier = false;
    };

    struct Fact
    {
        std::uint64_t id = 0;
        Actor::Id actor = 0, victim = 0;
        double minute = 0;
        bool death = false, intentional = false, crime = false, reported = false;
        RenderPoint position{0, 0};
        unsigned actorGeneration = 0, victimGeneration = 0;
    };

    struct Town
    {
        double food = 252, water = 168, treasury = 420, morale = 60, security = 58;
        double pollution = 22, plants = 700, production = 0, consumption = 0;
    };

    class World
    {
      public:
        static constexpr float Extent = 26;
        static constexpr int Side = 104;
        float extent = Extent;
        RenderPoint lakeCenter{14, -8};
        int side = Side, bucketSide = 9;
        SceneGraph scene;
        std::vector<Actor::Id> people;
        std::vector<RenderPoint> houses, trees;
        std::vector<Fact> facts;
        std::deque<std::wstring> messages;
        Town town;
        double minutes = 0;
        std::uint64_t epoch = 0, tick = 0, factSequence = 0;
        int prologue = 0, investigation = 0;
        double prologueStarted = 0;
        bool waterClue = false, forestClue = false, escorted = false;
        bool hunting = false, corridor = false, gameOver = false;
        double corridorWork = 0;
        Actor::Id playerId = 0, liaId = 0, edwinId = 0, darianId = 0, selinId = 0;
        const RenderPoint canal{-17, -17}, square{0, 3}, forest{15, 12}, board{0, 5};

        explicit World(bool arcanaMode = false);
        ~World();
        std::unique_ptr<Arcana::Module> arcana;
        static bool SelfTest();
        static bool ArcanaSelfTest();
        Person* Find(Actor::Id id) const;
        Person& Player() const;
        void Update(float dt);
        bool Move(Person& person, RenderPoint destination, float distance);
        bool Walkable(RenderPoint position) const;
        bool Visible(RenderPoint a, RenderPoint b) const;
        bool Water(RenderPoint position) const;
        RenderPoint Safe(RenderPoint position) const;
        void PlayerAttack();
        void Interact();
        void Choose(int choice);
        void Log(const std::wstring& text);
        WorldMonitor::Snapshot Capture(bool paused,
                                       size_t begin = 0,
                                       size_t count = static_cast<size_t>(-1),
                                       bool statistics = true) const;
        bool Save(const std::wstring& file) const;
        bool Load(const std::wstring& file);
        int Population(Species species, bool aliveOnly = true) const;

      private:
        friend class Arcana::Module;
        std::vector<unsigned char> walk;
        std::vector<int> safeCells;
        std::vector<std::vector<Actor::Id>> buckets;
        float pulse = 0;
        size_t observationCursor = 0;
        float observationCredits = 0;
        double economyMinute = 0;
        int routeBudget = 0;
        int routeCredits = 0;
        std::vector<int> routePrevious, routeCost;
        std::vector<unsigned> routeStamp;
        unsigned routeSearch = 0;
        Person& Spawn(const std::string& key,
                      const std::wstring& name,
                      Species species,
                      Role role,
                      RenderPoint position);
        void BuildNavigation();
        bool Route(Person& person, RenderPoint destination);
        int Cell(RenderPoint position) const;
        RenderPoint Position(int cell) const;
        void RebuildIndex();
        std::vector<Actor::Id> Nearby(RenderPoint p, float radius) const;
        void Decide(Person& person);
        void Act(Person& person, float dt);
        void Damage(Person& attacker, Person& target, float damage, bool intentional = false);
        bool CanKill(const Person& attacker, const Person& target) const;
        void Observe(Person& observer);
        void Economy(double elapsed);
        void Story(float dt = 0);
        void SetAction(Person& person, Action action, Actor::Id target = 0);
        bool Service(Person& person);
        Fact* GetFact(std::uint64_t id);
    };
} // namespace Lena
