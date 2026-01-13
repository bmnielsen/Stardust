#pragma once

#include "BWAPI.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"

#include <BWAPI/SimulateGatherPathResult.h>

namespace MiningOptimizationTraining
{
    struct RotationPath
    {
        BWAPI::ExactPosition startPosition;
        std::set<int> returnResends;
        std::vector<BWAPI::ExactPosition> returnPath;
        std::set<int> gatherResends;
        std::vector<BWAPI::ExactPosition> gatherPath;
    };

    class SimulateGatherPathTester
    {
    public:
        SimulateGatherPathTester(MapData &mapData, BWAPI::Unit worker, BWAPI::Unit patch, BWAPI::Unit depot)
            : worker(worker)
            , patch(patch)
            , depot(depot)
            , state(0)
            , preMiningFrames(0)
            , followingPath(false)
        {}

        void initialize()
        {
            worker->gather(patch);
        }

        void update();

        [[nodiscard]] bool isFinished() const { return false; };

        void outputDebugInformation() const {}

    private:
        BWAPI::Unit worker;
        BWAPI::Unit patch;
        BWAPI::Unit depot;

        // State for the state machine. Possible values:
        // 0 - approaching the patch
        // 1 - pre-mining
        // 2 - mining
        // 3 - approaching the depot
        int state;

        // How many frames the worker has been in the pre-mining state
        unsigned int preMiningFrames;

        // Whether there is a path being followed
        bool followingPath;

        // Frame numbers where resends are planned
        std::set<int> plannedResendFrames;

        // The path we expect to observe, which we trim from the front on each frame
        std::deque<BWAPI::ExactPosition> expectedPath;

        // The expected arrival data (excluding delay) of the previously-planned gather path
        std::unique_ptr<GatherArrivalData> expectedGatherArrivalData;

        // The expected arrival data (excluding delay) of the previously-planned return path
        std::unique_ptr<ReturnArrivalData> expectedReturnArrivalData;

        // The result of the last simulation, used to pass the state on to the next path
        std::unique_ptr<BWAPI::SimulateGatherPathResult> lastSimulationResult;

        // The current rotation path, a copy of the expected paths for return and gather that is saved for future analysis.
        std::unique_ptr<RotationPath> currentRotationPath;
    };

    class PrepareGatherPathTester
    {
    public:
        PrepareGatherPathTester(MapData &mapData, BWAPI::Unit worker, BWAPI::Unit patch, BWAPI::Unit depot)
                : worker(worker)
                , patch(patch)
                , frame(0)
        {}

        void initialize() {}

        void update();

        [[nodiscard]] bool isFinished() const { return frame > 10; }

        void outputDebugInformation() const {}

    private:
        BWAPI::Unit worker;
        BWAPI::Unit patch;

        int frame;
    };
}
