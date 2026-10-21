#include "bp/content.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
void check(bool b){if(!b)throw std::runtime_error("content test failed");}
int main(){
    const auto path=(std::filesystem::temp_directory_path()/"bp_content_test.sqlite").string();
    try{
        std::filesystem::remove(path);sqlite3* db=nullptr;check(sqlite3_open(path.c_str(),&db)==SQLITE_OK);
        // Fixture database only. Actual blood_principle_game.db is never created or replaced.
        check(sqlite3_exec(db,"CREATE TABLE wbg(id INTEGER PRIMARY KEY); WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<39) INSERT INTO wbg SELECT x FROM n;",nullptr,nullptr,nullptr)==SQLITE_OK);
        sqlite3_close(db);
        bp::Queries q{"SELECT 1,0.0", "SELECT 1,0,'npc','mage',1,1,10.0,0.0,0.0,10.0,2.0,1", "SELECT CAST(id AS TEXT),'fixture','{}' FROM wbg"};
        auto c=bp::loadContent(path,q);check(c.wbg.size()==39&&c.initial.actors.size()==1);
        bool threw=false;auto bad=q;bad.wbg="SELECT CAST(id AS TEXT),'fixture','{}' FROM wbg WHERE id<39";
        try{bp::loadContent(path,bad);}catch(const std::runtime_error&){threw=true;}check(threw);
        bad=q;bad.regions="SELECT 1,NULL";threw=false;
        try{bp::loadContent(path,bad);}catch(const std::runtime_error&){threw=true;}check(threw);
        bad=q;bad.wbg="DELETE FROM wbg";threw=false;
        try{bp::loadContent(path,bad);}catch(const std::runtime_error&){threw=true;}check(threw);
        check(bp::loadContent(path,q).wbg.size()==39);
        bad=q;bad.regions="SELECT 1,0.0; SELECT 2,0.0";threw=false;
        try{bp::loadContent(path,bad);}catch(const std::runtime_error&){threw=true;}check(threw);
        std::filesystem::remove(path);std::cout<<"content tests passed\n";return 0;
    }catch(const std::exception&e){std::filesystem::remove(path);std::cerr<<e.what()<<'\n';return 1;}
}
