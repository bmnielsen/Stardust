#include "Opponent.h"

namespace Opponent
{
#ifndef _DEBUG
    namespace
    {
#endif
        std::string name;
#ifndef _DEBUG
    }
#endif

    std::string& getName()
    {
        if (!name.empty()) return name;

        name = BWAPI::Broodwar->enemy()->getName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());

        return name;
    }
}