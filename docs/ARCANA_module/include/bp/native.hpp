#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <utility>
namespace bp::native {
// Native v1 database adapter. TEXT IDs are preserved, including save/profile/species IDs.
struct Node { int id=0; std::string name,category,description; };
struct ProfileNode { double baseWeight=1; bool enabled=false; };
struct Entity {
    std::string id,save,species,name,kind,profile,occupation,region,homeRegion,stateJson;
    bool alive=true;
    double health=0,manaCurrent=0,manaMax=0,hunger=0,thirst=0,fatigue=0,fear=0,anger=0,stress=0,wealth=0;
};
struct Territory { std::string id,region,owner,species; double food=0,disturbance=0,pressure=0; };
struct Relationship {
    std::string source,target;
    double affection=0,trust=0,attraction=0,commitment=0;
};
struct ManaConfig {
    double rate=.25,exponent=1.3,cap=1.5,repeatDecay=.12,windowSeconds=60;
};
struct BehaviorConfig { double hysteresis=.15,minSeconds=2; bool requireValidTarget=true; };
struct EventConfig {
    double invasionHunger=75,invasionDisturbance=80,securityBelow=40;
    int reportedCrimesMin=3,availableKnightsMin=1;
    double romanceAffection=65,romanceAttraction=50,marriageCommitment=75;
};
struct ActiveAction { int node=0; std::string target; double startedSeconds=0,minimumEndSeconds=0,cooldownUntilSeconds=0; };
struct Content {
    std::string save; double worldHours=0; std::int64_t seed=0;
    std::vector<Node> nodes;
    std::vector<Entity> entities;
    std::vector<Territory> territories;
    std::vector<Relationship> relationships;
    std::map<std::string,std::map<int,ProfileNode>> profiles;
    std::map<std::string,std::string> rulesJson;
    std::map<std::string,ActiveAction> activeActions;
    ManaConfig mana;BehaviorConfig behavior;EventConfig events;
};
Content load(const std::string& utf8DatabasePath,const std::string& saveId="demo");
// Engine supplies target validity, navigation/combat/social facts and normalized utility terms.
// Invalid/unimplemented actions return nullopt. No arbitrary code from the DB is executed.
struct Candidate {
    std::string target;
    // need,emotion,relationship,environment,personality,memory,goal,random
    std::array<double,8> terms{};
    bool valid=true,requiresTarget=false;
};
enum class Interrupt { None,LethalThreat,Fire,UrgentProtect };
struct Intent { std::string entity; int node=0; std::string name,target; double utility=0; bool changed=false; };
class Host {
public:
    virtual ~Host()=default;
    virtual std::optional<Candidate> probe(const Entity&,const Node&)=0;
    virtual void execute(const Intent&)=0; // Called on every decide(), including retained actions.
};
class Scheduler {
public:
    explicit Scheduler(const Content& content);
    std::optional<Intent> decide(const Entity&,double nowSeconds,Host&,Interrupt=Interrupt::None);
    const std::map<std::string,ActiveAction>& actions() const { return active_; }
    void clear(const std::string& entityId){active_.erase(entityId);}
private:
    std::vector<Node> nodes_;std::map<std::string,std::map<int,ProfileNode>> profiles_;
    BehaviorConfig config_;std::map<std::string,ActiveAction> active_;
};
bool shouldLeaveTerritory(const Entity&,const Territory&,const EventConfig&,
                         bool lured=false,bool territoryLost=false,bool breeding=false);
bool shouldDispatch(double security,int reportedUnresolvedCrimes,int availableKnights,const EventConfig&);
bool canDate(const Relationship& a,const Relationship& b,bool mutualConsent,const EventConfig& config={});
bool canMarry(const Relationship& a,const Relationship& b,bool mutualConsent,bool eligible,const EventConfig& config={});
double manaGain(double beastMana,double mageManaMax,unsigned previousKills,const ManaConfig&);
struct Eligibility { bool mageBloodline=false,eligibleContributor=false,inAbsorptionRange=false; };
// Runtime DB must be a COPY prepared with tools/prepare_runtime.py.
// These operations reopen one connection per transaction. Never pass the original content DB.
class RuntimeStore {
public:
    explicit RuntimeStore(std::string runtimePath):path_(std::move(runtimePath)){}
    bool recordDeath(const std::string& save,const std::string& beast,double hour);
    double absorb(const std::string& save,const std::string& mage,const std::string& beast,
                  double hour,const Eligibility&);
    void saveActions(const std::string& save,double worldHours,
                     const std::map<std::string,ActiveAction>&);
private:std::string path_;
};
}
