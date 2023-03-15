#include "ForgeFastExpand.h"

#include "Geo.h"
#include "General.h"
#include "Units.h"
#include "Map.h"
#include "Workers.h"
#include "Strategist.h"

/*
 * Our FFE play handles both performing the build order, defending the wall, and transitioning
 * into normal base defense if the wall is at any point determined to be indefensible.
 *
 * The play orders zealots it needs to defend the wall, but other army production is handled by the strategy engine.
 *
 * The logic is implemented as a state machine, see states defined in the class.
 */

ForgeFastExpand::ForgeFastExpand()
        : MainArmyPlay("ForgeFastExpand")
        , currentState(State::STATE_PYLON_PENDING)
        , squad(std::make_shared<DefendWallSquad>())
{
    General::addSquad(squad);
}

void ForgeFastExpand::update()
{
    auto &wall = BuildingPlacement::getForgeGatewayWall();

    // Update the current state
    switch (currentState)
    {
        case State::STATE_PYLON_PENDING:
            if (Units::myBuildingAt(wall.pylon))
            {
                currentState = State::STATE_FORGE_PENDING;
            }
            break;

        case State::STATE_FORGE_PENDING:
            // TODO: Add check for fast rush
            if (Units::myBuildingAt(wall.forge))
            {
                currentState = State::STATE_NEXUS_PENDING;
            }
            break;

        case State::STATE_NEXUS_PENDING:
        {
            auto natural = Map::getMyNatural();
            if (natural->resourceDepot && natural->resourceDepot->exists() && natural->owner == BWAPI::Broodwar->self())
            {
                currentState = State::STATE_GATEWAY_PENDING;
            }
            break;
        }

        case State::STATE_GATEWAY_PENDING:
            if (Units::myBuildingAt(wall.gateway))
            {
                currentState = State::STATE_FINISHED;
            }
            break;

        case State::STATE_FINISHED:
            // Final state
            break;

        case State::STATE_ANTIFASTRUSH:
            // TODO: Add logic for transitioning to DefendMyMain play when buildings at main are ready
            break;
    }
}

