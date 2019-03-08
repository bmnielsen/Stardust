#pragma once

#include "Common.h"

#include <bwem.h>

#include "MapSpecificOverride.h"
#include "Base.h"
#include "Choke.h"

namespace Map
{
    void initialize();
    void onUnitDiscover(BWAPI::Unit unit);
    void onUnitDestroy(BWAPI::Unit unit);
    void update();

    MapSpecificOverride * mapSpecificOverride();
    
    std::vector<Base *> & allBases();
    std::set<Base*> & getMyBases(BWAPI::Player player = BWAPI::Broodwar->self());
    std::set<Base*> getEnemyBases(BWAPI::Player player = BWAPI::Broodwar->self());
    Base* getEnemyMain();
    Base * baseNear(BWAPI::Position position);
    std::set<Base*> unscoutedStartingLocations();

    Choke * choke(const BWEM::ChokePoint * bwemChoke);

    int minChokeWidth();
}