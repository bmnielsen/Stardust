#include "Workers.h"

#include "Base.h"
#include "Units.h"
#include "PathFinding.h"
#include "Map.h"
#include "WorkerOrderTimer.h"
#include "Geo.h"

namespace Workers
{
    namespace
    {
        enum class Job
        {
            None, Minerals, Gas, Reserved
        };

        int _desiredGasWorkers;
        std::map<MyUnit, Job> workerJob;
        std::map<MyUnit, Base *> workerBase;
        std::map<Base *, std::set<MyUnit>> baseWorkers;
        std::map<MyUnit, BWAPI::Unit> workerMineralPatch;
        std::map<BWAPI::Unit, std::set<MyUnit>> mineralPatchWorkers;
        std::map<MyUnit, BWAPI::Unit> workerRefinery;
        std::map<BWAPI::Unit, std::set<MyUnit>> refineryWorkers;

        // Removes a worker's base and mineral patch or gas assignments
        void removeFromResource(
                const MyUnit &unit,
                std::map<MyUnit, BWAPI::Unit> &workerAssignment,
                std::map<BWAPI::Unit, std::set<MyUnit>> &resourceAssignment)
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
        void workerLost(const MyUnit &unit)
        {
            workerJob.erase(unit);
            removeFromResource(unit, workerMineralPatch, mineralPatchWorkers);
            removeFromResource(unit, workerRefinery, refineryWorkers);
        }

        int availableMineralAssignmentsAtBase(Base *base)
        {
            if (!base || base->owner != BWAPI::Broodwar->self()) return 0;
            if (!base->resourceDepot || !base->resourceDepot->exists()) return 0;

            int count = base->mineralPatchCount() * 2;
            for (const auto &worker : baseWorkers[base])
            {
                if (workerJob[worker] == Job::Minerals) count--;
            }

            return count;
        }

        int availableGasAssignmentsAtBase(Base *base)
        {
            if (base->owner != BWAPI::Broodwar->self()) return 0;
            if (!base->resourceDepot || !base->resourceDepot->exists()) return 0;

            int count = 0;
            for (auto refinery : base->refineries())
            {
                if (!refinery->isCompleted()) continue;
                count += 3;
            }
            for (const auto &worker : baseWorkers[base])
            {
                if (workerJob[worker] == Job::Gas) count--;
            }

            return count;
        }

        void assignInitialMineralWorkers()
        {
            auto base = Map::getMyMain();

            // Sort the mineral patches by proximity to the nexus
            auto mineralPatches = base->mineralPatches();
            std::sort(mineralPatches.begin(), mineralPatches.end(), [&](const BWAPI::Unit &a, const BWAPI::Unit &b) -> bool
            {
                return base->resourceDepot->bwapiUnit->getDistance(a) < base->resourceDepot->bwapiUnit->getDistance(b);
            });

            // We are only interested in the first four
            mineralPatches.resize(4);
            std::set<BWAPI::Unit> availablePatches(mineralPatches.begin(), mineralPatches.end());

            // Greedily take the closest matches until all probes are assigned
            // TODO: Should really be optimizing for 7th collection
            for (int i = 0; i < 4; i++)
            {
                int bestDist = INT_MAX;
                MyUnit bestWorker = nullptr;
                BWAPI::Unit bestPatch = nullptr;
                for (auto &worker : Units::allMine())
                {
                    if (!worker->type.isWorker()) continue;
                    if (!worker->completed) continue;
                    if (workerMineralPatch[worker]) continue;

                    for (auto &patch : availablePatches)
                    {
                        int dist = worker->bwapiUnit->getDistance(patch);
                        if (dist < bestDist)
                        {
                            bestDist = dist;
                            bestWorker = worker;
                            bestPatch = patch;
                        }
                    }
                }

                if (bestWorker && bestPatch)
                {
                    workerJob[bestWorker] = Job::Minerals;
                    CherryVis::log(bestWorker->id) << "Assigned to Minerals";
                    workerBase[bestWorker] = base;
                    baseWorkers[base].insert(bestWorker);
                    workerMineralPatch[bestWorker] = bestPatch;
                    mineralPatchWorkers[bestPatch].insert(bestWorker);
                    availablePatches.erase(bestPatch);
                }
            }
        }

