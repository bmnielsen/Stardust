#include "Workers.h"

#include "Base.h"
#include "Units.h"
#include "PathFinding.h"
#include "Map.h"
#include "Builder.h"

/*

Does this thing work?
if (u->order_process_timer) {
            --u->order_process_timer;
            return;
}
u->order_process_timer = 8;
switch (u->order_type->id) {
case Orders::MoveToGas:
            order_MoveToGas(u);
            break;
case Orders::WaitForGas:
            order_WaitForGas(u);
            break;
}

https://www.teamliquid.net/forum/brood-war/484849-improving-mineral-gathering-rate-in-brood-war

order_process_timer
order_timer_counter
*/

namespace Workers
{
#ifndef _DEBUG
    namespace
    {
#endif
        enum Job { None, Minerals, Gas, Build, Combat, Scout, ReturnCargo };

        int _desiredGasWorkers;
        std::map<BWAPI::Unit, Job>                              workerJob;
        std::map<BWAPI::Unit, Base *>                           workerBase;
        std::map<Base *, std::set<BWAPI::Unit>>                 baseWorkers;
        std::map<BWAPI::Unit, BWAPI::Unit>                      workerMineralPatch;
        std::map<BWAPI::Unit, std::set<BWAPI::Unit>>            mineralPatchWorkers;
        std::map<BWAPI::Unit, BWAPI::Unit>                      workerRefinery;
        std::map<BWAPI::Unit, std::set<BWAPI::Unit>>            refineryWorkers;

        // These are used for optimization of order_timer_counter
        // i.e. it is most efficient to send a gather order exactly 8+LF frames before the worker reaches the resource
        std::map<BWAPI::Unit, std::set<BWAPI::Position>>                                resourceOptimalOrderPositions;
        std::map<BWAPI::Unit, std::map<BWAPI::Unit, std::vector<BWAPI::Position>>>      resourceWorkerPositionHistory;

        // Removes a worker's base and mineral patch or gas assignments
        void removeFromResource(
            BWAPI::Unit unit, 
            std::map<BWAPI::Unit, BWAPI::Unit> & workerAssignment,
            std::map<BWAPI::Unit, std::set<BWAPI::Unit>> & resourceAssignment)
        {
            auto baseIt = workerBase.find(unit);
            if (baseIt != workerBase.end())
            {
                baseWorkers[baseIt->second].erase(unit);
                workerBase.erase(baseIt);
            }

            auto resourceIt = workerAssignment.find(unit);
            if (resourceIt != workerAssignment.end())
            {
                resourceAssignment[resourceIt->second].erase(unit);
                workerAssignment.erase(resourceIt);
            }
        }

        // Cleans up data maps when a worker is lost
        void workerLost(BWAPI::Unit unit)
        {
            workerJob.erase(unit);
            removeFromResource(unit, workerMineralPatch, mineralPatchWorkers);
            removeFromResource(unit, workerRefinery, refineryWorkers);
        }

        int availableMineralAssignmentsAtBase(Base * base)
        {
            if (base->owner != Base::Owner::Me) return 0;
            if (!base->resourceDepot || !base->resourceDepot->exists()) return 0;

            int count = base->mineralPatchCount() * 2;
            for (auto worker : baseWorkers[base])
            {
                if (workerJob[worker] == Minerals) count--;
            }

            return count;
        }

        int availableGasAssignmentsAtBase(Base * base)
        {
            if (base->owner != Base::Owner::Me) return 0;
            if (!base->resourceDepot || !base->resourceDepot->exists()) return 0;

            int count = 0;
            for (auto refinery : base->refineries())
            {
                if (!refinery->isCompleted()) continue;
                count += 3;
            }
            for (auto worker : baseWorkers[base])
            {
                if (workerJob[worker] == Gas) count--;
            }

            return count;
        }

