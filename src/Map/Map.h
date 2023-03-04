#pragma once

#include "Common.h"

#include <bwem.h>

#include "MapSpecificOverride.h"
#include "Base.h"
#include "Choke.h"

namespace Map
{
    void initialize();

    void initializeWalkability();

    void onUnitCreated(const Unit &unit);

    void onUnitCreated_Walkability(const Unit &unit);

    void onUnitDiscover(BWAPI::Unit unit);

    void onUnitDestroy(const Unit &unit);

    void onUnitDestroy(BWAPI::Unit unit);

    void onUnitDestroy_Walkability(const Unit &unit);

    void onUnitDestroy_Walkability(BWAPI::Unit unit);

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

    // A base with gas that is far away from both starting bases
    Base *getHiddenBase();

    Base *baseNear(BWAPI::Position position);

    std::set<Base *> unscoutedStartingLocations();

    std::vector<Choke *> allChokes();

    Choke *choke(const BWEM::ChokePoint *bwemChoke);

    Choke *getMyMainChoke();

    Choke *getMyNaturalChoke();

    void setMyMainChoke(Choke *choke);

    Choke *getEnemyMainChoke();

    void setEnemyMainChoke(Choke *choke);

    int minChokeWidth();

    std::set<const BWEM::Area *> &getMyMainAreas();

    std::set<const BWEM::Area *> &getStartingBaseAreas(Base *base);

    Base *getStartingBaseNatural(Base *base);

    std::pair<Choke*, Choke*> getStartingBaseChokes(Base *base);

    std::map<const BWEM::Area *, std::set<BWAPI::TilePosition>> &getAreasToEdgePositions();

    std::map<BWAPI::TilePosition, const BWEM::Area *> &getEdgePositionsToArea();

    void dumpPowerHeatmap();

    void dumpWalkability();

    // Walkable tiles are defined here as:
    // - All of its contained walk positions are walkable according to BWAPI
    // - It is not occupied by a building
    bool isWalkable(BWAPI::TilePosition pos);

    bool isWalkable(int x, int y);

    bool isTerrainWalkable(int tileX, int tileY);

    unsigned short unwalkableProximity(int x, int y);

    unsigned short walkableWidth(int x, int y);

    BWAPI::Position collisionVector(int x, int y);

    bool isInOwnMineralLine(BWAPI::TilePosition tile);

    bool isInOwnMineralLine(int x, int y);

    bool isInNarrowChoke(BWAPI::TilePosition pos);

    bool isInLeafArea(BWAPI::TilePosition pos);

    bool isOnIsland(BWAPI::TilePosition pos);

    int lastSeen(BWAPI::TilePosition tile);

    int lastSeen(int x, int y);

    void makePositionValid(int &x, int &y);
}