#pragma once

#include "PathExplorationModule.h"
#include "WorkerPathExploration.h"

namespace MiningOptimizationTraining
{
    // Module that just takes one of the initial workers and orders it to mine
    class SingleWorkerModule : public PathExplorationModule
    {
    public:
        explicit SingleWorkerModule(int resendFrame) : resendFrame(resendFrame) {}

    protected:
        bool initialize() override;

    private:
        int resendFrame;
    };
}
