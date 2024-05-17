#pragma once
#include <BWAPI.h>

namespace McRave::Terrain {
    BWAPI::Position getClosestMapCorner(BWAPI::Position);
    BWAPI::Position getClosestMapEdge(BWAPI::Position);
    BWAPI::Position getOldestPosition(const BWEM::Area *);
    BWAPI::Position getSafeSpot(BWEB::Station *);
    void onStart();
    void onFrame();

    std::set <const BWEM::Base*>& getAllBases();
    BWAPI::Position getAttackPosition();
    BWAPI::Position getDefendPosition();
    BWAPI::Position getHarassPosition();
    BWAPI::Position getEnemyStartingPosition();
    BWEB::Station * getEnemyMain();
    BWEB::Station * getEnemyNatural();
    BWEB::Station * getMyMain();
    BWEB::Station * getMyNatural();
    BWAPI::TilePosition getEnemyExpand();
    BWAPI::TilePosition getEnemyStartingTilePosition();
    const BWEM::ChokePoint * getDefendChoke();
    const BWEM::Area * getDefendArea();
    bool isIslandMap();
    bool isReverseRamp();
    bool isFlatRamp();
    bool isNarrowNatural();
    bool isDefendNatural();
    bool foundEnemy();
    bool inTerritory(PlayerState, BWAPI::Position);
    bool inTerritory(PlayerState, const BWEM::Area*);
    void addTerritory(PlayerState, BWEB::Station*);
    void removeTerritory(PlayerState, BWEB::Station*);

    inline bool isExplored(BWAPI::Position here) { return BWAPI::Broodwar->isExplored(BWAPI::TilePosition(here)); }
}
