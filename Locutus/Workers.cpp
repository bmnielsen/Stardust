#include "Workers.h"

#include "Base.h"
#include "Units.h"
#include "PathFinding.h"
#include "Map.h"

namespace Workers
{
    namespace
    {
        enum Job { None, Minerals, Gas, Build, Combat, Scout, ReturnCargo };

        std::map<BWAPI::Unit, Job>                      workerJob;
        std::map<BWAPI::Unit, Base*>                    workerBase;
        std::map<Base*, std::set<BWAPI::Unit>>          baseWorkers;
        std::map<BWAPI::Unit, BWAPI::Unit>              workerMineralPatch;
        std::map<BWAPI::Unit, std::set<BWAPI::Unit>>    mineralPatchWorkers;

        // Cleans up data maps when a worker is lost
        void workerLost(BWAPI::Unit unit)
        {
            workerJob.erase(unit);

            auto baseIt = workerBase.find(unit);
            if (baseIt != workerBase.end())
            {
                baseWorkers[baseIt->second].erase(unit);
                workerBase.erase(baseIt);
            }
            
            auto mineralIt = workerMineralPatch.find(unit);
            if (mineralIt != workerMineralPatch.end())
            {
                mineralPatchWorkers[mineralIt->second].erase(unit);
                workerMineralPatch.erase(mineralIt);
            }
        }

        // Assign a worker to the closest base with free mineral patches
        Base* assignBase(BWAPI::Unit unit)
        {
            // TODO: Prioritize empty bases over nearly-full bases
            // TODO: Consider whether or not there is a safe path to the base

            int bestFrames = INT_MAX;
            Base* bestBase = nullptr;
            for (auto & base : Map::allBases())
            {
                if (base.owner != Base::Owner::Me) continue;
                if (!base.resourceDepot || !base.resourceDepot->exists()) continue;

                if (baseWorkers[&base].size() >= base.mineralPatchCount() * 2) continue;

                int dist = PathFinding::GetGroundDistance(unit->getPosition(), base.getPosition(), unit->getType(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
                int frames = (int)((double)dist / unit->getType().topSpeed());

                if (!base.resourceDepot->isCompleted())
                    frames = std::max(frames, base.resourceDepot->getRemainingBuildTime());

                if (frames < bestFrames)
                {
                    bestFrames = frames;
                    bestBase = &base;
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
                if (workers > 0) dist *= 5;

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
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        // Handle lost workers
        if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self())
        {
            workerLost(unit);
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

    void update()
    {
        // FIXME: Handle gas workers

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
                // If the worker doesn't have an assigned base, assign it one
                auto base = workerBase[worker];
                if (!base || !base->resourceDepot || !base->resourceDepot->exists())
                {
                    base = assignBase(worker);
                    if (!base) continue; // Maybe we have no more bases
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
                    Units::getMine(worker).rightClick(base->resourceDepot);
                    continue;
                }

                // If the worker doesn't have an assigned mineral patch, assign it one
                auto mineralPatch = workerMineralPatch[worker];
                if (!mineralPatch || !mineralPatch->exists())
                {
                    mineralPatch = assignMineralPatch(worker);
                    
                    if (!mineralPatch) continue; // Maybe we have no available patches

                    Units::getMine(worker).rightClick(mineralPatch);
                    continue;
                }

                // Mineral lock
                if ((worker->getOrder() == BWAPI::Orders::MoveToMinerals ||
                    worker->getOrder() == BWAPI::Orders::WaitForMinerals) && worker->getOrderTarget() != mineralPatch)
                {
                    Units::getMine(worker).rightClick(mineralPatch);
                }

                break;
            }
        }
    }
}
