#include "StrategyEngine.h"

#include "Map.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Builder.h"

namespace
{
    int framesNeededFor(BWAPI::UnitType buildingType)
    {
        int earliestCompletion = INT_MAX;
        for (const auto &unit: Units::allMineIncompleteOfType(buildingType))
        {
            if (unit->estimatedCompletionFrame < earliestCompletion)
            {
                earliestCompletion = unit->estimatedCompletionFrame;
            }
        }

        if (earliestCompletion == INT_MAX)
        {
            return UnitUtil::BuildTime(buildingType);
        }

        return earliestCompletion - currentFrame;
    };
}

void StrategyEngine::buildDefensiveCannons(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                           bool atChoke,
                                           int frameNeeded,
                                           int atBases)
{
    if (frameNeeded < 0) return;

    auto buildCannonAt = [&](BWAPI::TilePosition pylonTile, BWAPI::TilePosition cannonTile)
    {
        if (!pylonTile.isValid()) return;
        if (!cannonTile.isValid()) return;

        // If we know what frame the cannon is needed, check if we need to start building it now
        if (frameNeeded > 0)
        {
            int frameStarted = frameNeeded - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon);
            if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Forge) == 0)
            {
                frameStarted -= framesNeededFor(BWAPI::UnitTypes::Protoss_Forge);
            }
            else if (!Units::myBuildingAt(pylonTile))
            {
                frameStarted -= UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon);
            }

            // TODO: When the Producer understands to build something at a specific frame, use that
            if ((currentFrame + 500) < frameStarted) return;
        }

        auto buildAtTile = [&prioritizedProductionGoals](BWAPI::TilePosition tile, BWAPI::UnitType type)
        {
            if (Units::myBuildingAt(tile) != nullptr) return;
            if (Builder::isPendingHere(tile)) return;

            auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(tile), 0, 0, 0);
            auto priority = (Units::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0
                             || Units::countEnemy(BWAPI::UnitTypes::Zerg_Lurker) > 0
                             || Units::countEnemy(BWAPI::UnitTypes::Zerg_Lurker_Egg) > 0)
                            ? PRIORITY_EMERGENCY
                            : PRIORITY_NORMAL;
            prioritizedProductionGoals[priority].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                              "SE",
                                                              type,
                                                              buildLocation);
        };

        auto pylon = Units::myBuildingAt(pylonTile);
        if (pylon && pylon->completed)
        {
            buildAtTile(cannonTile, BWAPI::UnitTypes::Protoss_Photon_Cannon);
        }
        else
        {
            buildAtTile(pylonTile, BWAPI::UnitTypes::Protoss_Pylon);
        }
    };

    // Check if we have a cannon at our choke
    auto cannonLocations = BuildingPlacement::mainChokeCannonLocations();
    MyUnit chokeCannon = nullptr;
    if (cannonLocations.first != BWAPI::TilePositions::Invalid)
    {
        chokeCannon = Units::myBuildingAt(cannonLocations.second);

        // Build it if requested
        if (atChoke && !chokeCannon && !Builder::isInEnemyStaticThreatRange(cannonLocations.second, BWAPI::UnitTypes::Protoss_Photon_Cannon))
        {
            buildCannonAt(cannonLocations.first, cannonLocations.second);
        }
    }
    else if (atChoke)
    {
        // If we ask for a cannon at the choke, but don't have a location for it, build one at our bases instead
        atBases = std::max(atBases, 1);
    }

    if (atBases > 0)
    {
        auto buildAtBase = [&buildCannonAt](Base *base, int count = 1)
        {
            if (!base || base->owner != BWAPI::Broodwar->self()) return;

            auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
            if (baseStaticDefenseLocations.isValid())
            {
                for (const auto &location: baseStaticDefenseLocations.workerDefenseCannons)
                {
                    if (count < 1) break;
                    count--;

                    if (Units::myBuildingAt(location))
                    {
                        continue;
                    }

                    buildCannonAt(baseStaticDefenseLocations.powerPylon, location);
                }
            }
        };

        buildAtBase(Map::getMyMain(), atBases);
        if (!Map::mapSpecificOverride()->hasBackdoorNatural())
        {
            // Always build two at natural
            buildAtBase(Map::getMyNatural(), 2);
        }
    }
}
