#pragma once
#include "bp/simulation.hpp"
#include <string>
namespace bp {
struct Queries { std::string regions, actors, wbg; };
struct Content { Snapshot initial; std::vector<Wbg> wbg; };
// Each query must return columns in the documented order. No schema mutation.
Content loadContent(const std::string& utf8Path,const Queries&,std::size_t expectedWbg=39);
std::string readSql(const std::string& path);
}
