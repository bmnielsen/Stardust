#pragma once

#include "Modules/InstrumentedDoNothingModule.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"
#include "MiningOptimizationTraining/DataModel/Serialization.h"

#include "Units.h"
#include "Map.h"
#include "BuildingPlacement.h"

namespace MiningOptimizationTraining
{
    // Abstract base class for a module that does path exploration
    // Specializations create the workers and depots and set up the mappings to patches
    template <typename WorkerStatusType>
    class PathExplorationModule : public InstrumentedDoNothingModule
    {
    public:
        PathExplorationModule() : InstrumentedDoNothingModule(false) {}

        void onStart() override
        {
            BWAPI::Broodwar->enableMiningTraining();

            InstrumentedDoNothingModule::onStart();

            // Initialize the minimum needed to have bases, start blocks and cannon placements available
            Units::initialize();
            Map::initialize();
            BuildingPlacement::initialize();
            BuildingPlacement::update();

            workerStatuses.clear();
            Serialization::readMapData(mapData);

            Log::Get() << "Initialized mining training on " << BWAPI::Broodwar->mapFileName() << " (" << BWAPI::Broodwar->mapHash() << ")";
        }

        void onFrame() override
        {
            InstrumentedDoNothingModule::onFrameStart();

            // Ensure all mineral patches keep enough minerals
            if (currentFrame % 500 == 42)
            {
                for (auto unit : BWAPI::Broodwar->getNeutralUnits())
                {
                    if (!unit->getType().isMineralField()) continue;
                    if (unit->getResources() < 200) unit->setResources(1500);
                }
            }

            if (initialize())
            {
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
            else
            {
                if ((currentFrame - lastWrite) > 1000)
                {
                    Serialization::writeMapData(mapData);
                    lastWrite = currentFrame;
                }
            }

            InstrumentedDoNothingModule::onFrameEnd();
        }

        void onEnd(bool isWinner) override
        {
            Serialization::writeMapData(mapData);
            InstrumentedDoNothingModule::onEnd(isWinner);
        }

    protected:
        MapData mapData;
        std::vector<std::unique_ptr<WorkerStatusType>> workerStatuses;

        // Function that is called every frame to check if the test is initialized
        // Should return true when the workerStatuses vector is populated and the test is ready to start
        virtual bool initialize() = 0;

    private:
        int lastWrite = 0;
    };
}