        // Assign a worker to the closest base with either free mineral or gas assignments depending on job
        Base *assignBase(const MyUnit &unit, Job job)
        {
            // TODO: Prioritize empty bases over nearly-full bases
            // TODO: Consider whether or not there is a safe path to the base

            int bestFrames = INT_MAX;
            Base *bestBase = nullptr;
            for (auto &base : Map::allBases())
            {
                if (job == Job::Minerals && availableMineralAssignmentsAtBase(base) <= 0) continue;
                if (job == Job::Gas && availableGasAssignmentsAtBase(base) <= 0) continue;

                int frames = PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                             base->getPosition(),
                                                             unit->type,
                                                             PathFinding::PathFindingOptions::UseNearestBWEMArea);

                if (!base->resourceDepot->completed)
                    frames = std::max(frames, base->resourceDepot->bwapiUnit->getRemainingBuildTime());

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
        BWAPI::Unit assignMineralPatch(const MyUnit &unit)
        {
            if (!workerBase[unit]) return nullptr;

            int closestDist = INT_MAX;
            int furthestDist = 0;
            BWAPI::Unit closest = nullptr;
            BWAPI::Unit furthest = nullptr;
            for (auto mineralPatch : workerBase[unit]->mineralPatches())
            {
                int workers = mineralPatchWorkers[mineralPatch].size();
                if (workers >= 2) continue;

                int dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Nexus,
                                                   workerBase[unit]->getPosition(),
                                                   mineralPatch->getType(),
                                                   mineralPatch->getInitialPosition());
                if (workers == 0 && dist < closestDist)
                {
                    closestDist = dist;
                    closest = mineralPatch;
                }
                else if (workers == 1 && dist > furthestDist)
                {
                    furthestDist = dist;
                    furthest = mineralPatch;
                }
            }

            BWAPI::Unit best = closest ? closest : furthest;
            if (best)
            {
                workerMineralPatch[unit] = best;
                mineralPatchWorkers[best].insert(unit);
            }

            return best;
        }

        // Assign a worker to a refinery in its current base
        // We only call this when the worker is close to the base depot, so we can assume minimal pathing issues
        BWAPI::Unit assignRefinery(const MyUnit &unit)
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

                int dist = unit->bwapiUnit->getDistance(refinery);
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
            MyUnit bestWorker = nullptr;
            for (const auto &worker : Units::allMine())
            {
                if (!isAvailableForReassignment(worker, false)) continue;

                for (auto refinery : BWAPI::Broodwar->self()->getUnits())
                {
                    if (refinery->getType() != BWAPI::Broodwar->self()->getRace().getRefinery()) continue;
                    if (!refinery->isCompleted()) continue;
                    if (refineryWorkers[refinery].size() >= 3) continue;

                    // TODO: Consider depleted geysers

                    int dist = worker->bwapiUnit->getDistance(refinery);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestWorker = worker;
                    }
                }
            }

