#include "bp/native.hpp"
#include <sqlite3.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace n=bp::native;
void check(bool value,const char*msg){if(!value)throw std::runtime_error(msg);}
struct TestHost:n::Host {
    double idle=0,wander=.1;bool wandering=true;unsigned executes=0;
    std::optional<n::Candidate> probe(const n::Entity&,const n::Node&node)override {
        n::Candidate c;if(node.name=="Idle"){c.terms[0]=idle;return c;}
        if(node.name=="Wander"&&wandering){c.terms[0]=wander;return c;}
        return std::nullopt;
    }
    void execute(const n::Intent&)override{++executes;}
};
void sql(sqlite3*db,const char*s){check(sqlite3_exec(db,s,nullptr,nullptr,nullptr)==SQLITE_OK,sqlite3_errmsg(db));}
int main(int argc,char**argv){
    std::filesystem::path runtime=std::filesystem::temp_directory_path()/("bp_native_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".db");
    try{
        check(argc==2,"expected original fixture DB path");const auto c=n::load(argv[1]);
        check(c.nodes.size()==39,"39 real behavior nodes");check(c.entities.size()==9,"9 real entities");check(c.profiles.size()==4,"4 profiles");
        auto mage=*std::find_if(c.entities.begin(),c.entities.end(),[](const auto&e){return e.id=="player";});
        auto beast=*std::find_if(c.entities.begin(),c.entities.end(),[](const auto&e){return e.id=="beast_01";});
        auto wolf=*std::find_if(c.entities.begin(),c.entities.end(),[](const auto&e){return e.id=="wolf_01";});
        check(wolf.kind=="animal"&&wolf.profile=="predator","animal is not NPC or beast");
        n::Scheduler scheduler(c);TestHost host;
        auto a=scheduler.decide(mage,0,host);check(a&&a->name=="Wander"&&a->changed,"utility select");
        host.idle=1;a=scheduler.decide(mage,1,host);check(a->name=="Wander","minimum action time");
        a=scheduler.decide(mage,2,host);check(a->name=="Idle","switch after minimum");
        host.wander=1.1;a=scheduler.decide(mage,5,host);check(a->name=="Idle","hysteresis keeps action");
        a=scheduler.decide(mage,5.1,host,n::Interrupt::Fire);check(a->name=="Wander","urgent interrupt");
        host.wandering=false;a=scheduler.decide(mage,5.2,host);check(a->name=="Idle","invalid action cancels minimum");
        check(n::shouldDispatch(39,3,1,c.events),"dispatch threshold");check(!n::shouldDispatch(40,3,1,c.events),"strict security boundary");
        check(!n::shouldLeaveTerritory(beast,c.territories[0],c.events),"beast stays home");beast.hunger=76;
        check(n::shouldLeaveTerritory(beast,c.territories[0],c.events),"hunger invasion");check(!n::shouldLeaveTerritory(wolf,c.territories[0],c.events,true),"animal is not beast invasion");
        n::Relationship ab{"a","b",70,0,55,80},ba{"b","a",70,0,55,80};
        check(!n::canDate(ab,ba,false)&&n::canDate(ab,ba,true),"mutual consent romance");check(!n::canMarry(ab,ba,true,false)&&n::canMarry(ab,ba,true,true),"marriage eligibility");
        const double expected=80*.25*1.5;check(std::abs(n::manaGain(80,30,0,c.mana)-expected)<1e-10,"DB mana formula");
        check(n::manaGain(5,100,1,c.mana)<n::manaGain(5,100,0,c.mana),"repeat decay");
        check(std::isfinite(n::manaGain(80,0,0,c.mana)),"zero denominator guard");
        bool refused=false;try{n::RuntimeStore original(argv[1]);original.recordDeath("demo","beast_01",0);}catch(const std::exception&){refused=true;}check(refused,"original DB must reject writes");
        sqlite3 *source=nullptr,*dest=nullptr;
        check(sqlite3_open_v2(argv[1],&source,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK,"open source");
        check(sqlite3_open(runtime.string().c_str(),&dest)==SQLITE_OK,"open runtime");
        auto*b=sqlite3_backup_init(dest,"main",source,"main");check(b,"backup init");check(sqlite3_backup_step(b,-1)==SQLITE_DONE,"backup copy");check(sqlite3_backup_finish(b)==SQLITE_OK,"backup finish");sqlite3_close(source);
        sql(dest,"CREATE TABLE bp_runtime_meta(key TEXT PRIMARY KEY,value TEXT NOT NULL); INSERT INTO bp_runtime_meta VALUES('format','bp-native-v1'); CREATE UNIQUE INDEX bp_unique_absorption ON mana_absorptions(save_id,mage_entity_id,beast_entity_id);");sqlite3_close(dest);
        n::RuntimeStore store(runtime.string());
        check(store.recordDeath("demo","beast_01",0),"record death");check(!store.recordDeath("demo","beast_01",0),"death idempotent");
        check(store.absorb("demo","player","beast_01",0,{false,true,true})==0,"bloodline required");
        check(store.absorb("demo","player","beast_01",0,{true,false,true})==0,"contribution required");
        check(store.absorb("demo","player","beast_01",0,{true,true,false})==0,"range required");
        check(std::abs(store.absorb("demo","player","beast_01",0,{true,true,true})-30)<1e-10,"transactional gain");
        check(store.absorb("demo","player","beast_01",0,{true,true,true})==0,"duplicate absorb blocked");
        check(store.absorb("demo","serdin_01","beast_01",61.0/3600,{true,true,true})==0,"60 second expiry");
        auto loaded=n::load(runtime.string());
        auto player=*std::find_if(loaded.entities.begin(),loaded.entities.end(),[](const auto&e){return e.id=="player";});
        check(player.manaMax==60&&player.manaCurrent==25,"max mana only; current unchanged");
        store.saveActions("demo",6.0/3600,scheduler.actions());loaded=n::load(runtime.string());
        check(loaded.activeActions.at("player").node==6,"action restored");
        refused=false;try{store.saveActions("demo",0,scheduler.actions());}catch(const std::exception&){refused=true;}check(refused,"backwards time rejected");
        sqlite3_open(runtime.string().c_str(),&dest);sqlite3_stmt* stmt=nullptr;
        sqlite3_prepare_v2(dest,"SELECT kill_count FROM mana_hunt_history WHERE mage_entity_id='player' AND beast_species_id='boar_beast'",-1,&stmt,nullptr);check(sqlite3_step(stmt)==SQLITE_ROW&&sqlite3_column_int(stmt,0)==1,"hunt history once");sqlite3_finalize(stmt);
        // Force a log insertion failure: the mana_max/history changes must roll back together.
        sql(dest,"INSERT INTO entities(id,save_id,species_id,display_name,region_id,profile_id,mana_max) VALUES('beast_02','demo','boar_beast','test','belheim_region','beast',80); CREATE TRIGGER reject_mana BEFORE INSERT ON mana_absorptions BEGIN SELECT RAISE(ABORT,'test rollback'); END;");sqlite3_close(dest);
        check(store.recordDeath("demo","beast_02",6.0/3600),"second beast death");
        refused=false;try{store.absorb("demo","player","beast_02",6.0/3600,{true,true,true});}catch(const std::exception&){refused=true;}check(refused,"forced failure");
        loaded=n::load(runtime.string());player=*std::find_if(loaded.entities.begin(),loaded.entities.end(),[](const auto&e){return e.id=="player";});check(player.manaMax==60,"rollback max mana");
        std::filesystem::remove(runtime);std::cout<<"native DB, WBG and transaction tests passed\n";return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';std::error_code ec;std::filesystem::remove(runtime,ec);return 1;}
}
