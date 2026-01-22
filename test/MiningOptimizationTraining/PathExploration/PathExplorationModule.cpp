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
        BuildingPlacement::setUseStartBlocksForAllStartingLocations(options.useStartBlockCannonsForStartingLocations);
        BuildingPlacement::initialize();
        BuildingPlacement::update();

        if (options.loadMapData) Serialization::readMapData(mapData);

        Log::Get() << "Initialized mining training on " << BWAPI::Broodwar->mapFileName() << " (" << BWAPI::Broodwar->mapHash() << ")";
    }

    void PathExplorationModule::onFrame()
    {
        InstrumentedDoNothingModule::onFrameStart();

        if (BWAPI::Broodwar->getFrameCount() == 0)
        {
            // First initialization step:
            // - kill all starting workers, blocking neutrals and critters
            // - create an observer at each base so we gain the vision needed to create the depot later
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
        }
        else if (BWAPI::Broodwar->getFrameCount() == 5)
        {
            // Second initialization steps:
            // - create a depot and pylon at each base
            // - create a power pylon in a block with one medium and one small build location
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

            for (auto &pylonLocation :
                    BuildingPlacement::getBuildLocations()[to_underlying(BuildingPlacement::Neighbourhood::MainBase)][2])
            {
                if (pylonLocation.powersMedium.size() > 1)
                {
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                BWAPI::UnitTypes::Protoss_Pylon,
                                                Geo::CenterOfUnit(pylonLocation.location.tile, BWAPI::UnitTypes::Protoss_Pylon));
                }
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 10)
        {
            // Third initialization step:
            // - kill the observers that are no longer needed
            // - create a forge and the sim worker
            std::set<BWAPI::TilePosition> pylons;
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
                else if (unit->getType() == BWAPI::UnitTypes::Protoss_Pylon)
                {
                    pylons.insert(unit->getTilePosition());
                }
            }

            for (auto &pylonLocation :
                    BuildingPlacement::getBuildLocations()[to_underlying(BuildingPlacement::Neighbourhood::MainBase)][2])
            {
                if (pylonLocation.powersMedium.size() > 1 && pylons.contains(pylonLocation.location.tile))
                {
                    auto firstTile = pylonLocation.powersMedium.begin()->location.tile;
                    auto secondTile = (pylonLocation.powersMedium.begin() + 1)->location.tile;
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                BWAPI::UnitTypes::Protoss_Forge,
                                                Geo::CenterOfUnit(firstTile, BWAPI::UnitTypes::Protoss_Forge));
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                BWAPI::UnitTypes::Protoss_Probe,
                                                BWAPI::Position(secondTile) + BWAPI::Position(48, 32));
                    break;
                }
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 15)
        {
            // Fourth initialization step:
            // - reference the sim worker
            // - create the required cannons
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker())
                {
                    simWorker = unit;
                    break;
                }
            }

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

                    for (int builtCannons = 0; builtCannons < options.cannonsPerBase; builtCannons++)
                    {
                        buildCannon();
                    }
                }
            }
        }
        else if (BWAPI::Broodwar->getFrameCount() == 20)
        {
            // Final step: copy the state
            initialState = BWAPI::Broodwar->getStateCopy();
        }
        else if (BWAPI::Broodwar->getFrameCount() >= 25)
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
