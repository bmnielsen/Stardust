#include "StrategyEngines/PvP.h"

#include "Units.h"
#include "Map.h"
#include "Strategist.h"
#include "UnitUtil.h"
#include "OpponentEconomicModel.h"

std::map<PvP::ProtossStrategy, std::string> PvP::ProtossStrategyNames = {
        {ProtossStrategy::Unknown,         "Unknown"},
        {ProtossStrategy::WorkerRush,      "WorkerRush"},
        {ProtossStrategy::ProxyRush,       "ProxyRush"},
        {ProtossStrategy::ZealotRush,      "ZealotRush"},
        {ProtossStrategy::EarlyForge,      "EarlyForge"},
        {ProtossStrategy::TwoGate,         "TwoGate"},
        {ProtossStrategy::NoZealotCore,    "NoZealotCore"},
        {ProtossStrategy::OneZealotCore,   "OneZealotCore"},
        {ProtossStrategy::FastExpansion,   "FastExpansion"},
        {ProtossStrategy::BlockScouting,   "BlockScouting"},
        {ProtossStrategy::ZealotAllIn,     "ZealotAllIn"},
        {ProtossStrategy::DragoonAllIn,    "DragoonAllIn"},
        {ProtossStrategy::EarlyRobo,       "EarlyRobo"},
        {ProtossStrategy::Turtle,          "Turtle"},
        {ProtossStrategy::DarkTemplarRush, "DarkTemplarRush"},
        {ProtossStrategy::MidGame,         "MidGame"},
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

    bool createdBeforeUnit(BWAPI::UnitType first, int firstCount, BWAPI::UnitType second, int secondCount)
    {
        auto &firstTimings = Units::getEnemyUnitTimings(first);
        if (firstTimings.size() < firstCount) return false;

        auto &secondTimings = Units::getEnemyUnitTimings(second);
        return secondTimings.size() < secondCount || firstTimings[firstCount - 1].first <= secondTimings[secondCount - 1].first;
    }

    bool justDiscovered(BWAPI::UnitType type)
    {
        auto &timings = Units::getEnemyUnitTimings(type);
        return std::any_of(timings.begin(), timings.end(), [](const auto timing) { return timing.second == currentFrame; });
    }

    bool isFastExpansion()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Nexus, 6000, 2);
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

    bool isZealotRush()
    {
        if (currentFrame >= 6000) return false;

        // We expect a zealot rush if we see an early zealot, early second zealot or early second gateway
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, 2700) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, 3300, 2) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Gateway, 2300, 2);
    }

    bool isProxy()
    {
        if (isFastExpansion()) return false;
        if (currentFrame >= 5000) return false;
        if (Units::countEnemy(BWAPI::UnitTypes::Protoss_Assimilator) > 0) return false;

        // Check if we have directly scouted an enemy building in a proxy location
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
            // Expect first pylon by frame 1300
            if (currentFrame > 1300 && !countAtLeast(BWAPI::UnitTypes::Protoss_Pylon, 1)) return true;

            // Expect first gateway or forge by frame 2200
            // This will sometimes fail if the enemy does a fast expansion we don't see
            if (currentFrame > 2200
                && !countAtLeast(BWAPI::UnitTypes::Protoss_Gateway, 1)
                && !countAtLeast(BWAPI::UnitTypes::Protoss_Forge, 1))
            {
                return true;
            }

            // If the enemy hasn't built a forge, expect a second pylon by frame 4000 if we still have a live scout
            if (currentFrame > 4000 &&
                Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::EnemyBaseScouted &&
                !countAtLeast(BWAPI::UnitTypes::Protoss_Forge, 1) &&
                !countAtLeast(BWAPI::UnitTypes::Protoss_Pylon, 2))
            {
                return true;
            }

            return false;
        }

        return false;
    }

    bool isZealotAllIn()
    {
        // In the early game, we consider the enemy to be doing a zealot all-in if we either see a lot of zealots or see two gateways with
        // no core or gas
        if (currentFrame < 6000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, 5000, 4) ||
                   (noneCreated(BWAPI::UnitTypes::Protoss_Assimilator) &&
                    noneCreated(BWAPI::UnitTypes::Protoss_Cybernetics_Core) &&
                    countAtLeast(BWAPI::UnitTypes::Protoss_Gateway, 2));
        }

        // Later on, we consider it to be a zealot all-in purely based on the counts
        if (currentFrame < 8000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, 7000, 8) &&
                   Units::countEnemy(BWAPI::UnitTypes::Protoss_Zealot) > 4;
        }

        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, 9000, 12) &&
               Units::countEnemy(BWAPI::UnitTypes::Protoss_Zealot) > 4;
    }

    bool isDragoonAllIn()
    {
        // It's unlikely our worker scout survives long enough, but detect this if the enemy gets four gates before robo / citadel
        if (currentFrame < 10000 &&
            createdBeforeUnit(BWAPI::UnitTypes::Protoss_Gateway, 4, BWAPI::UnitTypes::Protoss_Robotics_Facility, 1) &&
            createdBeforeUnit(BWAPI::UnitTypes::Protoss_Gateway, 4, BWAPI::UnitTypes::Protoss_Templar_Archives, 1))
        {
            return true;
        }

        // Check for four gates and no expansion from economic model
        if (OpponentEconomicModel::enabled() &&
            !createdBeforeFrame(BWAPI::UnitTypes::Protoss_Nexus, currentFrame, 2) &&
            OpponentEconomicModel::minimumProducerCount(BWAPI::UnitTypes::Protoss_Gateway) >= 4 &&
            !OpponentEconomicModel::hasBuilt(BWAPI::UnitTypes::Protoss_Robotics_Facility) &&
            !OpponentEconomicModel::hasBuilt(BWAPI::UnitTypes::Protoss_Templar_Archives))
        {
            return true;
        }

        return false;
    }

    bool isEarlyRobo()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Robotics_Facility, 7500);
    }

    bool isDarkTemplarRush()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Templar_Archives, 10000) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Dark_Templar, 12000);
    }

    bool isTurtle()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Photon_Cannon, 5000, 2) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Photon_Cannon, 6000, 3) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Photon_Cannon, 7000, 4) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Photon_Cannon, 8000, 5);
    }

    bool isMidGame()
    {
        // We consider ourselves to be in the mid-game after frame 14000 if the enemy has taken their natural or teched to something beyond goons
        if (currentFrame < 14000) return false;

        return countAtLeast(BWAPI::UnitTypes::Protoss_Nexus, 2) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Templar_Archives, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Dark_Templar, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_High_Templar, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Dark_Archon, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Archon, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Robotics_Facility, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Observatory, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Shuttle, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Reaver, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Observer, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Stargate, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Fleet_Beacon, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Arbiter_Tribunal, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Scout, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Corsair, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Carrier, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Arbiter, 1);
    }
}

