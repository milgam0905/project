#include "bp/content.hpp"
#include <sqlite3.h>
#include <cmath>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <stdexcept>
namespace bp {
namespace {
using Db=std::unique_ptr<sqlite3,decltype(&sqlite3_close)>;
using Stmt=std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)>;
std::string str(sqlite3_stmt*s,int col){
    if(sqlite3_column_type(s,col)!=SQLITE_TEXT)throw std::runtime_error("Expected non-null TEXT column");
    return std::string(reinterpret_cast<const char*>(sqlite3_column_text(s,col)),sqlite3_column_bytes(s,col));
}
Id integer(sqlite3_stmt*s,int col){
    if(sqlite3_column_type(s,col)!=SQLITE_INTEGER)throw std::runtime_error("Expected INTEGER column");
    return sqlite3_column_int64(s,col);
}
double real(sqlite3_stmt*s,int col){
    int t=sqlite3_column_type(s,col);
    if(t!=SQLITE_INTEGER&&t!=SQLITE_FLOAT)throw std::runtime_error("Expected numeric column");
    double v=sqlite3_column_double(s,col);if(!std::isfinite(v))throw std::runtime_error("Non-finite column");return v;
}
template<class F>void rows(sqlite3*d,const std::string&sql,int columns,F f){
    sqlite3_stmt* raw=nullptr;const char*tail=nullptr;
    int rc=sqlite3_prepare_v2(d,sql.c_str(),-1,&raw,&tail);Stmt s(raw,sqlite3_finalize);
    if(rc!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(d));
    if(!s||!sqlite3_stmt_readonly(s.get())||sqlite3_column_count(s.get())!=columns)throw std::runtime_error("Invalid SELECT column contract");
    // Allow whitespace/comments but reject any additional SQL statement.
    while(tail&&*tail){
        sqlite3_stmt* extra=nullptr;const char* next=nullptr;
        rc=sqlite3_prepare_v2(d,tail,-1,&extra,&next);Stmt e(extra,sqlite3_finalize);
        if(rc!=SQLITE_OK||e)throw std::runtime_error("Exactly one SELECT is required");
        if(next==tail)break;tail=next;
    }
    while((rc=sqlite3_step(s.get()))==SQLITE_ROW)f(s.get());
    if(rc!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(d));
}
}
std::string readSql(const std::string& p){std::ifstream f(p);if(!f)throw std::runtime_error("Cannot read SQL: "+p);return {std::istreambuf_iterator<char>(f),{}};}
Content loadContent(const std::string& p,const Queries&q,std::size_t expected){
    sqlite3* raw=nullptr;int rc=sqlite3_open_v2(p.c_str(),&raw,SQLITE_OPEN_READONLY,nullptr);Db d(raw,sqlite3_close);
    if(rc!=SQLITE_OK)throw std::runtime_error(raw?sqlite3_errmsg(raw):"Cannot open SQLite");
    sqlite3_busy_timeout(d.get(),2000);
    if(sqlite3_exec(d.get(),"BEGIN",nullptr,nullptr,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(d.get()));
    Content c;
    rows(d.get(),q.regions,2,[&](sqlite3_stmt*s){c.initial.regions.push_back({integer(s,0),real(s,1)});});
    rows(d.get(),q.actors,12,[&](sqlite3_stmt*s){
        Actor a;a.id=integer(s,0);a.archetype=integer(s,1);auto k=str(s,2),r=str(s,3);
        if(k=="npc")a.kind=Kind::Npc;else if(k=="beast")a.kind=Kind::Beast;else throw std::runtime_error("Unknown actor kind: "+k);
        if(r=="civilian")a.role=Role::Civilian;else if(r=="bandit")a.role=Role::Bandit;
        else if(r=="knight")a.role=Role::Knight;else if(r=="mage")a.role=Role::Mage;else throw std::runtime_error("Unknown actor role: "+r);
        a.region=integer(s,4);a.home=integer(s,5);a.power=real(s,6);a.mana=real(s,7);
        a.hunger=real(s,8);a.gold=real(s,9);a.food=real(s,10);
        Id alive=integer(s,11);if(alive!=0&&alive!=1)throw std::runtime_error("alive must be 0/1");a.alive=alive!=0;
        c.initial.actors.push_back(std::move(a));
    });
    std::set<std::string> ids;
    rows(d.get(),q.wbg,3,[&](sqlite3_stmt*s){Wbg w{str(s,0),str(s,1),str(s,2)};
        if(w.id.empty()||!ids.insert(w.id).second)throw std::runtime_error("Duplicate or empty WBG ID");c.wbg.push_back(std::move(w));});
    if(c.wbg.size()!=expected)throw std::runtime_error("Expected WBG count: "+std::to_string(expected)+", actual: "+std::to_string(c.wbg.size()));
    if(c.initial.regions.empty())throw std::runtime_error("No regions loaded");
    World validation;validation.restore(c.initial);c.initial=validation.state();
    if(sqlite3_exec(d.get(),"COMMIT",nullptr,nullptr,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(d.get()));
    return c;
}
}
