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

    void onUnitCreated(const Unit &gameUnit);

    void onUnitCreated_Walkability(const Unit &gameUnit);

    void onUnitDiscover(BWAPI::Unit gameUnit);

    void onUnitDestroy(const Unit &gameUnit);

    void onUnitDestroy(BWAPI::Unit gameUnit);

    void onUnitDestroy_Walkability(const Unit &gameUnit);

    void onUnitDestroy_Walkability(BWAPI::Unit gameUnit);

    void onBuildingLifted(BWAPI::UnitType type, BWAPI::TilePosition tilePos);

    void onBuildingLanded(BWAPI::UnitType type, BWAPI::TilePosition tilePos);

    void update();

    MapSpecificOverride *mapSpecificOverride();

    std::vector<Base *> &allBases();

    std::set<Base *> &getMyBases(BWAPI::Player player = BWAPI::Broodwar->self());

    std::set<Base *> getEnemyBases(BWAPI::Player player = BWAPI::Broodwar->self());

    std::vector<Base *> &getUntakenExpansions(BWAPI::Player player = BWAPI::Broodwar->self());

    std::vector<Base *> &getUntakenIslandExpansions(BWAPI::Player player = BWAPI::Broodwar->self());

    Base *getMyMain();

    Base *getMyNatural();

    void setMyNatural(Base *baseLoc);

    Base *getEnemyStartingMain();

    Base *getEnemyStartingNatural();

    Base *getEnemyMain();

    void setEnemyStartingMain(Base *baseLoc);

    void setEnemyStartingNatural(Base *baseLoc);

    /* A base with gas that is far away from both starting bases */
    Base *getHiddenBase();

    Base *baseNear(BWAPI::Position position);

    std::vector<Base *> allStartingLocations();

    std::set<Base *> unscoutedStartingLocations();

    std::vector<Choke *> allChokes();

    /* Gets the choke with the center nearest the given position, up to maxDistance away. */
    Choke *chokeNear(BWAPI::Position pt, int maxDistance = 160);

    Choke *choke(const BWEM::ChokePoint *bwemChoke);

    Choke *getMyMainChoke();

    Choke *getMyNaturalChoke();

    void setMyMainChoke(Choke *choke);

    Choke *getEnemyMainChoke();

    void setEnemyMainChoke(Choke *choke);

    int minChokeWidth();

    std::set<const BWEM::Area *> &getMyMainAreas();

    std::set<const BWEM::Area *> &getStartingBaseAreas(Base *baseLoc);

    Base *getStartingBaseNatural(Base *baseLoc);

    std::pair<Choke*, Choke*> getStartingBaseChokes(Base *baseLoc);

    std::map<const BWEM::Area *, std::set<BWAPI::TilePosition>> &getAreasToEdgePositions();

    std::map<BWAPI::TilePosition, const BWEM::Area *> &getEdgePositionsToArea();

    void dumpPowerHeatmap();

    void dumpWalkability();

    /* Walkable tiles are defined here as: */
    /* - All of its contained walk positions are walkable according to BWAPI */
    /* - It is not occupied by a building */
    bool isWalkable(BWAPI::TilePosition pt);

    bool isWalkable(int x, int y);

    bool isTerrainWalkable(int tileX, int tileY);

    unsigned short unwalkableProximity(int x, int y);

    unsigned short walkableWidth(int x, int y);

    BWAPI::Position collisionVector(int x, int y);

    bool isInOwnMineralLine(BWAPI::TilePosition tilePos);

    bool isInOwnMineralLine(int x, int y);

    bool bordersMineralPatch(int x, int y);

    bool isInNarrowChoke(BWAPI::TilePosition pt);

    bool isInLeafArea(BWAPI::TilePosition pt);

    bool isOnIsland(BWAPI::TilePosition pt);

    int lastSeen(BWAPI::TilePosition tilePos);

    int lastSeen(int x, int y);

    void makePositionValid(int &x, int &y);
}