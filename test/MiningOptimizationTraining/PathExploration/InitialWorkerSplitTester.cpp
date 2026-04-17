#include "InitialWorkerSplitTester.h"

namespace MiningOptimizationTraining
{
    void InitialWorkerSplitTesterModule::onStart()
    {
        InstrumentedDoNothingModule::onStart();

        Log::Get() << "Starting initial worker split validation on " << mapData.mapHash << " with enemy race " << knownEnemyRace;
    }

    void InitialWorkerSplitTesterModule::onFrame()
    {
        InstrumentedDoNothingModule::onFrameStart();

        InstrumentedDoNothingModule::onFrameEnd();
    }
}
