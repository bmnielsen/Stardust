#pragma once

#include "Modules/InstrumentedDoNothingModule.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"
#include "MiningOptimizationV2/DataModel/MapData.h"

namespace MiningOptimizationTraining
{
    struct PlannedPath
    {
        std::set<int> resends;
        std::vector<int> actionFrames;
        std::vector<BWAPI::ExactPosition> nextPathStartPositions;

        [[nodiscard]] bool containsActionFrame(int frame) const
        {
            // If this is a non-plan, just return true
            if (actionFrames.empty()) return true;

            for (auto actionFrame : actionFrames)
            {
                if (frame == actionFrame) return true;
            }
            return false;
        }

        friend std::ostream &operator<<(std::ostream &os, const PlannedPath &path)
        {
            auto outIntCollection = [&os]<typename T>(const T &intCollection)
            {
                os << "[";
                std::string sep;
                for (auto val : intCollection)
                {
                    os << sep << val;
                    sep = ",";
                }
                os << "]";
            };

            os << "resends: ";
            outIntCollection(path.resends);
            os << "; actions: ";
            outIntCollection(path.actionFrames);

            return os;
        }
    };
    struct WorkerGatherPlan
    {
        PlannedPath firstGather;
        PlannedPath firstReturn;
        PlannedPath secondGather;
        PlannedPath secondReturn;
    };

    struct WorkerStatus
    {
        std::optional<WorkerGatherPlan> gatherPlan;
        std::optional<MiningOptimization::InitialSplitData> initialSplitData;
        BWAPI::Unit firstPatch;
        BWAPI::Unit secondPatch;
        std::optional<MiningOptimization::InitialSplitRotation> chosenSecondRotation;

        // 0 - initial state
        // 1 - moving towards first patch
        // 2 - mining first patch
        // 3 - moving towards first delivery
        // 4 - moving towards second patch
        // 5 - mining second patch
        // 6 - moving towards second delivery
        // 7 - final state
        int state = 0;

        [[nodiscard]] bool isResendFrame() const
        {
            int resendFrame = currentFrame + BWAPI::Broodwar->getLatencyFrames();
            if (gatherPlan)
            {
                return gatherPlan->firstGather.resends.contains(resendFrame) ||
                       gatherPlan->firstReturn.resends.contains(resendFrame) ||
                       gatherPlan->secondGather.resends.contains(resendFrame) ||
                       gatherPlan->secondReturn.resends.contains(resendFrame);
            }
            if (initialSplitData)
            {
                return initialSplitData->firstRotation.resendFrames.contains(resendFrame);
            }
            if (chosenSecondRotation)
            {
                return chosenSecondRotation->resendFrames.contains(resendFrame);
            }
            return false;
        }

        [[nodiscard]] bool isFirstGatherActionFrame() const
        {
            if (gatherPlan) return gatherPlan->firstGather.containsActionFrame(currentFrame);
            if (initialSplitData) return initialSplitData->firstRotation.gatherActionFrame == currentFrame;
            return true;
        }

        [[nodiscard]] bool isFirstReturnActionFrame() const
        {
            if (gatherPlan) return gatherPlan->firstReturn.containsActionFrame(currentFrame);
            if (initialSplitData) return initialSplitData->firstRotation.returnActionFrames.contains(currentFrame);
            return true;
        }

        [[nodiscard]] bool isSecondGatherActionFrame() const
        {
            if (gatherPlan) return gatherPlan->secondGather.containsActionFrame(currentFrame);
            if (chosenSecondRotation) return chosenSecondRotation->gatherActionFrame == currentFrame;
            return true;
        }

        [[nodiscard]] bool isSecondReturnActionFrame() const
        {
            if (gatherPlan) return gatherPlan->secondReturn.containsActionFrame(currentFrame);
            if (chosenSecondRotation) return chosenSecondRotation->returnActionFrames.contains(currentFrame);
            return true;
        }
    };

    // Module to test that the trained initial worker split data is correct.
    // It takes the initial four workers and plans the best first two rotations. Along the way it verifies that all of the positions and timings
    // match the trained data.
    // The game will be initialized with the enemy as random since this is how we store the random seeds in our infrastructure. If the caller wishes
    // to test the behaviour as if the bot knew the enemy race, this can be specified in the constructor.
    // It can also be specified to pick random resends in order to test our path exploration.
    class InitialWorkerSplitTesterModule : public InstrumentedDoNothingModule
    {
    public:
        explicit InitialWorkerSplitTesterModule(const InitialWorkerMapData &mapData,
                                                BWAPI::Race enemyRace,
                                                bool chooseRandomResends)
            : mapData(mapData)
            , enemyRace(enemyRace)
            , chooseRandomResends(chooseRandomResends)
        {}

        void onStart() override;
        void onFrame() override;

    private:
        const InitialWorkerMapData &mapData;
        BWAPI::Race enemyRace;
        bool chooseRandomResends;

        std::vector<BWAPI::Unit> workers;
        std::vector<BWAPI::Unit> patches;

        std::map<BWAPI::Unit, WorkerStatus> workerStatuses;

        WorkerGatherPlan planPatchCombinationRandomly(BWAPI::Unit worker, BWAPI::Unit firstPatch, BWAPI::Unit secondPatch);
    };
}
