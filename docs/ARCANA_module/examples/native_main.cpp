#include "bp/native.hpp"
#include <iostream>
// Minimal non-destructive smoke example. Host handles only basic movement intents here.
// Add all game-specific probes/execution through existing AI, navigation and combat systems.
struct ExampleHost final:bp::native::Host {
    std::optional<bp::native::Candidate> probe(const bp::native::Entity&,const bp::native::Node&n) override {
        bp::native::Candidate c;
        if(n.name=="Idle")return c;
        if(n.name=="Wander"){c.terms[0]=.2;return c;}
        return std::nullopt;
    }
    void execute(const bp::native::Intent&i) override{std::cout<<i.entity<<" -> "<<i.name<<" changed="<<i.changed<<'\n';}
};
int main(int argc,char**argv){
    try{
        if(argc<2){std::cerr<<"usage: bp_native_example database.db [save_id]\n";return 2;}
        const auto c=bp::native::load(argv[1],argc>2?argv[2]:"demo");
        std::cout<<"nodes="<<c.nodes.size()<<" entities="<<c.entities.size()<<" territories="<<c.territories.size()<<'\n';
        bp::native::Scheduler scheduler(c);ExampleHost host;
        for(const auto&e:c.entities)if(e.id!="player")scheduler.decide(e,c.worldHours*3600,host);
        // Player ownership belongs to the existing game; no autonomous player update.
        return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
