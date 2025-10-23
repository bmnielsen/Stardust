#include "PathExplorationModule.h"

#include "MiningOptimizationTraining/DataModel/Serialization.h"

#include "Units.h"
#include "Map.h"
#include "BuildingPlacement.h"

namespace MiningOptimizationTraining
{
    void PathExplorationModule::reset()
    {
        workerStatuses.clear();
        Serialization::readMapData(mapData);
    }

    void PathExplorationModule::onStart()
    {
        InstrumentedDoNothingModule::onStart();

        // Initialize the minimum needed to have bases, start blocks and cannon placements available
        Units::initialize();
        Map::initialize();
        BuildingPlacement::initialize();

        Log::Get() << "Initialized mining training on " << BWAPI::Broodwar->mapFileName() << " (" << BWAPI::Broodwar->mapHash() << ")";
        CherryVis::setBoardValue("dummy", "yes"); // just setting it so we write the cvis file and don't get errors in the frontend
    }

    void PathExplorationModule::onFrame()
    {
        InstrumentedDoNothingModule::onFrameStart();

        // Ensure all mineral patches keep enough minerals
        if (currentFrame % 500 == 42)
        {
            for (auto unit : BWAPI::Broodwar->getNeutralUnits())
            {
                if (!unit->getType().isMineralField()) continue;
                if (unit->getResources() < 100) unit->setResources(1500);
            }
        }

        if (initialize())
        {
            for (auto &workerStatus : workerStatuses)
            {
                workerStatus.update(mapData);
            }
        }

        InstrumentedDoNothingModule::onFrameEnd();
    }

    void PathExplorationModule::onEnd(bool isWinner)
    {
        Serialization::writeMapData(mapData);
        InstrumentedDoNothingModule::onEnd(isWinner);
    }
}
