#pragma once

#include "MyWorker.h"
#include "Resource.h"

#define PATCH_OCCUPIED_HORIZON 50

namespace MiningOptimization
{
    class PatchOccupiedForecast
    {
    public:
        friend class OtherPatchesOccupiedForecast;

        PatchOccupiedForecast(Resource patch);

        friend std::ostream &operator<<(std::ostream &out, const PatchOccupiedForecast &f)
        {
            if (f.fullySaturated)
            {
                out << "1.0...";
                return out;
            }

            std::ostringstream os;
            os << std::fixed << std::setprecision(2);
            std::string sep;
            for (int i = 0; i < std::min(10, PATCH_OCCUPIED_HORIZON); i++)
            {
                os << sep << f.start[i];
                if (f.orderProcessIndices.size() == 2) os << "|" << f.middle[i];
                os << "|" << f.end[i];
                sep = ", ";
            }
            out << os.str();
            return out;
        }

        MyWorker miningWorker;
        MyWorker nextMiningWorker;

        std::optional<int> miningWorkerLatestEndFrame;

        // Update the forecast for takeover by the next mining worker at the given frame
        // The given frame should correspond to the frame where the worker transitions to WaitForMinerals
        void useTakeoverFrame(int frame);

        // Update the forecast for patch locking by the next mining worker at the given frame
        // The given frame should correspond to the frame where the worker transitions to WaitForMinerals
        void usePatchLockFrame(int frame);

        // Update the forecast for the given action frame probabilities
        void useActionFrameProbabilities(const std::map<int, double> &actionFramesAndProbabilities);

        // Gets the probability of the patch being mined at the given frame at the time the taking-over worker's orders are processed
        [[nodiscard]] double atFrame(const int frame) const
        {
            if (fullySaturated) return 1.0;
            int frameIdx = frame - startFrame - 1;
            if (frameIdx < 0 || frameIdx >= GATHER_FORECAST_FRAMES) return 0.0;

            // If there is only one worker or the workers have the same order process index, return the average of start and end
            if (orderProcessIndices.size() < 2) return (start[frameIdx] + end[frameIdx]) / 2.0;

            // Next mining worker's orders are processed first
            if (nextMiningWorker->orderProcessIndex > miningWorker->orderProcessIndex)
            {
                return start[frameIdx];
            }

            // Next mining worker's orders are processed last
            return middle[frameIdx];
        }

        // Gets the probability of the patch being mined at the end of the given frame
        [[nodiscard]] double atEndOfFrame(const int frame) const
        {
            if (fullySaturated) return 1.0;
            int frameIdx = frame - startFrame - 1;
            if (frameIdx < 0 || frameIdx >= GATHER_FORECAST_FRAMES) return 0.0;
            return end[frameIdx];
        }

    private:
        Resource patch;
        int startFrame;

        // If true, the forecast is for the patch to be mined through the entire forecast horizon
        bool fullySaturated = false;

        // The order process indices of the worker(s), used to determine which forecast array is relevant for aggregations
        std::set<int> orderProcessIndices;

        // The forecast before the first worker's orders are processed
        std::array<double, PATCH_OCCUPIED_HORIZON> start = {0.0};

        // The forecast between the two workers' orders being processed
        // Unused if there is only one worker
        std::array<double, PATCH_OCCUPIED_HORIZON> middle = {0.0};

        // The forecast after the second worker's orders are processed
        std::array<double, PATCH_OCCUPIED_HORIZON> end = {0.0};
    };
}
