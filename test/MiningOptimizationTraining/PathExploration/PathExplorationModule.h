#pragma once

#include <BWAPI/StateCopy.h>

#include "Modules/InstrumentedDoNothingModule.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"

namespace MiningOptimizationTraining
{
    struct PathExplorationModuleOptions
    {
        // Whether to load the optimization training data for the map at startup
        bool loadMapData = true;

        // WHether to save the optimization training data for the map at completion
        bool saveMapData = true;

        // How many cannons to create at each base
        unsigned int cannonsPerBase = 0;

        // Whether to use start block cannons at starting locations
        bool useStartBlockCannonsForStartingLocations = false;
    };

    // Abstract base class for a module that does path exploration
    // This creates all of the depots, pylons, and cannons needed
    // Specializations either create workers and gather or do simulations
    class PathExplorationModule : public InstrumentedDoNothingModule
    {
    public:
        explicit PathExplorationModule(const PathExplorationModuleOptions &options)
            : InstrumentedDoNothingModule(false)
            , simWorker(nullptr)
            , options(options)
            {}

        void onStart() override;
        void onFrame() override;
        void onEnd(bool isWinner) override;

    protected:
        MapData mapData;
        BWAPI::StateCopy initialState;

        // A worker not assigned to any particular role that can be used for running gather simulations
        BWAPI::Unit simWorker;

        // Allows a specialization to run its own initialization after this module is finished with its initialization
        // Should return true when the specialization is finished initializing
        virtual bool initialize() = 0;

        // Called each frame after initialization is complete
        virtual void run() = 0;

    private:
        const PathExplorationModuleOptions &options;
    };
}
