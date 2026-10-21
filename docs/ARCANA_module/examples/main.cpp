#include "bp/simulation.hpp"
#include <iostream>
int main(){
    bp::World world;bp::Snapshot s;s.regions={{1,0},{2,0}};
    bp::Actor mage;mage.id=1;mage.region=1;mage.home=1;mage.role=bp::Role::Mage;
    bp::Actor beast;beast.id=2;beast.archetype=100;beast.region=2;beast.home=2;beast.kind=bp::Kind::Beast;beast.power=20;
    s.actors={mage,beast};world.restore(s);world.lure(2);world.advance(1);
    world.kill(1,2);
    for(const auto&e:world.drainEvents())std::cout<<"tick="<<e.tick<<" type="<<int(e.type)<<" actor="<<e.actor<<" amount="<<e.amount<<'\n';
}
