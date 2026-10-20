#include "StrategyEngine.h"

#include "Map.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Builder.h"

void StrategyEngine::buildDefensiveCannons(std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                                           bool atChoke,
                                           int frameNeeded,
                                           int atBases)
{
    if (frameNeeded < 0) return;

    auto buildCannonAt = [&](BWAPI::TilePosition pylonTile, BWAPI::TilePosition cannonTile, Base *baseLoc)
    {
        if (!pylonTile.isValid()) return;
        if (!cannonTile.isValid()) return;

        auto buildAtTile = [&](BWAPI::TilePosition tilePos, BWAPI::UnitType type, int tick)
        {
            if (UnitMgr::myBuildingAt(tilePos) != nullptr) return;
            if (Constructor::isPendingHere(tilePos)) return;

            int framesUntilPowered = 0;
            if (type != BWAPI::UnitTypes::Protoss_Pylon)
            {
                framesUntilPowered = Constructor::framesUntilCompleted(
                        pylonTile,
                        UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Pylon) + 240);
            }

            auto buildLocation = BuildingPlacement::BuildLocation(BuildBlock::Location(tilePos),
                                                                  BuildingPlacement::builderFrames(baseLoc->getPosition(),
                                                                                                   tilePos,
                                                                                                   type),
                                                                  framesUntilPowered,
                                                                  0);
            auto priority = (UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0
                             || UnitMgr::countEnemy(BWAPI::UnitTypes::Zerg_Lurker) > 0
                             || UnitMgr::countEnemy(BWAPI::UnitTypes::Zerg_Lurker_Egg) > 0)
                            ? PRIORITY_EMERGENCY
                            : PRIORITY_NORMAL;
            prioProdGoals[priority].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                              "SE-defcan",
                                                              type,
                                                              buildLocation,
                                                              tick);
        };

        auto pylon = UnitMgr::myBuildingAt(pylonTile);
        if (!pylon)
        {
            buildAtTile(pylonTile, BWAPI::UnitTypes::Protoss_Pylon, std::max(0, frameNeeded - UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Pylon)));
        }

        int startFrame = frameNeeded;
        if (pylon && !pylon->completed)
        {
            startFrame = std::max(startFrame, pylon->completionFrame);
        }

        buildAtTile(cannonTile, BWAPI::UnitTypes::Protoss_Photon_Cannon, startFrame);
    };

    /* Check if we have a cannon at our choke */
    auto cannonLocations = BuildingPlacement::mainChokeCannonLocations();
    OwnedUnit chokeCannon;
    if (cannonLocations.first != BWAPI::TilePositions::Invalid)
    {
        chokeCannon = UnitMgr::myBuildingAt(cannonLocations.second);

        /* Build it if requested */
        if (atChoke && !chokeCannon && !Constructor::isInEnemyStaticThreatRange(cannonLocations.second, BWAPI::UnitTypes::Protoss_Photon_Cannon))
        {
            buildCannonAt(cannonLocations.first, cannonLocations.second, Map::getMyMain());
        }
    }
    else if (atChoke)
    {
        /* If we ask for a cannon at the choke, but don't have a location for it, build one at our bases instead */
        atBases = std::max(atBases, 1);
    }

    if (atBases > 0)
    {
        auto buildAtBase = [&buildCannonAt](Base *baseLoc, int count = 1)
        {
            if (!baseLoc || baseLoc->owner != BWAPI::Broodwar->self()) return;
            if (!baseLoc->resourceDepot || !baseLoc->resourceDepot->completed) return;

            auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(baseLoc);
            if (baseStaticDefenseLocations.isValid())
            {
                for (const auto &location: baseStaticDefenseLocations.workerDefenseCannons)
                {
                    if (count < 1) break;
                    count--;

                    if (UnitMgr::myBuildingAt(location))
                    {
                        continue;
                    }

                    buildCannonAt(baseStaticDefenseLocations.powerPylon, location, baseLoc);
                }
            }
        };

        buildAtBase(Map::getMyMain(), atBases);
        if (!Map::mapSpecificOverride()->hasBackdoorNatural())
        {
            /* Always build two at natural */
            buildAtBase(Map::getMyNatural(), 2);
        }
    }
}

bool StrategyEngine::hasCannonAtWall()
{
    /* Return true if we have a wall with at least one powered cannon */
    if (!BuildingPlacement::hasForgeGatewayWall()) return false;

    auto &wall = BuildingPlacement::getForgeGatewayWall();

    auto completedUnit = [](BWAPI::TilePosition tilePos)
    {
        auto gameUnit = UnitMgr::myBuildingAt(tilePos);
        if (!gameUnit) return false;
        return gameUnit->completed;
    };
    if (!completedUnit(wall.pylon)) return false;

    return std::any_of(wall.cannons.begin(), wall.cannons.end(), [&completedUnit](const auto &cannonTile){
        return completedUnit(cannonTile);
    });
}