        // Assign a worker to the closest base with either free mineral or gas assignments depending on job
        Base * assignBase(BWAPI::Unit unit, Job job)
        {
            // TODO: Prioritize empty bases over nearly-full bases
            // TODO: Consider whether or not there is a safe path to the base

            int bestFrames = INT_MAX;
            Base * bestBase = nullptr;
            for (auto & base : Map::allBases())
            {
                if (job == Minerals && availableMineralAssignmentsAtBase(base) <= 0) continue;
                if (job == Gas && availableGasAssignmentsAtBase(base) <= 0) continue;

                int frames = PathFinding::ExpectedTravelTime(unit->getPosition(), base->getPosition(), unit->getType(), PathFinding::PathFindingOptions::UseNearestBWEMArea);

                if (!base->resourceDepot->isCompleted())
                    frames = std::max(frames, base->resourceDepot->getRemainingBuildTime());

                if (frames < bestFrames)
                {
                    bestFrames = frames;
                    bestBase = base;
                }
            }

            if (bestBase)
            {
                workerBase[unit] = bestBase;
                baseWorkers[bestBase].insert(unit);
            }

            return bestBase;
        }

        // Assign a worker to a mineral patch in its current base
        // We only call this when the worker is close to the base depot, so we can assume minimal pathing issues
        BWAPI::Unit assignMineralPatch(BWAPI::Unit unit)
        {
            if (!workerBase[unit]) return nullptr;

            int bestDist = INT_MAX;
            BWAPI::Unit bestMineralPatch = nullptr;
            for (auto mineralPatch : workerBase[unit]->mineralPatches())
            {
                if (!mineralPatch->exists()) continue;

                int workers = mineralPatchWorkers[mineralPatch].size();
                if (workers >= 2) continue;

                int dist = unit->getDistance(mineralPatch);
                if (workers > 0) dist += 1000;

                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestMineralPatch = mineralPatch;
                }
            }

            if (bestMineralPatch)
            {
                workerMineralPatch[unit] = bestMineralPatch;
                mineralPatchWorkers[bestMineralPatch].insert(unit);
            }

            return bestMineralPatch;
        }

        // Assign a worker to a refinery in its current base
        // We only call this when the worker is close to the base depot, so we can assume minimal pathing issues
        BWAPI::Unit assignRefinery(BWAPI::Unit unit)
        {
            if (!workerBase[unit]) return nullptr;

            int bestDist = INT_MAX;
            BWAPI::Unit bestRefinery = nullptr;
            for (auto refinery : workerBase[unit]->refineries())
            {
                if (refinery->getPlayer() != BWAPI::Broodwar->self()) continue;
                if (!refinery->isCompleted()) continue;

                int workers = refineryWorkers[refinery].size();
                if (workers >= 3) continue;

                int dist = unit->getDistance(refinery);
                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestRefinery = refinery;
                }
            }

            if (bestRefinery)
            {
                workerRefinery[unit] = bestRefinery;
                refineryWorkers[bestRefinery].insert(unit);
            }

