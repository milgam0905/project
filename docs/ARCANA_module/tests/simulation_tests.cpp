#include "bp/simulation.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
void check(bool b){if(!b)throw std::runtime_error("test failed");}
bp::Actor npc(bp::Id id,bp::Role role=bp::Role::Civilian){bp::Actor a;a.id=id;a.region=1;a.home=1;a.role=role;return a;}
bp::Actor beast(bp::Id id,double power=5){auto a=npc(id);a.kind=bp::Kind::Beast;a.archetype=42;a.power=power;return a;}
int main(){try{
    bp::World w;bp::Snapshot s;s.regions={{1,0},{2,0}};
    s.actors={npc(1,bp::Role::Mage),beast(2),beast(3),beast(4,20)};w.restore(s);
    check(w.kill(1,2));auto mana=w.state().actors[0].mana;
    check(!w.kill(1,2));check(w.state().actors[0].mana==mana);
    check(w.kill(1,3));check(w.state().actors[0].mana-mana<mana);
    auto before=w.state().actors[0].mana;check(w.kill(1,4));check(w.state().actors[0].mana-before>mana);
    auto saved=w.state();bp::World restored;restored.restore(saved);check(restored.state().actors[0].weakKills.at(42)==2);
    s.actors={beast(2)};s.actors[0].home=2;s.actors[0].region=2;w.restore(s);w.advance(1);
    check(w.state().actors[0].region==2);check(w.lure(2));w.advance(1);check(w.state().actors[0].region==1);
    check(w.lure(2,false));w.advance(1);check(w.state().actors[0].region==2);
    s.actors={npc(1,bp::Role::Bandit),npc(2),npc(3,bp::Role::Knight)};w.restore(s);
    w.advance(4);check(w.state().deployments.size()==1);
    auto ev=w.drainEvents();unsigned dispatch=0;for(auto&e:ev)if(e.type==bp::EventType::Dispatch)++dispatch;check(dispatch==1);
    check(w.state().actors[0].gold+w.state().actors[1].gold==20);
    s.actors={npc(1),npc(2)};s.actors[0].food=0;w.restore(s);w.advance(1);
    check(w.state().actors[0].food==1&&w.state().actors[0].gold==9&&w.state().actors[1].gold==11);
    w.restore(s);bp::World split;split.restore(s);w.advance(2);for(int i=0;i<8;++i)split.advance(.25);
    check(w.state().tick==split.state().tick);check(w.state().actors[0].hunger==split.state().actors[0].hunger);
    bool threw=false;try{w.setWbg({{"x","x",""}}, {},39);}catch(const std::invalid_argument&){threw=true;}check(threw);
    unsigned calls=0;std::vector<bp::Wbg> rules;std::map<std::string,bp::WbgHandler> handlers;
    for(int i=0;i<39;++i){auto id=std::to_string(i);rules.push_back({id,"test fixture only",""});handlers[id]=[&](const auto&,const auto&,auto&){++calls;};}
    w.setWbg(rules,handlers);w.advance(1);check(calls==39);
    auto missing=handlers;missing.erase("0");threw=false;
    try{w.setWbg(rules,missing);}catch(const std::invalid_argument&){threw=true;}check(threw);
    auto duplicate=rules;duplicate[1].id=duplicate[0].id;threw=false;
    try{w.setWbg(duplicate,handlers);}catch(const std::invalid_argument&){threw=true;}check(threw);
    w.setWbg({}, {},0); // Clear fixture handlers before subsequent behavior checks.
    s.actors={beast(2)};s.actors[0].home=2;s.actors[0].region=2;s.actors[0].hunger=.79;s.actors[0].food=0;
    w.restore(s);w.advance(1);check(w.state().actors[0].region==1);
    s.actors={npc(1),beast(2)};w.restore(s);check(w.kill(1,2));check(w.state().actors[0].mana==0);
    s.actors={npc(1),npc(2)};w.restore(s);w.advance(5);auto midpoint=w.state();w.drainEvents();
    bp::World replay;replay.restore(midpoint);w.advance(5);replay.advance(5);
    auto original=w.drainEvents(), repeated=replay.drainEvents();check(original.size()==repeated.size());
    unsigned love=0;for(std::size_t i=0;i<original.size();++i){
        check(original[i].type==repeated[i].type&&original[i].tick==repeated[i].tick);
        if(original[i].type==bp::EventType::Love)++love;
    }check(love==1);
    s.actors[0].home=999;threw=false;try{w.restore(s);}catch(const std::invalid_argument&){threw=true;}check(threw);
    threw=false;try{w.advance(-1);}catch(const std::invalid_argument&){threw=true;}check(threw);
    s.actors={npc(1),npc(2)};w.restore(s);check(w.kill(1,2));check(w.state().actors[0].role==bp::Role::Bandit);
    std::cout<<"simulation tests passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
