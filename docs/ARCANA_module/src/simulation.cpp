#include "bp/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
namespace bp {
namespace { bool finiteNonnegative(double x){return std::isfinite(x)&&x>=0;} }
World::World(Config c):config_(c) {
    if(!std::isfinite(c.stepSeconds)||c.stepSeconds<=0||!c.maxStepsPerFrame||
       !finiteNonnegative(c.hungerPerTick)||!std::isfinite(c.invasionHunger)||c.invasionHunger<=0||c.invasionHunger>1||
       !finiteNonnegative(c.threatDecay)||!std::isfinite(c.dispatchThreat)||c.dispatchThreat<=0||!finiteNonnegative(c.manaScale))
        throw std::invalid_argument("Invalid simulation config");
}
void World::restore(Snapshot s) {
    std::set<Id> ids, regions;
    if(!finiteNonnegative(s.accumulator)||s.accumulator>=config_.stepSeconds) throw std::invalid_argument("Invalid accumulator");
    for(const auto& r:s.regions) if(r.id<=0||!regions.insert(r.id).second||!finiteNonnegative(r.threat)) throw std::invalid_argument("Invalid region");
    for(const auto& a:s.actors) {
        if(a.id<=0||!ids.insert(a.id).second||!regions.count(a.home)||!regions.count(a.region)||
           !std::isfinite(a.power)||a.power<=0||!finiteNonnegative(a.mana)||!finiteNonnegative(a.hunger)||a.hunger>1||
           !finiteNonnegative(a.gold)||!finiteNonnegative(a.food)||
           (a.kind!=Kind::Npc&&a.kind!=Kind::Beast)||
           (a.role!=Role::Civilian&&a.role!=Role::Bandit&&a.role!=Role::Knight&&a.role!=Role::Mage))
            throw std::invalid_argument("Invalid actor or region reference");
    }
    for(const auto& d:s.deployments) {
        auto it=std::find_if(s.actors.begin(),s.actors.end(),[&](const Actor& a){return a.id==d.first;});
        if(it==s.actors.end()||!it->alive||it->kind!=Kind::Npc||
           (it->role!=Role::Knight&&it->role!=Role::Mage)||!regions.count(d.second)) throw std::invalid_argument("Invalid deployment");
    }
    for(const auto& a:s.affinity) if(!ids.count(a.first.first)||!ids.count(a.first.second)||a.first.first>=a.first.second) throw std::invalid_argument("Invalid affinity");
    std::sort(s.actors.begin(),s.actors.end(),[](const Actor&a,const Actor&b){return a.id<b.id;});
    std::sort(s.regions.begin(),s.regions.end(),[](const Region&a,const Region&b){return a.id<b.id;});
    state_=std::move(s); events_.clear();
}
void World::setWbg(std::vector<Wbg> rules,std::map<std::string,WbgHandler> handlers,std::size_t expected) {
    if(rules.size()!=expected) throw std::invalid_argument("WBG count mismatch");
    std::set<std::string> ids;
    for(const auto& r:rules) if(r.id.empty()||!ids.insert(r.id).second||!handlers.count(r.id)||!handlers.at(r.id))
        throw std::invalid_argument("Duplicate WBG ID or missing WBG handler: "+r.id);
    std::sort(rules.begin(),rules.end(),[](const Wbg&a,const Wbg&b){return a.id<b.id;});
    rules_=std::move(rules); handlers_=std::move(handlers);
}
Actor* World::actor(Id id) {
    auto it=std::lower_bound(state_.actors.begin(),state_.actors.end(),id,[](const Actor&a,Id v){return a.id<v;});
    return it!=state_.actors.end()&&it->id==id?&*it:nullptr;
}
void World::emit(EventType t,Id a,Id b,Id r,double n){events_.push_back({t,a,b,r,n,state_.tick});}
bool World::lure(Id id,bool enabled){auto* a=actor(id);if(!a||!a->alive||a->kind!=Kind::Beast)return false;a->lured=enabled;return true;}
bool World::kill(Id killer,Id victim) {
    auto* k=actor(killer);auto* v=actor(victim);
    if(!k||!v||k==v||!k->alive||!v->alive||k->region!=v->region)return false;
    v->alive=false;state_.deployments.erase(victim);
    emit(EventType::Kill,killer,victim,v->region);
    if(v->kind==Kind::Npc&&k->kind==Kind::Npc&&k->role!=Role::Knight&&k->role!=Role::Mage){
        ++k->crimes;k->role=Role::Bandit;emit(EventType::Murder,killer,victim,v->region);
        for(auto& r:state_.regions)if(r.id==v->region)r.threat+=2;
    }
    if(v->kind==Kind::Beast&&k->kind==Kind::Npc&&k->role==Role::Mage){
        const double ratio=v->power/k->power;
        auto& repeats=k->weakKills[v->archetype];
        const double efficiency=ratio<1 ? ratio/(1.0+repeats) : 1.0;
        const double gain=config_.manaScale*v->power*efficiency;
        if(ratio<1)++repeats;
        k->mana+=gain;emit(EventType::ManaGain,killer,victim,v->region,gain);
    }
    return true;
}
void World::advance(double dt) {
    if(!finiteNonnegative(dt))throw std::invalid_argument("Invalid frame delta");
    // Explicit overload policy: discard excess wall time, do not accumulate unbounded debt.
    state_.accumulator+=std::min(dt,config_.stepSeconds*config_.maxStepsPerFrame);
    unsigned count=0;
    while(state_.accumulator>=config_.stepSeconds&&count<config_.maxStepsPerFrame){
        state_.accumulator-=config_.stepSeconds;++count;step();
    }
}
void World::step() {
    ++state_.tick;
    for(auto& r:state_.regions)r.threat=std::max(0.0,r.threat-config_.threatDecay);
    for(auto& a:state_.actors)if(a.alive){
        a.hunger=std::min(1.0,a.hunger+config_.hungerPerTick);
        if(a.kind==Kind::Beast){
            if(a.region==a.home&&(a.lured||a.hunger>=config_.invasionHunger)){
                auto dest=std::find_if(state_.regions.begin(),state_.regions.end(),[&](const Region&r){return r.id!=a.home;});
                if(dest!=state_.regions.end()){a.region=dest->id;emit(EventType::Invasion,a.id,0,a.region);}
            }
            if(a.region!=a.home){
                for(auto& r:state_.regions)if(r.id==a.region)r.threat+=1;
                if(!a.lured&&a.hunger<config_.invasionHunger){a.region=a.home;emit(EventType::ReturnHome,a.id,0,a.region);}
            }
        }
        if(a.food>=1&&a.hunger>=.5){a.food-=1;a.hunger=std::max(0.0,a.hunger-.5);}
        if(a.kind==Kind::Npc&&a.role==Role::Civilian&&a.hunger>.9&&a.gold<1)a.role=Role::Bandit;
    }
    // Stable ID order and one interaction per actor per tick avoid order-dependent double trades.
    std::set<Id> used;
    for(auto& a:state_.actors)if(a.alive&&a.kind==Kind::Npc&&!used.count(a.id)){
        for(auto& b:state_.actors)if(b.alive&&b.kind==Kind::Npc&&b.id!=a.id&&b.region==a.region&&!used.count(b.id)){
            if(a.role==Role::Bandit&&b.role==Role::Civilian&&b.gold>0){
                const double n=std::min(2.0,b.gold);b.gold-=n;a.gold+=n;++a.crimes;
                emit(EventType::Robbery,a.id,b.id,a.region,n);
                for(auto& r:state_.regions)if(r.id==a.region)r.threat+=1;
            } else if(a.role!=Role::Bandit&&b.role!=Role::Bandit&&a.food<1&&a.gold>=1&&b.food>=2){
                --a.gold;++b.gold;++a.food;--b.food;emit(EventType::Trade,a.id,b.id,a.region,1);
            } else if(a.role!=Role::Bandit&&b.role!=Role::Bandit){
                auto key=std::make_pair(std::min(a.id,b.id),std::max(a.id,b.id));
                auto& affinity=state_.affinity[key];
                if(affinity<10&&++affinity==10)emit(EventType::Love,a.id,b.id,a.region);
            } else continue;
            used.insert(a.id);used.insert(b.id);break;
        }
    }
    for(auto it=state_.deployments.begin();it!=state_.deployments.end();){
        auto* guard=actor(it->first);
        auto r=std::find_if(state_.regions.begin(),state_.regions.end(),[&](const Region& x){return x.id==it->second;});
        if(!guard||!guard->alive||r==state_.regions.end()||r->threat<config_.dispatchThreat){
            if(guard&&guard->alive)guard->region=guard->home;
            it=state_.deployments.erase(it);
        } else ++it;
    }
    for(auto& r:state_.regions)if(r.threat>=config_.dispatchThreat){
        const bool assigned=std::any_of(state_.deployments.begin(),state_.deployments.end(),[&](const auto&d){return d.second==r.id;});
        if(!assigned)for(auto& a:state_.actors)if(a.alive&&a.kind==Kind::Npc&&
            (a.role==Role::Knight||a.role==Role::Mage)&&!state_.deployments.count(a.id)){
            state_.deployments[a.id]=r.id;a.region=r.id;emit(EventType::Dispatch,a.id,0,r.id);break;
        }
    }
    // Combat remains with the host game. Dispatch emits intent and moves the coarse region only.
    for(const auto& r:rules_)handlers_.at(r.id)(r,state_,events_);
}
std::vector<Event> World::drainEvents(){std::vector<Event> out;out.swap(events_);return out;}
}