PvP::ProtossStrategy PvP::recognizeEnemyStrategy()
{
    auto strategy = enemyStrategy;
    for (int i = 0; i < 10; i++)
    {
        switch (strategy)
        {
            case ProtossStrategy::Unknown:
                if (isWorkerRush()) return ProtossStrategy::WorkerRush;
                if (isZealotRush()) return ProtossStrategy::ZealotRush;
                if (isProxy()) return ProtossStrategy::ProxyRush;

                // If the enemy blocks our scouting, set their strategy accordingly
                if (Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::ScoutingBlocked)
                {
                    strategy = ProtossStrategy::BlockScouting;
                    continue;
                }

                if (isFastExpansion()) return ProtossStrategy::FastExpansion;
                if (isEarlyRobo()) return ProtossStrategy::EarlyRobo;

                // Early forge if we have seen either a forge or a cannon before transitioning to something else
                if (countAtLeast(BWAPI::UnitTypes::Protoss_Forge, 1) || countAtLeast(BWAPI::UnitTypes::Protoss_Photon_Cannon, 1))
                {
                    strategy = ProtossStrategy::EarlyForge;
                    continue;
                }

                // Transition to one- or two-gate when we have the appropriate scouting information
                if (createdBeforeUnit(BWAPI::UnitTypes::Protoss_Gateway, 2, BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1) &&
                    createdBeforeUnit(BWAPI::UnitTypes::Protoss_Gateway, 2, BWAPI::UnitTypes::Protoss_Assimilator, 1))
                {
                    strategy = ProtossStrategy::TwoGate;
                    continue;
                }

                // Check economic model if we scout the core first
                // It might think one-zealot until the assimilator is scouted, but that is corrected later
                if (createdBeforeUnit(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, BWAPI::UnitTypes::Protoss_Gateway, 2))
                {
                    if (OpponentEconomicModel::enabled() &&
                        OpponentEconomicModel::worstCaseUnitCount(BWAPI::UnitTypes::Protoss_Zealot).second == 0)
                    {
                        strategy = ProtossStrategy::NoZealotCore;
                    }
                    else
                    {
                        strategy = ProtossStrategy::OneZealotCore;
                    }
                    continue;
                }

                // Assume one-zealot if we scout the assimilator first
                // Will be reconsidered when the core is scouted
                if (createdBeforeUnit(BWAPI::UnitTypes::Protoss_Assimilator, 1, BWAPI::UnitTypes::Protoss_Gateway, 2))
                {
                    strategy = ProtossStrategy::OneZealotCore;
                    continue;
                }

                // Default to something reasonable if our scouting completely fails
                if (currentFrame > 4000)
                {
                    strategy = ProtossStrategy::OneZealotCore;
                    continue;
                }

                break;
            case ProtossStrategy::WorkerRush:
                if (!isWorkerRush())
                {
                    strategy = ProtossStrategy::Unknown;
                    continue;
                }

                break;
            case ProtossStrategy::ProxyRush:
                if (isWorkerRush()) return ProtossStrategy::WorkerRush;

                // Handle a misdetected proxy, can happen if the enemy does a fast expand or builds further away from their nexus
                if (currentFrame < 5000 && !isProxy())
                {
                    strategy = ProtossStrategy::Unknown;
                    continue;
                }

                // We assume the enemy has transitioned from the proxy when either:
                // - They have taken gas
                // - Our scout is dead and we are past frame 5000
                if (Units::countEnemy(BWAPI::UnitTypes::Protoss_Assimilator) > 0 ||
                    (currentFrame >= 5000 && Strategist::isWorkerScoutComplete()))
                {
                    strategy = ProtossStrategy::TwoGate;
                    continue;
                }

                break;
            case ProtossStrategy::ZealotRush:
                if (isWorkerRush()) return ProtossStrategy::WorkerRush;

                // Consider the rush to be over after 6000 frames
                // From there the TwoGate handler will potentially transition into ZealotAllIn
                if (currentFrame >= 6000)
                {
                    strategy = ProtossStrategy::TwoGate;
                    continue;
                }

                break;
            case ProtossStrategy::EarlyForge:
                // The expected transition from here is into turtle or fast expansion, but detect others if the forge was a fake-out
                if (isWorkerRush()) return ProtossStrategy::WorkerRush;
                if (isZealotAllIn()) return ProtossStrategy::ZealotAllIn;
                if (isDragoonAllIn()) return ProtossStrategy::DragoonAllIn;
                if (isProxy()) return ProtossStrategy::ProxyRush;

                if (isFastExpansion())
                {
                    strategy = ProtossStrategy::FastExpansion;
                    continue;
                }

                if (isEarlyRobo())
                {
                    strategy = ProtossStrategy::EarlyRobo;
                    continue;
                }

                if (isTurtle())
                {
                    strategy = ProtossStrategy::Turtle;
                    continue;
                }

                if (isMidGame())
                {
                    strategy = ProtossStrategy::MidGame;
                    continue;
                }

                break;
            case ProtossStrategy::TwoGate:
                if (isWorkerRush()) return ProtossStrategy::WorkerRush;

                // We might detect a zealot rush late on large maps or if scouting is denied
                if (isZealotRush()) return ProtossStrategy::ZealotRush;

                if (isDarkTemplarRush()) return ProtossStrategy::DarkTemplarRush;
                if (isZealotAllIn()) return ProtossStrategy::ZealotAllIn;
                if (isDragoonAllIn()) return ProtossStrategy::DragoonAllIn;

                if (isEarlyRobo())
                {
                    strategy = ProtossStrategy::EarlyRobo;
                    continue;
                }

                if (isTurtle())
                {
                    strategy = ProtossStrategy::Turtle;
                    continue;
                }

                if (isMidGame())
                {
                    strategy = ProtossStrategy::MidGame;
                    continue;
                }

                break;
            case ProtossStrategy::NoZealotCore:
            case ProtossStrategy::OneZealotCore:
            case ProtossStrategy::FastExpansion:
                if (isDarkTemplarRush()) return ProtossStrategy::DarkTemplarRush;
                if (isZealotAllIn()) return ProtossStrategy::ZealotAllIn;
                if (isDragoonAllIn()) return ProtossStrategy::DragoonAllIn;

                if (isEarlyRobo())
                {
                    strategy = ProtossStrategy::EarlyRobo;
                    continue;
                }

                if (isTurtle())
                {
                    strategy = ProtossStrategy::Turtle;
                    continue;
                }

                if (isMidGame())
                {
                    strategy = ProtossStrategy::MidGame;
                    continue;
                }

                // Reconsider detected one-zealot core if we have just discovered an enemy core or assimilator
                if (enemyStrategy == ProtossStrategy::OneZealotCore && (
                        justDiscovered(BWAPI::UnitTypes::Protoss_Cybernetics_Core) || justDiscovered(BWAPI::UnitTypes::Protoss_Assimilator)))
                {
                    if (OpponentEconomicModel::enabled() &&
                        OpponentEconomicModel::worstCaseUnitCount(BWAPI::UnitTypes::Protoss_Zealot).second == 0)
                    {
                        strategy = ProtossStrategy::NoZealotCore;
                    }
                }

                break;
            case ProtossStrategy::BlockScouting:
                // An enemy that blocks our scouting could be doing anything, but we suspect some kind of rush or all-in
                if (isWorkerRush()) return ProtossStrategy::WorkerRush;
                if (isProxy()) return ProtossStrategy::ProxyRush; // If we see a proxy building somewhere
                if (isZealotRush()) return ProtossStrategy::ZealotRush;
                if (isDarkTemplarRush()) return ProtossStrategy::DarkTemplarRush;
                if (isZealotAllIn()) return ProtossStrategy::ZealotAllIn;
                if (isDragoonAllIn()) return ProtossStrategy::DragoonAllIn;

                if (isMidGame())
                {
                    strategy = ProtossStrategy::MidGame;
                    continue;
                }

                break;
            case ProtossStrategy::ZealotAllIn:
                // Enemy might transition from early zealot pressure into dark templar
                if (isDarkTemplarRush()) return ProtossStrategy::DarkTemplarRush;

                if (!isZealotAllIn())
                {
                    strategy = ProtossStrategy::TwoGate;
                    continue;
                }

                if (isMidGame())
                {
                    strategy = ProtossStrategy::MidGame;
                    continue;
                }

                break;
            case ProtossStrategy::DragoonAllIn:
                if (isDarkTemplarRush()) return ProtossStrategy::DarkTemplarRush;

                if (!isDragoonAllIn() && isEarlyRobo())
                {
                    strategy = ProtossStrategy::EarlyRobo;
                    continue;
                }

                if (isMidGame())
                {
                    strategy = ProtossStrategy::MidGame;
                    continue;
                }

                break;
            case ProtossStrategy::Turtle:
                if (isZealotAllIn()) return ProtossStrategy::ZealotAllIn;
                if (isDragoonAllIn()) return ProtossStrategy::DragoonAllIn;
                if (isProxy()) return ProtossStrategy::ProxyRush;
                if (isDarkTemplarRush()) return ProtossStrategy::DarkTemplarRush;

                if (isEarlyRobo())
                {
                    strategy = ProtossStrategy::EarlyRobo;
                    continue;
                }

                if (isMidGame())
                {
                    strategy = ProtossStrategy::MidGame;
                    continue;
                }

                break;
            case ProtossStrategy::EarlyRobo:
                if (isZealotAllIn()) return ProtossStrategy::ZealotAllIn;
                if (isDragoonAllIn()) return ProtossStrategy::DragoonAllIn;
                if (isDarkTemplarRush()) return ProtossStrategy::DarkTemplarRush;

                if (isMidGame())
                {
                    strategy = ProtossStrategy::MidGame;
                    continue;
                }

                break;
            case ProtossStrategy::DarkTemplarRush:
                if (!isDarkTemplarRush())
                {
                    strategy = ProtossStrategy::OneZealotCore;
                    continue;
                }

                if (isMidGame())
                {
                    strategy = ProtossStrategy::MidGame;
                    continue;
                }

                break;
            case ProtossStrategy::MidGame:
                break;
        }

        return strategy;
    }

    Log::Get() << "ERROR: Loop in strategy recognizer, ended on " << ProtossStrategyNames[strategy];
    return strategy;
}
