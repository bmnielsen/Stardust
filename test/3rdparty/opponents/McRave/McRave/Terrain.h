#pragma once
#include <BWAPI.h>

namespace McRave::Terrain {
    BWAPI::Position getClosestMapCorner(BWAPI::Position);
    BWAPI::Position getClosestMapEdge(BWAPI::Position);
    BWAPI::Position getOldestPosition(const BWEM::Area *);
    BWAPI::Position getSafeSpot(BWEB::Station *);
    bool isInAllyTerritory(BWAPI::TilePosition);
    bool isInAllyTerritory(const BWEM::Area *);
    bool isInEnemyTerritory(BWAPI::TilePosition);
    bool isInEnemyTerritory(const BWEM::Area *);
    bool isStartingBase(BWAPI::TilePosition);
    void onStart();
    void onFrame();

    std::set <const BWEM::Area*>& getAllyTerritory();
    std::set <const BWEM::Area*>& getEnemyTerritory();
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
    bool isShitMap();
    bool isIslandMap();
    bool isReverseRamp();
    bool isFlatRamp();
    bool isNarrowNatural();
    bool isDefendNatural();
    bool foundEnemy();

    inline bool isExplored(BWAPI::Position here) { return BWAPI::Broodwar->isExplored(BWAPI::TilePosition(here)); }
}