            return bestRefinery;
        }

        void assignGasWorker()
        {
            // Find worker closest to an available refinery
            int bestDist = INT_MAX;
            BWAPI::Unit bestWorker = nullptr;
            for (auto worker : BWAPI::Broodwar->self()->getUnits())
            {
                if (!isAvailableForReassignment(worker)) continue;

                for (auto refinery : BWAPI::Broodwar->self()->getUnits())
                {
                    if (refinery->getType() != BWAPI::Broodwar->self()->getRace().getRefinery()) continue;
                    if (!refinery->isCompleted()) continue;
                    if (refineryWorkers[refinery].size() >= 3) continue;

                    // TODO: Consider depleted geysers

                    int dist = worker->getDistance(refinery);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestWorker = worker;
                    }
                }
            }

            if (bestWorker)
            {
                workerJob[bestWorker] = Gas;
                removeFromResource(bestWorker, workerMineralPatch, mineralPatchWorkers);
                assignBase(bestWorker, Gas);
                assignRefinery(bestWorker);
            }
        }

        void removeGasWorker()
        {
            // Remove a gas worker at a base that has available mineral assignments
            for (auto workerAndRefinery : workerRefinery)
            {
                if (availableMineralAssignmentsAtBase(workerBase[workerAndRefinery.first]) > 0)
                {
                    workerJob[workerAndRefinery.first] = None;
                    removeFromResource(workerAndRefinery.first, workerRefinery, refineryWorkers);
                    return;
                }
            }
        }

        bool handleOrderTimerCounterOptimization(BWAPI::Unit worker, BWAPI::Unit resource)
        {
            // Verify the current order
            if (worker->getOrder() != BWAPI::Orders::MoveToMinerals && worker->getOrder() != BWAPI::Orders::MoveToGas) return false;

            // Break out early if the distance is larger than we need to worry about
            auto dist = worker->getDistance(resource);
            if (dist > 100) return false;

            auto & optimalPositions = resourceOptimalOrderPositions[resource];
            auto & positionHistory = resourceWorkerPositionHistory[resource][worker];

            // If the worker is at a position we know to be optimal, resend the order
            if (optimalPositions.find(worker->getPosition()) != optimalPositions.end())
            {
                Units::getMine(worker).gather(resource);
                positionHistory.push_back(worker->getPosition());
                return true;
            }

            // If the worker isn't at the resource yet, record its position
            if (dist > 0)
            {
                positionHistory.push_back(worker->getPosition());
                return false;
            }

            // The worker is at the resource, so if we have enough position history recorded,
            // record the optimal position
            size_t framesAgo = BWAPI::Broodwar->getLatencyFrames() + 11;
            if (positionHistory.size() >= framesAgo)
            {
                auto result = optimalPositions.insert(positionHistory[positionHistory.size() - framesAgo]);
                if (result.second)
                    Log::Debug() << worker->getType() << " " << worker->getID() << " : Inserted optimal position " << positionHistory[positionHistory.size() - framesAgo] << " for resource " << resource->getPosition();
            }

            positionHistory.clear();

            return false;
        }
#ifndef _DEBUG
    }