void ForgeFastExpand::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    /*
     * For all of the early states, this generates the current build order based on the information we have on the enemy.
     * When this is called, we expect the SaturateBases play to have added its probe goal, but nothing else to be added yet.
     */
    
    auto &wall = BuildingPlacement::getForgeGatewayWall();

    auto addBuilding = [&](BWAPI::UnitType type, BWAPI::TilePosition tile, int priority = PRIORITY_DEPOTS)
    {
        auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(tile),
                                                              BuildingPlacement::builderFrames(Map::getMyMain()->getPosition(),
                                                                                               tile,
                                                                                               type),
                                                              0,
                                                              0);
        prioritizedProductionGoals[priority].emplace_back(
                std::in_place_type<UnitProductionGoal>,
                label,
                type,
                1,
                1,
                buildLocation);
    };
    auto addCannons = [&](int desiredCount, int priority = PRIORITY_DEPOTS)
    {
        int currentCannons = 0;
        for (int i = 0; i < wall.cannons.size() && i < desiredCount; i++)
        {
            auto tile = wall.cannons[i];
            if (Units::myBuildingAt(tile))
            {
                currentCannons++;
                continue;
            }

            auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(tile),
                                                                  BuildingPlacement::builderFrames(Map::getMyMain()->getPosition(),
                                                                                                   tile,
                                                                                                   BWAPI::UnitTypes::Protoss_Photon_Cannon),
                                                                  0,
                                                                  0);
            prioritizedProductionGoals[priority].emplace_back(
                    std::in_place_type<UnitProductionGoal>,
                    label,
                    BWAPI::UnitTypes::Protoss_Photon_Cannon,
                    1,
                    1,
                    buildLocation);
        }
        return currentCannons;
    };
    auto addUnits = [&](BWAPI::UnitType type, int desiredCount, int priority = PRIORITY_DEPOTS)
    {
        int currentCount = Units::countAll(type);
        if (desiredCount > currentCount)
        {
            prioritizedProductionGoals[priority].emplace_back(
                    std::in_place_type<UnitProductionGoal>,
                    label,
                    type,
                    desiredCount - currentCount,
                    1);
        }
        return currentCount;
    };
    auto pauseProbeProduction = [&](int probeCountToPauseAt, int newPriority = PRIORITY_DEPOTS)
    {
        // Abort if there is no probe production in progress
        auto &workerGoals = prioritizedProductionGoals[PRIORITY_WORKERS];
        if (workerGoals.empty()) return;
        for (auto &workerGoal : workerGoals)
        {
            auto probeGoal = std::get_if<UnitProductionGoal>(&workerGoal);
            if (!probeGoal || !probeGoal->unitType().isWorker())
            {
                Log::Get() << "ERROR: Non-probe production goal in worker priority queue; unable to pause probe production";
                return;
            }
        }

        auto &newGoals = prioritizedProductionGoals[newPriority];

        // Get number of probes without counting scout worker
        int currentProbes = Units::countAll(BWAPI::UnitTypes::Protoss_Probe);
        if (Strategist::getWorkerScoutStatus() != Strategist::WorkerScoutStatus::Unstarted && !Strategist::isWorkerScoutComplete()) currentProbes--;

        // Move the required amount of probe production to a lower priority
        int probes = currentProbes;
        auto it = workerGoals.begin();
        while (it != workerGoals.end() && probes < probeCountToPauseAt)
        {
            auto probeGoal = std::get_if<UnitProductionGoal>(&*it);
            probes += probeGoal->countToProduce();
            if (probes > probeCountToPauseAt)
            {
                probeGoal->setCountToProduce(probeCountToPauseAt - (probes - probeGoal->countToProduce()));

                newGoals.emplace_back(std::in_place_type<UnitProductionGoal>,
                                      probeGoal->requester,
                                      BWAPI::UnitTypes::Protoss_Probe,
                                      probes - probeCountToPauseAt,
                                      probeGoal->getProducerLimit(),
                                      probeGoal->getLocation());
            }

            it++;
        }

        if (it != workerGoals.end())
        {
            newGoals.insert(newGoals.end(), std::make_move_iterator(it), std::make_move_iterator(workerGoals.end()));
            workerGoals.erase(it, workerGoals.end());
        }
    };

    switch (currentState)
    {
        case State::STATE_PYLON_PENDING:
            addBuilding(BWAPI::UnitTypes::Protoss_Pylon, wall.pylon);
            addBuilding(BWAPI::UnitTypes::Protoss_Forge, wall.forge);
            addCannons(2);
            addBuilding(BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            addBuilding(BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            pauseProbeProduction(14);
            addUnits(BWAPI::UnitTypes::Protoss_Zealot, 2);
            break;
        case State::STATE_FORGE_PENDING:
            addBuilding(BWAPI::UnitTypes::Protoss_Forge, wall.forge);
            addCannons(2);
            addBuilding(BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            addBuilding(BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            pauseProbeProduction(14);
            addUnits(BWAPI::UnitTypes::Protoss_Zealot, 2);
            break;
        case State::STATE_NEXUS_PENDING:
        {
            addCannons(2);
            addBuilding(BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            addBuilding(BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            pauseProbeProduction(14);
            addUnits(BWAPI::UnitTypes::Protoss_Zealot, 2);
            break;
        }
        case State::STATE_GATEWAY_PENDING:
        {
            addBuilding(BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            pauseProbeProduction(14);
            addUnits(BWAPI::UnitTypes::Protoss_Zealot, 2);
            break;
        }
        case State::STATE_FINISHED:
            addUnits(BWAPI::UnitTypes::Protoss_Zealot, 2);
            break;
        case State::STATE_ANTIFASTRUSH:
            // TODO: Pylon, cannon, gateway in main
            break;
    }
}
