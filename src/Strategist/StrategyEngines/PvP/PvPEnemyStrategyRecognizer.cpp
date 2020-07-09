#include "StrategyEngines/PvP.h"

#include "Units.h"
#include "Map.h"
#include "Strategist.h"

std::map<PvP::ProtossStrategy, std::string> PvP::ProtossStrategyNames = {
        {ProtossStrategy::Unknown,         "Unknown"},
        {ProtossStrategy::GasSteal,        "GasSteal"},
        {ProtossStrategy::ProxyRush,       "ProxyRush"},
        {ProtossStrategy::ZealotRush,      "ZealotRush"},
        {ProtossStrategy::EarlyForge,      "EarlyForge"},
        {ProtossStrategy::TwoGate,         "TwoGate"},
        {ProtossStrategy::OneGateCore,     "OneGateCore"},
        {ProtossStrategy::FastExpansion,   "FastExpansion"},
        {ProtossStrategy::BlockScouting,   "BlockScouting"},
        {ProtossStrategy::ZealotAllIn,     "ZealotAllIn"},
        {ProtossStrategy::DragoonAllIn,    "DragoonAllIn"},
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

    bool isGasSteal()
    {
        for (const Unit &unit : Units::allEnemyOfType(BWAPI::Broodwar->enemy()->getRace().getRefinery()))
        {
            if (!unit->lastPositionValid) continue;
            if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition)) != Map::getMyMain()->getArea()) continue;

            return true;
        }

        return false;
    }

    bool isFastExpansion()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Nexus, 6000, 2);
    }

    bool isZealotRush()
    {
        if (BWAPI::Broodwar->getFrameCount() >= 6000) return false;

        // We expect a zealot rush if we see an early zealot, early second zealot or early second gateway
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, 2800) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, 3300, 2) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Gateway, 2300, 2);
    }

    bool isProxy()
    {
        if (BWAPI::Broodwar->getFrameCount() >= 6000) return false;

        // If the enemy main has been scouted, determine if there is a proxy by looking at what they have built
        if (Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::EnemyBaseScouted ||
            Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::ScoutingCompleted)
        {
            // Expect first pylon by frame 1300
            if (BWAPI::Broodwar->getFrameCount() > 1300 && !countAtLeast(BWAPI::UnitTypes::Protoss_Pylon, 1)) return true;

            // Expect first gateway or forge by frame 2200
            if (BWAPI::Broodwar->getFrameCount() > 2200
                && !countAtLeast(BWAPI::UnitTypes::Protoss_Gateway, 1)
                && !countAtLeast(BWAPI::UnitTypes::Protoss_Forge, 1))
            {
                return true;
            }

            // If the enemy hasn't built a forge, expect a second pylon by frame 4000 if we still have a live scout
            if (BWAPI::Broodwar->getFrameCount() > 4000 &&
                Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::EnemyBaseScouted &&
                !countAtLeast(BWAPI::UnitTypes::Protoss_Forge, 1) &&
                !countAtLeast(BWAPI::UnitTypes::Protoss_Pylon, 2))
            {
                return true;
            }

            return false;
        }

        // Otherwise check if we have directly scouted an enemy building in a proxy location
        auto enemyMain = Map::getEnemyStartingMain();
        auto enemyNatural = enemyMain ? Map::getNaturalForStartLocation(enemyMain->getTilePosition()) : nullptr;
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

        return false;
    }

    bool isZealotAllIn()
    {
        // In the early game, we consider the enemy to be doing a zealot all-in if we either see a lot of zealots or see two gateways with
        // no core or gas
        if (BWAPI::Broodwar->getFrameCount() < 6000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, 5000, 4) ||
                   (noneCreated(BWAPI::UnitTypes::Protoss_Assimilator) &&
                    noneCreated(BWAPI::UnitTypes::Protoss_Cybernetics_Core) &&
                    countAtLeast(BWAPI::UnitTypes::Protoss_Gateway, 2));
        }

        // Later on, we consider it to be a zealot all-in purely based on the counts
        if (BWAPI::Broodwar->getFrameCount() < 8000)
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
        if (BWAPI::Broodwar->getFrameCount() < 10000 &&
            createdBeforeUnit(BWAPI::UnitTypes::Protoss_Gateway, 4, BWAPI::UnitTypes::Protoss_Robotics_Facility, 1) &&
            createdBeforeUnit(BWAPI::UnitTypes::Protoss_Gateway, 4, BWAPI::UnitTypes::Protoss_Templar_Archives, 1))
        {
            return true;
        }

        // Otherwise work off of goon timings
        if (BWAPI::Broodwar->getFrameCount() < 8000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Dragoon, 8000, 6);
        }
        if (BWAPI::Broodwar->getFrameCount() < 9000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Dragoon, 8000, 6) ||
                   createdBeforeFrame(BWAPI::UnitTypes::Protoss_Dragoon, 9000, 10);
        }
        if (BWAPI::Broodwar->getFrameCount() < 10000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Dragoon, 9000, 10) ||
                   createdBeforeFrame(BWAPI::UnitTypes::Protoss_Dragoon, 10000, 14);
        }
        if (BWAPI::Broodwar->getFrameCount() < 11000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Dragoon, 10000, 14) ||
                   createdBeforeFrame(BWAPI::UnitTypes::Protoss_Dragoon, 11000, 18);
        }
        if (BWAPI::Broodwar->getFrameCount() < 12000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Dragoon, 11000, 18) ||
                   createdBeforeFrame(BWAPI::UnitTypes::Protoss_Dragoon, 12000, 22);
        }

        return false;
    }

    bool isDarkTemplarRush()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Templar_Archives, 8000) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Dark_Templar, 10000);
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
        // We consider ourselves to be in the mid-game after frame 10000 if the enemy has taken their natural or teched to something beyond goons
        if (BWAPI::Broodwar->getFrameCount() < 10000) return false;

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
                if (isZealotRush()) return ProtossStrategy::ZealotRush;
                if (isGasSteal()) return ProtossStrategy::GasSteal;
                if (isProxy()) return ProtossStrategy::ProxyRush;

                // If the enemy blocks our scouting, set their strategy accordingly
                if (Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::ScoutingBlocked)
                {
                    strategy = ProtossStrategy::BlockScouting;
                    continue;
                }

                if (isFastExpansion()) return ProtossStrategy::FastExpansion;

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
                if (createdBeforeUnit(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, BWAPI::UnitTypes::Protoss_Gateway, 2) ||
                    createdBeforeUnit(BWAPI::UnitTypes::Protoss_Assimilator, 1, BWAPI::UnitTypes::Protoss_Gateway, 2))
                {
                    strategy = ProtossStrategy::OneGateCore;
                    continue;
                }

                // Default to something reasonable if our scouting completely fails
                if (BWAPI::Broodwar->getFrameCount() > 4000)
                {
                    strategy = ProtossStrategy::OneGateCore;
                    continue;
                }

                break;
            case ProtossStrategy::GasSteal:
                if (!isGasSteal())
                {
                    strategy = ProtossStrategy::Unknown;
                    continue;
                }
                break;
            case ProtossStrategy::ProxyRush:
            case ProtossStrategy::ZealotRush:
                // Consider the rush to be over after 6000 frames
                // From there the TwoGate handler will potentially transition into ZealotAllIn
                if (BWAPI::Broodwar->getFrameCount() >= 6000)
                {
                    strategy = ProtossStrategy::TwoGate;
                    continue;
                }

                break;
            case ProtossStrategy::EarlyForge:
                if (isGasSteal()) return ProtossStrategy::GasSteal;

                // The expected transition from here is into turtle or fast expansion, but detect others if the forge was a fake-out
                if (isZealotAllIn()) return ProtossStrategy::ZealotAllIn;
                if (isDragoonAllIn()) return ProtossStrategy::DragoonAllIn;
                if (isProxy()) return ProtossStrategy::ProxyRush;

                if (isFastExpansion())
                {
                    strategy = ProtossStrategy::FastExpansion;
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
                // We might detect a zealot rush late on large maps or if scouting is denied
                if (isZealotRush()) return ProtossStrategy::ZealotRush;

                if (isGasSteal()) return ProtossStrategy::GasSteal;
                if (isDarkTemplarRush()) return ProtossStrategy::DarkTemplarRush;
                if (isZealotAllIn()) return ProtossStrategy::ZealotAllIn;
                if (isDragoonAllIn()) return ProtossStrategy::DragoonAllIn;

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
            case ProtossStrategy::OneGateCore:
            case ProtossStrategy::FastExpansion:
                if (isGasSteal()) return ProtossStrategy::GasSteal;
                if (isDarkTemplarRush()) return ProtossStrategy::DarkTemplarRush;
                if (isZealotAllIn()) return ProtossStrategy::ZealotAllIn;
                if (isDragoonAllIn()) return ProtossStrategy::DragoonAllIn;

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
            case ProtossStrategy::BlockScouting:
                // An enemy that blocks our scouting could be doing anything, but we suspect some kind of rush or all-in
                if (isGasSteal()) return ProtossStrategy::GasSteal;
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
                if (isGasSteal()) return ProtossStrategy::GasSteal;

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
                if (isGasSteal()) return ProtossStrategy::GasSteal;
                if (isDarkTemplarRush()) return ProtossStrategy::DarkTemplarRush;

                if (isMidGame())
                {
                    strategy = ProtossStrategy::MidGame;
                    continue;
                }

                break;
            case ProtossStrategy::Turtle:
                if (isGasSteal()) return ProtossStrategy::GasSteal;
                if (isZealotAllIn()) return ProtossStrategy::ZealotAllIn;
                if (isDragoonAllIn()) return ProtossStrategy::DragoonAllIn;
                if (isProxy()) return ProtossStrategy::ProxyRush;
                if (isDarkTemplarRush()) return ProtossStrategy::DarkTemplarRush;

                if (isMidGame())
                {
                    strategy = ProtossStrategy::MidGame;
                    continue;
                }

                break;
            case ProtossStrategy::DarkTemplarRush:
                if (isGasSteal()) return ProtossStrategy::GasSteal;

                if (!isDarkTemplarRush())
                {
                    strategy = ProtossStrategy::OneGateCore;
                    continue;
                }

                if (isMidGame())
                {
                    strategy = ProtossStrategy::MidGame;
                    continue;
                }

                break;
            case ProtossStrategy::MidGame:
                if (isGasSteal()) return ProtossStrategy::GasSteal;

                break;
        }

        return strategy;
    }

    Log::Get() << "ERROR: Loop in strategy recognizer, ended on " << ProtossStrategyNames[strategy];
    return strategy;
}
