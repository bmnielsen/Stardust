#include "StrategyEngines/PvT.h"

#include "Units.h"
#include "Map.h"
#include "Strategist.h"
#include "UnitUtil.h"

std::map<PvT::TerranStrategy, std::string> PvT::TerranStrategyNames = {
        {TerranStrategy::Unknown,       "Unknown"},
        {TerranStrategy::WorkerRush,    "WorkerRush"},
        {TerranStrategy::ProxyRush,     "ProxyRush"},
        {TerranStrategy::MarineRush,    "MarineRush"},
        {TerranStrategy::WallIn,        "WallIn"},
        {TerranStrategy::BlockScouting, "BlockScouting"},
        {TerranStrategy::FastExpansion, "FastExpansion"},
        {TerranStrategy::TwoFactory,    "TwoFactory"},
        {TerranStrategy::NormalOpening, "Normal"},
        {TerranStrategy::MidGameMech,   "MidGameMech"},
        {TerranStrategy::MidGameBio,    "MidGameBio"},
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

    bool isWorkerRush()
    {
        if (currentFrame >= 6000) return false;

        int workers = 0;
        for (const Unit &unit : Units::allEnemy())
        {
            if (!unit->lastPositionValid) continue;
            if (unit->type.isBuilding()) continue;

            bool isInArea = false;
            for (const auto &area : Map::getMyMainAreas())
            {
                if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition)) == area)
                {
                    isInArea = true;
                    break;
                }
            }
            if (!isInArea) continue;

            // If there is a normal combat unit in our main, it isn't a worker rush
            if (UnitUtil::IsCombatUnit(unit->type) && unit->type.canAttack()) return false;

            if (unit->type.isWorker()) workers++;
        }

        return workers > 2;
    }

    bool isMarineRush()
    {
        // In the early game, we consider the enemy to be doing a marine rush or all-in if we either see a lot of marines
        // or see two barracks with no factory or gas
        if (currentFrame < 6000)
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
        if (currentFrame < 8000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Terran_Marine, 7000, 8) &&
                   Units::countEnemy(BWAPI::UnitTypes::Terran_Marine) > 4;
        }

        return createdBeforeFrame(BWAPI::UnitTypes::Terran_Marine, 9000, 12) &&
               Units::countEnemy(BWAPI::UnitTypes::Terran_Marine) > 4;
    }

    bool isProxy()
    {
        if (currentFrame >= 6000) return false;
        if (Units::countEnemy(BWAPI::UnitTypes::Terran_Refinery) > 0) return false;

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
        if (Strategist::hasWorkerScoutCompletedInitialBaseScan())
        {
            // Expect first barracks, refinery or command center by frame 2400
            if (currentFrame > 2400
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
        if (currentFrame > 6000) return false;

        auto enemyMain = Map::getEnemyStartingMain();
        auto mainChoke = Map::getMyMainChoke();
        if (!enemyMain || !mainChoke) return false;

        auto grid = PathFinding::getNavigationGrid(enemyMain->getPosition());
        if (!grid) return false;

        auto &node = (*grid)[mainChoke->center];
        return node.nextNode == nullptr;
    }

    bool isMidGame()
    {
        // Never in the mid-game before frame 8000
        if (currentFrame < 8000) return false;

        // We consider ourselves to be in the mid-game if the enemy has expanded

        // Scouted expansion
        if (countAtLeast(BWAPI::UnitTypes::Terran_Command_Center, 2)) return true;

        // Inferred taken natural, usually by seeing a bunker
        auto enemyNatural = Map::getEnemyStartingNatural();
        if (enemyNatural && enemyNatural->owner == BWAPI::Broodwar->enemy()) return true;

        // We consider ourselves to be in the mid-game if the enemy has siege tech and a number of siege tanks
        return Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) &&
               (Units::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) +
                Units::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)) > (currentFrame > 10000 ? 2 : 4);
    }

    bool isMidGameMech()
    {
        int mech = Units::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) +
                   Units::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) +
                   Units::countEnemy(BWAPI::UnitTypes::Terran_Vulture) +
                   Units::countEnemy(BWAPI::UnitTypes::Terran_Goliath);
        int bio = Units::countEnemy(BWAPI::UnitTypes::Terran_Marine) +
                  Units::countEnemy(BWAPI::UnitTypes::Terran_Medic) +
                  Units::countEnemy(BWAPI::UnitTypes::Terran_Firebat);

        // Mech if they don't have many bio units
        if (bio < 10) return true;

        // Mech if they have less than twice as many bio units compared to mech
        return mech >= (bio / 2);
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
                if (isWorkerRush()) return TerranStrategy::WorkerRush;
                if (isMarineRush()) return TerranStrategy::MarineRush;
                if (isProxy()) return TerranStrategy::ProxyRush;
                if (isWallIn()) return TerranStrategy::WallIn;
                if (isFastExpansion()) return TerranStrategy::FastExpansion;

                // If the enemy blocks our scouting, set their strategy accordingly
                if (Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::ScoutingBlocked)
                {
                    strategy = TerranStrategy::BlockScouting;
                    continue;
                }

                // Two factory
                if (countAtLeast(BWAPI::UnitTypes::Terran_Factory, 2))
                {
                    strategy = TerranStrategy::TwoFactory;
                    continue;
                }

                // Default to something reasonable if we don't detect anything else
                if (currentFrame > 4000 ||
                    Strategist::hasWorkerScoutCompletedInitialBaseScan())
                {
                    strategy = TerranStrategy::NormalOpening;
                    continue;
                }

                break;
            case TerranStrategy::WorkerRush:
                if (!isWorkerRush())
                {
                    strategy = TerranStrategy::Unknown;
                    continue;
                }

                break;
            case TerranStrategy::ProxyRush:
                if (isWorkerRush()) return TerranStrategy::WorkerRush;

                // Handle a misdetected proxy, can happen if the enemy does a fast expand or builds further away from their depot
                if (currentFrame < 6000 && !isProxy())
                {
                    strategy = TerranStrategy::Unknown;
                    continue;
                }

                // We assume the enemy has transitioned from the proxy when either:
                // - They have taken gas
                // - Our scout is dead and we are past frame 5000
                if (Units::countEnemy(BWAPI::UnitTypes::Terran_Refinery) > 0 ||
                    (currentFrame >= 6000 && Strategist::isWorkerScoutComplete()))
                {
                    strategy = TerranStrategy::Unknown;
                    continue;
                }

                // Also bail out of thinking it is a proxy rush if we have more dragoons than the enemy has marines
                if (currentFrame >= 6000 &&
                    Units::countEnemy(BWAPI::UnitTypes::Terran_Marine) < Units::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon))
                {
                    strategy = TerranStrategy::Unknown;
                    continue;
                }

                break;
            case TerranStrategy::MarineRush:
                if (isWorkerRush()) return TerranStrategy::WorkerRush;

                // Consider the rush to be over after 6000 frames
                // From there the Normal handler will potentially transition into MarineAllIn
                if (currentFrame >= 6000)
                {
                    strategy = TerranStrategy::NormalOpening;
                    continue;
                }

                break;
            case TerranStrategy::WallIn:
                if (isWorkerRush()) return TerranStrategy::WorkerRush;
                if (isMarineRush()) return TerranStrategy::MarineRush;
                if (isFastExpansion()) return TerranStrategy::FastExpansion;

                if (isMidGame())
                {
                    strategy = TerranStrategy::MidGameMech;
                    continue;
                }

                break;
            case TerranStrategy::BlockScouting:
                if (isWorkerRush()) return TerranStrategy::WorkerRush;
                if (isProxy()) return TerranStrategy::ProxyRush;
                if (isMarineRush()) return TerranStrategy::MarineRush;
                if (isFastExpansion()) return TerranStrategy::FastExpansion;

                // If we haven't had any evidence of a rush for about 3 1/2 minutes, assume the enemy is opening normally
                if (currentFrame > 5000)
                {
                    strategy = TerranStrategy::NormalOpening;
                    continue;
                }

                if (isMidGame())
                {
                    strategy = TerranStrategy::MidGameMech;
                    continue;
                }

                break;
            case TerranStrategy::TwoFactory:
            case TerranStrategy::FastExpansion:
            case TerranStrategy::NormalOpening:
                if (isWorkerRush()) return TerranStrategy::WorkerRush;
                if (isProxy()) return TerranStrategy::ProxyRush;
                if (isMarineRush()) return TerranStrategy::MarineRush;

                if (isMidGame())
                {
                    strategy = TerranStrategy::MidGameMech;
                    continue;
                }

                break;
            case TerranStrategy::MidGameMech:
                if (!isMidGameMech())
                {
                    strategy = TerranStrategy::MidGameBio;
                    continue;
                }
                break;
            case TerranStrategy::MidGameBio:
                if (isMidGameMech())
                {
                    strategy = TerranStrategy::MidGameMech;
                    continue;
                }
                break;
        }

        return strategy;
    }

    Log::Get() << "ERROR: Loop in strategy recognizer, ended on " << TerranStrategyNames[strategy];
    return strategy;
}
