#include "StrategyEngines/PvT.h"

#include "Units.h"
#include "Map.h"
#include "Strategist.h"

std::map<PvT::TerranStrategy, std::string> PvT::TerranStrategyNames = {
        {TerranStrategy::Unknown,       "Unknown"},
        {TerranStrategy::ProxyRush,     "ProxyRush"},
        {TerranStrategy::MarineRush,    "MarineRush"},
        {TerranStrategy::WallIn,        "WallIn"},
        {TerranStrategy::FastExpansion, "FastExpansion"},
        {TerranStrategy::TwoFactory,    "TwoFactory"},
        {TerranStrategy::Normal,        "Normal"},
        {TerranStrategy::MidGame,       "MidGame"},
};

namespace
{
    bool countAtLeast(BWAPI::UnitType type, int count)
    {
        auto &timings = Units::getEnemyUnitTimings(type);
        return timings.size() >= count;
    }

    bool noneCreated(BWAPI::UnitType type)
    {
        return Units::getEnemyUnitTimings(type).empty();
    }

    bool createdBeforeFrame(BWAPI::UnitType type, int frame, int count = 1)
    {
        auto &timings = Units::getEnemyUnitTimings(type);
        if (timings.size() < count) return false;

        return timings[count - 1].first < frame;
    }

    bool isFastExpansion()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Terran_Command_Center, 7000, 2);
    }

    bool isMarineRush()
    {
        // In the early game, we consider the enemy to be doing a marine rush or all-in if we either see a lot of marines
        // or see two barracks with no factory or gas
        if (BWAPI::Broodwar->getFrameCount() < 6000)
        {
            // Early rushes
            if (createdBeforeFrame(BWAPI::UnitTypes::Terran_Marine, 3500, 2) ||
                createdBeforeFrame(BWAPI::UnitTypes::Terran_Barracks, 2300, 2))
            {
                return true;
            }

            // Later all-ins
            return createdBeforeFrame(BWAPI::UnitTypes::Terran_Marine, 5000, 4) ||
                   (noneCreated(BWAPI::UnitTypes::Terran_Refinery) &&
                    noneCreated(BWAPI::UnitTypes::Terran_Factory) &&
                    countAtLeast(BWAPI::UnitTypes::Terran_Barracks, 2));
        }

        // Later on, we consider it to be a marine all-in purely based on the counts
        if (BWAPI::Broodwar->getFrameCount() < 8000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Terran_Marine, 7000, 8) &&
                   Units::countEnemy(BWAPI::UnitTypes::Terran_Marine) > 4;
        }

        return createdBeforeFrame(BWAPI::UnitTypes::Terran_Marine, 9000, 12) &&
               Units::countEnemy(BWAPI::UnitTypes::Terran_Marine) > 4;
    }

    bool isProxy()
    {
        if (BWAPI::Broodwar->getFrameCount() >= 6000) return false;

        // Otherwise check if we have directly scouted an enemy building in a proxy location
        auto enemyMain = Map::getEnemyStartingMain();
        auto enemyNatural = Map::getEnemyStartingNatural();
        for (const auto &enemyUnit : Units::allEnemy())
        {
            if (!enemyUnit->type.isBuilding()) continue;
            if (enemyUnit->type.isRefinery()) continue; // Don't count gas steals
            if (enemyUnit->type.isResourceDepot()) continue; // Don't count expansions

            // If the enemy main is unknown, this means the Map didn't consider this building to be part of that base and it indicates a proxy
            if (!enemyMain) return true;

            // Otherwise consider this a proxy if the building is not close to the enemy main or natural
            int dist = PathFinding::GetGroundDistance(
                    enemyUnit->lastPosition,
                    enemyMain->getPosition(),
                    BWAPI::UnitTypes::Protoss_Probe,
                    PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (dist != -1 && dist < 1500)
            {
                continue;
            }

            if (enemyNatural)
            {
                int naturalDist = PathFinding::GetGroundDistance(
                        enemyUnit->lastPosition,
                        enemyNatural->getPosition(),
                        BWAPI::UnitTypes::Protoss_Probe,
                        PathFinding::PathFindingOptions::UseNearestBWEMArea);
                if (naturalDist != -1 && naturalDist < 640)
                {
                    continue;
                }
            }

            return true;
        }

        // If the enemy main has been scouted, determine if there is a proxy by looking at what they have built
        if (Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::EnemyBaseScouted ||
            Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::ScoutingCompleted)
        {
            // Expect first barracks, refinery or command center by frame 2400
            if (BWAPI::Broodwar->getFrameCount() > 2400
                && !countAtLeast(BWAPI::UnitTypes::Terran_Barracks, 1)
                && !countAtLeast(BWAPI::UnitTypes::Terran_Refinery, 1)
                && !countAtLeast(BWAPI::UnitTypes::Terran_Command_Center, 2))
            {
                return true;
            }

            return false;
        }

        return false;
    }

    bool isWallIn()
    {
        // The enemy is considered walled-in if our scout was unable to get into the base and
        // there is no path from our main to their base

        // TODO: This is too strict - the scout might get into the base before the wall-in is finished

        if (Strategist::getWorkerScoutStatus() != Strategist::WorkerScoutStatus::ScoutingBlocked) return false;
        if (BWAPI::Broodwar->getFrameCount() > 6000) return false;

        auto enemyMain = Map::getEnemyStartingMain();
        auto mainChoke = Map::getMyMainChoke();
        if (!enemyMain || !mainChoke) return false;

        auto grid = PathFinding::getNavigationGrid(enemyMain->getTilePosition());
        if (!grid) return false;

        auto node = (*grid)[mainChoke->center];
        return node.nextNode == nullptr;
    }

    bool isMidGame()
    {
        // TODO: Extend this
        return BWAPI::Broodwar->getFrameCount() > 10000 &&
               (countAtLeast(BWAPI::UnitTypes::Terran_Command_Center, 2) ||
                (Units::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) +
                 Units::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)) > 4);
    }
}

