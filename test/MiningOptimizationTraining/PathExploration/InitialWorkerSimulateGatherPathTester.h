#pragma once

#include <BWAPI/StateCopy.h>

#include "Modules/InstrumentedDoNothingModule.h"

namespace MiningOptimizationTraining
{
    class InitialWorkerSimulateGatherPathTesterModule : public InstrumentedDoNothingModule
    {
    public:
        class WorkerState
        {
        public:
            WorkerState(BWAPI::StateCopy &initialState, BWAPI::Unit worker, BWAPI::Unit firstPatch, BWAPI::Unit secondPatch)
                : initialState(initialState)
                , worker(worker)
                , firstPatch(firstPatch)
                , secondPatch(secondPatch)
                , state(0)
            {}

            void onFrame();

        private:
            BWAPI::StateCopy &initialState;
            BWAPI::Unit worker;
            BWAPI::Unit firstPatch;
            BWAPI::Unit secondPatch;

            // State machine:
            // 0: initial state
            // 1: moving to first patch
            // 2: mining first patch
            // 3: moving to return from first patch
            // 4: moving to second patch
            // 5: mining second patch
            // 6: moving to return from second patch
            // 7: simulation complete
            int state;
        };

        void onStart() override;
        void onFrame() override;
        void onEnd(bool isWinner) override;

    private:
        BWAPI::StateCopy initialState;
        std::vector<WorkerState> workers;
    };
}
