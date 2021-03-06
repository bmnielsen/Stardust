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

    std::set<Base *> &getMyBases(BWAPI::Player player = BWAPI::Broodwar->self());

    std::set<Base *> getEnemyBases(BWAPI::Player player = BWAPI::Broodwar->self());

    std::vector<Base *> &getUntakenExpansions(BWAPI::Player player = BWAPI::Broodwar->self());

    std::vector<Base *> &getUntakenIslandExpansions(BWAPI::Player player = BWAPI::Broodwar->self());

    Base *getMyMain();

    Base *getMyNatural();

    void setMyNatural(Base *base);

    Base *getEnemyStartingMain();

    Base *getEnemyStartingNatural();

    Base *getEnemyMain();

    void setEnemyStartingMain(Base *base);

    void setEnemyStartingNatural(Base *base);

    Base *baseNear(BWAPI::Position position);

    std::set<Base *> unscoutedStartingLocations();

    std::vector<Choke *> allChokes();

    Choke *choke(const BWEM::ChokePoint *bwemChoke);

    Choke *getMyMainChoke();

    void setMyMainChoke(Choke *choke);

    Choke *getEnemyMainChoke();

    void setEnemyMainChoke(Choke *choke);

    int minChokeWidth();

    std::set<const BWEM::Area *> &getMyMainAreas();

    void dumpVisibilityHeatmap();

    // Walkable tiles are defined here as:
    // - All of its contained walk positions are walkable according to BWAPI
    // - It is not occupied by a building
    bool isWalkable(BWAPI::TilePosition pos);

    bool isWalkable(int x, int y);

    int unwalkableProximity(int x, int y);

    BWAPI::Position collisionVector(int x, int y);

    bool isInOwnMineralLine(BWAPI::TilePosition tile);

    bool isInOwnMineralLine(int x, int y);

    bool isInNarrowChoke(BWAPI::TilePosition pos);

    bool isInLeafArea(BWAPI::TilePosition pos);

    int lastSeen(BWAPI::TilePosition tile);

    int lastSeen(int x, int y);
}