#pragma once

#include <BWAPI/StateCopy.h>

#include "Modules/InstrumentedDoNothingModule.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"
#include "MiningOptimizationV2/DataModel/CannonPlacement.h"

namespace MiningOptimizationTraining
{
    struct PathExplorationModuleOptions
    {
        // Whether to load the optimization training data for the map at startup
        bool loadMapData = true;

        // Whether to save the optimization training data for the map at completion
        bool saveMapData = true;

        // Whether to load the optimization training data for the map at startup
        bool loadInitialWorkerMapData = true;

        // Whether to save the optimization training data for the map at completion
        bool saveInitialWorkerMapData = true;
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
            , simWorkerPosition(BWAPI::Positions::Invalid)
            , forgePosition(BWAPI::TilePositions::Invalid)
            {}

        void onStart() override;
        void onFrame() override;
        void onEnd(bool isWinner) override;

    protected:
        MapData mapData;
        InitialWorkerMapData initialWorkerMapData;

        std::map<BWAPI::TilePosition, std::vector<BWAPI::TilePosition>> patchToCannons;
        std::map<BWAPI::TilePosition, std::vector<BWAPI::TilePosition>> patchToStartBlockCannons;

        std::map<BWAPI::TilePosition, std::map<MiningOptimization::CannonPlacement, BWAPI::StateCopy*>> patchToCannonsToStateCopy;

        BWAPI::StateCopy initialStateWithNoCannons;
        BWAPI::StateCopy initialStateWithFirstCannon;
        BWAPI::StateCopy initialStateWithBothCannons;
        BWAPI::StateCopy initialStateWithFirstStartBlockCannon;
        BWAPI::StateCopy initialStateWithBothStartBlockCannons;

        // A worker not assigned to any particular role that can be used for running gather simulations
        BWAPI::Unit simWorker;

        // Allows a specialization to run its own initialization after this module is finished with its initialization
        // Should return true when the specialization is finished initializing
        virtual bool initialize() = 0;

        // Called each frame after initialization is complete
        virtual void run() = 0;

    private:
        const PathExplorationModuleOptions &options;

        BWAPI::Position simWorkerPosition;
        BWAPI::TilePosition forgePosition;

        std::map<Base*, std::pair<BWAPI::TilePosition, std::vector<BWAPI::TilePosition>>> baseToPylonAndCannons;
        std::map<Base*, std::pair<BWAPI::TilePosition, std::vector<BWAPI::TilePosition>>> baseToStartBlockPylonAndCannons;
    };
}
