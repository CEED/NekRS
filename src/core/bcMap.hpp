#if !defined(nekrs_bcmap_hpp_)
#define nekrs_bcmap_hpp_

#include <string>
#include <vector>
#include "nekInterfaceAdapter.hpp"

namespace bcMap
{
void setup(std::vector<std::string> slist, std::string field);
int id(int bid, std::string field);
int type(int bid, std::string field);
std::string text(int bid, std::string field);
int size(int isTmesh);
void check(mesh_t* mesh);
void setBcMap(std::string field, int* map, int nbid);
}

#endif
