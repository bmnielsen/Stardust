#include "Opponent.h"

namespace Opponent
{
    namespace
    {
        std::string name;
        bool raceUnknown;
    }

    void initialize()
    {
        name.clear();
        raceUnknown =
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Protoss &&
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran &&
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg;
    }

    bool isUnknownRace()
    {
        return raceUnknown;
    }

    std::string &getName()
    {
        if (!name.empty()) return name;

        name = BWAPI::Broodwar->enemy()->getName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());

        return name;
    }

    bool hasRaceJustBeenDetermined()
    {
        if (!raceUnknown) return false;

        raceUnknown =
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Protoss &&
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran &&
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg;
        return !raceUnknown;
    }
}
