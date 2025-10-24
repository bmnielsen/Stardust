#pragma once

#include "Modules/InstrumentedDoNothingModule.h"
#include "WorkerPathExploration.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"

namespace MiningOptimizationTraining
{
    // Abstract base class for a module that does path exploration
    // Specializations create the workers and depots and set up the mappings to patches
    class PathExplorationModule : public InstrumentedDoNothingModule
    {
    public:
        PathExplorationModule() : InstrumentedDoNothingModule(false) {}

        void onStart() override;

        void onFrame() override;

        void onEnd(bool isWinner) override;

    protected:
        MapData mapData;
        std::vector<WorkerPathExploration> workerStatuses;

        // Function that is called every frame to check if the test is initialized
        // Should return true when the workerStatuses vector is populated and the test is ready to start
        virtual bool initialize() = 0;
    };
}
