#include "GatheringWorkersModule.h"

#include "SimulateGatherPathTester.h"

#include "Geo.h"

#include <random>

#define DEBUG_LOGGING false

namespace MiningOptimizationTraining
{
    template<typename WorkerStatusType>
    bool GatheringWorkersModule<WorkerStatusType>::initialize()
    {
        if (BWAPI::Broodwar->getFrameCount() % 10000 == 0)
        {
            // Reset the workers every 10000 frames
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker() && unit != simWorker)
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() % 10000 == 25)
        {
            workerCreationOrderAndBase.clear();
            workerStatuses.clear();

            // Gather tiles occupied by cannons
            std::set<BWAPI::TilePosition> cannonTiles;
            for (auto cannon : BWAPI::Broodwar->self()->getUnits())
            {
                if (cannon->getType() != BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;
                cannonTiles.insert(cannon->getTilePosition());
                cannonTiles.insert(cannon->getTilePosition() + BWAPI::TilePosition(1, 0));
                cannonTiles.insert(cannon->getTilePosition() + BWAPI::TilePosition(0, 1));
                cannonTiles.insert(cannon->getTilePosition() + BWAPI::TilePosition(1, 1));
            }

            // Create workers
            int idx = 0;
            for (auto &base : Map::allBases())
            {
#if DEBUG_LOGGING
                Log::Get() << "Base @ " << base->getTilePosition();
#endif

                std::set<std::pair<int, BWAPI::WalkPosition>> positionsByDistToMineralLineCenter;
                std::set<BWAPI::WalkPosition> availablePositions;
                for (auto tile : base->mineralLineTiles)
                {
                    if (!Map::isWalkable(tile)) continue;
                    if (cannonTiles.contains(tile)) continue;

                    for (int x = 0; x < 4; x++)
                    {
                        for (int y = 0; y < 4; y++)
                        {
                            auto here = BWAPI::WalkPosition(tile) + BWAPI::WalkPosition(x, y);
                            if (!BWAPI::Broodwar->isWalkable(here)) continue;

                            availablePositions.insert(here);

                            auto workerCenter = Geo::CenterOfUnit(BWAPI::Position(here), BWAPI::UnitTypes::Protoss_Probe);
                            if (Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Nexus,
                                                        base->getPosition(),
                                                        BWAPI::UnitTypes::Protoss_Probe,
                                                        workerCenter) < 16)
                            {
                                continue;
                            }

                            positionsByDistToMineralLineCenter.insert(std::make_pair(Geo::EdgeToPointDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                                                                              workerCenter,
                                                                                                              base->mineralLineCenter), here));
                        }
                    }
                }

                for (int built = 0; built < base->mineralPatchCount(); built++)
                {
                    for (auto [_, start] : positionsByDistToMineralLineCenter)
                    {
                        BWAPI::Position workerCenter;

                        for (int x = 0; x < 3; x++)
                        {
                            for (int y = 0; y < 3; y++)
                            {
                                if (!availablePositions.contains(start + BWAPI::WalkPosition(x, y)))
                                {
                                    goto nextStartPosition;
                                }
                            }
                        }

                        workerCenter = Geo::CenterOfUnit(BWAPI::Position(start), BWAPI::UnitTypes::Protoss_Probe);
                        BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Probe, workerCenter);
                        workerCreationOrderAndBase[workerCenter] = std::make_pair(++idx, base);

#if DEBUG_LOGGING
                        Log::Get() << "Created worker @ " << workerCenter;
#endif

                        for (int x = 0; x < 3; x++)
                        {
                            for (int y = 0; y < 3; y++)
                            {
                                availablePositions.erase(start + BWAPI::WalkPosition(x, y));
                            }
                        }

                        goto nextWorker;

                        nextStartPosition:;
                    }

                    Log::Get() << "ERROR: Could not build worker " << built << " at base " << base->getTilePosition();

                    nextWorker:;
                }
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() % 10000 == 30)
        {
            // Gather all available mineral assignments for each base, and sort them to ensure stability between test runs
            // Also get the depot for each base
            std::map<Base *, std::vector<Resource>> baseMineralPatches;
            std::map<Base *, BWAPI::Unit> baseDepots;
            for (auto &base : Map::allBases())
            {
                auto patches = base->mineralPatches();
                std::sort(patches.begin(), patches.end(), [](const Resource &a, const Resource &b)
                {
                    return a->tile < b->tile;
                });

                baseMineralPatches[base] = patches;

                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType().isResourceDepot() && unit->getTilePosition() == base->getTilePosition())
                    {
                        baseDepots[base] = unit;
                        break;
                    }
                }
            }

            // Gather workers
            std::vector<BWAPI::Unit> workers;
            for (const auto &unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker() && unit != simWorker)
                {
                    workers.push_back(unit);

#if DEBUG_LOGGING
                    Log::Get() << "Found worker @ " << unit->getPosition();
#endif
                }
            }

            // Sort the workers by location so we get stable behaviour across runs
            std::sort(workers.begin(), workers.end(), [](const BWAPI::Unit &a, const BWAPI::Unit &b)
            {
                return a->getPosition() < b->getPosition();
            });

            // After first iteration, shuffle using same seeds so we vary which workers are assigned to which patch
            auto rng = std::default_random_engine(BWAPI::Broodwar->getFrameCount());
            std::shuffle(std::begin(workers), std::end(workers), rng);

            // Assign a patch to each worker
            for (auto &worker : workers)
            {
                auto it = workerCreationOrderAndBase.find(worker->getPosition());
                if (it != workerCreationOrderAndBase.end())
                {
                    auto &basePatches = baseMineralPatches[it->second.second];
                    if (!basePatches.empty())
                    {
                        auto patch = (*basePatches.rbegin())->getBwapiUnitIfVisible();
                        auto depot = baseDepots[it->second.second];
                        if (!patch || !depot)
                        {
                            Log::Get() << "ERROR: Couldn't get BWAPI unit for patch or depot";
                        }
                        else
                        {
                            if ((options.oneBase.isValid() && options.oneBase != it->second.second->getTilePosition())
                                || (options.onePatch.isValid() && options.onePatch != patch->getTilePosition()))
                            {
                                BWAPI::Broodwar->killUnit(worker);
                            }
                            else
                            {
                                auto workerStatus =
                                        std::make_unique<WorkerStatusType>(mapData, worker, patch, depot, simWorker, initialState);
                                CherryVis::log(worker->getID()) << "Assigned to patch @ " << BWAPI::WalkPosition(patch->getPosition());
                                workerStatus->initialize();
                                workerStatuses.emplace_back(std::move(workerStatus));
                            }
                        }

                        basePatches.pop_back();
                    }
                    else
                    {
                        Log::Get() << "ERROR: Couldn't get base patches for worker @ " << worker->getPosition();
                    }
                }
                else
                {
                    Log::Get() << "ERROR: Couldn't get base for worker @ " << worker->getPosition();
                    BWAPI::Broodwar->leaveGame();
                }
            }
        }

        return (BWAPI::Broodwar->getFrameCount() % 10000 >= 35);
    }

    template<typename WorkerStatusType>
    void GatheringWorkersModule<WorkerStatusType>::run()
    {
        // Ensure all mineral patches keep enough minerals
        if (currentFrame % 500 == 42)
        {
            for (auto unit : BWAPI::Broodwar->getNeutralUnits())
            {
                if (!unit->getType().isMineralField()) continue;
                if (unit->getResources() < 200) unit->setResources(1500);
            }
        }

        for (auto it = workerStatuses.begin(); it != workerStatuses.end(); )
        {
            (*it)->update();
            if ((*it)->isFinished())
            {
                it = workerStatuses.erase(it);
                if (workerStatuses.empty())
                {
                    Log::Get() << "No more workers left; leaving game";
                    BWAPI::Broodwar->leaveGame();
                }
            }
            else
            {
                it++;
            }
        }

        if ((currentFrame % 2000 == 0 && currentFrame % 10000 != 0) || currentFrame % 10000 == 9950)
        {
            for (auto &workerStatus : workerStatuses)
            {
                workerStatus->outputDebugInformation();
            }
        }
    }

    template class GatheringWorkersModule<SimulateGatherPathTester>;
}