#endif

    void onUnitDestroy(BWAPI::Unit unit)
    {
        // Handle lost workers
        if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self())
        {
            workerLost(unit);
        }

        // Handle lost assimilators
        // FIXME: I think this actually morphs instead
        else if (unit->getType() == BWAPI::Broodwar->self()->getRace().getRefinery() && unit->getPlayer() == BWAPI::Broodwar->self())
        {
            auto it = refineryWorkers.find(unit);
            if (it != refineryWorkers.end())
            {
                for (auto & worker : it->second)
                {
                    workerRefinery.erase(worker);
                }
                refineryWorkers.erase(it);
            }
        }

        // Handle resources mined out
        else if (unit->getType().isMineralField())
        {
            auto it = mineralPatchWorkers.find(unit);
            if (it != mineralPatchWorkers.end())
            {
                for (auto & worker : it->second)
                {
                    workerMineralPatch.erase(worker);
                }
                mineralPatchWorkers.erase(it);
            }
        }
    }

    void onUnitRenegade(BWAPI::Unit unit)
    {
        // If it might've been our worker, treat it as lost
        if (unit->getType().isWorker() && unit->getPlayer() != BWAPI::Broodwar->self())
        {
            workerLost(unit);
        }
    }

    void updateAssignments()
    {
        for (auto & worker : BWAPI::Broodwar->self()->getUnits())
        {
            if (!worker->getType().isWorker()) continue;
            if (!worker->isCompleted()) continue;

            switch (workerJob[worker])
            {
            case None:
                workerJob[worker] = Job::Minerals;
                // Fall-through

            case Minerals:
                {
                    // If the worker is already assigned to a mineral patch, we don't need to do any more
                    auto mineralPatch = workerMineralPatch[worker];
                    if (mineralPatch && mineralPatch->exists()) continue;

                    // If the worker doesn't have an assigned base, assign it one
                    // FIXME: Release from bases when mined out
                    auto base = workerBase[worker];
                    if (!base || !base->resourceDepot || !base->resourceDepot->exists())
                    {
                        base = assignBase(worker, Minerals);

                        // Maybe we have no more with available patches
                        if (!base)
                        {
                            workerJob[worker] = Job::None;
                            continue;
                        }
                    }

                    // Assign a mineral patch if:
                    // - It is close to its assigned base
                    // - It is not carrying cargo
                    if (worker->getDistance(base->getPosition()) <= 200 &&
                        !worker->isCarryingMinerals() &&
                        !worker->isCarryingGas())
                    {
                        assignMineralPatch(worker);
                    }

                    break;
                }
            case Gas:
                {
                    // If the worker is already assigned to a refinery, we don't need to do any more
                    auto refinery = workerRefinery[worker];
                    if (refinery && refinery->exists()) continue;

                    // If the worker doesn't have an assigned base, assign it one
                    auto base = workerBase[worker];
                    if (!base || !base->resourceDepot || !base->resourceDepot->exists())
                    {
                        base = assignBase(worker, Gas);

                        // Maybe we have no more with available gas
                        if (!base)
                        {
                            workerJob[worker] = Job::None;
                            continue;
                        }
                    }

                    // Assign a refinery if:
                    // - It is close to its assigned base
                    // - It is not carrying cargo
                    if (worker->getDistance(base->getPosition()) <= 200 &&
                        !worker->isCarryingMinerals() &&
                        !worker->isCarryingGas())
                    {
                        assignRefinery(worker);
                    }

                    break;
                }
            }
        }
    }

    void issueOrders()
    {
        // Adjust number of gas workers to desired count
        for (int i = gasWorkers(); i < _desiredGasWorkers; i++)
            assignGasWorker();
        for (int i = gasWorkers(); i > _desiredGasWorkers; i--)
            removeGasWorker();

        for (auto & pair : workerJob)
        {
            auto & worker = pair.first;
            switch (pair.second)
            {
            case Minerals:
            case Gas:
                // Skip if the worker doesn't have a valid base
                auto base = workerBase[worker];
                if (!base || !base->resourceDepot || !base->resourceDepot->exists())
                {
                    continue;
                }

                // Handle mining from an assigned mineral patch
                auto mineralPatch = workerMineralPatch[worker];
                if (mineralPatch && mineralPatch->exists())
                {
                    // If the unit is currently mining, leave it alone
                    if (worker->getOrder() == BWAPI::Orders::MiningMinerals ||
                        worker->getOrder() == BWAPI::Orders::ResetCollision)
                    {
                        continue;
                    }

                    // TODO: Look into the same for gas
                    // It doesn't really help though unless we only put two workers on gas
                    if (handleOrderTimerCounterOptimization(worker, mineralPatch)) continue;

                    // If the unit is returning cargo, leave it alone
                    if (worker->getOrder() == BWAPI::Orders::ReturnMinerals) continue;

                    // Check if another worker is currently mining this patch
                    BWAPI::Unit otherWorker = nullptr;
                    for (auto unit : mineralPatchWorkers[mineralPatch])
                    {
                        if (unit != worker)
                        {
                            otherWorker = unit;
                            break;
                        }
                    }
                                        
                    // Resend the gather command when we expect the other worker to be finished mining in 11+LF frames
                    if (otherWorker && otherWorker->getOrder() == BWAPI::Orders::MiningMinerals &&
                        (otherWorker->getOrderTimer() + 7) == (11 + BWAPI::Broodwar->getLatencyFrames()))
                    {
                        Units::getMine(worker).gather(mineralPatch);
                        continue;
                    }

                    // Mineral locking: if the unit is moving to or waiting for minerals, make sure it doesn't switch targets
                    if (worker->getOrder() == BWAPI::Orders::MoveToMinerals ||
                        worker->getOrder() == BWAPI::Orders::WaitForMinerals)
                    {
                        if (worker->getOrderTarget() != mineralPatch)
                        {
                            Units::getMine(worker).gather(mineralPatch);
                        }

                        continue;
                    }

                    // Otherwise for all other orders click on the mineral patch
                    Units::getMine(worker).gather(mineralPatch);
                    continue;
                }

                // Handle gathering from an assigned refinery
                auto refinery = workerRefinery[worker];
                if (refinery && refinery->exists() && worker->getDistance(refinery) < 500)
                {
                    // If the unit is currently gathering, leave it alone
                    if (worker->getOrder() == BWAPI::Orders::MoveToGas ||
                        worker->getOrder() == BWAPI::Orders::WaitForGas ||
                        worker->getOrder() == BWAPI::Orders::HarvestGas ||
                        worker->getOrder() == BWAPI::Orders::Harvest1 ||
                        worker->getOrder() == BWAPI::Orders::ReturnGas)
                    {
                        continue;
                    }

                    // Otherwise click on the refinery
                    Units::getMine(worker).gather(refinery);
                    continue;
                }

                // If the worker is a long way from its base, move towards it
                if (worker->getDistance(base->getPosition()) > 200)
                {
                    Units::getMine(worker).moveTo(base->getPosition());
                    continue;
                }

                // If the worker has cargo, return it
                if (worker->isCarryingMinerals() || worker->isCarryingGas())
                {
                    Units::getMine(worker).returnCargo();
                    continue;
                }

                // For some reason the worker doesn't have anything good to do, so let's just move towards the base
                Units::getMine(worker).moveTo(base->getPosition());
                break;
            }
        }
    }

    bool isAvailableForReassignment(BWAPI::Unit unit)
    {
        if (!unit || !unit->exists() || !unit->isCompleted() || !unit->getType().isWorker()) return false;

        auto job = workerJob[unit];
        if (job == Job::None) return true;
        if (job == Job::Minerals)
        {
            if (unit->isCarryingMinerals()) return false;
            if (unit->isCarryingGas()) return false;
            return (unit->getOrder() == BWAPI::Orders::Move 
                || unit->getOrder() == BWAPI::Orders::MoveToMinerals
                || unit->getOrder() == BWAPI::Orders::WaitForMinerals);
        }

        return false;
    }

    void setBuilder(BWAPI::Unit unit)
    {
        if (!unit || !unit->exists() || !unit->getType().isWorker() || !unit->isCompleted()) return;

        workerJob[unit] = Job::Build;
        removeFromResource(unit, workerMineralPatch, mineralPatchWorkers);

    }

    void releaseWorker(BWAPI::Unit unit)
    {
        if (!unit || !unit->exists() || !unit->getType().isWorker() || !unit->isCompleted()) return;

        workerJob[unit] = Job::None;
    }

    int availableMineralAssignments()
    {
        int count = 0;

        for (auto & base : Map::allBases())
        {
            if (base->owner != Base::Owner::Me) continue;
            if (!base->resourceDepot || !base->resourceDepot->exists() || !base->resourceDepot->isCompleted()) continue;

            for (auto mineralPatch : base->mineralPatches())
            {
                if (!mineralPatch->exists()) continue;

                int workers = mineralPatchWorkers[mineralPatch].size();
                if (workers < 2) count += 2 - workers;
            }
        }

        return count;
    }

    int availableGasAssignments()
    {
        return 0;
    }

    void setDesiredGasWorkers(int gasWorkers)
    {
        _desiredGasWorkers = gasWorkers;
    }

    int desiredGasWorkers()
    {
        return _desiredGasWorkers;
    }

    int mineralWorkers()
    {
        int mineralWorkers = 0;
        for (auto & workerAndAssignedPatch : workerMineralPatch)
        {
            if (workerAndAssignedPatch.first->exists() && workerAndAssignedPatch.second && workerAndAssignedPatch.second->exists() &&
                workerAndAssignedPatch.first->getDistance(workerAndAssignedPatch.second) < 200) // Don't count workers that are on a long journey towards the patch
            {
                mineralWorkers++;
            }
        }

        return mineralWorkers;
    }

    int gasWorkers()
    {
        int gasWorkers = 0;
        for (auto & workerAndAssignedRefinery : workerRefinery)
        {
            if (workerAndAssignedRefinery.first->exists() && workerAndAssignedRefinery.second && workerAndAssignedRefinery.second->exists() &&
                workerAndAssignedRefinery.first->getDistance(workerAndAssignedRefinery.second) < 200) // Don't count workers that are on a long journey towards the refinery
            {
                gasWorkers++;
            }
        }

        return gasWorkers;
    }
}
