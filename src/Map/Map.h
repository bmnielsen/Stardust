#pragma once

#include "Common.h"

#include <bwem.h>

#include "MapSpecificOverride.h"
#include "Base.h"
#include "Choke.h"

namespace Map
{
    void initialize();

    void onUnitCreated(const Unit &unit);

    void onUnitDiscover(BWAPI::Unit unit);

    void onUnitDestroy(const Unit &unit);

    void onUnitDestroy(BWAPI::Unit unit);

    void onBuildingLifted(BWAPI::UnitType type, BWAPI::TilePosition tile);

    void onBuildingLanded(BWAPI::UnitType type, BWAPI::TilePosition tile);

    void update();

    MapSpecificOverride *mapSpecificOverride();

    std::vector<Base *> &allBases();

    std::vector<std::vector<Base *>> &allBaseClusters();

    std::set<Base *> &getMyBases(BWAPI::Player player = BWAPI::Broodwar->self());

    std::set<Base *> getEnemyBases(BWAPI::Player player = BWAPI::Broodwar->enemy());

    std::vector<Base *> &getUntakenExpansions(BWAPI::Player player = BWAPI::Broodwar->self());

    Base *getMyMain();

    Base *getMyNatural();

    Base *getEnemyStartingMain();

    Base *getEnemyMain();

    Base *baseNear(BWAPI::Position position);

    std::set<Base *> unscoutedStartingLocations();

    Base *getNaturalForStartLocation(BWAPI::TilePosition startLocation);

    std::vector<Choke *> allChokes();

    Choke *choke(const BWEM::ChokePoint *bwemChoke);

    bool nearNarrowChokepoint(BWAPI::Position position);

    int minChokeWidth();

    void dumpVisibilityHeatmap();

    // Walkable tiles are defined here as:
    // - All of its contained walk positions are walkable according to BWAPI
    // - It is not occupied by a building
    // - It is not part of a mineral line (i.e. located between minerals and their associated resource depot)
    bool isWalkable(BWAPI::TilePosition pos);

    bool isWalkable(int x, int y);

    int unwalkableProximity(int x, int y);
}