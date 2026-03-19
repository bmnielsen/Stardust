#include "PathExplorationModule.h"

#include "Units.h"
#include "Map.h"
#include "BuildingPlacement.h"
#include "UnitUtil.h"

#include "MiningOptimizationTraining/DataModel/Serialization.h"

namespace MiningOptimizationTraining
{
    void PathExplorationModule::onStart()
    {
        BWAPI::Broodwar->enableMiningTraining();

        InstrumentedDoNothingModule::onStart();

        // Initialize the minimum needed to have bases, start blocks and cannon placements available
        Units::initialize();
        Map::initialize();

        if (options.loadMapData) Serialization::readMapData(mapData);

        Log::Get() << "Initialized mining training on " << BWAPI::Broodwar->mapFileName() << " (" << BWAPI::Broodwar->mapHash() << ")";
    }

    void PathExplorationModule::onFrame()
    {
        InstrumentedDoNothingModule::onFrameStart();

        if (BWAPI::Broodwar->getFrameCount() == 0)
        {
            // - kill all starting workers, blocking neutrals and critters
            // - create an observer at each base so we gain the vision needed to create the depot later
            // - find a free position for the sim worker and create an observer to gain the vision needed to create it

            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker())
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
            }

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

            auto bestDist = 0;
            for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
            {
                for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
                {
                    if (!Map::isWalkable(x, y)) continue;

                    auto center = BWAPI::Position(BWAPI::TilePosition(x, y)) + BWAPI::Position(16, 16);
                    int minDist = INT_MAX;
                    for (auto base : Map::allBases())
                    {
                        minDist = std::min(minDist, center.getApproxDistance(base->getPosition()));
                    }

                    if (minDist > bestDist)
                    {
                        simWorkerPosition = center;
                        bestDist = minDist;
                    }
                }
            }
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, simWorkerPosition);
        }
        else if (BWAPI::Broodwar->getFrameCount() == 5)
        {
            // - create a depot at each base
            // - create the sim worker

            for (auto base : Map::allBases())
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Nexus,
                                            Geo::CenterOfUnit(base->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus));
            }

            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        simWorkerPosition);
        }
        else if (BWAPI::Broodwar->getFrameCount() == 10)
        {
            // - kill the observers

            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 15)
        {
            // - reference the sim worker
            // - create the state copy with no cannons

            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker())
                {
                    simWorker = unit;
                    break;
                }
            }

            initialStateWithNoCannons = BWAPI::Broodwar->getStateCopy();
        }
        else if (BWAPI::Broodwar->getFrameCount() == 20)
        {
            // - find a free position for the pylon+forge and create an observer to gain the vision needed to create it
            // - populate the various pylon and cannon data structures

            auto bestDist = 0;
            auto simWorkerTile = BWAPI::TilePosition(simWorkerPosition);
            for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
            {
                for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
                {
                    bool allBuildable = true;
                    for (int dy = 0; dy < 2 && allBuildable; dy++)
                    {
                        for (int dx = 0; dx < 5 && allBuildable; dx++)
                        {
                            auto tile = BWAPI::TilePosition(x + dx, y + dy);
                            if (!Map::isWalkable(tile.x, tile.y) || !BWAPI::Broodwar->isBuildable(tile.x, tile.y) || tile == simWorkerTile)
                            {
                                allBuildable = false;
                            }
                        }
                    }
                    if (!allBuildable) continue;

                    auto center = BWAPI::Position(BWAPI::TilePosition(x, y)) + BWAPI::Position(80, 32);
                    int minDist = INT_MAX;
                    for (auto base : Map::allBases())
                    {
                        minDist = std::min(minDist, center.getApproxDistance(base->getPosition()));
                    }

                    if (minDist > bestDist)
                    {
                        forgePosition = BWAPI::TilePosition(x, y);
                        bestDist = minDist;
                    }
                }
            }
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Observer,
                                        BWAPI::Position(forgePosition) + BWAPI::Position(80, 32));

            // Gather the non-start block locations
            BuildingPlacement::setUseStartBlocksForAllStartingLocations(false);
            BuildingPlacement::initialize();
            BuildingPlacement::update();
            for (auto base : Map::allBases())
            {
                auto &staticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
                if (staticDefenseLocations.powerPylon.isValid() && staticDefenseLocations.workerDefenseCannons.size() > 1)
                {
                    std::vector<BWAPI::TilePosition> cannons = {*staticDefenseLocations.workerDefenseCannons.begin(),
                                                                *(staticDefenseLocations.workerDefenseCannons.begin() + 1)};
                    baseToPylonAndCannons[base] = std::make_pair(staticDefenseLocations.powerPylon, cannons);

                    for (auto &patch : base->mineralPatches())
                    {
                        patchToCannons[patch->tile] = cannons;
                        patchToCannonsToStateCopy[patch->tile][CannonConfiguration::FirstNormalCannon] = nullptr;
                        patchToCannonsToStateCopy[patch->tile][CannonConfiguration::BothNormalCannons] = nullptr;
                    }
                }
            }

            // Gather the start block locations, but only register them if they are different to the normal cannons
            BuildingPlacement::setUseStartBlocksForAllStartingLocations(true);
            BuildingPlacement::initialize();
            BuildingPlacement::update();
            for (auto base : Map::allBases())
            {
                if (!base->isStartingBase()) continue;

                auto &staticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
                if (staticDefenseLocations.powerPylon.isValid() && staticDefenseLocations.workerDefenseCannons.size() > 1)
                {
                    // As long as there are more than 2 cannons, remove the one furthest from the mineral line
                    while (staticDefenseLocations.workerDefenseCannons.size() > 2)
                    {
                        int maxDist = 0;
                        BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
                        for (auto &tile : staticDefenseLocations.workerDefenseCannons)
                        {
                            int dist = base->mineralLineCenter.getApproxDistance(Geo::CenterOfUnit(tile, BWAPI::UnitTypes::Protoss_Photon_Cannon));
                            if (dist > maxDist)
                            {
                                maxDist = dist;
                                best = tile;
                            }
                        }
                        staticDefenseLocations.workerDefenseCannons.erase(
                                std::remove(staticDefenseLocations.workerDefenseCannons.begin(),
                                            staticDefenseLocations.workerDefenseCannons.end(),
                                            best), staticDefenseLocations.workerDefenseCannons.end());
                    }

                    std::vector<BWAPI::TilePosition> cannons = {*staticDefenseLocations.workerDefenseCannons.begin(),
                                                                *(staticDefenseLocations.workerDefenseCannons.begin() + 1)};

                    if (baseToPylonAndCannons.contains(base) && baseToPylonAndCannons[base].second[0] == cannons[0]
                        && baseToPylonAndCannons[base].second[1] == cannons[1])
                    {
                        continue;
                    }

                    baseToStartBlockPylonAndCannons[base] = std::make_pair(staticDefenseLocations.powerPylon, cannons);

                    for (auto &patch : base->mineralPatches())
                    {
                        patchToStartBlockCannons[patch->tile] = cannons;
                        patchToCannonsToStateCopy[patch->tile][CannonConfiguration::FirstStartBlockCannon] = nullptr;
                        patchToCannonsToStateCopy[patch->tile][CannonConfiguration::BothStartBlockCannons] = nullptr;
                    }
                }
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 25)
        {
            // - create the pylon for the forge

            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Pylon,
                                        Geo::CenterOfUnit(forgePosition + BWAPI::TilePosition(3, 0), BWAPI::UnitTypes::Protoss_Pylon));
        }
        else if (BWAPI::Broodwar->getFrameCount() == 30)
        {
            // - create the forge
            // - kill the observer
            // - create the non-start block pylons

            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Forge,
                                        Geo::CenterOfUnit(forgePosition, BWAPI::UnitTypes::Protoss_Forge));
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
            }

            for (auto &[_, locations] : baseToPylonAndCannons)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Pylon,
                                            Geo::CenterOfUnit(locations.first, BWAPI::UnitTypes::Protoss_Pylon));
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 35)
        {
            // - create the first non-start block cannon

            for (auto &[_, locations] : baseToPylonAndCannons)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                            Geo::CenterOfUnit(locations.second[0], BWAPI::UnitTypes::Protoss_Photon_Cannon));
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 40)
        {
            // - create the state copy with the first non-start block cannon
            // - create the second non-start block cannon

            initialStateWithFirstCannon = BWAPI::Broodwar->getStateCopy();

            for (auto &[_, locations] : baseToPylonAndCannons)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                            Geo::CenterOfUnit(locations.second[1], BWAPI::UnitTypes::Protoss_Photon_Cannon));
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 45)
        {
            // - create the state copy with both non-start block cannons
            // - kill the non-start block pylons and cannons

            initialStateWithBothCannons = BWAPI::Broodwar->getStateCopy();

            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
                    (unit->getType() == BWAPI::UnitTypes::Protoss_Pylon && unit->getTilePosition() != (forgePosition + BWAPI::TilePosition(3, 0))))
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 50)
        {
            // - create the start block pylons

            for (auto &[_, locations] : baseToStartBlockPylonAndCannons)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Pylon,
                                            Geo::CenterOfUnit(locations.first, BWAPI::UnitTypes::Protoss_Pylon));
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 55)
        {
            // - create the first start block cannon

            for (auto &[_, locations] : baseToStartBlockPylonAndCannons)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                            Geo::CenterOfUnit(locations.second[0], BWAPI::UnitTypes::Protoss_Photon_Cannon));
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 60)
        {
            // - create the state copy with the first start block cannon
            // - create the second start block cannon

            initialStateWithFirstStartBlockCannon = BWAPI::Broodwar->getStateCopy();

            for (auto &[_, locations] : baseToStartBlockPylonAndCannons)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                            Geo::CenterOfUnit(locations.second[1], BWAPI::UnitTypes::Protoss_Photon_Cannon));
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 65)
        {
            // - create the state copy with both start block cannons
            // - populate the state copy pointers in the data structure

            initialStateWithBothStartBlockCannons = BWAPI::Broodwar->getStateCopy();

            for (auto &[_, map] : patchToCannonsToStateCopy)
            {
                for (auto &[cannonConfiguration, stateCopy] : map)
                {
                    switch (cannonConfiguration)
                    {
                        case CannonConfiguration::FirstNormalCannon:
                            stateCopy = &initialStateWithFirstCannon;
                            break;
                        case CannonConfiguration::BothNormalCannons:
                            stateCopy = &initialStateWithBothCannons;
                            break;
                        case CannonConfiguration::FirstStartBlockCannon:
                            stateCopy = &initialStateWithFirstStartBlockCannon;
                            break;
                        case CannonConfiguration::BothStartBlockCannons:
                            stateCopy = &initialStateWithBothStartBlockCannons;
                            break;
                    }
                }
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() > 65)
        {
            if (initialize())
            {
                run();
            }
        }

        InstrumentedDoNothingModule::onFrameEnd();
    }

    void PathExplorationModule::onEnd(bool isWinner)
    {
        if (options.loadMapData && options.saveMapData) Serialization::writeMapData(mapData);
        InstrumentedDoNothingModule::onEnd(isWinner);
    }
}