            if (bestWorker)
            {
                workerJob[bestWorker] = Job::Gas;
                removeFromResource(bestWorker, workerMineralPatch, mineralPatchWorkers);
                assignBase(bestWorker, Job::Gas);
                assignRefinery(bestWorker);

                CherryVis::log(bestWorker->id) << "Assigned to Gas";
            }
        }

        void removeGasWorker()
        {
            // Remove a gas worker at a base that has available mineral assignments
            for (const auto &workerAndRefinery : workerRefinery)
            {
                if (availableMineralAssignmentsAtBase(workerBase[workerAndRefinery.first]) > 0)
                {
                    workerJob[workerAndRefinery.first] = Job::None;
                    removeFromResource(workerAndRefinery.first, workerRefinery, refineryWorkers);
                    return;
                }
            }
        }
    }

    void initialize()
    {
        _desiredGasWorkers = 0;
        workerJob.clear();
        workerBase.clear();
        baseWorkers.clear();
        workerMineralPatch.clear();
        mineralPatchWorkers.clear();
        workerRefinery.clear();
        refineryWorkers.clear();
    }

    void onUnitDestroy(const Unit &unit)
    {
        // Handle lost workers
        if (unit->type.isWorker() && unit->player == BWAPI::Broodwar->self())
        {
            workerLost(Units::mine(unit->bwapiUnit));
        }
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        // Handle resources mined out
        if (unit->getType().isMineralField())
        {
            auto it = mineralPatchWorkers.find(unit);
            if (it != mineralPatchWorkers.end())
            {
                for (auto &worker : it->second)
                {
                    workerMineralPatch.erase(worker);
                }
                mineralPatchWorkers.erase(it);
            }
        }
    }

    void updateAssignments()
    {
        // Special case on frame 0: try to optimize for earliest possible fifth mineral returned
        if (BWAPI::Broodwar->getFrameCount() == 0)
        {
            assignInitialMineralWorkers();
        }

        for (auto &worker : Units::allMine())
        {
            if (!worker->type.isWorker()) continue;
            if (!worker->completed) continue;

            switch (workerJob[worker])
            {
                case Job::None:
                    workerJob[worker] = Job::Minerals;
                    CherryVis::log(worker->id) << "Assigned to Minerals";
                    // Fall-through

                case Job::Minerals:
                {
                    // If the worker is already assigned to a mineral patch, we don't need to do any more
                    auto mineralPatch = workerMineralPatch[worker];
                    if (mineralPatch) continue;

                    // Release from assigned base if it is mined out
                    auto base = workerBase[worker];
                    if (base && availableMineralAssignmentsAtBase(base) <= 0)
                    {
                        baseWorkers[base].erase(worker);
                        workerBase[worker] = nullptr;
                        base = nullptr;
                    }

                    // If the worker doesn't have an assigned base, assign it one
                    if (!base || !base->resourceDepot || !base->resourceDepot->exists())
                    {
                        base = assignBase(worker, Job::Minerals);

                        // Maybe we have no more with available patches
                        if (!base)
                        {
                            workerJob[worker] = Job::None;
                            continue;
                        }
                    }

                    // Assign a mineral patch if it is close to its assigned base
                    if (worker->getDistance(base->getPosition()) <= 200)
                    {
                        assignMineralPatch(worker);
                    }

                    break;
                }
                case Job::Gas:
                {
                    // If the worker is already assigned to a refinery, we don't need to do any more
                    auto refinery = workerRefinery[worker];
                    if (refinery && refinery->exists()) continue;

                    // If the worker doesn't have an assigned base, assign it one
                    auto base = workerBase[worker];
                    if (!base || !base->resourceDepot || !base->resourceDepot->exists())
                    {
                        base = assignBase(worker, Job::Gas);

                        // Maybe we have no more with available gas
                        if (!base)
                        {
                            workerJob[worker] = Job::None;
                            continue;
                        }
                    }

                    // Assign a refinery if it is close to its assigned base
                    if (worker->getDistance(base->getPosition()) <= 200)
                    {
                        assignRefinery(worker);
                    }

                    break;
                }
                default:
                {
                    // Nothing needed for other cases
                    break;
                }
            }
        }
    }

    void issueOrders()
    {
        // Adjust number of gas workers to desired count
        auto workers = gasWorkers();
        for (int i = workers.first + workers.second; i < _desiredGasWorkers; i++)
        {
            assignGasWorker();
        }
        for (int i = workers.first + workers.second; i > _desiredGasWorkers; i--)
        {
            removeGasWorker();
        }

        for (auto &pair : workerJob)
        {
            auto &worker = pair.first;
            switch (pair.second)
            {
                case Job::Minerals:
                case Job::Gas:
                {
                    // Skip if the worker doesn't have a valid base
                    auto base = workerBase[worker];
                    if (!base || !base->resourceDepot || !base->resourceDepot->exists())
                    {
                        continue;
                    }

                    // If the worker has cargo, return it
                    if (worker->bwapiUnit->isCarryingMinerals() || worker->bwapiUnit->isCarryingGas())
                    {
                        // Handle the special case when the worker is harvesting from a base without a completed depot
                        if (!base->resourceDepot || !base->resourceDepot->completed)
                        {
                            // Find the nearest base with a completed depot
                            int closestTime = INT_MAX;
                            Base *closestBase = nullptr;
                            for (auto otherBase : Map::getMyBases())
                            {
                                if (otherBase == base) continue;
                                if (!otherBase->resourceDepot || !otherBase->resourceDepot->completed) continue;

                                int time = PathFinding::ExpectedTravelTime(worker->lastPosition, otherBase->getPosition(), worker->type);
                                if (time < closestTime)
                                {
                                    closestTime = time;
                                    closestBase = otherBase;
                                }
                            }

                            // If there is one, and the worker can get there and back before the base depot is complete, deliver there
                            if (closestBase)
                            {
                                int baseToBaseTime = PathFinding::ExpectedTravelTime(base->getPosition(),
                                                                                     closestBase->getPosition(),
                                                                                     BWAPI::UnitTypes::Protoss_Probe);
                                if (closestTime + baseToBaseTime < base->resourceDepot->bwapiUnit->getRemainingBuildTime())
                                {
                                    if (worker->getDistance(closestBase->resourceDepot) > 200)
                                    {
                                        worker->moveTo(closestBase->getPosition());
                                    }
                                    else if (worker->bwapiUnit->getOrder() != BWAPI::Orders::ReturnMinerals &&
                                             worker->bwapiUnit->getOrder() != BWAPI::Orders::ReturnGas)
                                    {
                                        worker->rightClick(closestBase->resourceDepot->bwapiUnit);
                                    }
                                    continue;
                                }
                            }

                            // There wasn't another base to return to, or it would take too long, so move towards the preferred base instead
                            if (base->resourceDepot)
                            {
                                worker->moveTo(base->getPosition());
                            }
                            else
                            {
                                worker->moveTo(base->mineralLineCenter);
                            }
                            continue;
                        }

                        // Leave it alone if it already has the return order or is resetting after finishing gathering
                        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::ReturnMinerals ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::ReturnGas ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::ResetCollision)
                        {
                            continue;
                        }

                        worker->returnCargo();
                        continue;
                    }

                    // Handle mining from an assigned mineral patch
                    auto mineralPatch = workerMineralPatch[worker];
                    if (mineralPatch)
                    {
                        if (WorkerOrderTimer::optimizeMineralWorker(worker, mineralPatch)) continue;

                        // If the unit is currently mining, leave it alone
                        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::ResetCollision)
                        {
                            continue;
                        }

                        // If the unit is returning cargo, leave it alone
                        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::ReturnMinerals) continue;

                        // If we don't have vision on the mineral patch, move towards it
                        if (!mineralPatch->exists())
                        {
                            worker->moveTo(mineralPatch->getInitialPosition());
                            continue;
                        }

                        // Check if another worker is currently mining this patch
                        MyUnit otherWorker = nullptr;
                        for (const auto &unit : mineralPatchWorkers[mineralPatch])
                        {
                            if (unit != worker)
                            {
                                otherWorker = unit;
                                break;
                            }
                        }

                        // Resend the gather command when we expect the other worker to be finished mining in 11+LF frames
                        if (otherWorker && otherWorker->exists() && otherWorker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals &&
                            (otherWorker->bwapiUnit->getOrderTimer() + 7) == (11 + BWAPI::Broodwar->getLatencyFrames()))
                        {
                            // Exception: If we are not at the patch yet, and our last command was sent a long time ago,
                            // we probably want to wait and allow our order timer optimization to send the command instead.
                            int dist = worker->bwapiUnit->getDistance(mineralPatch);
                            if (dist > 20 && worker->bwapiUnit->getLastCommandFrame() < (BWAPI::Broodwar->getFrameCount() - 20)) continue;

                            worker->gather(mineralPatch);
                            continue;
                        }

                        // Mineral locking: if the unit is moving to or waiting for minerals, make sure it doesn't switch targets
                        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MoveToMinerals ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
                        {
                            if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
                                && worker->bwapiUnit->getOrderTarget() != mineralPatch
                                && worker->bwapiUnit->getLastCommandFrame()
                                   < (BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames()))
                            {
                                worker->gather(mineralPatch);
                            }

                            continue;
                        }

                        // Otherwise for all other orders click on the mineral patch
                        worker->gather(mineralPatch);
                        continue;
                    }

                    // Handle gathering from an assigned refinery
                    auto refinery = workerRefinery[worker];
                    if (refinery && refinery->exists() && worker->bwapiUnit->getDistance(refinery) < 500)
                    {
                        // If the unit is currently gathering, leave it alone
                        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MoveToGas ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForGas ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::HarvestGas ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::Harvest1 ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::ReturnGas)
                        {
                            continue;
                        }

                        // Otherwise click on the refinery
                        worker->gather(refinery);
                        continue;
                    }

                    // If the worker is a long way from its base, move towards it
                    if (worker->getDistance(base->getPosition()) > 200)
                    {
#if DEBUG_UNIT_ORDERS
                        CherryVis::log(worker->id) << "moveTo: Assigned base (far away)";
#endif
                        worker->moveTo(base->getPosition());
                        continue;
                    }

                    // For some reason the worker doesn't have anything good to do, so let's just move towards the base
