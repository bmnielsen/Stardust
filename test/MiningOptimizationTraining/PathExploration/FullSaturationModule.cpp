#include "FullSaturationModule.h"

#include "Geo.h"

#include <random>

namespace MiningOptimizationTraining
{
    bool FullSaturationModule::initialize()
    {
        // Initialization steps:
        // - Kill initial workers, blocking neutrals and critters
        // - Add observers at expansions
        // - Add depots at expansions
        // - Add pylons at each base
        // - Add forge at main base
        // - Add required number of cannons
        // - Create workers
        if (BWAPI::Broodwar->getFrameCount() % 10000 == 0)
        {
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker())
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
            }

            workerCreationOrderAndBase.clear();

            if (BWAPI::Broodwar->getFrameCount() == 0)
            {
                for (auto base : Map::allBases())
                {
                    for (const auto &blockingNeutral : base->blockingNeutrals)
                    {
                        BWAPI::Broodwar->killUnit(blockingNeutral);
                    }
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, base->getPosition());
                }

                for (auto unit : BWAPI::Broodwar->getNeutralUnits())
                {
                    if (unit->getType().isCritter()) BWAPI::Broodwar->killUnit(unit);
                }
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 5)
        {
            for (auto base : Map::allBases())
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Nexus,
                                            Geo::CenterOfUnit(base->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus));

                auto &staticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
                if (staticDefenseLocations.powerPylon.isValid())
                {
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                BWAPI::UnitTypes::Protoss_Pylon,
                                                Geo::CenterOfUnit(staticDefenseLocations.powerPylon, BWAPI::UnitTypes::Protoss_Pylon));
                }
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 10)
        {
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
            }

            auto &locations = BuildingPlacement::getBuildLocations()[to_underlying(BuildingPlacement::Neighbourhood::MainBase)][3];
            if (!locations.empty())
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Forge,
                                            Geo::CenterOfUnit(locations.begin()->location.tile, BWAPI::UnitTypes::Protoss_Forge));
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 15)
        {
            for (auto base : Map::allBases())
            {
                auto &staticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
                if (staticDefenseLocations.powerPylon.isValid())
                {
                    auto cannonLocations = std::set<BWAPI::TilePosition>(staticDefenseLocations.workerDefenseCannons.begin(),
                                                                         staticDefenseLocations.workerDefenseCannons.end());

                    auto buildCannon = [&]()
                    {
                        BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
                        int bestDist = INT_MAX;
                        for (auto tile : cannonLocations)
                        {
                            int dist = base->mineralLineCenter.getApproxDistance(Geo::CenterOfUnit(tile,
                                                                                                   BWAPI::UnitTypes::Protoss_Photon_Cannon));
                            if (dist < bestDist)
                            {
                                bestDist = dist;
                                best = tile;
                            }
                        }

                        if (best != BWAPI::TilePositions::Invalid)
                        {
                            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                        BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                        Geo::CenterOfUnit(best, BWAPI::UnitTypes::Protoss_Photon_Cannon));
                            cannonLocations.erase(best);
                        }
                    };

                    for (int builtCannons = 0; builtCannons < cannons; builtCannons++)
                    {
                        buildCannon();
                    }
                }
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() % 10000 == 20)
        {
            // Create workers
            int idx = 0;
            for (auto &base : Map::allBases())
            {
                auto wpDepot = BWAPI::WalkPosition(base->getPosition());
                std::set<std::pair<int, BWAPI::WalkPosition>> positionsByDistToDepot;
                std::set<BWAPI::WalkPosition> availablePositions;
                for (auto tile : base->mineralLineTiles)
                {
                    if (!Map::isWalkable(tile)) continue;

                    for (int x = 0; x < 4; x++)
                    {
                        for (int y = 0; y < 4; y++)
                        {
                            auto here = BWAPI::WalkPosition(tile) + BWAPI::WalkPosition(x, y);
                            if (Geo::EdgeToPointDistance(BWAPI::UnitTypes::Protoss_Nexus,
                                                         base->getPosition(),
                                                         BWAPI::Position(here) + BWAPI::Position(4, 4)) < 4)
                            {
                                continue;
                            }
                            if (BWAPI::Broodwar->isWalkable(here))
                            {
                                availablePositions.insert(here);
                                positionsByDistToDepot.insert(std::make_pair(here.getApproxDistance(wpDepot), here));
                            }
                        }
                    }
                }

                for (int built = 0; built < base->mineralPatchCount(); built++)
                {
                    for (auto [_, start] : positionsByDistToDepot)
                    {
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

                        BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                    BWAPI::UnitTypes::Protoss_Probe,
                                                    Geo::CenterOfUnit(BWAPI::Position(start), BWAPI::UnitTypes::Protoss_Probe));
                        workerCreationOrderAndBase[Geo::CenterOfUnit(BWAPI::Position(start),
                                                                     BWAPI::UnitTypes::Protoss_Probe)] = std::make_pair(++idx, base);

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
        else if (BWAPI::Broodwar->getFrameCount() % 10000 == 23)
        {
            workerStatuses.clear();

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
                if (unit->getType().isWorker()) workers.push_back(unit);
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
                            workerStatuses.emplace_back(mapData, worker, patch, depot);
                            CherryVis::log(worker->getID()) << "Assigned to patch @ " << BWAPI::WalkPosition(patch->getPosition());
                            worker->gather(patch);
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

        return (BWAPI::Broodwar->getFrameCount() % 10000 > 30);
    }
}
