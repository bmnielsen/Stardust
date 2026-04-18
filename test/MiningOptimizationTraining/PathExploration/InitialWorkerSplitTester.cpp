#include "InitialWorkerSplitTester.h"

namespace MiningOptimizationTraining
{
    void InitialWorkerSplitTesterModule::onStart()
    {
        InstrumentedDoNothingModule::onStart();

        Log::Get() << "Starting initial worker split validation on " << mapData.mapHash << " with enemy race " << knownEnemyRace;

        // Find the best path for each worker on each combination of patches
        std::vector<TilePosition> patches;
        std::map<BWAPI::Unit, std::map<std::pair<TilePosition, TilePosition>, WorkerGatherPlan>> allGatherPlans;
        for (auto worker : BWAPI::Broodwar->self()->getUnits())
        {
            if (!worker->getType().isWorker()) continue;

            auto firstGatherPaths = mapData.startingWorkerPositionToPatchToFirstGatherPath.find(worker->getExactPosition());
            EXPECT_NE(mapData.startingWorkerPositionToPatchToFirstGatherPath.end(), firstGatherPaths)
                << "No first gather paths found for worker starting position " << worker->getExactPosition();

            // Lazily-initialize the vector of patches
            if (patches.empty())
            {
                for (const auto &[patchTile, _] : firstGatherPaths->second)
                {
                    patches.emplace_back(patchTile);
                }
            }

            // Plan all of the patch combinations for this worker
            auto &workerPlans = allGatherPlans[worker];
            for (auto firstPatch : patches)
            {
                for (auto secondPatch : patches)
                {
                    workerPlans.emplace(std::make_pair(firstPatch, secondPatch),
                                        planPatchCombination(worker->getExactPosition(), firstPatch, secondPatch));
                }
            }
        }

        // Select the best combination of patch pairs for all of the workers
        // We do this by evaluating all of the possible combinations and scoring them based on (in order):
        // - earliest 7th collection, capped at frame 300 (so we can build our second worker as early as possible)
        // - fastest average second collection (so the workers are assigned to fast patches after the initial split)
        // TODO: How to handle uncertainty in order process timer resets in the scoring

    }

    void InitialWorkerSplitTesterModule::onFrame()
    {
        InstrumentedDoNothingModule::onFrameStart();

        // Loop through the plans and execute them

        InstrumentedDoNothingModule::onFrameEnd();
    }

    WorkerGatherPlan InitialWorkerSplitTesterModule::planPatchCombination(BWAPI::ExactPosition startPosition,
                                                                          TilePosition firstPatch,
                                                                          TilePosition secondPatch)
    {
        // This method creates the best plan for the given combination of patches for this worker
        // The best plan is the one that gets the earliest second delivery
        // If there is variance in the possible order process timer resets, we use the worst case
        // TODO: Test if it is better to use the average case

        // Implement a stripped-down version of the solver logic
    }
}