#if DEBUG_UNIT_ORDERS
                    CherryVis::log(worker->id) << "moveTo: Assigned base (default)";
#endif
                    worker->moveTo(base->getPosition());
                    break;
                }
                default:
                {
                    // Nothing needed for other cases
                    break;
                }
            }
        }
    }

    bool isAvailableForReassignment(const MyUnit &unit, bool allowCarryMinerals)
    {
        if (!unit || !unit->exists() || !unit->completed || !unit->type.isWorker()) return false;

        auto job = workerJob[unit];
        if (job == Job::None) return true;
        if (job == Job::Minerals)
        {
            if (unit->bwapiUnit->isCarryingGas()) return false;
            if (!allowCarryMinerals && unit->bwapiUnit->isCarryingMinerals()) return false;

            // Don't interrupt a worker that is currently mining, but other states are OK
            return (unit->bwapiUnit->getOrder() == BWAPI::Orders::Move
                    || unit->bwapiUnit->getOrder() == BWAPI::Orders::MoveToMinerals
                    || unit->bwapiUnit->getOrder() == BWAPI::Orders::ReturnMinerals);
        }

        return false;
    }

    MyUnit getClosestReassignableWorker(BWAPI::Position position, bool allowCarryMinerals, int *bestTravelTime)
    {
        int bestTime = INT_MAX;
        MyUnit bestWorker = nullptr;
        for (auto &unit : Units::allMine())
        {
            if (!isAvailableForReassignment(unit, allowCarryMinerals)) continue;

            int travelTime =
                    PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                    position,
                                                    unit->type,
                                                    PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (travelTime < bestTime)
            {
                bestTime = travelTime;
                bestWorker = unit;
            }
        }

        if (bestTravelTime) *bestTravelTime = bestTime;
        return bestWorker;
    }

    std::vector<MyUnit> getBaseWorkers(Base *base)
    {
        std::vector<MyUnit> result;
        result.reserve(baseWorkers[base].size());
        for (auto &worker : baseWorkers[base])
        {
            result.push_back(worker);
        }
        return result;
    }

    void reserveBaseWorkers(std::vector<MyUnit> &workers, Base *base)
    {
        for (auto &worker : baseWorkers[base])
        {
            workers.push_back(worker);
        }

        for (auto &worker : workers)
        {
            reserveWorker(worker);
        }
    }

    void reserveWorker(const MyUnit &unit)
    {
        if (!unit || !unit->exists() || !unit->type.isWorker() || !unit->completed) return;

        workerJob[unit] = Job::Reserved;
        removeFromResource(unit, workerMineralPatch, mineralPatchWorkers);
        removeFromResource(unit, workerRefinery, refineryWorkers);
        CherryVis::log(unit->id) << "Reserved for non-mining duties";
    }

    void releaseWorker(const MyUnit &unit)
    {
        if (!unit || !unit->exists() || !unit->type.isWorker() || !unit->completed) return;

        workerJob[unit] = Job::None;
    }

    int availableMineralAssignments(Base *base)
    {
        if (base) return availableMineralAssignmentsAtBase(base);

        int count = 0;
        for (auto &myBase : Map::getMyBases())
        {
            count += availableMineralAssignmentsAtBase(myBase);
        }

        return count;
    }

    void addDesiredGasWorkers(int gasWorkers)
    {
        _desiredGasWorkers += gasWorkers;
    }

    int desiredGasWorkers()
    {
        return _desiredGasWorkers;
    }

    int mineralWorkers()
    {
        int mineralWorkers = 0;
        for (auto &workerAndAssignedPatch : workerMineralPatch)
        {
            if (workerAndAssignedPatch.first->exists() && workerAndAssignedPatch.second &&
                workerAndAssignedPatch.first->bwapiUnit->getDistance(workerAndAssignedPatch.second)
                < 200) // Don't count workers that are on a long journey towards the patch
            {
                mineralWorkers++;
            }
        }

        return mineralWorkers;
    }

    std::pair<int, int> gasWorkers()
    {
        auto result = std::make_pair(0, 0);
        for (auto &workerAndAssignedRefinery : workerRefinery)
        {
            if (workerAndAssignedRefinery.first->exists() && workerAndAssignedRefinery.second && workerAndAssignedRefinery.second->exists() &&
                workerAndAssignedRefinery.first->bwapiUnit->getDistance(workerAndAssignedRefinery.second)
                < 200) // Don't count workers that are on a long journey towards the refinery
            {
                if (workerAndAssignedRefinery.second->getResources() <= 0)
                {
                    result.second++;
                }
                else
                {
                    result.first++;
                }
            }
        }

        CherryVis::setBoardValue("gasWorkers", (std::ostringstream() << result.first << ":" << result.second).str());

        return result;
    }
}
