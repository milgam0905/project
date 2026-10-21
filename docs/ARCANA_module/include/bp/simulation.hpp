#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace bp {
using Id = std::int64_t;
enum class Kind { Npc, Beast };
enum class Role { Civilian, Bandit, Knight, Mage };
enum class EventType { Trade, Robbery, Murder, Love, Dispatch, Invasion, ReturnHome, Kill, ManaGain };
struct Actor {
    Id id=0, region=0, home=0;
    Kind kind=Kind::Npc;
    Role role=Role::Civilian;
    double power=10, mana=0, hunger=0, gold=10, food=2;
    bool alive=true, lured=false;
    unsigned crimes=0;
    std::map<Id,unsigned> weakKills; // Per beast archetype, persistent diminishing returns.
    Id archetype=0;
};
struct Region { Id id=0; double threat=0; };
struct Event { EventType type; Id actor=0, target=0, region=0; double amount=0; std::uint64_t tick=0; };
struct Wbg { std::string id, name, payload; };
struct Snapshot {
    std::uint64_t tick=0; double accumulator=0;
    std::vector<Actor> actors;
    std::vector<Region> regions;
    std::map<Id,Id> deployments; // guard -> destination, no repeated dispatch
    std::map<std::pair<Id,Id>,unsigned> affinity;
};
struct Config {
    double stepSeconds=1, hungerPerTick=.02, invasionHunger=.8;
    double threatDecay=.1, dispatchThreat=3, manaScale=.1;
    unsigned maxStepsPerFrame=8;
};
// Rules run after built-in behavior. Do not mutate the World directly from callbacks:
// return application commands via your own queue, then apply them after advance().
using WbgHandler=std::function<void(const Wbg&,const Snapshot&,std::vector<Event>&)>;
class World {
public:
    explicit World(Config config={});
    void restore(Snapshot state);
    const Snapshot& state() const noexcept { return state_; }
    void setWbg(std::vector<Wbg> rules, std::map<std::string,WbgHandler> handlers,
                std::size_t expectedCount=39);
    void advance(double frameSeconds);
    bool lure(Id beast, bool enabled=true);
    bool kill(Id killer, Id victim); // Call only after authoritative combat confirms kill.
    std::vector<Event> drainEvents();
private:
    Config config_; Snapshot state_;
    std::vector<Wbg> rules_; std::map<std::string,WbgHandler> handlers_;
    std::vector<Event> events_;
    Actor* actor(Id id);
    void step();
    void emit(EventType type,Id a,Id b,Id region,double amount=0);
};
}
