#include "Opponent.h"

namespace Opponent
{
    namespace
    {
        std::string name;
    }

    void initialize()
    {
        name.clear();
    }

    std::string &getName()
    {
        if (!name.empty()) return name;

        name = BWAPI::Broodwar->enemy()->getName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());

        return name;
    }
}