PvT::TerranStrategy PvT::recognizeEnemyStrategy()
{
    auto strategy = enemyStrategy;
    for (int i = 0; i < 10; i++)
    {
        switch (strategy)
        {
            case TerranStrategy::Unknown:
                if (isMarineRush()) return TerranStrategy::MarineRush;
                if (isProxy()) return TerranStrategy::ProxyRush;
                if (isWallIn()) return TerranStrategy::WallIn;
                if (isFastExpansion()) return TerranStrategy::FastExpansion;

                // Two factory
                if (countAtLeast(BWAPI::UnitTypes::Terran_Factory, 2))
                {
                    strategy = TerranStrategy::TwoFactory;
                    continue;
                }

                // Default to something reasonable if we don't detect anything else
                if (BWAPI::Broodwar->getFrameCount() > 4000)
                {
                    strategy = TerranStrategy::Normal;
                    continue;
                }

                break;
            case TerranStrategy::ProxyRush:
                // Handle a misdetected proxy, can happen if the enemy does a fast expand or builds further away from their command center
                if (!isProxy())
                {
                    strategy = TerranStrategy::Unknown;
                    continue;
                }

                // Otherwise intentionally fall through to marine rush handling

            case TerranStrategy::MarineRush:
                // Consider the rush to be over after 6000 frames
                // From there the Normal handler will potentially transition into MarineAllIn
                if (BWAPI::Broodwar->getFrameCount() >= 6000)
                {
                    strategy = TerranStrategy::Normal;
                    continue;
                }

                break;
            case TerranStrategy::WallIn:
                if (isMarineRush()) return TerranStrategy::MarineRush;
                if (isFastExpansion()) return TerranStrategy::FastExpansion;

                if (isMidGame())
                {
                    strategy = TerranStrategy::MidGame;
                    continue;
                }

                break;
            case TerranStrategy::TwoFactory:
                if (isMarineRush()) return TerranStrategy::MarineRush;

                if (isMidGame())
                {
                    strategy = TerranStrategy::MidGame;
                    continue;
                }

                break;
            case TerranStrategy::FastExpansion:
            case TerranStrategy::Normal:
                if (isMarineRush()) return TerranStrategy::MarineRush;

                if (isMidGame())
                {
                    strategy = TerranStrategy::MidGame;
                    continue;
                }

                break;
            case TerranStrategy::MidGame:
                break;
        }

        return strategy;
    }

    Log::Get() << "ERROR: Loop in strategy recognizer, ended on " << TerranStrategyNames[strategy];
    return strategy;
}
