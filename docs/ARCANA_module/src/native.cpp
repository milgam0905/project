#include "bp/native.hpp"
#include <sqlite3.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>
namespace bp::native {
namespace {
struct Db {
    sqlite3* p=nullptr;
    Db(const std::string& path,bool write=false){
        int rc=sqlite3_open_v2(path.c_str(),&p,write?SQLITE_OPEN_READWRITE:SQLITE_OPEN_READONLY,nullptr);
        if(rc!=SQLITE_OK){std::string m=p?sqlite3_errmsg(p):"SQLite open failed";sqlite3_close(p);p=nullptr;throw std::runtime_error(m);}
        sqlite3_busy_timeout(p,2000);
        try{exec("PRAGMA foreign_keys=ON");}catch(...){sqlite3_close(p);p=nullptr;throw;}
    }
    ~Db(){sqlite3_close(p);}Db(const Db&)=delete;Db& operator=(const Db&)=delete;
    void exec(const char* sql){if(sqlite3_exec(p,sql,nullptr,nullptr,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(p));}
};
struct Query {
    Db& db;sqlite3_stmt* s=nullptr;
    Query(Db& d,const char* sql):db(d){if(sqlite3_prepare_v2(d.p,sql,-1,&s,nullptr)!=SQLITE_OK){sqlite3_finalize(s);s=nullptr;throw std::runtime_error(sqlite3_errmsg(d.p));}}
    ~Query(){sqlite3_finalize(s);}Query(const Query&)=delete;
    void bind(int i,const std::string& v){if(sqlite3_bind_text(s,i,v.data(),static_cast<int>(v.size()),SQLITE_TRANSIENT)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db.p));}
    void bind(int i,double v){if(!std::isfinite(v)||sqlite3_bind_double(s,i,v)!=SQLITE_OK)throw std::runtime_error("Invalid numeric bind");}
    bool row(){int rc=sqlite3_step(s);if(rc==SQLITE_ROW)return true;if(rc==SQLITE_DONE)return false;throw std::runtime_error(sqlite3_errmsg(db.p));}
    void done(){if(row())throw std::runtime_error("Unexpected result row");}
    std::string text(int i){if(sqlite3_column_type(s,i)==SQLITE_NULL)return {};return {reinterpret_cast<const char*>(sqlite3_column_text(s,i)),static_cast<std::size_t>(sqlite3_column_bytes(s,i))};}
    double number(int i){int type=sqlite3_column_type(s,i);if(type!=SQLITE_FLOAT&&type!=SQLITE_INTEGER)throw std::runtime_error("Missing/non-numeric DB value");double x=sqlite3_column_double(s,i);if(!std::isfinite(x))throw std::runtime_error("Non-finite DB value");return x;}
    std::int64_t integer(int i){if(sqlite3_column_type(s,i)!=SQLITE_INTEGER)throw std::runtime_error("Expected DB integer");return sqlite3_column_int64(s,i);}
};
struct Tx {Db&d;bool committed=false;Tx(Db&v,bool write):d(v){d.exec(write?"BEGIN IMMEDIATE":"BEGIN");}~Tx(){if(!committed)sqlite3_exec(d.p,"ROLLBACK",nullptr,nullptr,nullptr);}void commit(){d.exec("COMMIT");committed=true;}};
void runtime(Db& d){Query q(d,"SELECT value FROM bp_runtime_meta WHERE key='format'");if(!q.row()||q.text(0)!="bp-native-v1")throw std::runtime_error("Not a prepared runtime copy");}
const char* names[]={"Evaluate","Observe","Investigate","Remember","Decide","Idle","Wander","Travel","Sleep","Eat","Drink","Work","Rest","Talk","Greet","Trade","Help","Follow","Avoid","Warn","CallHelp","Argue","Threaten","Intimidate","Steal","Chase","Surrender","Attack","Defend","Dodge","Flee","SeekCover","Protect","Hunt","Kill","InspectBody","Loot","Mourn","Report"};
void positive(double v,const char* field){if(!std::isfinite(v)||v<=0)throw std::runtime_error(std::string("Invalid ")+field);}
void configValid(const ManaConfig&c){positive(c.rate,"mana rate");positive(c.exponent,"exponent");positive(c.cap,"cap");positive(c.windowSeconds,"window");if(!std::isfinite(c.repeatDecay)||c.repeatDecay<0)throw std::runtime_error("Invalid repeat decay");}
ManaConfig readMana(Db&d){
    Query q(d,"SELECT json_extract(config_json,'$.base_absorption_rate'),json_extract(config_json,'$.relative_power_exponent'),json_extract(config_json,'$.relative_factor_cap'),json_extract(config_json,'$.repeat_decay'),json_extract(config_json,'$.absorption_window_seconds'),json_extract(config_json,'$.requires_mage_bloodline'),json_extract(config_json,'$.max_mana_only'),json_extract(config_json,'$.credit_rule') FROM simulation_rules WHERE id='mana'");
    if(!q.row())throw std::runtime_error("Missing mana config");ManaConfig c{q.number(0),q.number(1),q.number(2),q.number(3),q.number(4)};configValid(c);
    if(q.integer(5)!=1||q.integer(6)!=1||q.text(7)!="eligible_contributor_not_nearby_auto")throw std::runtime_error("Unsupported mana policy; implement explicit migration");return c;
}
std::string key(const std::string& a,const std::string& b){return std::to_string(a.size())+":"+a+std::to_string(b.size())+":"+b;}
void validateTime(Db& d,const std::string& save,double hour){
    if(!std::isfinite(hour)||hour<0)throw std::invalid_argument("Invalid world hour");
    Query q(d,"SELECT world_time_hours FROM world_saves WHERE id=?");q.bind(1,save);
    if(!q.row()||hour<q.number(0))throw std::invalid_argument("Missing save or time moved backwards");
}
void setTime(Db&d,const std::string& save,double hour){Query q(d,"UPDATE world_saves SET world_time_hours=? WHERE id=?");q.bind(1,hour);q.bind(2,save);q.done();}
}
Content load(const std::string&path,const std::string&save){
    Db d(path);Tx tx(d,false);Content c;c.save=save;
    {Query q(d,"SELECT world_time_hours,seed FROM world_saves WHERE id=?");q.bind(1,save);if(!q.row())throw std::runtime_error("Save not found: "+save);c.worldHours=q.number(0);c.seed=q.integer(1);}
    {Query q(d,"SELECT id,name,category,description FROM behavior_nodes ORDER BY id");while(q.row()){
        Node n{static_cast<int>(q.integer(0)),q.text(1),q.text(2),q.text(3)};
        if(n.id!=static_cast<int>(c.nodes.size())+1||n.id>39||n.name!=names[n.id-1])throw std::runtime_error("Unsupported behavior_nodes ID/name contract");c.nodes.push_back(std::move(n));}
        if(c.nodes.size()!=39)throw std::runtime_error("Expected exactly 39 behavior_nodes");}
    {Query q(d,"SELECT profile_id,node_id,base_weight,enabled FROM profile_nodes ORDER BY profile_id,node_id");while(q.row()){
        int id=static_cast<int>(q.integer(1));double weight=q.number(2);auto enabled=q.integer(3);
        if(id<1||id>39||weight<0||(enabled!=0&&enabled!=1))throw std::runtime_error("Invalid profile node");c.profiles[q.text(0)][id]={weight,enabled==1};}}
    {Query q(d,"SELECT e.id,e.species_id,e.display_name,s.kind,e.profile_id,COALESCE(e.occupation,''),COALESCE(e.region_id,''),e.alive,e.health,e.mana_current,e.mana_max,e.hunger,e.thirst,e.fatigue,e.fear,e.anger,e.stress,e.wealth,e.state_json FROM entities e JOIN species s ON s.id=e.species_id WHERE e.save_id=? ORDER BY e.id");q.bind(1,save);while(q.row()){
        Entity e;e.save=save;e.id=q.text(0);e.species=q.text(1);e.name=q.text(2);e.kind=q.text(3);e.profile=q.text(4);e.occupation=q.text(5);e.region=q.text(6);e.homeRegion=e.region;e.alive=q.integer(7)!=0;
        e.health=q.number(8);e.manaCurrent=q.number(9);e.manaMax=q.number(10);e.hunger=q.number(11);e.thirst=q.number(12);e.fatigue=q.number(13);e.fear=q.number(14);e.anger=q.number(15);e.stress=q.number(16);e.wealth=q.number(17);e.stateJson=q.text(18);
        if(!c.profiles.count(e.profile)||c.profiles.at(e.profile).size()!=39)throw std::runtime_error("Missing complete entity profile: "+e.profile);
        if(e.manaMax<0||e.manaCurrent<0||e.hunger<0||e.hunger>100||e.thirst<0||e.thirst>100||e.fatigue<0||e.fatigue>100)throw std::runtime_error("Invalid entity needs/mana");
        c.entities.push_back(std::move(e));}}
    {Query q(d,"SELECT id,region_id,COALESCE(dominant_entity_id,''),COALESCE(species_id,''),food_supply,disturbance,territorial_pressure FROM territories WHERE save_id=? ORDER BY id");q.bind(1,save);while(q.row())c.territories.push_back({q.text(0),q.text(1),q.text(2),q.text(3),q.number(4),q.number(5),q.number(6)});}
    for(auto&e:c.entities)for(const auto&t:c.territories)if(t.owner==e.id){e.homeRegion=t.region;break;}
    {Query q(d,"SELECT source_id,target_id,affection,trust,attraction,commitment FROM relationships WHERE save_id=? ORDER BY source_id,target_id");q.bind(1,save);while(q.row())c.relationships.push_back({q.text(0),q.text(1),q.number(2),q.number(3),q.number(4),q.number(5)});}
    {Query q(d,"SELECT id,config_json,json_valid(config_json) FROM simulation_rules ORDER BY id");while(q.row()){if(q.integer(2)!=1)throw std::runtime_error("Invalid rule JSON");c.rulesJson[q.text(0)]=q.text(1);}}
    c.mana=readMana(d);
    {Query q(d,"SELECT json_extract(config_json,'$.hysteresis'),json_extract(config_json,'$.min_action_seconds'),json_extract(config_json,'$.require_valid_target') FROM simulation_rules WHERE id='wbg'");if(!q.row())throw std::runtime_error("Missing wbg config");c.behavior={q.number(0),q.number(1),q.integer(2)!=0};if(c.behavior.hysteresis<0||c.behavior.minSeconds<0)throw std::runtime_error("Invalid WBG policy");}
    {Query q(d,"SELECT json_extract(trigger_json,'$.hunger_above'),json_extract(trigger_json,'$.or_disturbance_above') FROM event_templates WHERE id='beast_invasion'");if(!q.row())throw std::runtime_error("Missing invasion template");c.events.invasionHunger=q.number(0);c.events.invasionDisturbance=q.number(1);}
    {Query q(d,"SELECT json_extract(trigger_json,'$.security_below'),json_extract(trigger_json,'$.reported_crimes_min'),json_extract(trigger_json,'$.available_knights_min') FROM event_templates WHERE id='dispatch_knights'");if(!q.row())throw std::runtime_error("Missing dispatch template");c.events.securityBelow=q.number(0);c.events.reportedCrimesMin=static_cast<int>(q.integer(1));c.events.availableKnightsMin=static_cast<int>(q.integer(2));}
    {Query q(d,"SELECT json_extract(trigger_json,'$.mutual_affection_min'),json_extract(trigger_json,'$.mutual_attraction_min'),json_extract(trigger_json,'$.consent') FROM event_templates WHERE id='romance'");if(!q.row())throw std::runtime_error("Missing romance template");c.events.romanceAffection=q.number(0);c.events.romanceAttraction=q.number(1);if(q.integer(2)!=1)throw std::runtime_error("Unsupported romance consent policy");}
    {Query q(d,"SELECT json_extract(trigger_json,'$.mutual_commitment_min'),json_extract(trigger_json,'$.consent'),json_extract(trigger_json,'$.eligible') FROM event_templates WHERE id='marriage'");if(!q.row())throw std::runtime_error("Missing marriage template");c.events.marriageCommitment=q.number(0);if(q.integer(1)!=1||q.integer(2)!=1)throw std::runtime_error("Unsupported marriage consent policy");}
    {Query q(d,"SELECT a.entity_id,a.node_id,COALESCE(a.target_id,''),a.started_hour,a.minimum_end_hour,a.cooldown_until_hour FROM active_actions a JOIN entities e ON e.id=a.entity_id WHERE e.save_id=?");q.bind(1,save);while(q.row()){
        ActiveAction a{static_cast<int>(q.integer(1)),q.text(2),q.number(3)*3600,q.number(4)*3600,q.number(5)*3600};
        if(a.node<1||a.node>39||a.startedSeconds<0||a.minimumEndSeconds<a.startedSeconds||a.cooldownUntilSeconds<0)throw std::runtime_error("Invalid saved action");c.activeActions[q.text(0)]=std::move(a);}}
    tx.commit();return c;
}
Scheduler::Scheduler(const Content&c):nodes_(c.nodes),profiles_(c.profiles),config_(c.behavior),active_(c.activeActions){}
std::optional<Intent> Scheduler::decide(const Entity&e,double now,Host&host,Interrupt interrupt){
    if(!std::isfinite(now)||now<0)throw std::invalid_argument("Invalid decision time");
    if(!e.alive){active_.erase(e.id);return std::nullopt;}
    auto profile=profiles_.find(e.profile);if(profile==profiles_.end())throw std::invalid_argument("Unknown behavior profile");
    std::vector<Intent> candidates;
    for(const auto& n:nodes_){
        auto p=profile->second.find(n.id);if(p==profile->second.end()||!p->second.enabled)continue;
        auto probe=host.probe(e,n);if(!probe||!probe->valid)continue;
        if(config_.requireValidTarget&&probe->requiresTarget&&probe->target.empty())continue;
        double score=p->second.baseWeight;
        for(double term:probe->terms){if(!std::isfinite(term))throw std::invalid_argument("Non-finite utility term");score+=term;}
        if(!std::isfinite(score))throw std::invalid_argument("Utility overflow");
        candidates.push_back({e.id,n.id,n.name,probe->target,score,false});
    }
    if(candidates.empty()){active_.erase(e.id);return std::nullopt;}
    auto best=std::max_element(candidates.begin(),candidates.end(),[](const Intent&a,const Intent&b){return a.utility<b.utility;});
    auto old=active_.find(e.id);
    if(old!=active_.end()){
        if(now<old->second.startedSeconds)throw std::invalid_argument("Decision time moved backwards");
        auto previous=std::find_if(candidates.begin(),candidates.end(),[&](const Intent&i){return i.node==old->second.node&&i.target==old->second.target;});
        if(interrupt==Interrupt::None&&previous!=candidates.end()&&
           (now<old->second.minimumEndSeconds||now<old->second.cooldownUntilSeconds||
            best->utility<=previous->utility+config_.hysteresis))best=previous;
    }
    Intent out=*best;out.changed=old==active_.end()||old->second.node!=out.node||old->second.target!=out.target;
    if(out.changed)active_[e.id]={out.node,out.target,now,now+config_.minSeconds,now};
    host.execute(out);return out;
}
bool shouldLeaveTerritory(const Entity&e,const Territory&t,const EventConfig&c,bool lure,bool lost,bool breeding){return e.alive&&e.kind=="beast"&&(e.hunger>c.invasionHunger||t.disturbance>c.invasionDisturbance||lure||lost||breeding);}
bool shouldDispatch(double security,int crimes,int knights,const EventConfig&c){return security<c.securityBelow&&crimes>=c.reportedCrimesMin&&knights>=c.availableKnightsMin;}
namespace {bool mutual(const Relationship&a,const Relationship&b){return a.source==b.target&&a.target==b.source&&a.source!=a.target;}}
bool canDate(const Relationship&a,const Relationship&b,bool consent,const EventConfig&c){return consent&&mutual(a,b)&&a.affection>=c.romanceAffection&&b.affection>=c.romanceAffection&&a.attraction>=c.romanceAttraction&&b.attraction>=c.romanceAttraction;}
bool canMarry(const Relationship&a,const Relationship&b,bool consent,bool eligible,const EventConfig&c){return consent&&eligible&&mutual(a,b)&&a.commitment>=c.marriageCommitment&&b.commitment>=c.marriageCommitment;}
double manaGain(double beast,double mage,unsigned kills,const ManaConfig&c){
    configValid(c);if(!std::isfinite(beast)||beast<0||!std::isfinite(mage)||mage<0)throw std::invalid_argument("Invalid mana values");
    const double relative=std::min(c.cap,std::pow(beast/std::max(1.0,mage),c.exponent));
    const double gain=beast*c.rate*relative/(1+c.repeatDecay*kills);
    if(!std::isfinite(gain))throw std::overflow_error("Mana reward overflow");return gain;
}
bool RuntimeStore::recordDeath(const std::string&save,const std::string&beast,double hour){
    Db d(path_,true);runtime(d);Tx tx(d,true);validateTime(d,save,hour);
    Query q(d,"SELECT e.alive,e.region_id,e.mana_max,s.kind FROM entities e JOIN species s ON s.id=e.species_id WHERE e.save_id=? AND e.id=?");q.bind(1,save);q.bind(2,beast);
    if(!q.row()||q.text(3)!="beast")throw std::invalid_argument("Not a beast in this save");
    if(q.integer(0)==0){tx.commit();return false;}
    const std::string region=q.text(1);const double beastMana=q.number(2);
    Query update(d,"UPDATE entities SET alive=0,health=0 WHERE save_id=? AND id=? AND alive=1");update.bind(1,save);update.bind(2,beast);update.done();
    Query ev(d,"INSERT INTO world_events(id,save_id,event_type,region_id,target_id,occurred_hour,payload_json) VALUES(?,?,'beast_death',NULLIF(?,''),?,?,json_object('beast_mana',?))");
    ev.bind(1,"bp:death:"+key(save,beast));ev.bind(2,save);ev.bind(3,region);ev.bind(4,beast);ev.bind(5,hour);ev.bind(6,beastMana);ev.done();setTime(d,save,hour);tx.commit();return true;
}
double RuntimeStore::absorb(const std::string&save,const std::string&mage,const std::string&beast,double hour,const Eligibility&eligible){
    if(!eligible.mageBloodline||!eligible.eligibleContributor||!eligible.inAbsorptionRange)return 0;
    Db d(path_,true);runtime(d);Tx tx(d,true);validateTime(d,save,hour);const auto config=readMana(d);
    Query victim(d,"SELECT e.species_id,e.alive,v.occurred_hour,json_extract(v.payload_json,'$.beast_mana') FROM entities e JOIN world_events v ON v.id=? AND v.save_id=e.save_id WHERE e.id=? AND e.save_id=?");
    victim.bind(1,"bp:death:"+key(save,beast));victim.bind(2,beast);victim.bind(3,save);
    if(!victim.row()||victim.integer(1)!=0){tx.commit();return 0;}
    const auto species=victim.text(0);const double death=victim.number(2),beastMana=victim.number(3);
    if(hour<death||(hour-death)*3600>config.windowSeconds+1e-8){tx.commit();return 0;}
    Query duplicate(d,"SELECT 1 FROM mana_absorptions WHERE save_id=? AND mage_entity_id=? AND beast_entity_id=?");duplicate.bind(1,save);duplicate.bind(2,mage);duplicate.bind(3,beast);if(duplicate.row()){tx.commit();return 0;}
    Query m(d,"SELECT e.mana_max,e.alive,s.kind FROM entities e JOIN species s ON s.id=e.species_id WHERE e.id=? AND e.save_id=?");m.bind(1,mage);m.bind(2,save);
    if(!m.row()||m.integer(1)!=1||m.text(2)!="human"){tx.commit();return 0;}const double maximum=m.number(0);
    Query count(d,"SELECT kill_count FROM mana_hunt_history WHERE mage_entity_id=? AND beast_species_id=?");count.bind(1,mage);count.bind(2,species);std::int64_t kills=count.row()?count.integer(0):0;
    if(kills<0||static_cast<std::uint64_t>(kills)>std::numeric_limits<unsigned>::max())throw std::runtime_error("Invalid hunt history");
    const double gain=manaGain(beastMana,maximum,static_cast<unsigned>(kills),config);
    if(gain<=0){tx.commit();return 0;}
    Query update(d,"UPDATE entities SET mana_max=mana_max+? WHERE id=? AND save_id=?");update.bind(1,gain);update.bind(2,mage);update.bind(3,save);update.done();
    Query history(d,"INSERT INTO mana_hunt_history(mage_entity_id,beast_species_id,kill_count) VALUES(?,?,1) ON CONFLICT(mage_entity_id,beast_species_id) DO UPDATE SET kill_count=kill_count+1");history.bind(1,mage);history.bind(2,species);history.done();
    Query absorption(d,"INSERT INTO mana_absorptions(id,save_id,mage_entity_id,beast_entity_id,occurred_hour,beast_mana,relative_factor,repetition_factor,absorbed_amount) VALUES(?,?,?,?,?,?,?,?,?)");
    absorption.bind(1,"bp:absorb:"+key(key(save,mage),beast));absorption.bind(2,save);absorption.bind(3,mage);absorption.bind(4,beast);absorption.bind(5,hour);absorption.bind(6,beastMana);absorption.bind(7,std::min(config.cap,std::pow(beastMana/std::max(1.0,maximum),config.exponent)));absorption.bind(8,1/(1+config.repeatDecay*kills));absorption.bind(9,gain);absorption.done();setTime(d,save,hour);tx.commit();return gain;
}
void RuntimeStore::saveActions(const std::string&save,double hour,const std::map<std::string,ActiveAction>&actions){
    Db d(path_,true);runtime(d);Tx tx(d,true);validateTime(d,save,hour);
    Query clear(d,"DELETE FROM active_actions WHERE entity_id IN (SELECT id FROM entities WHERE save_id=?)");clear.bind(1,save);clear.done();
    for(const auto& pair:actions){const auto&a=pair.second;
        if(a.node<1||a.node>39||!std::isfinite(a.startedSeconds)||!std::isfinite(a.minimumEndSeconds)||!std::isfinite(a.cooldownUntilSeconds)||a.startedSeconds<0||a.minimumEndSeconds<a.startedSeconds||a.cooldownUntilSeconds<0||a.startedSeconds>hour*3600+1e-8)throw std::invalid_argument("Invalid action state");
        Query own(d,"SELECT 1 FROM entities WHERE id=? AND save_id=? AND alive=1");own.bind(1,pair.first);own.bind(2,save);if(!own.row())throw std::invalid_argument("Action owner missing from save");
        if(!a.target.empty()){Query target(d,"SELECT 1 FROM entities WHERE id=? AND save_id=?");target.bind(1,a.target);target.bind(2,save);if(!target.row())throw std::invalid_argument("Action target missing from save");}
        Query insert(d,"INSERT INTO active_actions(entity_id,node_id,target_id,started_hour,minimum_end_hour,cooldown_until_hour) VALUES(?,?,NULLIF(?,''),?,?,?)");
        insert.bind(1,pair.first);insert.bind(2,double(a.node));insert.bind(3,a.target);insert.bind(4,a.startedSeconds/3600);insert.bind(5,a.minimumEndSeconds/3600);insert.bind(6,a.cooldownUntilSeconds/3600);insert.done();
    }setTime(d,save,hour);tx.commit();
}
}
